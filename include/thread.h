#ifndef COORDINATE_THREAD_H
#define COORDINATE_THREAD_H

#include <stdint.h>
#include <pthread.h>

typedef struct cdt_thread_t {
  int valid;

  pthread_t local_id;

  uint32_t remote_peer_id;
  uint32_t remote_thread_id;
} cdt_thread_t;

/**
 * Obtain the thread identifier of the calling thread.
 */
cdt_thread_t cdt_thread_self();

/**
 * Check if two threads are equal.
 * 
 * Returns 0 if t1 is the same thread is as t2, otherwise will return a nonzero value.
 */
int cdt_thread_equal(cdt_thread_t *t1, cdt_thread_t *t2);

/**
 * Create a new thread that invokes start_routine with argument arg.
 * 
 * arg, if not NULL, MUST point to a region in shared memomry.
 */
int cdt_thread_create(cdt_thread_t *thread, void *(*start_routine) (void *), void *arg);

/**
 * Make the calling thread wait until the specified thread terminates.
 * 
 * If return_value is not NULL, the result is stored in *return_value.
 */
int cdt_thread_join(cdt_thread_t *thread, void **return_value);

#endif
