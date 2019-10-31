#include <stdio.h>
#include "server.h"
#include "connection.h"
#include "host.h"

void* cdt_host_thread(void *arg) {
  cdt_server *server = (cdt_server*)arg;

  printf("Server started\n");

  cdt_connection connection;
  while (cdt_server_accept(server, &connection) == 0) {
    printf("Client connected\n");
    char buffer[100];
    int n = cdt_connection_read(&connection, buffer, sizeof(buffer));
    printf("Read %d characters: %s", n, buffer);
  }

  return 0;
}

int cdt_host_start(pthread_t *thread, cdt_server *server) {
  return pthread_create(thread, NULL, cdt_host_thread, (void*)server);
}
