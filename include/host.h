#ifndef COORDINATE_HOST_H
#define COORDINATE_HOST_H

#include "peer.h"

typedef struct cdt_server_t cdt_server_t;

#define CDT_MAX_MACHINES 32
#define CDT_MAX_SHARED_PAGES 10240 // note: we may want to change this 
#define CDT_SHARED_VA_START (1L << 32)
#define CDT_SHARED_VA_END ((1L << 32) + CDT_MAX_SHARED_PAGES * PAGESIZE)
#define INVALID_PAGE 0
#define READ_ONLY_PAGE 1
#define READ_WRITE_PAGE 2
#define SHARED_VA_TO_IDX(va) (((uint64_t)(va) - CDT_SHARED_VA_START) / PAGESIZE)
#define PGROUNDDOWN(a) ((uint64_t)(a) & ~(PAGESIZE-1))

extern const char* const cdt_task_queue_names[CDT_MAX_MACHINES];

/* Pagetable entry for a single page in a machine's page table (NOT the manager). 
   The PTE must be locked before being accessed in any way. */
typedef struct cdt_host_pte_t {
  int in_use;
  /* shared_va should never be changed after init */
  uint64_t shared_va;
  /* access is one of READ_ONLY, READ_WRITE, and INVALID */
  int access;
  /* if access = INVALID then page = NULL */
  void * page;
  pthread_mutex_t lock;
} cdt_host_pte_t;

/* Pagetable entry for a single page in the manager's page table. 
   The PTE must be locked before being accessed in any way. */
typedef struct cdt_manager_pte_t {
  int in_use;
  /* shared_va should never be changed after init */
  uint64_t shared_va; 
  /* The set of machines that have read access. 
     Each entry is 0 or 1 indicating no access or read access */
  int read_set[CDT_MAX_MACHINES]; 
  /* The machine ID with R/W access. If this is -1, there is no writer. 
     If this is >=0 then read_set must be all zeros */
  int writer;
  /* Pointer to the page itself which is only valid when the page is in R/O mode */
  void * page;
  pthread_mutex_t lock;
} cdt_manager_pte_t;

typedef struct cdt_host_t {
  /* Will be 1 if this machine is the manager, otherwise 0. */
  int manager;
  cdt_server_t *server;
  pthread_t server_thread;

  /* The id of this machine. Will be 0 if this is the manager. */
  uint32_t self_id;
  int num_peers;
  /* Each bit represents whether a peer is waiting to be connected. */
  uint32_t peers_to_be_connected;
  cdt_peer_t peers[CDT_MAX_MACHINES];
  cdt_host_pte_t shared_pagetable[CDT_MAX_SHARED_PAGES];
  /* This array is only valid if the host is the manager. */
  cdt_manager_pte_t manager_pagetable[CDT_MAX_SHARED_PAGES];
  unsigned int manager_first_unallocated_pg_idx;

  pthread_mutex_t thread_lock;
  uint32_t thread_counter;
  int num_threads;
} cdt_host_t;

/**
 * Initialize a host.
 * 
 * manager shoud be 1 if this machine is the manager, otherwise 0.
 */
cdt_host_t* cdt_host_init(int manager, cdt_server_t *server, uint32_t peers_to_be_connected);

/**
 * Start the host thrad.
 * 
 * Returns 0 on success.
 */
int cdt_host_start();

/**
 * Get the host for this machine.
 * 
 * If the host has not been initialized yet, returns NULL.
 */
cdt_host_t *cdt_get_host();

/**
 * Wait for the host thread to finish.
 * 
 * Returns 0 on success, -1 on error.
 */
int cdt_host_join();

#endif
