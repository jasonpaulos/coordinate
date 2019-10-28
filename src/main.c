#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "server.h"
#include "connection.h"

/**
 * Parse an IPv4 address of the form ip:port
 */
int parseIPAddress(const char *address, char *ip, int *port, int ipLength) {
  const char *lastColon = strrchr(address, ':');
  if (lastColon == 0)
    return -1;
  
  int lastColonIndex = lastColon - address + 1;
  if (lastColonIndex + 1 > ipLength)
    return -1;
  
  memmove(ip, address, lastColonIndex - 1);
  ip[lastColonIndex - 1] = 0;

  *port = atoi(lastColon + 1);

  return 0;
}

int main(int argc, const char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: coordinate [OPTION]... COMMAND [INITIAL_ARGS]...\n");
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

  char ip[100];
  int port = 0;
  if (parseIPAddress(argv[hostIndex | connectionIndex], ip, &port, sizeof(ip)) == -1) {
    fprintf(stderr, "Unable to parse address: %s\n", argv[hostIndex | connectionIndex]);
    return -1;
  }

  if (hostIndex) {
    cdt_server server;
    if (cdt_server_create(&server, ip, port) == -1) {
      fprintf(stderr, "Cannot create server\n");
      return -1;
    }
    if (cdt_server_listen(&server, 10) == -1) {
      fprintf(stderr, "Server cannot listen\n");
      return -1;
    }

    printf("Listening at %s:%d\n", ip, port);

    while (1) {
      cdt_connection connection;
      if (cdt_server_accept(&server, &connection) == -1) {
        fprintf(stderr, "Error accepting client\n");
        continue;
      }

      printf("Client connected\n");
      char buffer[100];
      int n = cdt_connection_read(&connection, buffer, sizeof(buffer));
      printf("Read %d characters: %s", n, buffer);
    }

  } else {
    cdt_connection connection;
    if (cdt_connection_connect(&connection, ip, port) == -1) {
      fprintf(stderr, "Error connecting to server\n");
      return -1;
    }
    printf("Connected to %s:%d\n", ip, port);

    char message[] = "Hello world!\n";
    int n = cdt_connection_write(&connection, message, sizeof(message));
    printf("Wrote %d characters\n", n);
  }
}
