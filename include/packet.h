#ifndef COORDINATE_PACKET_H
#define COORDINATE_PACKET_H

#include "util.h"

typedef struct cdt_thread_t cdt_thread_t;

#define CDT_PACKET_DATA_SIZE PAGESIZE

enum cdt_packet_type {
  CDT_PACKET_SELF_IDENTIFY         = 0,
  CDT_PACKET_PEER_ID_ASSIGN        = 2,
  CDT_PACKET_PEER_ID_CONFIRM       = 4,

  CDT_PACKET_NEW_PEER              = 6,
  CDT_PACKET_EXISTING_PEER         = 8,

  CDT_PACKET_ALLOC_REQ             = 10,
  CDT_PACKET_ALLOC_RESP            = 11,
  
  CDT_PACKET_THREAD_CREATE_REQ     = 12,
  CDT_PACKET_THREAD_CREATE_RESP    = 13,
  CDT_PACKET_THREAD_ASSIGN_REQ     = 14,
  CDT_PACKET_THREAD_ASSIGN_RESP    = 15,

  CDT_PACKET_READ_REQ              = 16,
  CDT_PACKET_READ_RESP             = 17,
  CDT_PACKET_READ_INVALIDATE_REQ   = 18,
  CDT_PACKET_READ_INVALIDATE_RESP  = 19,

  CDT_PACKET_WRITE_REQ             = 20,
  CDT_PACKET_WRITE_RESP            = 21,
  CDT_PACKET_WRITE_DEMOTE_REQ      = 22,
  CDT_PACKET_WRITE_DEMOTE_RESP     = 23,
  CDT_PACKET_WRITE_INVALIDATE_REQ  = 24,
  CDT_PACKET_WRITE_INVALIDATE_RESP = 25,
};

/**
 * A data type to represent a network packet.
 */
typedef struct cdt_packet_t {
  /* The size of the packet's data. */
  uint32_t size;
  /* The type of the packet. */
  uint32_t type;
  /* The payload of the packet. */
  char data[CDT_PACKET_DATA_SIZE];
} cdt_packet_t;

int cdt_packet_self_identify_create(cdt_packet_t *packet, const char *address, const char *port);
void cdt_packet_self_identify_parse(cdt_packet_t *packet, char **address, char **port);

void cdt_packet_peer_id_assign_create(cdt_packet_t *packet, uint32_t peer_id);
void cdt_packet_peer_id_assign_parse(cdt_packet_t *packet, uint32_t *peer_id);

void cdt_packet_peer_id_confim_create(cdt_packet_t *packet);

int cdt_packet_new_peer_create(cdt_packet_t *packet, uint32_t peer_id, const char *address, const char *port);
void cdt_packet_new_peer_parse(cdt_packet_t *packet, uint32_t *peer_id, char **address, char **port);

void cdt_packet_existing_peer_create(cdt_packet_t *packet, uint32_t peer_id);
void cdt_packet_existing_peer_parse(cdt_packet_t *packet, uint32_t *peer_id);

void cdt_packet_alloc_req_create(cdt_packet_t *packet, uint32_t peer_id);
void cdt_packet_alloc_req_parse(cdt_packet_t *packet, uint32_t *peer_id);

void cdt_packet_alloc_resp_create(cdt_packet_t *packet, uint64_t page);
void cdt_packet_alloc_resp_parse(cdt_packet_t *packet, uint64_t *page);

void cdt_packet_thread_create_req_create(cdt_packet_t *packet, uint64_t procedure, uint64_t arg);
void cdt_packet_thread_create_req_parse(cdt_packet_t *packet, uint64_t *procedure, uint64_t *arg);

void cdt_packet_thread_create_resp_create(cdt_packet_t *packet, cdt_thread_t *thread);
void cdt_packet_thread_create_resp_parse(cdt_packet_t *packet, cdt_thread_t *thread);

void cdt_packet_thread_assign_req_create(cdt_packet_t *packet, uint64_t procedure, uint64_t arg, uint32_t thread_id);
void cdt_packet_thread_assign_req_parse(cdt_packet_t *packet, uint64_t *procedure, uint64_t *arg, uint32_t *thread_id);

void cdt_packet_thread_assign_resp_create(cdt_packet_t *packet, uint32_t status);
void cdt_packet_thread_assign_resp_parse(cdt_packet_t *packet, uint32_t *status);

void cdt_packet_read_req_create(cdt_packet_t *packet, uint64_t page_addr);
void cdt_packet_read_req_parse(cdt_packet_t *packet, uint64_t *page_addr);

void cdt_packet_read_resp_create(cdt_packet_t *packet, void *page);
void cdt_packet_read_resp_parse(cdt_packet_t *packet, void **page);

void cdt_packet_read_invalidate_req_create(cdt_packet_t *packet, uint64_t page_addr);
void cdt_packet_read_invalidate_req_parse(cdt_packet_t *packet, uint64_t *page_addr);

void cdt_packet_read_invalidate_resp_create(cdt_packet_t *packet);

void cdt_packet_write_req_create(cdt_packet_t *packet, uint64_t page_addr);
void cdt_packet_write_req_parse(cdt_packet_t *packet, uint64_t *page_addr);

void cdt_packet_write_resp_create(cdt_packet_t *packet, void *page);
void cdt_packet_write_resp_parse(cdt_packet_t *packet, void **page);

void cdt_packet_write_demote_req_create(cdt_packet_t *packet, uint64_t page_addr);
void cdt_packet_write_demote_req_parse(cdt_packet_t *packet, uint64_t *page_addr);

void cdt_packet_write_demote_resp_create(cdt_packet_t *packet, void *page);
void cdt_packet_write_demote_resp_parse(cdt_packet_t *packet, void **page);

void cdt_packet_write_invalidate_req_create(cdt_packet_t *packet, uint64_t page_addr);
void cdt_packet_write_invalidate_req_parse(cdt_packet_t *packet, uint64_t *page_addr);

void cdt_packet_write_invalidate_resp_create(cdt_packet_t *packet, void *page);
void cdt_packet_write_invalidate_resp_parse(cdt_packet_t *packet, void **page);

#endif
