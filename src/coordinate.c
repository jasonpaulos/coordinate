#include <mqueue.h>
#include <stdlib.h>
#include <stdio.h>
#include "host.h"
#include "packet.h"
#include "coordinate.h"

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
    if (cdt_find_unused_pte(&fresh_pte, 0) < 0) {
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

void* cdt_memcpy(void *dest, const void *src, size_t n) {
  // TODO
  return NULL;
}
