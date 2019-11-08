#ifndef COORDINATE_HOST_H
#define COORDINATE_HOST_H

#include "peer.h"

typedef struct cdt_server_t cdt_server_t;

#define CDT_MAX_MACHINES 32

typedef struct cdt_host_t {
  /* Will be 1 if this machine is the manager, otherwise 0. */
  int manager;
  cdt_server_t *server;
  pthread_t server_thread;

  /* The id of this machine. Will be 0 if this is the manager. */
  int self_id;
  int num_peers;
  /* Each bit represents whether a peer is waiting to be connected. */
  int peers_to_be_connected;
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
