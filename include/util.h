#ifndef COORDINATE_UTIL_H
#define COORDINATE_UTIL_H
#define PAGESIZE 4096

#include <stdio.h>

#define debug_print(fmt, ...) \
        do { if (DEBUG) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                                __LINE__, __func__, ##__VA_ARGS__); } while (0)

#endif

uint64_t htonll(uint64_t x);
uint64_t ntohll(uint64_t x);