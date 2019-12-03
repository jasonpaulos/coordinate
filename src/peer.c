#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include "util.h"
#include "packet.h"
#include "worker.h"
#include "host.h"

int cdt_peer_greet_existing_peer(cdt_host_t *host, int peer_id, const char *peer_address, const char *peer_port) {
  if (!host)
    return -1;

  cdt_peer_t *peer = &host->peers[peer_id];

  if (cdt_connection_connect(&peer->connection, peer_address, peer_port) == -1) {
    debug_print("Error connecting to peer at %s:%s\n", peer_address, peer_port);
    return -1;
  }

  cdt_packet_t packet;
  cdt_packet_existing_peer_create(&packet, host->self_id);
  if (cdt_connection_send(&peer->connection, &packet) != 0) {
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
  cdt_packet_alloc_resp_create(&packet, fresh_pte->shared_va);
  if (cdt_connection_send(&peer->connection, &packet) != 0) {
    debug_print("Error providing allocated page to peer %d at %s:%d\n", peer_id, peer->connection.address, peer->connection.port);
    cdt_connection_close(&peer->connection);
    return -1;
  }
  printf("Sent packet for allocated pte\n");

  // TODO: should we release the lock earlier than this?
  pthread_mutex_unlock(&fresh_pte->lock);
  return 0;
}

int cdt_peer_setup_task_queue(cdt_peer_t *peer) {
  struct mq_attr task_queue_attr = {
    .mq_maxmsg = 10,
    .mq_msgsize = sizeof(cdt_packet_t),
  };

  mq_unlink(cdt_task_queue_names[peer->id]); // close any lingering message queues
  peer->task_queue = mq_open(cdt_task_queue_names[peer->id], O_RDWR | O_CREAT, 0660, &task_queue_attr);
  if (peer->task_queue == -1) {
    debug_print("Failed to create task queue %s for peer %d\n", cdt_task_queue_names[peer->id], peer->id);
    return -1;
  }

  if (peer->id == cdt_get_host()->self_id) // user thread will use this queue not a worker thread
    return 0;

  if (pthread_create(&peer->worker_thread, NULL, cdt_worker_thread_start, (void*)peer) != 0) {
    debug_print("Failed to create worker thread for peer %d\n", peer->id);
    mq_close(peer->task_queue);
    mq_unlink(cdt_task_queue_names[peer->id]);
    return -1;
  }

  return 0;
}

void* cdt_peer_thread(void *arg) {
  cdt_peer_t *peer = (cdt_peer_t*)arg;
  cdt_host_t *host = cdt_get_host();

  if (cdt_peer_setup_task_queue(peer) != 0)
    return NULL;

  while(1) {
    cdt_packet_t packet;
    if (cdt_connection_receive(&peer->connection, &packet) != 0) {
      fprintf(stderr, "Error reading connection from %s:%d\n", peer->connection.address, peer->connection.port);
      break;
    }

    if (packet.type == CDT_PACKET_NEW_PEER && peer->id == 0) { // only the manager (peer 0) can notify of new peers
      uint32_t peer_id;
      char *address, *port;
      cdt_packet_new_peer_parse(&packet, &peer_id, &address, &port);
      
      if (cdt_peer_greet_existing_peer(host, peer_id, address, port) != 0) {
        fprintf(stderr, "Failed to greet new peer at %s:%s\n", address, port);
        break;
      }

      printf("Greeted new peer %d at %s:%s\n", peer_id, address, port);
    } else if (packet.type == CDT_PACKET_ALLOC_REQ && host->manager == 1) { // only the manager can allocate a page
      uint32_t peer_id;
      cdt_packet_alloc_req_parse(&packet, &peer_id);
      printf("Received allocation request from peer %d\n", peer_id);

      if (cdt_allocate_shared_page(peer_id, peer) != 0) {
        fprintf(stderr, "Failed to allocate new page for peer %d\n", peer_id);
        break;
      }
    } else if (packet.type == CDT_PACKET_ALLOC_RESP && peer->id == 0) { // only the manager (peer 0) can respond to allocation reqs
      uint64_t page;
      cdt_packet_alloc_resp_parse(&packet, &page);
      printf("Received allocation response with page %p\n", (void *)page);

      if (mq_send(host->peers[host->self_id].task_queue, (char*)&packet, sizeof(packet), 0) == -1) {
        debug_print("Failed to send allocation response message to main thread\n");
        return NULL;
      }
    } else if (packet.type == CDT_PACKET_THREAD_CREATE_RESP && peer->id == 0) { // only the manager (peer 0) can respond to thread create reqs
      if (mq_send(host->peers[host->self_id].task_queue, (char*)&packet, sizeof(packet), 0) == -1) {
        debug_print("Failed to send thread create response message to worker thread: %s\n", strerror(errno));
      }
    } else if (packet.type % 2 == 1) {
      uint32_t requester_id = cdt_packet_response_get_requester(&packet);
      if (mq_send(host->peers[requester_id].task_queue, (char*)&packet, sizeof(packet), 0) == -1) {
        debug_print("Failed to send response packet to worker thread %d: %s\n", requester_id, strerror(errno));
      }
    } else {
      if (mq_send(host->peers[peer->id].task_queue, (char*)&packet, sizeof(packet), 0) == -1) {
        debug_print("Failed to send request packet to worker thread: %s\n", strerror(errno));
      }
    }
  }
  
  return NULL;
}

int cdt_peer_start(cdt_peer_t *peer) {
  return pthread_create(&peer->read_thread, NULL, cdt_peer_thread, (void*)peer);
}

void cdt_peer_join(cdt_peer_t *peer) {
  pthread_join(peer->read_thread, NULL);
}
