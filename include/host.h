#ifndef COORDINATE_HOST_H
#define COORDINATE_HOST_H

#include "peer.h"

typedef struct cdt_server_t cdt_server_t;

#define CDT_MAX_MACHINES 32
#define CDT_MAX_SHARED_PAGES 1024 // note: we may want to change this 


/* Pagetable entry for a single page in a machine's page table (NOT the manager). 
   The PTE must be locked before being accessed in any way. */
typedef struct cdt_host_pte_t {
  int in_use;
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
  uint64_t shared_va; 
  /* The set of machines that have read access. 
     Each entry is 0 or 1 indicating no access or read access */
  int read_set[CDT_MAX_MACHINES]; 
  /* The machine ID with R/W access. If this is -1, there is no writer. 
     If this is >=0 then read_set must be all zeros */
  int writer;
  /* Pointer to the page itself */
  void * page;
  pthread_mutex_t lock;
} cdt_manager_pte_t;

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
  cdt_host_pte_t shared_pagetable[CDT_MAX_SHARED_PAGES];
  /* This array is only valid if the host is the manager. */
  cdt_manager_pte_t manager_pagetable[CDT_MAX_SHARED_PAGES];
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
