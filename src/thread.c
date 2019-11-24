 #include <stdlib.h>
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

int cdt_thread_create_remote(cdt_thread_t *thread, void *(*start_routine) (void *), void *arg) {
  cdt_host_t *host = cdt_get_host();
  if (!host) {
    debug_print("Host not yet started");
    return -1;
  }

  if (host->manager) {
    
  }

  return -1;
}

int cdt_thread_create(cdt_thread_t *thread, void *(*start_routine) (void *), void *arg) {
  cdt_host_t *host = cdt_get_host();
  if (!host) {
    debug_print("Host not yet started");
    return -1;
  }

  // TODO: acquire host threads lock

  if (host->num_threads >= host->num_peers) {
    debug_print("Maximum number of threads exceeded");
    return -1;
  }

  for (int i = 0; i < CDT_MAX_MACHINES; i++) {
    if (host->threads[i] == NULL) {
      host->threads[i] = thread;
      break;
    }
  }

  int res;
#ifdef COORDINATE_LOCAL
  res = cdt_thread_create_local(thread, start_routine, arg);
#else
  res = cdt_thread_create_remote(thread, start_routine, arg);
#endif

  // TODO: release host threads lock

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
