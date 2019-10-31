#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include "util.h"
#include "connection.h"
#include "server.h"

int cdt_server_create(cdt_server *server, const char *address, const char *port) {
  struct addrinfo hints;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  hints.ai_protocol = 0;
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;

  struct addrinfo *result;
  int s = getaddrinfo(address, port, &hints, &result);
  if (s != 0) {
    debug_print("getaddrinfo: %s\n", gai_strerror(s));
    return -1;
  }

  struct addrinfo *rp;
  int sfd;
  for (rp = result; rp != NULL; rp = rp->ai_next) {
    sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sfd == -1)
      continue;
    
    if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
      break;
    
    close(sfd);
  }

  freeaddrinfo(result);

  if (rp == NULL) {
    debug_print("Could not bind\n");
    return -1;
  }

  memset(server, 0, sizeof(cdt_server));
  server->fd = sfd;
  return 0;
}

int cdt_server_listen(cdt_server *server, int queue) {
  return listen(server->fd, queue);
}

void cdt_server_close(cdt_server *server) {
  close(server->fd);
  server->fd = -1;
}

int cdt_server_accept(const cdt_server *server, cdt_connection *connection) {
  int connfd = accept(server->fd, (struct sockaddr*)NULL, 0);
  return cdt_connection_create(connection, connfd);
}

