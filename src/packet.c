#include <assert.h>
#include <string.h>
#include <arpa/inet.h>
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

int cdt_packet_self_identify_parse(cdt_packet_t *packet, char **address, char **port) {
  assert(packet->type == CDT_PACKET_SELF_IDENTIFY);
  
  *address = packet->data;
  *port = *address + strlen(*address) + 1;

  return 0;
}

int cdt_packet_peer_id_assign_create(cdt_packet_t *packet, int peer_id) {
  packet->type = CDT_PACKET_PEER_ID_ASSIGN;
  packet->size = sizeof(int);
  *(int*)packet->data = htonl(peer_id);

  return 0;
}

int cdt_packet_peer_id_assign_parse(cdt_packet_t *packet, int *peer_id) {
  assert(packet->type == CDT_PACKET_PEER_ID_ASSIGN);
  *peer_id = ntohl(*(int*)packet->data);
  return 0;
}

void cdt_packet_peer_id_confim_create(cdt_packet_t *packet) {
  packet->type = CDT_PACKET_PEER_ID_CONFIRM;
  packet->size = 0;
}

int cdt_packet_new_peer_create(cdt_packet_t *packet, int peer_id, const char *address, const char *port) {
  int address_len = strlen(address) + 1; // + 1 to include null terminating character
  int port_len = strlen(port) + 1;

  if (address_len + port_len + sizeof(int) > CDT_PACKET_DATA_SIZE)
    return -1;
  
  packet->type = CDT_PACKET_NEW_PEER;
  packet->size = address_len + port_len + sizeof(int);

  *(int*)packet->data = htonl(peer_id);
  memmove(packet->data + sizeof(int), address, address_len);
  memmove(packet->data + address_len + sizeof(int), port, port_len);

  return 0;
}

int cdt_packet_new_peer_parse(cdt_packet_t *packet, int *peer_id, char **address, char **port) {
  assert(packet->type == CDT_PACKET_NEW_PEER);

  *peer_id = ntohl(*(int*)packet->data);
  *address = packet->data + sizeof(int);
  *port = *address + strlen(*address) + 1;

  return 0;
}

int cdt_packet_existing_peer_create(cdt_packet_t *packet, int peer_id) {
  packet->type = CDT_PACKET_EXISTING_PEER;
  packet->size = sizeof(int);
  *(int*)packet->data = htonl(peer_id);

  return 0;
}

int cdt_packet_existing_peer_parse(cdt_packet_t *packet, int *peer_id) {
  assert(packet->type == CDT_PACKET_EXISTING_PEER);

  *peer_id = ntohl(*(int*)packet->data);

  return 0;
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
}
