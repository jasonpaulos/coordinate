#ifndef COORDINATE_PEER_H
#define COORDINATE_PEER_H

#include <pthread.h>
#include "connection.h"

typedef struct cdt_host_t cdt_host_t;

/**
 * Represents a peer machine in the network.
 */
typedef struct cdt_peer_t {
  /* The id of the peer. This should be the same as the index of the peer in the host.peers array. */
  int id;
  pthread_t read_thread;
  /* The network connection associated with this peer. */
  cdt_connection_t connection;
  cdt_host_t *host;
} cdt_peer_t;

/**
 * Start a peer reading thread. peer must be already initialized to a valid cdt_peer_t object.
 */
int cdt_peer_start(cdt_peer_t *peer);

/**
 * Wait for the peer reading thred to finish.
 */
void cdt_peer_join(cdt_peer_t *peer);

#endif
