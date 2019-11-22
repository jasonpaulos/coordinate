#ifndef COORDINATE_THREAD_H
#define COORDINATE_THREAD_H

#include <pthread.h>

typedef struct cdt_thread_t {
  pthread_t id;
} cdt_thread_t;

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
