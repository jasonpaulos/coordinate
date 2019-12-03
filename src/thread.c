#include <stdlib.h>
#include "host.h"
#include "packet.h"
#include "worker.h"

cdt_thread_t cdt_thread_self() {
#ifdef COORDINATE_LOCAL
  cdt_thread_t self = {
    .local_id = pthread_self()
  };
  return self;
#else
  cdt_host_t *host = cdt_get_host();
  if (!host) {
    debug_print("Host not yet started");
    exit(-1);
  }

  cdt_thread_t *self_thread = &host->peers[host->self_id].thread;
  if (!self_thread->valid) {
    debug_print("Host thread not yet started");
    exit(-1);
  }

  return *self_thread;
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
int cdt_thread_create_remote(cdt_host_t *host, cdt_thread_t *thread, uint64_t start_routine, uint64_t arg) {
  cdt_packet_t packet;
  cdt_packet_thread_create_req_create(&packet, start_routine, arg);

  if (host->manager) {
    cdt_thread_t *new_thread = cdt_worker_do_thread_create(host, &host->peers[0], &packet);
    if (new_thread == NULL)
      return -1;
    
    *thread = *new_thread;

    return 0;
  }

  if (cdt_connection_send(&host->peers[0].connection, &packet) != 0) {
    debug_print("Failed to send thread create request to manager\n");
    return -1;
  }

  cdt_peer_t *self = &host->peers[host->self_id];
  if (mq_receive(self->task_queue, (char*)&packet, sizeof(packet), NULL) == -1) {
    debug_print("Failed to receive thread create response\n");
    return -1;
  }

  cdt_packet_thread_create_resp_parse(&packet, thread);

  if (!thread->valid) {
    debug_print("Failed to create thread\n");
    return -1;
  }

  self->thread = *thread;

  return 0;
}

int cdt_thread_create(cdt_thread_t *thread, void *(*start_routine) (void *), void *arg) {
#ifdef COORDINATE_LOCAL
  return cdt_thread_create_local(thread, start_routine, arg);
#else
  cdt_host_t *host = cdt_get_host();
  if (!host) {
    debug_print("Host not yet started\n");
    return -1;
  }

  pthread_mutex_lock(&host->thread_lock);

  if (host->num_threads >= host->num_peers) {
    debug_print("Maximum number of threads exceeded\n");
    return -1;
  }

  int res = cdt_thread_create_remote(host, thread, (uint64_t)start_routine, (uint64_t)arg);

  if (res == 0)
    host->num_threads++;

  pthread_mutex_unlock(&host->thread_lock);

  return res;
#endif
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
