#include <mqueue.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "host.h"
#include "packet.h"
#include "coordinate.h"
#include "worker.h"

// Returns NULL on failure. size is the number of bytes requested.
void* cdt_malloc(size_t size) {
  cdt_host_t *host = cdt_get_host();
  if (!host) {
    debug_print("Host not yet initialized\n");
    return NULL;
  }

  uint32_t num_pages_req = (uint32_t)(size / PAGESIZE);
  if (size % PAGESIZE  != 0) 
    num_pages_req++;

  if (host->manager) {
    printf("Manager trying to malloc\n");
    int start_pte_idx;
    if (cdt_find_unused_pte(&start_pte_idx, host->self_id, num_pages_req) < 0) {
      return NULL;
    }
    // If we've gotten to this point, assume we're holding lock of all PTEs from start_pte_idx to start_pte_idx + num_pages_req
    for (int i = start_pte_idx; i < start_pte_idx + num_pages_req; i++) {
      host->manager_pagetable[i].page = calloc(1, PAGESIZE);
      pthread_mutex_unlock(&host->manager_pagetable[i].lock);
    }
    return (void *)host->manager_pagetable[start_pte_idx].shared_va;
  }
  // Not the manager, so send msg to manager requesting allocation
  cdt_packet_t packet;
  cdt_packet_alloc_req_create(&packet, host->self_id, num_pages_req);
  
  if (cdt_connection_send(&host->peers[0].connection, &packet) != 0) {
    fprintf(stderr, "Failed to send allocation request packet\n");
    return NULL;
  }

  if (mq_receive(host->peers[host->self_id].task_queue, (char*)&packet, sizeof(packet), NULL) == -1) {
    debug_print("Failed to receive a message from manager receiver-thread\n");
    return NULL;
  }

  uint64_t page_address;
  uint32_t resp_num_pages;
  cdt_packet_alloc_resp_parse(&packet, &page_address, &resp_num_pages);
  assert(resp_num_pages == num_pages_req);

  printf("Received packet from manager receiver-thread with shared VA %p and %d pages\n", (void*)page_address, resp_num_pages);

  int pte_idx = SHARED_VA_TO_IDX(page_address);
  // Note: there's probably a race here
  for (int i = pte_idx; i < pte_idx + resp_num_pages; i++) {
    printf("pte idx: %d\n", i);
    pthread_mutex_lock(&host->shared_pagetable[i].lock);
    host->shared_pagetable[i].in_use = 1;
    host->shared_pagetable[i].access = READ_WRITE_PAGE;
    host->shared_pagetable[i].page = calloc(1, PAGESIZE);
    printf("Filled in PTE index %d with new local pointer %p\n", i, host->shared_pagetable[i].page);
    pthread_mutex_unlock(&host->shared_pagetable[i].lock);
  }

  return (void *)page_address;
}

void cdt_free(void *ptr) {
  // TODO
}

int is_shared_va(void * addr) {
  // TODO: ensure the user's malloc never gives them an address in the shared region?
  return (uint64_t)addr >= CDT_SHARED_VA_START && (uint64_t)addr < CDT_SHARED_VA_END;
}

void* cdt_memcpy(void *dest, void *src, size_t n) {
  cdt_host_t *host = cdt_get_host();

  // Write shared mem: src is local and dest is shared
  // TODO: handle the case where we read or write beyond page boundaries - currently assuming only R/W within page bounds
  if (is_shared_va(dest) == 1 &&  is_shared_va(src) == 0) {
    debug_print("write case\n");
    int va_idx = SHARED_VA_TO_IDX(PGROUNDDOWN(dest));
    if (host->manager == 0) {
      pthread_mutex_lock(&host->shared_pagetable[va_idx].lock);
      if (host->shared_pagetable[va_idx].in_use && host->shared_pagetable[va_idx].access == READ_WRITE_PAGE) {
        // Our machine has R/W access to the page, so go ahead and write to the page
        void * local_copy = host->shared_pagetable[va_idx].page;
        uint64_t offset = (uint64_t)dest - PGROUNDDOWN(dest);
        memmove(((void *)(uint64_t)local_copy + offset), src, n);
        pthread_mutex_unlock(&host->shared_pagetable[va_idx].lock);
        return dest;
      } else {
        // We don't have R/W access to the page, so request write access from the manager
        cdt_packet_t packet;
        cdt_packet_write_req_create(&packet, PGROUNDDOWN(dest));
        if (cdt_connection_send(&host->peers[0].connection, &packet) != 0) {
          debug_print("Failed to send write request packet\n");
          return NULL;
        }
        debug_print("Sent write req packet from manager for page %p with idx %d\n", (void *)PGROUNDDOWN(dest), va_idx);

        cdt_packet_t resp_packet;
        if (mq_receive(host->peers[host->self_id].task_queue, (char*)&resp_packet, sizeof(resp_packet), NULL) == -1) {
          debug_print("Failed to receive a message from manager receiver-thread\n");
          return NULL;
        }

        void * page;
        cdt_packet_write_resp_parse(&resp_packet, &page);
        debug_print("Parsed write response packet from manager\n");
        // Update machine PTE access and page
        host->shared_pagetable[va_idx].access = READ_WRITE_PAGE;
        host->shared_pagetable[va_idx].in_use = 1;
        host->shared_pagetable[va_idx].page = calloc(1, PAGESIZE);

        void * local_copy = host->shared_pagetable[va_idx].page;
        memmove(local_copy, page, PAGESIZE);
        uint64_t offset = (uint64_t)dest - PGROUNDDOWN(dest);
        memmove(((void *)(uint64_t)local_copy + offset), src, n);
        
        pthread_mutex_unlock(&host->shared_pagetable[va_idx].lock);
        return dest;
      }
    } else { // Manager is attempting to write
      pthread_mutex_lock(&host->manager_pagetable[va_idx].lock);
      if (host->manager_pagetable[va_idx].in_use && host->manager_pagetable[va_idx].writer >= 0) { // page is in Write mode
        if (host->manager_pagetable[va_idx].writer == host->self_id) { // Manager has R/W access 
          void * local_copy = host->manager_pagetable[va_idx].page;
          uint64_t offset = (uint64_t)dest - PGROUNDDOWN(dest);
          memmove(((void *)(uint64_t)local_copy + offset), src, n);
          pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
          return dest;
        } else {
          // Send request to writer for invalidation and page
          debug_print("Creating and sending invalidation pkt for page %p w index %d\n", (void *)PGROUNDDOWN(dest), va_idx);
          cdt_packet_t invalidation_pkt;
          cdt_packet_write_invalidate_req_create(&invalidation_pkt, PGROUNDDOWN(dest), host->self_id);
          if (cdt_connection_send(&host->peers[host->manager_pagetable[va_idx].writer].connection, &invalidation_pkt) != 0) {
            debug_print("Failed to send write-invalidate request packet\n");
            pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
            return NULL;
          }
          cdt_packet_t resp_packet;
          if (mq_receive(host->peers[host->self_id].task_queue, (char*)&resp_packet, sizeof(resp_packet), NULL) == -1) {
            debug_print("Failed to receive a write-invalidate response message from manager receiver-thread\n");
            return NULL;
          }
          void * page;
          uint32_t requester_id;
          cdt_packet_write_invalidate_resp_parse(&resp_packet, &page, &requester_id);
          debug_print("Parsed write invalidate resp packet\n");
          assert(requester_id == host->self_id);
          // Update mngr PTE access and page
          host->manager_pagetable[va_idx].writer = host->self_id;
          host->shared_pagetable[va_idx].in_use = 1;
          host->shared_pagetable[va_idx].page = calloc(1, PAGESIZE);

          void * local_copy = host->shared_pagetable[va_idx].page;
          memmove(local_copy, page, PAGESIZE);
          debug_print("old page: %s\n", (char *)local_copy);
          uint64_t offset = (uint64_t)dest - PGROUNDDOWN(dest);
          memmove(((void *)(uint64_t)local_copy + offset), src, n);
          debug_print("new page: %s\n", (char *)local_copy);
          
          pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
          return dest;
        }
      } else { // page is in R/O mode
        // Send invalidation requests to all readers
        cdt_packet_t read_inval;
        cdt_packet_read_invalidate_req_create(&read_inval, PGROUNDDOWN(dest), host->self_id);
        for (int i = 0; i < CDT_MAX_MACHINES; i++) {
          if (host->manager_pagetable[va_idx].read_set[i] && i != host->self_id) { 
            if (cdt_connection_send(&host->peers[i].connection, &read_inval) != 0) {
              debug_print("Failed to send read-invalidate request packet to peer %d\n", i);
              pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
              return NULL;
            }
            cdt_packet_t resp_packet;
            if (mq_receive(host->peers[host->self_id].task_queue, (char*)&resp_packet, sizeof(resp_packet), NULL) == -1) {
              pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
              return NULL;
            }
            uint64_t resp_page_addr;
            uint32_t requester_id;
            cdt_packet_read_invalidate_resp_parse(&resp_packet, &resp_page_addr, &requester_id);
            assert(requester_id == host->self_id);
            assert(resp_page_addr == PGROUNDDOWN(dest));
            host->manager_pagetable[va_idx].read_set[i] = 0;
          }
        }
        // Update manager PTE
        assert(host->manager_pagetable[va_idx].read_set[host->self_id] == 1);
        host->manager_pagetable[va_idx].read_set[host->self_id] = 0;
        host->manager_pagetable[va_idx].writer = host->self_id;

        void * local_copy = host->manager_pagetable[va_idx].page;
        debug_print("old page: %s\n", (char *)local_copy);
        uint64_t offset = (uint64_t)dest - PGROUNDDOWN(dest);
        memmove(((void *)(uint64_t)local_copy + offset), src, n);
        debug_print("new page: %s\n", (char *)local_copy);
        
        pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
        return dest;
      }
    }
  }
  // Read shared mem: dest is local and src is shared
  if (is_shared_va(src) == 1 &&  is_shared_va(dest) == 0) {
    debug_print("read case\n");
    int va_idx = SHARED_VA_TO_IDX(PGROUNDDOWN(src));
    if (host->manager == 0) {
      pthread_mutex_lock(&host->shared_pagetable[va_idx].lock);
      if (host->shared_pagetable[va_idx].in_use && host->shared_pagetable[va_idx].access != INVALID_PAGE) {
        // Our machine has R/W access to the page, so go ahead and write to the page
        void * local_copy = host->shared_pagetable[va_idx].page;
        uint64_t offset = (uint64_t)src - PGROUNDDOWN(src);
        memmove(dest, ((void *)(uint64_t)local_copy + offset), n);
        pthread_mutex_unlock(&host->shared_pagetable[va_idx].lock);
        return dest;
      } else {
        // We don't have read access to the page, so request R/O access from the manager
        cdt_packet_t packet;
        cdt_packet_read_req_create(&packet, PGROUNDDOWN(src));
        if (cdt_connection_send(&host->peers[0].connection, &packet) != 0) {
          debug_print("Failed to send write request packet\n");
          return NULL;
        }
        debug_print("Sent read req packet from manager for page %p with idx %d\n", (void *)PGROUNDDOWN(src), va_idx);

        if (mq_receive(host->peers[host->self_id].task_queue, (char*)&packet, sizeof(packet), NULL) == -1) {
          debug_print("Failed to receive a message from manager receiver-thread\n");
          return NULL;
        }

        void * page;
        cdt_packet_read_resp_parse(&packet, &page);
        debug_print("Parsed read response packet from manager\n");
        // Update machine PTE access and page
        host->shared_pagetable[va_idx].access = READ_ONLY_PAGE;
        host->shared_pagetable[va_idx].in_use = 1;
        host->shared_pagetable[va_idx].page = calloc(1, PAGESIZE);

        void * local_copy = host->shared_pagetable[va_idx].page;
        memmove(local_copy, page, PAGESIZE);
        uint64_t offset = (uint64_t)src - PGROUNDDOWN(src);
        memmove(dest, ((void *)(uint64_t)local_copy + offset), n);
        
        pthread_mutex_unlock(&host->shared_pagetable[va_idx].lock);
        return dest;
      }
    }
  }

  // TODO
  // Case 3: src is local and dest is local
  // Case 4: src is shared and dest is shared
  return NULL;
}
