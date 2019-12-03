#ifndef COORDINATE_WORKER_H
#define COORDINATE_WORKER_H

#include <stdint.h>

typedef struct cdt_peer_t cdt_peer_t;
typedef struct cdt_packet_t cdt_packet_t;
typedef struct cdt_host_t cdt_host_t;
typedef struct cdt_thread_t cdt_thread_t;

/**
 * Start a worker thread for a peer.
 * 
 * arg should be a pointer to the cdt_peer_t that this thread is for.
 */
void* cdt_worker_thread_start(void *arg);

/**
 * Handle CDT_PACKET_THREAD_CREATE_REQ
 */
int cdt_worker_thread_create(cdt_peer_t *sender, cdt_packet_t *packet);

int cdt_find_unused_pte(cdt_manager_pte_t ** fresh_pte, int peer_id);

int cdt_allocate_shared_page(cdt_peer_t * peer);
/**
 * The underlying implementation of creating a thread.
 * 
 * host->thread_lock MUST be held before calling this
 * 
 * Returns NULL on failure, otherwise a pointer to the newly created thread.
 */
cdt_thread_t* cdt_worker_do_thread_create(cdt_host_t *host, cdt_peer_t *sender, cdt_packet_t *packet);

/**
 * Handle CDT_PACKET_THREAD_ASSIGN_REQ
 */
int cdt_worker_thread_assign(cdt_peer_t *sender, cdt_packet_t *packet);

#endif
