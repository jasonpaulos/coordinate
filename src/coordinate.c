#include <mqueue.h>
#include <stdlib.h>
#include "coordinate.h"

extern cdt_host_t host;
extern cdt_connection_t manager_connection;
extern mqd_t qd_manager_peer_thread;

// Returns NULL on failure
void* cdt_malloc(size_t size) {
  if (host.manager) {
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
  if (cdt_packet_alloc_req_create(&packet, host.self_id) != 0) {
    fprintf(stderr, "Cannot create allocation request packet\n");
    return NULL;
  }
  if (cdt_connection_send(&manager_connection, &packet) != 0) {
    fprintf(stderr, "Failed to send allocation request packet\n");
    return NULL;
  }

  cdt_message_t allocation_response;

  if (mq_receive (qd_manager_peer_thread, (char *)&allocation_response, sizeof(allocation_response), NULL) == -1) {
    debug_print("Failed to receive a message from manager peer-thread\n");
    return NULL;
  }

  printf("Received message from manager peer-thread with type %d and shared VA %p\n", 
    allocation_response.type, (void *)allocation_response.shared_va);

  int pte_idx = SHARED_VA_TO_IDX(allocation_response.shared_va);
  printf("pte idx: %d\n", pte_idx);
  pthread_mutex_lock(&host.shared_pagetable[pte_idx].lock);
  host.shared_pagetable[pte_idx].in_use = 1;
  host.shared_pagetable[pte_idx].access = READ_WRITE_PAGE;
  host.shared_pagetable[pte_idx].page = malloc(PAGESIZE);
  printf("Filled in PTE index %d with new local pointer %p\n", pte_idx, host.shared_pagetable[pte_idx].page);
  pthread_mutex_unlock(&host.shared_pagetable[pte_idx].lock);

  return (void *)allocation_response.shared_va;
}

void cdt_free(void *ptr) {
  // TODO
}

void* cdt_memcpy(void *dest, const void *src, size_t n) {
  // TODO
  return NULL;
}
