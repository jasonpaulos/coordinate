#include "host.h"
#include "packet.h"
#include "worker.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

void* cdt_worker_thread_start(void *arg) {
  cdt_peer_t *peer = (cdt_peer_t*)arg;
  cdt_packet_t packet;

  while (mq_receive(peer->task_queue, (char*)&packet, sizeof(packet), NULL) != -1) {
    int res;

    switch (packet.type) {
    case CDT_PACKET_THREAD_CREATE_REQ:
      res = cdt_worker_thread_create(peer, &packet);
      break;
    case CDT_PACKET_THREAD_ASSIGN_REQ:
      res = cdt_worker_thread_assign(peer, &packet);
      break;
    case CDT_PACKET_READ_REQ:
      res = cdt_worker_read_req(peer, &packet);
      break;
    case CDT_PACKET_WRITE_DEMOTE_REQ:
      res = cdt_worker_write_demote(peer, &packet);
      break;
    case CDT_PACKET_WRITE_REQ:
      debug_print("Received a write request from peer %d\n", peer->id);
      res = cdt_worker_write_req(peer, &packet);
      break;
    case CDT_PACKET_WRITE_INVALIDATE_REQ:
      debug_print("Received a write-invalidate request from peer %d\n", peer->id);
      res = cdt_worker_write_invalidate_req(peer, &packet);
      break;
    case CDT_PACKET_READ_INVALIDATE_REQ:
      debug_print("Received a read-invalidate request from peer %d\n", peer->id);
      res = cdt_worker_read_invalidate_req(peer, &packet);
      break;
    case CDT_PACKET_THREAD_JOIN_REQ:
      res = cdt_worker_thread_join(peer, &packet);
      break;
    // more cases...
    default:
      debug_print("Unexpected packet type: %d\n", packet.type);
      res = -1;
    }

    if (res != 0) {
      debug_print("Peer %d's worker thread failed to handle packet type: %d\n", peer->id, packet.type);
    }
  }

  return NULL;
}

int cdt_worker_read_invalidate_req(cdt_peer_t *sender, cdt_packet_t *packet) {
  uint64_t page_addr;
  uint32_t requester_id;
  cdt_packet_read_invalidate_req_parse(packet, &page_addr, &requester_id);

  cdt_host_t * host = cdt_get_host();
  int va_idx = SHARED_VA_TO_IDX(page_addr);
  pthread_mutex_lock(&host->manager_pagetable[va_idx].lock);

  assert(page_addr - PGROUNDDOWN(page_addr) == 0);
  assert(!host->manager); // The mngr should never receive invalidation requests
  assert(host->shared_pagetable[va_idx].in_use && host->shared_pagetable[va_idx].access == READ_ONLY_PAGE);

  cdt_packet_t resp_pkt;
  cdt_packet_read_invalidate_resp_create(&resp_pkt, page_addr, requester_id);

  if (cdt_connection_send(&sender->connection, &resp_pkt) != 0) {
    debug_print("Failed to send read invalidate response packet to peer %d\n", sender->id);
    pthread_mutex_unlock(&host->shared_pagetable[va_idx].lock);
    return -1;
  }

  host->shared_pagetable[va_idx].access = INVALID_PAGE;
  free(host->shared_pagetable[va_idx].page);
  host->shared_pagetable[va_idx].page = NULL;
  pthread_mutex_unlock(&host->shared_pagetable[va_idx].lock);
  return 0;  
}

int cdt_worker_write_invalidate_req(cdt_peer_t *sender, cdt_packet_t *packet) {
  uint64_t page_addr;
  uint32_t requester_id;
  cdt_packet_write_invalidate_req_parse(packet, &page_addr, &requester_id);

  cdt_host_t * host = cdt_get_host();
  int va_idx = SHARED_VA_TO_IDX(page_addr);
  pthread_mutex_lock(&host->manager_pagetable[va_idx].lock);

  assert(page_addr - PGROUNDDOWN(page_addr) == 0);
  assert(!host->manager); // The mngr should never receive invalidation requests
  assert(host->shared_pagetable[va_idx].in_use && host->shared_pagetable[va_idx].access == READ_WRITE_PAGE);

  cdt_packet_t resp_pkt;
  cdt_packet_write_invalidate_resp_create(&resp_pkt, host->shared_pagetable[va_idx].page, requester_id);

  if (cdt_connection_send(&sender->connection, &resp_pkt) != 0) {
    debug_print("Failed to send write invalidate response packet to peer %d\n", sender->id);
    pthread_mutex_unlock(&host->shared_pagetable[va_idx].lock);
    return -1;
  }

  host->shared_pagetable[va_idx].access = INVALID_PAGE;
  free(host->shared_pagetable[va_idx].page);
  host->shared_pagetable[va_idx].page = NULL;
  pthread_mutex_unlock(&host->shared_pagetable[va_idx].lock);
  return 0;  
}

int cdt_worker_write_req(cdt_peer_t *sender, cdt_packet_t *packet) {
  uint64_t page_addr;
  cdt_packet_write_req_parse(packet, &page_addr);
  assert(page_addr - PGROUNDDOWN(page_addr) == 0);

  int va_idx = SHARED_VA_TO_IDX(page_addr);
  cdt_host_t * host = cdt_get_host();
  pthread_mutex_lock(&host->manager_pagetable[va_idx].lock);
  if (!host->manager_pagetable[va_idx].in_use) {
    debug_print("Got a write request for page %p with idx %d that is not in use in the manager page table\n", (void *)page_addr, va_idx);
    pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
    return -1;
  }
  if (host->manager_pagetable[va_idx].writer >= 0) { // page currently has a writer
    // Request invalidation and a copy of the page from the writer
    debug_print("page idx %d has a writer\n", va_idx);
    if (host->manager_pagetable[va_idx].writer == host->self_id) { // mngr is owner, update PTE and send page
      debug_print("Manager is writer for write request by %d\n", sender->id);
      host->manager_pagetable[va_idx].writer = sender->id;
      cdt_packet_t write_resp_packet;
      cdt_packet_write_resp_create(&write_resp_packet, host->manager_pagetable[va_idx].page);
      
      if (cdt_connection_send(&sender->connection, &write_resp_packet) != 0) {
        debug_print("Failed to send write response packet to peer %d\n", sender->id);
        pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
        return -1;
      }
      free(host->manager_pagetable[va_idx].page);
      host->manager_pagetable[va_idx].page = NULL;
      pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
      return 0;
    } else {
      // Request invalidation and page from writer
      debug_print("Creating and sending invalidation pkt for page %p w index %d with owner %d\n", (void *)PGROUNDDOWN(page_addr), va_idx, host->manager_pagetable[va_idx].writer);
      cdt_packet_t invalidation_pkt;
      cdt_packet_write_invalidate_req_create(&invalidation_pkt, PGROUNDDOWN(page_addr), sender->id);
      if (cdt_connection_send(&host->peers[host->manager_pagetable[va_idx].writer].connection, &invalidation_pkt) != 0) {
        debug_print("Failed to send write-invalidate request packet\n");
        pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
        return -1;
      }
      cdt_packet_t resp_packet;
      if (mq_receive(sender->task_queue, (char*)&resp_packet, sizeof(resp_packet), NULL) == -1) {
        debug_print("Failed to receive a write-invalidate response message from manager receiver-thread\n");
        return -1;
      }
      void * page;
      uint32_t requester_id;
      cdt_packet_write_invalidate_resp_parse(&resp_packet, &page, &requester_id);
      debug_print("Parsed write invalidate resp packet\n");
      assert(requester_id == sender->id);
      // Update mngr PTE access and page
      host->manager_pagetable[va_idx].writer = sender->id;
      host->manager_pagetable[va_idx].in_use = 1;
      host->manager_pagetable[va_idx].page = NULL; // technically should already be null

      cdt_packet_t write_resp;
      cdt_packet_write_resp_create(&write_resp, page);
      if (cdt_connection_send(&sender->connection, &write_resp) != 0) {
        debug_print("Failed to send write response packet\n");
        pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
        return -1;
      }    
      pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
      return 0;
    }

  } else { // Currently in R/O (not tested yet - need to get read working for this)
    // Send invalidation requests to all readers and send the page back to the requester
    cdt_packet_t read_inval;
    cdt_packet_read_invalidate_req_create(&read_inval, page_addr, sender->id);
    for (int i = 0; i < CDT_MAX_MACHINES; i++) {
      if (host->manager_pagetable[va_idx].read_set[i] && i != host->self_id) { 
        if (cdt_connection_send(&host->peers[i].connection, &read_inval) != 0) {
          debug_print("Failed to send read-invalidate request packet to peer %d\n", i);
          pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
          return -1;
        }
        cdt_packet_t resp_packet;
        if (mq_receive(sender->task_queue, (char*)&resp_packet, sizeof(resp_packet), NULL) == -1) {
          pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
          return -1;
        }
        uint64_t resp_page_addr;
        uint32_t requester_id;
        cdt_packet_read_invalidate_resp_parse(&resp_packet, &resp_page_addr, &requester_id);
        assert(requester_id == sender->id);
        assert(resp_page_addr == PGROUNDDOWN(page_addr));
        host->manager_pagetable[va_idx].read_set[i] = 0;
      }
    }
    // Update manager PTE
    assert(host->manager_pagetable[va_idx].read_set[host->self_id] == 1);
    host->manager_pagetable[va_idx].read_set[host->self_id] = 0;
    host->manager_pagetable[va_idx].writer = sender->id;

    // Send page to requester
    cdt_packet_t write_resp;
    cdt_packet_write_resp_create(&write_resp, host->manager_pagetable[va_idx].page);
    if (cdt_connection_send(&sender->connection, &write_resp) != 0) {
      debug_print("Failed to send write response packet\n");
      pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
      return -1;
    }    
    free(host->manager_pagetable[va_idx].page);
    host->manager_pagetable[va_idx].page = NULL;
    pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
    return 0;
  }
  return -1;
}

int cdt_worker_write_demote(cdt_peer_t *sender, cdt_packet_t *packet) {
  cdt_host_t *host = cdt_get_host();

  assert(!host->manager);

  uint64_t page_addr;
  uint32_t requester_id;
  cdt_packet_write_demote_req_parse(packet, &page_addr, &requester_id);
  assert(page_addr - PGROUNDDOWN(page_addr) == 0);

  int va_idx = SHARED_VA_TO_IDX(page_addr);
  // TODO: verify va_idx is valid

  pthread_mutex_lock(&host->shared_pagetable[va_idx].lock);

  if (host->shared_pagetable[va_idx].in_use && host->shared_pagetable[va_idx].access == READ_WRITE_PAGE) {
    host->shared_pagetable[va_idx].access = READ_ONLY_PAGE;
  }

  cdt_packet_write_demote_resp_create(packet, host->shared_pagetable[va_idx].page, requester_id);

  pthread_mutex_unlock(&host->shared_pagetable[va_idx].lock);

  if (cdt_connection_send(&sender->connection, packet) != 0) {
    debug_print("Failed to send write demote response to peer %d\n", sender->id);
    return -1;
  }

  return 0;
}

int cdt_worker_read_req(cdt_peer_t *sender, cdt_packet_t *packet) {
  uint64_t page_addr;
  cdt_packet_read_req_parse(packet, &page_addr);
  assert(page_addr - PGROUNDDOWN(page_addr) == 0);

  int va_idx = SHARED_VA_TO_IDX(page_addr);
  // TODO: verify va_idx is valid

  cdt_host_t * host = cdt_get_host();
  pthread_mutex_lock(&host->manager_pagetable[va_idx].lock);
  if (!host->manager_pagetable[va_idx].in_use) {
    debug_print("Got a read request for page %p with idx %d that is not in use in the manager page table\n", (void *)page_addr, va_idx);
    pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
    return -1;
  }
  if (host->manager_pagetable[va_idx].writer >= 0) { // page currently has a writer
    // Request demotion and a copy of the page from the writer
    uint32_t writer = host->manager_pagetable[va_idx].writer;
    debug_print("page idx %d has writer %d\n", va_idx, writer);
    if (writer == host->self_id) { // mngr is owner, update PTE and send page
      debug_print("Manager is writer for read request by %d\n", sender->id);
      host->manager_pagetable[va_idx].writer = -1;
      host->manager_pagetable[va_idx].read_set[host->self_id] = 1;
      host->manager_pagetable[va_idx].read_set[sender->id] = 1;
      cdt_packet_read_resp_create(packet, host->manager_pagetable[va_idx].page);
      
      if (cdt_connection_send(&sender->connection, packet) != 0) {
        debug_print("Failed to send read response packet to peer %d\n", sender->id);
        pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
        return -1;
      }

      pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
      return 0;
    } else {
      // Request demotion and page from writer
      cdt_packet_write_demote_req_create(packet, page_addr, sender->id);

      if (cdt_connection_send(&host->peers[writer].connection, packet) != 0) {
        debug_print("Failed to send write demote request packet to peer %d\n", writer);
        pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
        return -1;
      }

      if (mq_receive(sender->task_queue, (char*)packet, sizeof(*packet), NULL) == -1) {
        pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
        return -1;
      }

      void *page;
      uint32_t requester_id;
      cdt_packet_write_demote_resp_parse(packet, &page, &requester_id);

      host->manager_pagetable[va_idx].writer = -1;
      host->manager_pagetable[va_idx].read_set[host->self_id] = 1;
      host->manager_pagetable[va_idx].read_set[writer] = 1;
      host->manager_pagetable[va_idx].read_set[sender->id] = 1;

      void *local_page = host->manager_pagetable[va_idx].page = calloc(1, PAGESIZE);
      memmove(local_page, page, PAGESIZE);

      cdt_packet_read_resp_create(packet, local_page);
      
      if (cdt_connection_send(&sender->connection, packet) != 0) {
        debug_print("Failed to send read response packet to peer %d\n", sender->id);
        pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
        return -1;
      }

      pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
      return 0;
    }

  } else { // Currently in R/O
    // Send the page to the requester
    cdt_packet_read_resp_create(packet, host->manager_pagetable[va_idx].page);
    
    if (cdt_connection_send(&sender->connection, packet) != 0) {
      debug_print("Failed to send read response packet to peer %d\n", sender->id);
      pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
      return -1;
    }

    pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
    return 0;

  }
}

int cdt_worker_thread_create(cdt_peer_t *sender, cdt_packet_t *packet) {
  cdt_host_t *host = cdt_get_host();
  if (!host) return -1;

  pthread_mutex_lock(&host->thread_lock);

  cdt_thread_t *new_thread = cdt_worker_do_thread_create(host, sender, packet);

  if (new_thread != NULL) {
    host->thread_counter++;
    cdt_packet_thread_create_resp_create(packet, new_thread);
  } else {
    cdt_thread_t invalid_thread = { .valid = 0 };
    cdt_packet_thread_create_resp_create(packet, &invalid_thread);
  }

  pthread_mutex_unlock(&host->thread_lock);

  if (new_thread == NULL) {
    if (cdt_connection_send(&sender->connection, packet) != 0)
      debug_print("Failed to send thread create response to peer %d\n", sender->id);

    return -1;
  }

  if (cdt_connection_send(&sender->connection, packet) != 0) {
    debug_print("Failed to send thread create response to peer %d\n", sender->id);
    return -1;
  }

  return 0;
}

cdt_thread_t* cdt_worker_do_thread_create(cdt_host_t *host, cdt_peer_t *sender, cdt_packet_t *packet) {
  uint64_t procedure, arg;
  cdt_packet_thread_create_req_parse(packet, &procedure, &arg);

  if (host->num_threads >= host->num_peers) {
    debug_print("Maximum number of threads exceeded\n");
    return NULL;
  }

  cdt_peer_t *idle_peer = NULL;

  for (uint32_t i = 0; i < host->num_peers; i++) {
    cdt_peer_t *peer = host->peers + i;
    if (!peer->thread.valid) {
      idle_peer = peer;
      break;
    }
  }

  if (idle_peer == NULL) {
    debug_print("No idle peers\n");
    return NULL;
  }

  idle_peer->thread.remote_peer_id = idle_peer->id;
  idle_peer->thread.remote_thread_id = host->thread_counter++;

  // TODO: handle case where idle_peer is the manager

  cdt_packet_thread_assign_req_create(packet, sender->id, procedure, arg, idle_peer->thread.remote_thread_id);

  if (cdt_connection_send(&idle_peer->connection, packet) != 0) {
    debug_print("Failed to send thread assign request for thread %d to peer %d\n", idle_peer->thread.remote_thread_id, idle_peer->id);
    return NULL;
  }

  if (mq_receive(sender->task_queue, (char*)packet, sizeof(*packet), NULL) == -1)
    return NULL;

  uint32_t requester, status;
  cdt_packet_thread_assign_resp_parse(packet, &requester, &status);

  if (status != 0) {
    debug_print("Failed to assign thread to peer %d\n", idle_peer->id);
    return NULL;
  }

  idle_peer->thread.valid = 1;

  return &idle_peer->thread;
}

unsigned int cdt_worker_do_thread_assign(cdt_host_t *host, cdt_peer_t *sender,
                                uint32_t parent_id, uint64_t procedure, uint64_t arg, uint32_t thread_id) {
  cdt_peer_t *self = &host->peers[host->self_id];
  if (self->thread.valid)
    return 1;

  self->thread.remote_peer_id = host->self_id;
  self->thread.remote_thread_id = thread_id;
  self->thread.valid = 1;

  if (pthread_create(&self->thread.local_id, NULL, (void*(*)(void*))procedure, (void*)arg) != 0) {
    self->thread.valid = 0;
    return 1;
  }

  return 0;
}

int cdt_worker_thread_assign(cdt_peer_t *sender, cdt_packet_t *packet) {
  cdt_host_t *host = cdt_get_host();
  if (!host) return -1;

  uint32_t parent_id, thread_id;
  uint64_t procedure, arg;
  cdt_packet_thread_assign_req_parse(packet, &parent_id, &procedure, &arg, &thread_id);

  if (sender->id != 0) { // sender must be manager
    cdt_packet_thread_assign_resp_create(packet, parent_id, 1);

    if (cdt_connection_send(&sender->connection, packet)) {
      debug_print("Failed to reject thread assign request from non-manager\n");
      return -1;
    }

    return 0;
  }

  pthread_mutex_lock(&host->thread_lock);

  int res = cdt_worker_do_thread_assign(host, sender, parent_id, procedure, arg, thread_id);
  
  pthread_mutex_unlock(&host->thread_lock);

  cdt_packet_thread_assign_resp_create(packet, parent_id, res);
  if (cdt_connection_send(&sender->connection, packet)) {
    debug_print("Failed to sent thread assign response\n");
    return -1;
  }

  return 0;
}

int cdt_worker_thread_join(cdt_peer_t *sender, cdt_packet_t *packet) {
  cdt_host_t *host = cdt_get_host();
  if (!host) return -1;

  cdt_thread_t thread;
  cdt_packet_thread_join_req_parse(packet, &thread);

  cdt_thread_t self_thread = cdt_thread_self();

  void *return_value = NULL;
  int res;

  if (cdt_thread_equal(&thread, &self_thread)) {
    res = pthread_join(self_thread.local_id, &return_value) != 0;
  } else {
    res = 1;
  }

  cdt_packet_thread_join_resp_create(packet, res, (uint64_t)return_value);
  if (cdt_connection_send(&sender->connection, packet)) {
    debug_print("Failed to sent thread join response\n");
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