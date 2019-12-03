#include <mqueue.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "host.h"
#include "packet.h"
#include "coordinate.h"
#include "worker.h"

extern mqd_t qd_manager_peer_thread;

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
    fresh_pte->page = calloc(1, PAGESIZE);
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
  host->shared_pagetable[pte_idx].page = calloc(1, PAGESIZE);
  printf("Filled in PTE index %d with new local pointer %p\n", pte_idx, host->shared_pagetable[pte_idx].page);
  pthread_mutex_unlock(&host->shared_pagetable[pte_idx].lock);

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
  // Write shared mem: src is local and dest is shared
  // TODO: handle the case where we read or write beyond page boundaries - currently assuming only R/W within page bounds
  if (is_shared_va(dest) == 1 &&  is_shared_va(src) == 0) {
    debug_print("write case\n");
    cdt_host_t * host = cdt_get_host();
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
          fprintf(stderr, "Failed to send allocation request packet\n");
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
        
        pthread_mutex_unlock(&host->shared_pagetable[va_idx].lock);
        return dest;
      }
    } else { // Manager is attempting to write
      debug_print("manager write case\n");
      pthread_mutex_lock(&host->manager_pagetable[va_idx].lock);
      if (host->manager_pagetable[va_idx].in_use && host->manager_pagetable[va_idx].writer == host->self_id) {
        // Our machine has R/W access to the page, so go ahead and write to the page
        void * local_copy = host->manager_pagetable[va_idx].page;
        uint64_t offset = (uint64_t)dest - PGROUNDDOWN(dest);
        memmove(((void *)(uint64_t)local_copy + offset), src, n);
        pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
        return dest;
      }
    }
  }
  // Read shared mem: dest is local and src is shared
  if (is_shared_va(src) == 1 &&  is_shared_va(dest) == 0) {
    
  }

  // TODO
  // Case 3: src is local and dest is local
  // Case 4: src is shared and dest is shared
  return NULL;
}
