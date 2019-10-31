#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "server.h"
#include "connection.h"

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s [OPTION]... COMMAND [INITIAL_ARGS]...\n", argv[0]);
    return -1;
  }

  int hostIndex = 0;
  for (int i = 1; i < argc - 1; i++) {
    if (strcmp(argv[i], "--host") == 0) {
      hostIndex = i + 1;
      break;
    }
  }

  int connectionIndex = 0;
  for (int i = 1; i < argc - 1; i++) {
    if (strcmp(argv[i], "--connect") == 0) {
      connectionIndex = i + 1;
      break;
    }
  }

  if (!hostIndex && !connectionIndex) {
    fprintf(stderr, "Must specify one of --host IP:PORT or --connect IP:PORT options.\n");
    return -1;
  } else if (hostIndex && connectionIndex) {
    fprintf(stderr, "Cannot specifiy both --host and --connect options.\n");
    return -1;
  }

  char *port;
  char *address = argv[hostIndex | connectionIndex];
  char *lastColon = strrchr(address, ':');
  if (lastColon == NULL) {
    if (!hostIndex) {
      fprintf(stderr, "Unable to parse address: %s\n", address);
      return -1;
    }

    port = address;
    address = NULL;
  } else {
    port = lastColon + 1;
    *lastColon = 0;
  }

  if (hostIndex) {
    cdt_server server;
    if (cdt_server_create(&server, address, port) == -1) {
      fprintf(stderr, "Cannot create server\n");
      return -1;
    }
    if (cdt_server_listen(&server, 10) == -1) {
      fprintf(stderr, "Server cannot listen\n");
      return -1;
    }

    printf("Listening at %s:%s\n", address == NULL ? "*" : address, port);

    pthread_t thread;
    cdt_server_start(&server, &thread);
    pthread_join(thread, NULL);

  } else {
    cdt_connection connection;
    if (cdt_connection_connect(&connection, address, port) == -1) {
      fprintf(stderr, "Error connecting to server\n");
      return -1;
    }
    printf("Connected to %s:%s\n", address, port);

    char message[] = "Hello world!\n";
    int n = cdt_connection_write(&connection, message, sizeof(message));
    printf("Wrote %d characters\n", n);
  }
}
