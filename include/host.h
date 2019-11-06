#ifndef COORDINATE_HOST_H
#define COORDINATE_HOST_H

#include <pthread.h>
#include "peer.h"

typedef struct cdt_server_t cdt_server_t;

#define CDT_MAX_MACHINES 32

typedef struct cdt_host_t {
  int manager;
  cdt_server_t *server;
  pthread_t server_thread;

  int num_peers;
  cdt_peer_t peers[CDT_MAX_MACHINES];
} cdt_host_t;

/**
 * Start a host on its own thread. host must be already initialized to a valid cdt_host_t object.
 */
int cdt_host_start(cdt_host_t *host);

/**
 * Wait for the host thred to finish.
 */
void cdt_host_join(cdt_host_t *host);

#endif
