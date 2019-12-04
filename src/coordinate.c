#include <mqueue.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "host.h"
#include "packet.h"
#include "coordinate.h"
#include "worker.h"

// Returns NULL on failure
void* cdt_malloc(size_t size) {
  cdt_host_t *host = cdt_get_host();
  if (!host) {
    debug_print("Host not yet initialized\n");
    return NULL;
  }

  if (host->manager) {
    printf("Manager trying to malloc\n");
    cdt_manager_pte_t * fresh_pte;
    if (cdt_find_unused_pte(&fresh_pte, host->self_id) < 0) {
      return NULL;
    }
    // If we've gotten to this point, assume we're holding fresh_pte's lock
    fresh_pte->in_use = 1;
    fresh_pte->writer = 0;
    fresh_pte->page = malloc(PAGESIZE);
    pthread_mutex_unlock(&fresh_pte->lock);
    return (void *)fresh_pte->shared_va;
    return NULL;

  }
  // Not the manager, so send msg to manager requesting allocation
  cdt_packet_t packet;
  cdt_packet_alloc_req_create(&packet, host->self_id);
  
  if (cdt_connection_send(&host->peers[0].connection, &packet) != 0) {
    fprintf(stderr, "Failed to send allocation request packet\n");
    return NULL;
  }

  if (mq_receive(host->peers[host->self_id].task_queue, (char*)&packet, sizeof(packet), NULL) == -1) {
    debug_print("Failed to receive a message from manager receiver-thread\n");
    return NULL;
  }

  uint64_t page_address;
  cdt_packet_alloc_resp_parse(&packet, &page_address);

  printf("Received packet from manager receiver-thread with shared VA %p\n", (void*)page_address);

  int pte_idx = SHARED_VA_TO_IDX(page_address);
  printf("pte idx: %d\n", pte_idx);
  pthread_mutex_lock(&host->shared_pagetable[pte_idx].lock);
  host->shared_pagetable[pte_idx].in_use = 1;
  host->shared_pagetable[pte_idx].access = READ_WRITE_PAGE;
  host->shared_pagetable[pte_idx].page = malloc(PAGESIZE);
  printf("Filled in PTE index %d with new local pointer %p\n", pte_idx, host->shared_pagetable[pte_idx].page);
  pthread_mutex_unlock(&host->shared_pagetable[pte_idx].lock);

  return (void *)page_address;
}

void cdt_free(void *ptr) {
  // TODO
}

int is_shared_va(const void * addr) {
  // TODO: ensure the user's malloc never gives them an address in the shared region?
  return (uint64_t)addr >= CDT_SHARED_VA_START && (uint64_t)addr < CDT_SHARED_VA_END;
}

int cdt_copyout(void *dest, const void *src, size_t n) {
  cdt_host_t *host = cdt_get_host();

  int start_va_idx = SHARED_VA_TO_IDX(PGROUNDDOWN(dest));
  int end_va_idx = SHARED_VA_TO_IDX(PGROUNDDOWN(dest + n - 1));

  if (host->manager == 0) {
    for (int i = start_va_idx; i <= end_va_idx; i++) {
      cdt_host_pte_t *pte = &host->shared_pagetable[i];
      uint64_t page_addr = pte->shared_va;
      uint64_t offset = i == start_va_idx ? (uint64_t)dest - PGROUNDDOWN(dest) : 0;
      size_t length = i == end_va_idx ? (uint64_t)dest + n - PGROUNDDOWN(dest + n - 1) : PAGESIZE;
      const void *src_addr = src + (i - start_va_idx) * PAGESIZE + offset;

      if (pte->in_use && pte->access == READ_WRITE_PAGE) {
        // Our machine has R/W access to the page, so go ahead and write to the page
        void * local_copy = pte->page + offset;

        memmove(local_copy, src_addr, length);
      } else {
        // We don't have R/W access to the page, so request write access from the manager
        cdt_packet_t packet;
        cdt_packet_write_req_create(&packet, page_addr);
        if (cdt_connection_send(&host->peers[0].connection, &packet) != 0)
          return -1;

        debug_print("Sent write req packet from manager for page %p with idx %d\n", (void *)page_addr, i);

        if (mq_receive(host->peers[host->self_id].task_queue, (char*)&packet, sizeof(packet), NULL) == -1)
          return -1;

        void * page;
        cdt_packet_write_resp_parse(&packet, &page);
        debug_print("Parsed write response packet for page %p from manager\n", (void*)page_addr);
        // Update machine PTE access and page
        pte->access = READ_WRITE_PAGE;
        pte->in_use = 1;
        void *local_copy = pte->page = malloc(PAGESIZE);

        memmove(local_copy, page, PAGESIZE);

        local_copy += offset;

        memmove(local_copy, src_addr, length);
      }
    }
  } else {
    // Manager is attempting to write
    for (int i = start_va_idx; i <= end_va_idx; i++) {
      cdt_manager_pte_t *pte = &host->manager_pagetable[i];
      uint64_t page_addr = SHARED_IDX_TO_VA(i);
      uint64_t offset = i == start_va_idx ? (uint64_t)dest - PGROUNDDOWN(dest) : 0;
      size_t length = i == end_va_idx ? (uint64_t)dest + n - PGROUNDDOWN(dest + n - 1) : PAGESIZE;
      const void *src_addr = src + (i - start_va_idx) * PAGESIZE + offset;

      if (pte->in_use && pte->writer >= 0) {
        // page is in write mode
        if (pte->writer == host->self_id) {
          // manager has R/W access
          void *local_copy = pte->page + offset;
          memmove(local_copy, src_addr, length);
        } else {
          // Send request to writer for invalidation and page
          debug_print("Creating and sending invalidation pkt for page %p w index %d\n", (void *)page_addr, i);

          cdt_packet_t packet;
          cdt_packet_write_invalidate_req_create(&packet, page_addr, host->self_id);
          if (cdt_connection_send(&host->peers[pte->writer].connection, &packet) != 0)
            return -1;
          
          if (mq_receive(host->peers[host->self_id].task_queue, (char*)&packet, sizeof(packet), NULL) == -1)
            return -1;
          
          void *page;
          uint32_t requester_id;
          cdt_packet_write_invalidate_resp_parse(&packet, &page, &requester_id);
          debug_print("Parsed write invalidate resp packet for page %p\n", (void*)page_addr);
          assert(requester_id == host->self_id);

          // Update mngr PTE access and page
          pte->writer = host->self_id;
          pte->in_use = 1;
          pte->page = malloc(PAGESIZE);

          void *local_copy = pte->page;
          memmove(local_copy, page, PAGESIZE);

          local_copy += offset;

          memmove(local_copy, src_addr, length);
        }
      } else { // page is in R/O mode
        // Send invalidation requests to all readers
        int read_count = 0;
        cdt_packet_t packet;
        cdt_packet_read_invalidate_req_create(&packet, page_addr, host->self_id);
        for (int p = 0; p < CDT_MAX_MACHINES; p++) {
          if (p != host->self_id && pte->read_set[p]) {
            read_count++;
            if (cdt_connection_send(&host->peers[p].connection, &packet) != 0)
              return -1;
          }
        }

        for (int j = 0; j < read_count; j++) {
          if (mq_receive(host->peers[host->self_id].task_queue, (char*)&packet, sizeof(packet), NULL) == -1)
            return -1;
          
          uint64_t resp_page_addr;
          uint32_t requester_id;
          cdt_packet_read_invalidate_resp_parse(&packet, &resp_page_addr, &requester_id);
          assert(requester_id == host->self_id);
          assert(resp_page_addr == page_addr);
        }

        for (int p = 0; p < CDT_MAX_MACHINES; p++) {
          if (p != host->self_id)
            pte->read_set[p] = 0;
        }

        // Update manager PTE
        assert(pte->read_set[host->self_id] == 1);
        pte->read_set[host->self_id] = 0;
        pte->writer = host->self_id;

        void *local_copy = pte->page + offset;
        memmove(local_copy, src_addr, length);
      }
    }
  }

  return 0;
}

int cdt_copyin(void *dest, const void *src, size_t n) {
  cdt_host_t *host = cdt_get_host();

  int start_va_idx = SHARED_VA_TO_IDX(PGROUNDDOWN(src));
  int end_va_idx = SHARED_VA_TO_IDX(PGROUNDDOWN(src + n - 1));

  if (host->manager == 0) {
    for (int i = start_va_idx; i <= end_va_idx; i++) {
      cdt_host_pte_t *pte = &host->shared_pagetable[i];
      uint64_t page_addr = pte->shared_va;
      uint64_t offset = i == start_va_idx ? (uint64_t)src - PGROUNDDOWN(src) : 0;
      size_t length = i == end_va_idx ? (uint64_t)src + n - PGROUNDDOWN(src + n - 1) : PAGESIZE;
      void *dest_addr = dest + (i - start_va_idx) * PAGESIZE + offset;

      if (pte->in_use && pte->access != INVALID_PAGE) {
        // Our machine has R/W access to the page, so go ahead and write to the page
        void *local_copy = pte->page + offset;
        memmove(dest_addr, local_copy, length);
      } else {
        // We don't have read access to the page, so request R/O access from the manager
        cdt_packet_t packet;
        cdt_packet_read_req_create(&packet, page_addr);
        if (cdt_connection_send(&host->peers[0].connection, &packet) != 0)
          return -1;

        debug_print("Sent read req packet from manager for page %p with idx %d\n", (void *)page_addr, i);

        if (mq_receive(host->peers[host->self_id].task_queue, (char*)&packet, sizeof(packet), NULL) == -1)
          return -1;

        void *page;
        cdt_packet_read_resp_parse(&packet, &page);
        debug_print("Parsed read response packet from manager\n");

        // Update machine PTE access and page
        pte->access = READ_ONLY_PAGE;
        pte->in_use = 1;
        void *local_copy = pte->page = malloc(PAGESIZE);

        memmove(local_copy, page, PAGESIZE);

        local_copy += offset;
        
        memmove(dest_addr, local_copy, length);
      }
    }
  } else {
    // Manager is attempting to read
    for (int i = start_va_idx; i <= end_va_idx; i++) {
      cdt_manager_pte_t *pte = &host->manager_pagetable[i];
      uint64_t page_addr = SHARED_IDX_TO_VA(i);
      uint64_t offset = i == start_va_idx ? (uint64_t)src - PGROUNDDOWN(src) : 0;
      size_t length = i == end_va_idx ? (uint64_t)src + n - PGROUNDDOWN(src + n - 1) : PAGESIZE;
      void *dest_addr = dest + (i - start_va_idx) * PAGESIZE + offset;

      if (pte->in_use && pte->writer >= 0) {
        // page is in write mode
        if (pte->writer == host->self_id) {
          // manager has R/W access
          void *local_copy = pte->page + offset;
          memmove(dest_addr, local_copy, length);
        } else {
          // Send request to writer for demotion and page
          cdt_packet_t packet;
          cdt_packet_write_demote_req_create(&packet, page_addr, host->self_id);

          if (cdt_connection_send(&host->peers[pte->writer].connection, &packet) != 0)
            return -1;

          if (mq_receive(host->peers[host->self_id].task_queue, (char*)&packet, sizeof(packet), NULL) == -1)
            return -1;
          
          void *page;
          uint32_t requester_id;
          cdt_packet_write_demote_resp_parse(&packet, &page, &requester_id);

          pte->writer = -1;
          pte->read_set[host->self_id] = 1;
          pte->read_set[pte->writer] = 1;

          void *local_page = pte->page = malloc(PAGESIZE);
          memmove(local_page, page, PAGESIZE);

          local_page += offset;

          memmove(dest_addr, local_page, length);
        }
      } else {
        // Currently in R/O
        void *local_page = pte->page + offset;
        memmove(dest_addr, local_page, length);
      }
    }
  }

  return 0;
}

void* cdt_memcpy(void *dest, const void *src, size_t n) {
  cdt_host_t *host = cdt_get_host();

  // src and dest are both local
  if (is_shared_va(dest) == 0 && is_shared_va(src) == 0) {
    return memcpy(dest, src, n);
  }

  // Write shared mem: src is local and dest is shared
  if (is_shared_va(dest) == 1 &&  is_shared_va(src) == 0) {
    debug_print("write case\n");
    int start_va_idx = SHARED_VA_TO_IDX(PGROUNDDOWN(dest));
    int end_va_idx = SHARED_VA_TO_IDX(PGROUNDDOWN(dest + n - 1));

    for (int i = start_va_idx; i <= end_va_idx; i++) {
      pthread_mutex_lock(host->manager ? &host->manager_pagetable[i].lock : &host->shared_pagetable[i].lock);
    }

    int res = cdt_copyout(dest, src, n);

    for (int i = start_va_idx; i <= end_va_idx; i++) {
      pthread_mutex_unlock(host->manager ? &host->manager_pagetable[i].lock : &host->shared_pagetable[i].lock);
    }

    return res != 0 ? NULL : dest;
  }
  // Read shared mem: dest is local and src is shared
  if (is_shared_va(src) == 1 &&  is_shared_va(dest) == 0) {
    debug_print("read case\n");
    int start_va_idx = SHARED_VA_TO_IDX(PGROUNDDOWN(src));
    int end_va_idx = SHARED_VA_TO_IDX(PGROUNDDOWN(src + n - 1));

    for (int i = start_va_idx; i <= end_va_idx; i++) {
      pthread_mutex_lock(host->manager ? &host->manager_pagetable[i].lock : &host->shared_pagetable[i].lock);
    }

    int res = cdt_copyin(dest, src, n);

    for (int i = start_va_idx; i <= end_va_idx; i++) {
      pthread_mutex_unlock(host->manager ? &host->manager_pagetable[i].lock : &host->shared_pagetable[i].lock);
    }

    return res != 0 ? NULL : dest;
  }

  // src is shared and dest is shared
  void *buffer = malloc(n);
  if (!buffer)
    return NULL;
  
  int res = cdt_memcpy(buffer, src, n) != NULL && cdt_memcpy(dest, buffer, n) != NULL;

  free(buffer);

  return res == 0 ? NULL : dest;
}
