#ifndef COORDINATE_H
#define COORDINATE_H

#include <stddef.h>
#include "packet.h"
#include "host.h"
#include "message.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "thread.h"

/**
 * Allocates size bytes of shared memory and returns a pointer to the allocated memory.
 */
void* cdt_malloc(size_t size);

/**
 * Do we want to support this?
 */
void cdt_free(void *ptr);

/**
 * Copies n bytes from memory area src to memory area dest. The memory areas must not overlap.
 * 
 * One, both, or none of dest and src may reside in shared memory.
 */
void* cdt_memcpy(void *dest, const void *src, size_t n);

#ifdef __cplusplus
}
#endif

#endif
