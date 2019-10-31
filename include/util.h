#ifndef COORDINATE_UTIL_H
#define COORDINATE_UTIL_H

#include <stdio.h>

#define debug_print(fmt, ...) \
        do { if (DEBUG) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                                __LINE__, __func__, ##__VA_ARGS__); } while (0)

#endif
