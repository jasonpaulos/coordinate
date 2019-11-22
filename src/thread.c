#include "host.h"
#include "util.h"
#include "thread.h"

#define COORDINATE_LOCAL

extern cdt_host_t* get_host();

int cdt_thread_create_local(cdt_thread_t *thread, void *(*start_routine) (void *), void *arg) {
  return pthread_create(&thread->id, NULL, start_routine, arg);
}

int cdt_thread_create_remote(cdt_thread_t *thread, void *(*start_routine) (void *), void *arg) {
  // TODO
  return -1;
}

int cdt_thread_create(cdt_thread_t *thread, void *(*start_routine) (void *), void *arg) {
  cdt_host_t *host = get_host();

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
  return pthread_join(thread->id, return_value);
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
