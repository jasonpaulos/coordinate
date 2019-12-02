#include "host.h"
#include "packet.h"
#include "worker.h"
#include <assert.h>

void* cdt_worker_thread_create(void *arg) {
  cdt_host_t *host = cdt_get_host();
  if (!host) {
    debug_print("Host not yet started\n");
    return NULL;
  }

  uint64_t procedure, procedure_arg;
  cdt_packet_thread_create_req_parse((cdt_packet_t*)arg, &procedure, &procedure_arg);

  // TODO: assign a peer this thread

  return NULL;
}

/* Finds an unused page table entry and puts the pointer to the PTE in *fresh_pte. 
   IMPORTANT: it returns holding the lock for the unused PTE if it succeeds in finding a fresh PTE.
   Returns 0 if successful in finding an unused PTE, -1 otherwise. */
int cdt_find_unused_pte(cdt_manager_pte_t ** fresh_pte, int peer_id) {
  cdt_host_t *host = cdt_get_host();

  assert(host->manager == 1);
  int i;
  for (i = 0; i < CDT_MAX_SHARED_PAGES; i++) {
    cdt_manager_pte_t * current_pte = &host->manager_pagetable[i];
    if (current_pte->in_use == 0) {
      pthread_mutex_lock(&current_pte->lock);
      if (current_pte->in_use == 0) {
        *fresh_pte = &host->manager_pagetable[i];
        printf("fresh pte shared VA %p\n", (void *)host->manager_pagetable[i].shared_va);
        break;
      }
      // We raced on this PTE and lost, so unlock it and keep looking
      pthread_mutex_unlock(&current_pte->lock);
      continue;
    }
    if (i == CDT_MAX_SHARED_PAGES - 1 && current_pte->in_use != 0) {
      // Note: we COULD go back and keep looping over manager PTEs in hopes that another thread sets a PTE.in_use = 0
      // if we support freeing (which we don't yet)
      debug_print("Failed to find an unused shared page for allocation request from peer from %d\n", peer_id);
      return -1;
    }
  }
  printf("Found empty PTE with index %d and VA %p\n", i, (void *)(*fresh_pte)->shared_va);
  return 0;
}

int cdt_allocate_shared_page(cdt_peer_t * peer) {
  // Find the next not in-use PTE
  cdt_manager_pte_t * fresh_pte;
  if (cdt_find_unused_pte(&fresh_pte, peer->id) < 0) {
    return -1;
  }

  // If we've gotten to this point, assume we're holding fresh_pte's lock
  fresh_pte->in_use = 1;
  fresh_pte->writer = peer->id;

  // Send the new shared VA back to the requester
  cdt_packet_t packet;
  cdt_packet_alloc_resp_create(&packet, fresh_pte->shared_va);
  if (cdt_connection_send(&peer->connection, &packet) != 0) {
    debug_print("Error providing allocated page to peer %d at %s:%d\n", peer->id, peer->connection.address, peer->connection.port);
    cdt_connection_close(&peer->connection);
    return -1;
  }
  printf("Sent packet for allocated pte\n");

  // TODO: should we release the lock earlier than this?
  pthread_mutex_unlock(&fresh_pte->lock);
  return 0;
}