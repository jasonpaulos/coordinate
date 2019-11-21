#include "coordinate.h"

extern cdt_host_t host;
extern cdt_connection_t manager_connection;

void* cdt_malloc(size_t size) {
  if (host.manager) {
    // Make the allocation
    printf("Manager trying to malloc\n");

  }
  // Not the manager, so send msg to manager requesting allocation
  cdt_packet_t packet;
  if (cdt_packet_alloc_req_create(&packet, host.self_id) != 0) {
    fprintf(stderr, "Cannot create allocation request packet\n");
    return NULL;
  }
  if (cdt_connection_send(&manager_connection, &packet) != 0) {
    fprintf(stderr, "Failed to send self identify packet\n");
    return NULL;
  }
  return NULL;
}

void cdt_free(void *ptr) {
  // TODO
}

void* cdt_memcpy(void *dest, const void *src, size_t n) {
  // TODO
  return NULL;
}
