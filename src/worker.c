#include "host.h"
#include "packet.h"
#include "worker.h"

void* cdt_worker_thread_create(void *arg) {
  cdt_host_t *host = cdt_get_host();
  if (!host) {
    debug_print("Host not yet started\n");
    return NULL;
  }

  uint64_t procedure, procedure_arg;
  cdt_packet_thread_create_req_parse((cdt_packet_t*)arg, &procedure, &procedure_arg);

  // TODO: assign a peer this thread

  return NULL;
}
