#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include "util.h"
#include "connection.h"

int cdt_connection_create(cdt_connection *connection, int fd) {
  if (fd < 0)
    return -1;

  connection->fd = fd;

  return 0;
}

int cdt_connection_connect(cdt_connection *connection, const char *address, const char *port) {
  struct addrinfo hints;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = 0;
  hints.ai_protocol = 0;

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

    if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
      break;
    
    close(sfd);
  }

  freeaddrinfo(result);

  if (rp == NULL) {
    debug_print("Could not connect\n");
    return -1;
  }

  memset(connection, 0, sizeof(cdt_connection));
  connection->fd = sfd;
  return 0;
}

void cdt_connection_close(cdt_connection *connection) {
  shutdown(connection->fd, SHUT_RDWR);
  close(connection->fd);
  connection->fd = -1;
}

int cdt_connection_read(const cdt_connection *connection, char *data, size_t length) {
  return read(connection->fd, data, length);
}

int cdt_connection_write(const cdt_connection *connection, const char *data, size_t length) {
  return write(connection->fd, data, length);
}
