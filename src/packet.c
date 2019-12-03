#include <assert.h>
#include <string.h>
#include <arpa/inet.h>
#include "thread.h"
#include "packet.h"
#include "util.h"

uint32_t cdt_packet_response_get_requester(cdt_packet_t *packet) {
  assert(packet->type % 2 == 1);

  uint32_t requester_id;
  memmove(&requester_id, packet->data, sizeof(requester_id));

  return ntohl(requester_id);
}

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
  packet->size = sizeof(peer_id);

  peer_id = htonl(peer_id);
  memmove(packet->data, &peer_id, sizeof(peer_id));
}

void cdt_packet_peer_id_assign_parse(cdt_packet_t *packet, uint32_t *peer_id) {
  assert(packet->type == CDT_PACKET_PEER_ID_ASSIGN);

  memmove(peer_id, packet->data, sizeof(*peer_id));
  *peer_id = ntohl(*peer_id);
}

void cdt_packet_peer_id_confim_create(cdt_packet_t *packet) {
  packet->type = CDT_PACKET_PEER_ID_CONFIRM;
  packet->size = 0;
}

int cdt_packet_new_peer_create(cdt_packet_t *packet, uint32_t peer_id, const char *address, const char *port) {
  int address_len = strlen(address) + 1; // + 1 to include null terminating character
  int port_len = strlen(port) + 1;

  if (address_len + port_len + sizeof(peer_id) > CDT_PACKET_DATA_SIZE)
    return -1;
  
  packet->type = CDT_PACKET_NEW_PEER;
  packet->size = address_len + port_len + sizeof(peer_id);

  peer_id = htonl(peer_id);
  memmove(packet->data, &peer_id, sizeof(peer_id));
  memmove(packet->data + sizeof(peer_id), address, address_len);
  memmove(packet->data + address_len + sizeof(peer_id), port, port_len);

  return 0;
}

void cdt_packet_new_peer_parse(cdt_packet_t *packet, uint32_t *peer_id, char **address, char **port) {
  assert(packet->type == CDT_PACKET_NEW_PEER);

  memmove(peer_id, packet->data, sizeof(*peer_id));
  *peer_id = ntohl(*peer_id);

  *address = packet->data + sizeof(*peer_id);
  *port = *address + strlen(*address) + 1;
}

void cdt_packet_existing_peer_create(cdt_packet_t *packet, uint32_t peer_id) {
  packet->type = CDT_PACKET_EXISTING_PEER;
  packet->size = sizeof(peer_id);

  peer_id = htonl(peer_id);
  memmove(packet->data, &peer_id, sizeof(peer_id));
}

void cdt_packet_existing_peer_parse(cdt_packet_t *packet, uint32_t *peer_id) {
  assert(packet->type == CDT_PACKET_EXISTING_PEER);

  memmove(peer_id, packet->data, sizeof(*peer_id));
  *peer_id = ntohl(*peer_id);
}

void cdt_packet_alloc_req_create(cdt_packet_t *packet, uint32_t peer_id) {
  packet->type = CDT_PACKET_ALLOC_REQ;
  packet->size = sizeof(peer_id);

  peer_id = htonl(peer_id);
  memmove(packet->data, &peer_id, sizeof(peer_id));
}

void cdt_packet_alloc_req_parse(cdt_packet_t *packet, uint32_t *peer_id) {
  assert(packet->type == CDT_PACKET_ALLOC_REQ);

  memmove(peer_id, packet->data, sizeof(*peer_id));
  *peer_id = ntohl(*peer_id);
}

void cdt_packet_alloc_resp_create(cdt_packet_t *packet, uint64_t page) {
  packet->type = CDT_PACKET_ALLOC_RESP;
  packet->size = sizeof(page);

  page = htonll(page);
  memmove(packet->data, &page, sizeof(page));
}

void cdt_packet_alloc_resp_parse(cdt_packet_t *packet, uint64_t *page) {
  assert(packet->type == CDT_PACKET_ALLOC_RESP);

  memmove(page, packet->data, sizeof(*page));
  *page = ntohll(*page);
}

void cdt_packet_thread_create_req_create(cdt_packet_t *packet, uint64_t procedure, uint64_t arg) {
  packet->type = CDT_PACKET_THREAD_CREATE_REQ;
  packet->size = sizeof(procedure) + sizeof(arg);

  procedure = htonll(procedure);
  arg = htonll(arg);
  memmove(packet->data, &procedure, sizeof(procedure));
  memmove(packet->data + sizeof(procedure), &arg, sizeof(arg));
}

void cdt_packet_thread_create_req_parse(cdt_packet_t *packet, uint64_t *procedure, uint64_t *arg) {
  assert(packet->type == CDT_PACKET_THREAD_CREATE_REQ);

  memmove(procedure, packet->data, sizeof(*procedure));
  memmove(arg, packet->data + sizeof(*procedure), sizeof(*arg));

  *procedure = ntohll(*procedure);
  *arg = ntohll(*arg);
}

void cdt_packet_thread_create_resp_create(cdt_packet_t *packet, cdt_thread_t *thread) {
  packet->type = CDT_PACKET_THREAD_CREATE_RESP;

  char valid = !!thread->valid;
  uint32_t data[2] = {
    htonl(thread->remote_peer_id),
    htonl(thread->remote_thread_id)
  };

  packet->size = sizeof(valid) + sizeof(data);

  memmove(packet->data, &valid, sizeof(valid));
  memmove(packet->data + sizeof(valid), data, sizeof(data));
}

void cdt_packet_thread_create_resp_parse(cdt_packet_t *packet, cdt_thread_t *thread) {
  assert(packet->type == CDT_PACKET_THREAD_CREATE_RESP);

  char valid;
  uint32_t data[2];
  memmove(&valid, packet->data, sizeof(valid));
  memmove(data + sizeof(valid), packet->data, sizeof(data));

  thread->valid = valid;
  thread->remote_peer_id = ntohl(data[0]);
  thread->remote_thread_id = ntohl(data[1]);
}

void cdt_packet_thread_assign_req_create(cdt_packet_t *packet, uint32_t parent_id, uint64_t procedure, uint64_t arg, uint32_t thread_id) {
  packet->type = CDT_PACKET_THREAD_ASSIGN_REQ;
  packet->size = sizeof(parent_id) + sizeof(procedure) + sizeof(arg) + sizeof(thread_id);

  parent_id = htonl(parent_id);
  procedure = htonll(procedure);
  arg = htonll(arg);
  thread_id = htonl(thread_id);
  memmove(packet->data, &parent_id, sizeof(parent_id));
  memmove(packet->data + sizeof(parent_id), &procedure, sizeof(procedure));
  memmove(packet->data + sizeof(parent_id) + sizeof(procedure), &arg, sizeof(arg));
  memmove(packet->data + sizeof(parent_id) + sizeof(procedure) + sizeof(arg), &thread_id, sizeof(thread_id));
}

void cdt_packet_thread_assign_req_parse(cdt_packet_t *packet, uint32_t *parent_id, uint64_t *procedure, uint64_t *arg, uint32_t *thread_id) {
  assert(packet->type == CDT_PACKET_THREAD_ASSIGN_REQ);

  memmove(parent_id, packet->data, sizeof(*parent_id));
  memmove(procedure, packet->data + sizeof(*parent_id), sizeof(*procedure));
  memmove(arg, packet->data + sizeof(*parent_id) + sizeof(*procedure), sizeof(*arg));
  memmove(thread_id, packet->data + sizeof(*parent_id) + sizeof(*procedure) + sizeof(*arg), sizeof(*thread_id));

  *parent_id = ntohl(*parent_id);
  *procedure = ntohll(*procedure);
  *arg = ntohll(*arg);
  *thread_id = ntohl(*thread_id);
}

void cdt_packet_thread_assign_resp_create(cdt_packet_t *packet, uint32_t requester_id, uint32_t status) {
  packet->type = CDT_PACKET_THREAD_ASSIGN_RESP;
  packet->size = sizeof(requester_id) + sizeof(status);

  requester_id = htonl(requester_id);
  status = htonl(status);
  memmove(packet->data, &requester_id, sizeof(requester_id));
  memmove(packet->data + sizeof(requester_id), &status, sizeof(status));
}

void cdt_packet_thread_assign_resp_parse(cdt_packet_t *packet, uint32_t *requester_id, uint32_t *status) {
  assert(packet->type == CDT_PACKET_THREAD_ASSIGN_RESP);

  memmove(requester_id, packet->data, sizeof(*requester_id));
  memmove(status, packet->data + sizeof(*requester_id), sizeof(*status));

  *requester_id = ntohl(*requester_id);
  *status = ntohl(*status);
}

void cdt_packet_read_req_create(cdt_packet_t *packet, uint64_t page_addr) {
  packet->type = CDT_PACKET_READ_REQ;
  packet->size = sizeof(page_addr);

  page_addr = htonll(page_addr);
  memmove(packet->data, &page_addr, sizeof(page_addr));
}

void cdt_packet_read_req_parse(cdt_packet_t *packet, uint64_t *page_addr) {
  assert(packet->type == CDT_PACKET_READ_REQ);

  memmove(page_addr, packet->data, sizeof(*page_addr));
  *page_addr = ntohll(*page_addr);
}

void cdt_packet_read_resp_create(cdt_packet_t *packet, void *page) {
  packet->type = CDT_PACKET_READ_RESP;
  packet->size = PAGESIZE;

  memmove(packet->data, page, PAGESIZE);
}

void cdt_packet_read_resp_parse(cdt_packet_t *packet, void **page) {
  assert(packet->type == CDT_PACKET_READ_RESP);

  *page = packet->data;
}

void cdt_packet_read_invalidate_req_create(cdt_packet_t *packet, uint64_t page_addr) {
  packet->type = CDT_PACKET_READ_INVALIDATE_REQ;
  packet->size = sizeof(page_addr);

  page_addr = htonll(page_addr);
  memmove(packet->data, &page_addr, sizeof(page_addr));
}

void cdt_packet_read_invalidate_req_parse(cdt_packet_t *packet, uint64_t *page_addr) {
  assert(packet->type == CDT_PACKET_READ_INVALIDATE_REQ);

  memmove(page_addr, packet->data, sizeof(*page_addr));
  *page_addr = ntohll(*page_addr);
}

void cdt_packet_read_invalidate_resp_create(cdt_packet_t *packet) {
  packet->type = CDT_PACKET_READ_INVALIDATE_RESP;
}

void cdt_packet_write_req_create(cdt_packet_t *packet, uint64_t page_addr) {
  packet->type = CDT_PACKET_WRITE_REQ;
  packet->size = sizeof(page_addr);

  page_addr = htonll(page_addr);
  memmove(packet->data, &page_addr, sizeof(page_addr));
}

void cdt_packet_write_req_parse(cdt_packet_t *packet, uint64_t *page_addr) {
  assert(packet->type == CDT_PACKET_WRITE_REQ);

  memmove(page_addr, packet->data, sizeof(*page_addr));
  *page_addr = ntohll(*page_addr);
}

void cdt_packet_write_resp_create(cdt_packet_t *packet, void *page) {
  packet->type = CDT_PACKET_WRITE_RESP;
  packet->size = PAGESIZE;

  memmove(packet->data, page, PAGESIZE);
}

void cdt_packet_write_resp_parse(cdt_packet_t *packet, void **page) {
  assert(packet->type == CDT_PACKET_WRITE_RESP);

  *page = packet->data;
}

void cdt_packet_write_demote_req_create(cdt_packet_t *packet, uint64_t page_addr) {
  packet->type = CDT_PACKET_WRITE_DEMOTE_REQ;
  packet->size = sizeof(page_addr);

  page_addr = htonll(page_addr);
  memmove(packet->data, &page_addr, sizeof(page_addr));
}

void cdt_packet_write_demote_req_parse(cdt_packet_t *packet, uint64_t *page_addr) {
  assert(packet->type == CDT_PACKET_WRITE_DEMOTE_REQ);

  memmove(page_addr, packet->data, sizeof(*page_addr));
  *page_addr = ntohll(*page_addr);
}

void cdt_packet_write_demote_resp_create(cdt_packet_t *packet, void *page) {
  packet->type = CDT_PACKET_WRITE_DEMOTE_RESP;
  packet->size = PAGESIZE;

  memmove(packet->data, page, PAGESIZE);
}

void cdt_packet_write_demote_resp_parse(cdt_packet_t *packet, void **page) {
  assert(packet->type == CDT_PACKET_WRITE_DEMOTE_RESP);

  *page = packet->data;
}

void cdt_packet_write_invalidate_req_create(cdt_packet_t *packet, uint64_t page_addr) {
  packet->type = CDT_PACKET_WRITE_INVALIDATE_REQ;
  packet->size = sizeof(page_addr);

  page_addr = htonll(page_addr);
  memmove(packet->data, &page_addr, sizeof(page_addr));
}

void cdt_packet_write_invalidate_req_parse(cdt_packet_t *packet, uint64_t *page_addr) {
  assert(packet->type == CDT_PACKET_WRITE_INVALIDATE_REQ);

  memmove(page_addr, packet->data, sizeof(*page_addr));
  *page_addr = ntohll(*page_addr);
}

void cdt_packet_write_invalidate_resp_create(cdt_packet_t *packet, void *page) {
  packet->type = CDT_PACKET_WRITE_INVALIDATE_RESP;
  packet->size = PAGESIZE;

  memmove(packet->data, page, PAGESIZE);
}

void cdt_packet_write_invalidate_resp_parse(cdt_packet_t *packet, void **page) {
  assert(packet->type == CDT_PACKET_WRITE_INVALIDATE_RESP);

  *page = packet->data;
}
