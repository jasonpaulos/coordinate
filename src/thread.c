#include <stdlib.h>
#include "packet.h"
#include "host.h"
#include "util.h"

cdt_thread_t cdt_thread_self() {
#ifdef COORDINATE_LOCAL
  cdt_thread_t self = {
    .local_id = pthread_self()
  };
  return self;
#else
  cdt_host_t *host = cdt_get_host();
  if (!host || !host->self_thread) {
    debug_print("Host thread not yet started");
    exit(-1);
  }

  return *host->self_thread;
#endif
}

int cdt_thread_equal(cdt_thread_t *t1, cdt_thread_t *t2) {
#ifdef COORDINATE_LOCAL
  return pthread_equal(t1->local_id, t2->local_id);
#else
  return t1->remote_peer_id == t2->remote_peer_id && t1->remote_thread_id == t2->remote_thread_id;
#endif
}

int cdt_thread_create_local(cdt_thread_t *thread, void *(*start_routine) (void *), void *arg) {
  return pthread_create(&thread->local_id, NULL, start_routine, arg);
}

/**
 * host->thread_lock MUST be held before calling this
 */
int cdt_thread_create_remote(cdt_host_t *host, cdt_thread_t *thread, void *(*start_routine) (void *), void *arg) {
  if (!host->manager) {
    cdt_packet_t packet;
    cdt_packet_thread_create_req_create(&packet, (uint64_t)start_routine, (uint64_t)arg);

    if (cdt_connection_send(&host->peers[0].connection, &packet) != 0) {
      debug_print("Failed to send thread create request to manager\n");
      return -1;
    }

    // TODO: wait for CDT_PACKET_THREAD_CREATE_RESP

    // cdt_packet_thread_create_resp_parse(&packet, thread);

    // TODO: add thread to host->threads

    return 0;
  }

  uint32_t thread_mask = 0;
  for (uint32_t i = 0; i < CDT_MAX_THREADS; i++) {
    if (host->threads[i].valid) {
      thread_mask |= 1 << host->threads[i].remote_peer_id;
    }
  }

  uint32_t idle_peer = CDT_MAX_MACHINES;

  for (uint32_t i = 0; i < CDT_MAX_MACHINES; i++) {
    if (!(thread_mask & (1 << i))) {
      idle_peer = i;
      break;
    }
  }

  if (idle_peer == CDT_MAX_MACHINES) { // no idle peers
    return -1;
  }

  thread->remote_peer_id = idle_peer;
  thread->remote_thread_id = host->thread_counter++;

  cdt_packet_t packet;
  cdt_packet_thread_assign_req_create(&packet, (uint64_t)start_routine, (uint64_t)arg, thread->remote_thread_id);

  if (cdt_connection_send(&host->peers[idle_peer].connection, &packet) != 0) {
    debug_print("Failed to send thread assign request for thread %d to peer %d\n", thread->remote_thread_id, idle_peer);
    return -1;
  }

  // TODO: wait for CDT_PACKET_THREAD_ASSIGN_RESP

  return -1;
}

int cdt_thread_create(cdt_thread_t *thread, void *(*start_routine) (void *), void *arg) {
  cdt_host_t *host = cdt_get_host();
  if (!host) {
    debug_print("Host not yet started");
    return -1;
  }

  pthread_mutex_lock(&host->thread_lock);

  if (host->num_threads >= host->num_peers) {
    debug_print("Maximum number of threads exceeded");
    return -1;
  }

  cdt_thread_t *system_thread = NULL;

  for (int i = 0; i < CDT_MAX_MACHINES; i++) {
    if (!host->threads[i].valid) {
      system_thread = &host->threads[i];
      break;
    }
  }

  if (system_thread == NULL) {
    debug_print("Error: no idle threads. Maybe num_threads has drifted?");
    return -1;
  }

  int res;
#ifdef COORDINATE_LOCAL
  res = cdt_thread_create_local(thread, start_routine, arg);
#else
  res = cdt_thread_create_remote(host, thread, start_routine, arg);
#endif

  if (res == 0) {
    host->num_threads++;
    *thread = *system_thread;
  }

  pthread_mutex_unlock(&host->thread_lock);

  return res;
}

int cdt_thread_join_local(cdt_thread_t *thread, void **return_value) {
  return pthread_join(thread->local_id, return_value);
}

int cdt_thread_join_remote(cdt_thread_t *thread, void **return_value) {
  // TODO
  return -1;
}

int cdt_thread_join(cdt_thread_t *thread, void **return_value) {
#ifdef COORDINATE_LOCAL
  return cdt_thread_join_local(thread, return_value);
#else
  return cdt_thread_join_remote(thread, return_value);
#endif
}
