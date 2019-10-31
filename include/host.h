#ifndef COORDINATE_HOST_H
#define COORDINATE_HOST_H

#include <pthread.h>

typedef struct cdt_server cdt_server;

/**
 * Start a host thread with a listening server.
 */
int cdt_host_start(pthread_t *thread, cdt_server *server);

#endif
