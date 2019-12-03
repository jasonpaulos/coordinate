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
  if (!host) {
    debug_print("Host not yet started\n");
    return -1;
  }

  uint64_t procedure, arg;
  cdt_packet_thread_create_req_parse(packet, &procedure, &arg);

  pthread_mutex_lock(&host->thread_lock);

  if (host->num_threads >= host->num_peers) {
    debug_print("Maximum number of threads exceeded\n");
    pthread_mutex_unlock(&host->thread_lock);
    // TODO send response with failure
    return -1;
  }

  cdt_peer_t *idle_peer = NULL;

  for (uint32_t i = 0; i < CDT_MAX_MACHINES; i++) {
    cdt_peer_t *peer = host->peers + i;
    if (!peer->thread.valid) {
      idle_peer = peer;
      break;
    }
  }

  if (idle_peer == NULL) {
    debug_print("No idle peers\n");
    pthread_mutex_unlock(&host->thread_lock);
    // TODO send response with failure
    return -1;
  }

  idle_peer->thread.remote_peer_id = idle_peer->id;
  idle_peer->thread.remote_thread_id = host->thread_counter++;

  cdt_packet_thread_assign_req_create(packet, procedure, arg, idle_peer->thread.remote_thread_id);

  if (cdt_connection_send(&idle_peer->connection, packet) != 0) {
    debug_print("Failed to send thread assign request for thread %d to peer %d\n", idle_peer->thread.remote_thread_id, idle_peer->id);
    pthread_mutex_unlock(&host->thread_lock);
    return -1;
  }

  if (mq_receive(sender->task_queue, (char*)packet, sizeof(*packet), NULL) == -1) {
    pthread_mutex_unlock(&host->thread_lock);
    return -1;
  }

  uint32_t status;
  cdt_packet_thread_assign_resp_parse(packet, &status);

  if (status != 0) {
    debug_print("Failed to assign thread to peer %d\n", idle_peer->id);
    pthread_mutex_unlock(&host->thread_lock);
    return -1;
  }

  idle_peer->thread.valid = 1;

  pthread_mutex_unlock(&host->thread_lock);

  return 0;
}
