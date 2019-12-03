#include "host.h"
#include "packet.h"
#include "worker.h"

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

  cdt_peer_t *self = &host->peers[host->self_id];
  if (self->thread.valid) {
    pthread_mutex_unlock(&host->thread_lock);
    
    cdt_packet_thread_assign_resp_create(packet, parent_id, 1);

    if (cdt_connection_send(&sender->connection, packet)) {
      debug_print("Failed to reject thread assign request because of already running thread\n");
      return -1;
    }

    return 0;
  }

  self->thread.remote_peer_id = host->self_id;
  self->thread.remote_thread_id = thread_id;
  self->thread.valid = 1;

  if (pthread_create(&self->thread.local_id, NULL, (void*(*)(void*))procedure, (void*)arg) != 0) {
    self->thread.valid = 0;
    pthread_mutex_unlock(&host->thread_lock);

    debug_print("Failed to start thread for procedure %p and arg %p\n", (void*)procedure, (void*)arg);

    cdt_packet_thread_assign_resp_create(packet, parent_id, 1);

    if (cdt_connection_send(&sender->connection, packet)) {
      debug_print("Failed to reject thread assign request\n");
      return -1;
    }

    return 0;
  }

  pthread_mutex_unlock(&host->thread_lock);

  cdt_packet_thread_assign_resp_create(packet, parent_id, 0);

  if (cdt_connection_send(&sender->connection, packet)) {
    debug_print("Failed to send thread assignment confirmation\n");
    return -1;
  }

  return 0;
}
