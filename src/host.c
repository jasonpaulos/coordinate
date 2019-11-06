#include <stdio.h>
#include <string.h>
#include "server.h"
#include "connection.h"
#include "host.h"

void* cdt_host_thread(void *arg) {
  cdt_host_t *host = (cdt_host_t*)arg;

  printf("Server started\n");

  host->peers[0].self = 1;
  host->num_peers = 1;

  cdt_connection_t *connection;
  while (host->num_peers < CDT_MAX_MACHINES && cdt_server_accept(host->server, connection = &host->peers[host->num_peers].connection) == 0) {
    printf("Client connected from %s:%d\n", connection->address, connection->port);
    char buffer[100];
    int n = cdt_connection_read(connection, buffer, sizeof(buffer));
    printf("Read %d characters: %s", n, buffer);
    host->num_peers++;
  }

  return NULL;
}

int cdt_host_start(cdt_host_t *host) {
  return pthread_create(&host->server_thread, NULL, cdt_host_thread, (void*)host);
}

void cdt_host_join(cdt_host_t *host) {
  pthread_join(host->server_thread, NULL);
}
