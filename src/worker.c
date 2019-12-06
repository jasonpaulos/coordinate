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
      res = cdt_worker_write_req(peer, &packet);
      break;
    case CDT_PACKET_WRITE_INVALIDATE_REQ:
      res = cdt_worker_write_invalidate_req(peer, &packet);
      break;
    case CDT_PACKET_READ_INVALIDATE_REQ:
      res = cdt_worker_read_invalidate_req(peer, &packet);
      break;
    case CDT_PACKET_THREAD_JOIN_REQ:
      res = cdt_worker_thread_join(peer, &packet);
      break;
    case CDT_PACKET_ALLOC_REQ:
      res = cdt_allocate_shared_page(peer, &packet);
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
  pthread_mutex_lock(&host->shared_pagetable[va_idx].lock);

  assert(page_addr - PGROUNDDOWN(page_addr) == 0);
  assert(!host->manager); // The mngr should never receive invalidation requests
  assert(host->shared_pagetable[va_idx].in_use);
  assert(host->shared_pagetable[va_idx].access == READ_ONLY_PAGE);

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
  pthread_mutex_lock(&host->shared_pagetable[va_idx].lock);

  assert(page_addr - PGROUNDDOWN(page_addr) == 0);
  assert(!host->manager); // The mngr should never receive invalidation requests
  assert(host->shared_pagetable[va_idx].in_use);
  assert(host->shared_pagetable[va_idx].access == READ_WRITE_PAGE);

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
    if (host->manager_pagetable[va_idx].writer == host->self_id) { // mngr is owner, update PTE and send page
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
        pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
        return -1;
      }
      void * page;
      uint32_t requester_id;
      cdt_packet_write_invalidate_resp_parse(&resp_packet, &page, &requester_id);
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

  } else {
    // Send invalidation requests to all readers and send the page back to the requester
    int read_count = 0;
    cdt_packet_t packet;
    cdt_packet_read_invalidate_req_create(&packet, page_addr, sender->id);
    for (int i = 0; i < CDT_MAX_MACHINES; i++) {
      if (host->manager_pagetable[va_idx].read_set[i] && i != host->self_id && i != sender->id) {
        read_count++;
        if (cdt_connection_send(&host->peers[i].connection, &packet) != 0) {
          debug_print("Failed to send read-invalidate request packet to peer %d\n", i);
          pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
          return -1;
        }
      }
    }

    for (int j = 0; j < read_count; j++) {
      if (mq_receive(sender->task_queue, (char*)&packet, sizeof(packet), NULL) == -1) {
        pthread_mutex_unlock(&host->manager_pagetable[va_idx].lock);
        return -1;
      }
      uint64_t resp_page_addr;
      uint32_t requester_id;
      cdt_packet_read_invalidate_resp_parse(&packet, &resp_page_addr, &requester_id);
      assert(requester_id == sender->id);
      assert(resp_page_addr == PGROUNDDOWN(page_addr));
    }

    for (int p = 0; p < CDT_MAX_MACHINES; p++)
      host->manager_pagetable[va_idx].read_set[p] = 0;
    
    host->manager_pagetable[va_idx].writer = sender->id;

    // Send page to requester
    cdt_packet_write_resp_create(&packet, host->manager_pagetable[va_idx].page);
    if (cdt_connection_send(&sender->connection, &packet) != 0) {
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
    if (writer == host->self_id) { // mngr is owner, update PTE and send page
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

int cdt_find_unused_pte(uint32_t peer_id, uint32_t num_pages) {
  cdt_host_t *host = cdt_get_host();

  assert(host->manager == 1);

  unsigned int first_unalloc_page = __atomic_fetch_add(&host->manager_first_unallocated_pg_idx, num_pages, __ATOMIC_SEQ_CST);
  if (first_unalloc_page + num_pages > CDT_MAX_SHARED_PAGES)
    return -1;
  
  for (unsigned int i = first_unalloc_page; i < first_unalloc_page + num_pages; i++) {
    pthread_mutex_lock(&host->manager_pagetable[i].lock);
    host->manager_pagetable[i].in_use = 1;
    host->manager_pagetable[i].writer = peer_id;
  }

  return first_unalloc_page;
}

int cdt_allocate_shared_page(cdt_peer_t * sender, cdt_packet_t *packet) {
  cdt_host_t * host = cdt_get_host();
  uint32_t peer_id;
  uint32_t num_pages;
  cdt_packet_alloc_req_parse(packet, &peer_id, &num_pages);
  // Find the next not in-use PTE
  int start_pte_idx = cdt_find_unused_pte(peer_id, num_pages);
  uint64_t page_addr;

  if (start_pte_idx >= 0) {
    page_addr = host->manager_pagetable[start_pte_idx].shared_va;

    // Release all held locks
    for (int j = start_pte_idx; j < start_pte_idx + num_pages; j++) {
      pthread_mutex_unlock(&host->manager_pagetable[j].lock);
    }
  } else {
    page_addr = 0;
    num_pages = 0;
  }

  cdt_packet_alloc_resp_create(packet, page_addr, num_pages);

  if (cdt_connection_send(&sender->connection, packet) != 0) {
    debug_print("Error providing allocated page to peer %d at %s:%d\n", sender->id, sender->connection.address, sender->connection.port);
    return -1;
  }

  return 0;
}