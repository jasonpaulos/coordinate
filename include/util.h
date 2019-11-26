#ifndef COORDINATE_UTIL_H
#define COORDINATE_UTIL_H

#include <stdio.h>
#include <stdint.h>

#define PAGESIZE 4096

#define debug_print(fmt, ...) \
        do { if (DEBUG) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                                __LINE__, __func__, ##__VA_ARGS__); } while (0)

uint64_t htonll(uint64_t x);
uint64_t ntohll(uint64_t x);

#endif
