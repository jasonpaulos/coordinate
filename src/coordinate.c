#include <mqueue.h>
#include "coordinate.h"
#include "message.h"

extern cdt_host_t host;
extern cdt_connection_t manager_connection;
extern mqd_t qd_manager_peer_thread;

void* cdt_malloc(size_t size) {
  if (host.manager) {
    // Make the allocation
    printf("Manager trying to malloc\n");
    return NULL;

  }
  // Not the manager, so send msg to manager requesting allocation
  cdt_packet_t packet;
  if (cdt_packet_alloc_req_create(&packet, host.self_id) != 0) {
    fprintf(stderr, "Cannot create allocation request packet\n");
    return NULL;
  }
  if (cdt_connection_send(&manager_connection, &packet) != 0) {
    fprintf(stderr, "Failed to send allocation request packet\n");
    return NULL;
  }

  cdt_message_t allocation_response;

  if (mq_receive (qd_manager_peer_thread, (char *)&allocation_response, sizeof(allocation_response), NULL) == -1) {
    debug_print("Failed to receive a message from manager peer-thread\n");
    return NULL;
  }

  printf("Received message from manager peer-thread with type %d and shared VA %p\n", 
    allocation_response.type, (void *)allocation_response.shared_va);

  return NULL;
}

void cdt_free(void *ptr) {
  // TODO
}

void* cdt_memcpy(void *dest, const void *src, size_t n) {
  // TODO
  return NULL;
}
