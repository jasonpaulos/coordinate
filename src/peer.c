#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <mqueue.h>
#include "util.h"
#include "packet.h"
#include "host.h"
#include "message.h"

extern cdt_host_t host;

int cdt_peer_greet_existing_peer(cdt_host_t *host, int peer_id, const char *peer_address, const char *peer_port) {
  cdt_peer_t *peer = &host->peers[peer_id];

  if (cdt_connection_connect(&peer->connection, peer_address, peer_port) == -1) {
    debug_print("Error connecting to peer at %s:%s\n", peer_address, peer_port);
    return -1;
  }

  cdt_packet_t packet;
  if (cdt_packet_existing_peer_create(&packet, host->self_id) != 0 || cdt_connection_send(&peer->connection, &packet) != 0) {
    debug_print("Error greeting peer %d at %s:%s\n", peer_id, peer_address, peer_port);
    cdt_connection_close(&peer->connection);
    return -1;
  }

  peer->id = peer_id;
  host->num_peers++;
  
  if (cdt_peer_start(peer) != 0) {
    peer->id = 0;
    host->num_peers--;
    debug_print("Error starting thread for peer %d (%s:%s)\n", peer_id, peer_address, peer_port);
    cdt_connection_close(&peer->connection);
    return -1;
  }

  return 0;
}

/* Finds an unused page table entry and puts the pointer to the PTE in *fresh_pte. 
   IMPORTANT: it returns holding the lock for the unused PTE if it succeeds in finding a fresh PTE.
   Returns 0 if successful in finding an unused PTE, -1 otherwise. */
int cdt_find_unused_pte(cdt_manager_pte_t ** fresh_pte, int peer_id) {
  assert(host.manager == 1);
  int i;
  for (i = 0; i < CDT_MAX_SHARED_PAGES; i++) {
    cdt_manager_pte_t * current_pte = &host.manager_pagetable[i];
    if (current_pte->in_use == 0) {
      pthread_mutex_lock(&current_pte->lock);
      if (current_pte->in_use == 0) {
        *fresh_pte = &host.manager_pagetable[i];
        printf("fresh pte shared VA %p\n", (void *)host.manager_pagetable[i].shared_va);
        break;
      }
      // We raced on this PTE and lost, so unlock it and keep looking
      pthread_mutex_unlock(&current_pte->lock);
      continue;
    }
    if (i == CDT_MAX_SHARED_PAGES - 1 && current_pte->in_use != 0) {
      // Note: we COULD go back and keep looping over manager PTEs in hopes that another thread sets a PTE.in_use = 0
      // if we support freeing (which we don't yet)
      fprintf(stderr, "Failed to find an unused shared page for allocation request from peer from %d\n", peer_id);
      return -1;
    }
  }
  printf("Found empty PTE with index %d and VA %p\n", i, (void *)(*fresh_pte)->shared_va);
  return 0;
}

int cdt_allocate_shared_page(int peer_id, cdt_peer_t * peer) {
  // Find the next not in-use PTE
  cdt_manager_pte_t * fresh_pte;
  if (cdt_find_unused_pte(&fresh_pte, peer_id) < 0) {
    return -1;
  }

  // If we've gotten to this point, assume we're holding fresh_pte's lock
  fresh_pte->in_use = 1;
  fresh_pte->writer = peer_id;

  // Send the new shared VA back to the requester
  cdt_packet_t packet;
  if (cdt_packet_alloc_resp_create(&packet, fresh_pte->shared_va) != 0 || cdt_connection_send(&peer->connection, &packet) != 0) {
    debug_print("Error providing allocated page to peer %d at %s:%d\n", peer_id, peer->connection.address, peer->connection.port);
    cdt_connection_close(&peer->connection);
    return -1;
  }
  printf("Sent packet for allocated pte\n");

  // TODO: should we release the lock earlier than this?
  pthread_mutex_unlock(&fresh_pte->lock);
  return 0;
}

void* cdt_peer_thread(void *arg) {
  cdt_peer_t *peer = (cdt_peer_t*)arg;
  // queue descriptor for message queue TO the main thread (W/O) - only valid if this is the manage peer-thread
  mqd_t qd_main_thread;   
  // Check if this is the manager peer-thread - if so, open up the message queue to the main thread
  if (peer->id == 0) {
    if ((qd_main_thread = mq_open (MAIN_MANAGER_QUEUE_NAME, O_WRONLY)) == -1) {
        debug_print("Main thread failed to create message queue to manager peer-thread\n");
        return NULL; // TODO: is there a better failure return val?
    }
  }

  while(1) {
    cdt_packet_t packet;
    if (cdt_connection_receive(&peer->connection, &packet) != 0) {
      fprintf(stderr, "Error reading connection from %s:%d\n", peer->connection.address, peer->connection.port);
      break;
    }

    if (packet.type == CDT_PACKET_NEW_PEER && peer->id == 0) { // only the manager (peer 0) can notify of new peers
      int peer_id;
      char *address, *port;
      if (cdt_packet_new_peer_parse(&packet, &peer_id, &address, &port) != 0) {
        fprintf(stderr, "Failed to parse new peer packet from %s:%d\n", peer->connection.address, peer->connection.port);
        break;
      }
      
      if (cdt_peer_greet_existing_peer(peer->host, peer_id, address, port) != 0) {
        fprintf(stderr, "Failed to greet new peer at %s:%s\n", address, port);
        break;
      }

      printf("Greeted new peer %d at %s:%s\n", peer_id, address, port);
    }

    if (packet.type == CDT_PACKET_ALLOC_REQ && host.manager == 1) { // only the manager can allocate a page
      int peer_id;
      if (cdt_packet_alloc_req_parse(&packet, &peer_id) != 0) {
        fprintf(stderr, "Failed to parse allocation request packet from %s:%d\n", peer->connection.address, peer->connection.port);
        break;
      }
      printf("Received allocation request from peer %d\n", peer_id);

      if (cdt_allocate_shared_page(peer_id, peer) != 0) {
        fprintf(stderr, "Failed to allocate new page for peer %d\n", peer_id);
        break;
      }
    }

    if (packet.type == CDT_PACKET_ALLOC_RESP) { 
      assert(peer->id == 0);
      uint64_t page;
      if (cdt_packet_alloc_resp_parse(&packet, &page) != 0) {
        fprintf(stderr, "Failed to parse allocation response packet from %s:%d\n", peer->connection.address, peer->connection.port);
        break;
      }
      printf("Received allocation response with page %p\n", (void *)page);

      cdt_message_t allocation_message;
      allocation_message.type = ALLOCATE_RESP;
      allocation_message.shared_va = page;

      if (mq_send (qd_main_thread, (char *)&allocation_message, sizeof(allocation_message), 0) == -1) {
        debug_print ("Failed to send allocation response message to main thread\n");
        return NULL;
      }
      printf("Sent allocation response message to main thread\n");
    }

    // TODO: handle other packets
  }
  
  return NULL;
}

int cdt_peer_start(cdt_peer_t *peer) {
  return pthread_create(&peer->read_thread, NULL, cdt_peer_thread, (void*)peer);
}

void cdt_peer_join(cdt_peer_t *peer) {
  pthread_join(peer->read_thread, NULL);
}
