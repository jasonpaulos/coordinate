#ifndef COORDINATE_PEER_H
#define COORDINATE_PEER_H

#include <mqueue.h>
#include "thread.h"
#include "connection.h"

typedef struct cdt_host_t cdt_host_t;
typedef struct cdt_manager_pte_t cdt_manager_pte_t;

/**
 * Represents a peer machine in the network.
 */
typedef struct cdt_peer_t {
  /* The id of the peer. This should be the same as the index of the peer in the host.peers array. */
  uint32_t id;
  pthread_t read_thread;
  /* The network connection associated with this peer. */
  cdt_connection_t connection;

  /* The thread that this peer may be running. */
  cdt_thread_t thread;

  mqd_t task_queue;
  pthread_t worker_thread;
} cdt_peer_t;

/**
 * Setup the task queue associated with this peer.
 */
int cdt_peer_setup_task_queue(cdt_peer_t *peer);

/**
 * Start a peer reading thread.
 * 
 * peer must be already initialized to a valid cdt_peer_t object, but its task
 * queue should NOT be initialized.
 */
int cdt_peer_start(cdt_peer_t *peer);

/**
 * Wait for the peer reading thread to finish.
 */
void cdt_peer_join(cdt_peer_t *peer);

#endif
