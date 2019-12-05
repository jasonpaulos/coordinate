#ifndef COORDINATE_WORKER_H
#define COORDINATE_WORKER_H

#include <stdint.h>
#include "host.h"

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

/**
 * Handle CDT_PACKET_WRITE_REQ
 */
int cdt_worker_write_req(cdt_peer_t *sender, cdt_packet_t *packet);
/**
 * Handle CDT_PACKET_WRITE_INVALIDATE_REQ
 */
int cdt_worker_write_invalidate_req(cdt_peer_t *sender, cdt_packet_t *packet);
/**
 * Handle CDT_PACKET_READ_INVALIDATE_REQ
 */
int cdt_worker_read_invalidate_req(cdt_peer_t *sender, cdt_packet_t *packet);

/** Finds a series of unused page table entries and returns the beginning index of the PTE if successful.
  * If unsuccessful, returns -1.
  * IMPORTANT: it returns holding the locks for every PTE in the range if successful.
  */
int cdt_find_unused_pte(uint32_t peer_id, uint32_t num_pages);

/**
 * Handle CDT_PACKET_WRITE_DEMOTE_RESP
 */
int cdt_worker_write_demote(cdt_peer_t *sender, cdt_packet_t *packet);

/**
 * Handle CDT_PACKET_READ_REQ
 */
int cdt_worker_read_req(cdt_peer_t *sender, cdt_packet_t *packet);

int cdt_allocate_shared_page(cdt_peer_t *sender, cdt_packet_t *packet);
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

/**
 * Handle CDT_PACKET_THREAD_JOIN_REQ
 */
int cdt_worker_thread_join(cdt_peer_t *sender, cdt_packet_t *packet);

#endif
