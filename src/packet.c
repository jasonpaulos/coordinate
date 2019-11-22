#include <assert.h>
#include <string.h>
#include <arpa/inet.h>
#include "util.h"
#include "packet.h"
#include "util.h"

int cdt_packet_self_identify_create(cdt_packet_t *packet, const char *address, const char *port) {
  int address_len = strlen(address) + 1; // + 1 to include null terminating character
  int port_len = strlen(port) + 1;

  if (address_len + port_len > CDT_PACKET_DATA_SIZE)
    return -1;

  packet->type = CDT_PACKET_SELF_IDENTIFY;
  packet->size = address_len + port_len;

  memmove(packet->data, address, address_len);
  memmove(packet->data + address_len, port, port_len);

  return 0;
}

void cdt_packet_self_identify_parse(cdt_packet_t *packet, char **address, char **port) {
  assert(packet->type == CDT_PACKET_SELF_IDENTIFY);
  
  *address = packet->data;
  *port = *address + strlen(*address) + 1;
}

void cdt_packet_peer_id_assign_create(cdt_packet_t *packet, uint32_t peer_id) {
  packet->type = CDT_PACKET_PEER_ID_ASSIGN;
  packet->size = sizeof(uint32_t);
  *(uint32_t*)packet->data = htonl(peer_id);
}

void cdt_packet_peer_id_assign_parse(cdt_packet_t *packet, uint32_t *peer_id) {
  assert(packet->type == CDT_PACKET_PEER_ID_ASSIGN);
  *peer_id = ntohl(*(uint32_t*)packet->data);
}

void cdt_packet_peer_id_confim_create(cdt_packet_t *packet) {
  packet->type = CDT_PACKET_PEER_ID_CONFIRM;
  packet->size = 0;
}

int cdt_packet_new_peer_create(cdt_packet_t *packet, uint32_t peer_id, const char *address, const char *port) {
  int address_len = strlen(address) + 1; // + 1 to include null terminating character
  int port_len = strlen(port) + 1;

  if (address_len + port_len + sizeof(uint32_t) > CDT_PACKET_DATA_SIZE)
    return -1;
  
  packet->type = CDT_PACKET_NEW_PEER;
  packet->size = address_len + port_len + sizeof(uint32_t);

  *(uint32_t*)packet->data = htonl(peer_id);
  memmove(packet->data + sizeof(uint32_t), address, address_len);
  memmove(packet->data + address_len + sizeof(uint32_t), port, port_len);

  return 0;
}

void cdt_packet_new_peer_parse(cdt_packet_t *packet, uint32_t *peer_id, char **address, char **port) {
  assert(packet->type == CDT_PACKET_NEW_PEER);

  *peer_id = ntohl(*(uint32_t*)packet->data);
  *address = packet->data + sizeof(uint32_t);
  *port = *address + strlen(*address) + 1;
}

void cdt_packet_existing_peer_create(cdt_packet_t *packet, uint32_t peer_id) {
  packet->type = CDT_PACKET_EXISTING_PEER;
  packet->size = sizeof(uint32_t);
  *(uint32_t*)packet->data = htonl(peer_id);
}

void cdt_packet_existing_peer_parse(cdt_packet_t *packet, uint32_t *peer_id) {
  assert(packet->type == CDT_PACKET_EXISTING_PEER);

  *peer_id = ntohl(*(uint32_t*)packet->data);
}

int cdt_packet_alloc_req_create(cdt_packet_t *packet, int peer_id) {
  packet->type = CDT_PACKET_ALLOC_REQ;
  packet->size = sizeof(int);
  *(int*)packet->data = htonl(peer_id);

  return 0;
}

int cdt_packet_alloc_req_parse(cdt_packet_t *packet, int *peer_id) {
  assert(packet->type == CDT_PACKET_ALLOC_REQ);

  *peer_id = ntohl(*(int*)packet->data);

  return 0;
}

int cdt_packet_alloc_resp_create(cdt_packet_t *packet, uint64_t page) {
  packet->type = CDT_PACKET_ALLOC_RESP;
  packet->size = sizeof(uint64_t);
  *(uint64_t*)packet->data = htonll(page);

  return 0;
}

int cdt_packet_alloc_resp_parse(cdt_packet_t *packet, uint64_t *page) {
  assert(packet->type == CDT_PACKET_ALLOC_RESP);

  *page = ntohll(*(uint64_t*)packet->data);

  return 0;

void cdt_packet_read_req_create(cdt_packet_t *packet, uint64_t page_addr) {
  packet->type = CDT_PACKET_READ_REQ;
  packet->size = sizeof(uint64_t);

  *(uint64_t*)packet->data = htonll(page_addr);
}

void cdt_packet_read_req_parse(cdt_packet_t *packet, uint64_t *page_addr) {
  assert(packet->type == CDT_PACKET_READ_REQ);

  *page_addr = ntohll(*(uint64_t*)packet->data);
}

void cdt_packet_read_resp_create(cdt_packet_t *packet, void *page) {
  packet->type = CDT_PACKET_READ_RESP;
  packet->size = PGSIZE;

  memmove(packet->data, page, PGSIZE);
}

void cdt_packet_read_resp_parse(cdt_packet_t *packet, void **page) {
  assert(packet->type == CDT_PACKET_READ_RESP);

  *page = packet->data;
}

void cdt_packet_read_invalidate_req_create(cdt_packet_t *packet, uint64_t page_addr) {
  packet->type = CDT_PACKET_READ_INVALIDATE_REQ;
  packet->size = sizeof(uint64_t);

  *(uint64_t*)packet->data = htonll(page_addr);
}

void cdt_packet_read_invalidate_req_parse(cdt_packet_t *packet, uint64_t *page_addr) {
  assert(packet->type == CDT_PACKET_READ_INVALIDATE_REQ);

  *page_addr = ntohll(*(uint64_t*)packet->data);
}

void cdt_packet_read_invalidate_resp_create(cdt_packet_t *packet) {
  packet->type = CDT_PACKET_READ_INVALIDATE_RESP;
}

void cdt_packet_write_req_create(cdt_packet_t *packet, uint64_t page_addr) {
  packet->type = CDT_PACKET_WRITE_REQ;
  packet->size = sizeof(uint64_t);

  *(uint64_t*)packet->data = htonll(page_addr);
}

void cdt_packet_write_req_parse(cdt_packet_t *packet, uint64_t *page_addr) {
  assert(packet->type == CDT_PACKET_WRITE_REQ);

  *page_addr = ntohll(*(uint64_t*)packet->data);
}

void cdt_packet_write_resp_create(cdt_packet_t *packet, void *page) {
  packet->type = CDT_PACKET_WRITE_RESP;
  packet->size = PGSIZE;

  memmove(packet->data, page, PGSIZE);
}

void cdt_packet_write_resp_parse(cdt_packet_t *packet, void **page) {
  assert(packet->type == CDT_PACKET_WRITE_RESP);

  *page = packet->data;
}

void cdt_packet_write_demote_req_create(cdt_packet_t *packet, uint64_t page_addr) {
  packet->type = CDT_PACKET_WRITE_DEMOTE_REQ;
  packet->size = sizeof(uint64_t);

  *(uint64_t*)packet->data = htonll(page_addr);
}

void cdt_packet_write_demote_req_parse(cdt_packet_t *packet, uint64_t *page_addr) {
  assert(packet->type == CDT_PACKET_WRITE_DEMOTE_REQ);

  *page_addr = ntohll(*(uint64_t*)packet->data);
}

void cdt_packet_write_demote_resp_create(cdt_packet_t *packet, void *page) {
  packet->type = CDT_PACKET_WRITE_DEMOTE_RESP;
  packet->size = PGSIZE;

  memmove(packet->data, page, PGSIZE);
}

void cdt_packet_write_demote_resp_parse(cdt_packet_t *packet, void **page) {
  assert(packet->type == CDT_PACKET_WRITE_DEMOTE_RESP);

  *page = packet->data;
}

void cdt_packet_write_invalidate_req_create(cdt_packet_t *packet, uint64_t page_addr) {
  packet->type = CDT_PACKET_WRITE_INVALIDATE_REQ;
  packet->size = sizeof(uint64_t);

  *(uint64_t*)packet->data = htonll(page_addr);
}

void cdt_packet_write_invalidate_req_parse(cdt_packet_t *packet, uint64_t *page_addr) {
  assert(packet->type == CDT_PACKET_WRITE_INVALIDATE_REQ);

  *page_addr = ntohll(*(uint64_t*)packet->data);
}

void cdt_packet_write_invalidate_resp_create(cdt_packet_t *packet, void *page) {
  packet->type = CDT_PACKET_WRITE_INVALIDATE_RESP;
  packet->size = PGSIZE;

  memmove(packet->data, page, PGSIZE);
}

void cdt_packet_write_invalidate_resp_parse(cdt_packet_t *packet, void **page) {
  assert(packet->type == CDT_PACKET_WRITE_INVALIDATE_RESP);

  *page = packet->data;
}
