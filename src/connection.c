#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include "util.h"
#include "connection.h"

int extract_location_from_addr(int family, struct sockaddr *addr, cdt_connection_t *connection) {
  if (family == AF_INET) {

    struct sockaddr_in *s = (struct sockaddr_in *)addr;
    connection->port = ntohs(s->sin_port);
    if (inet_ntop(AF_INET, &s->sin_addr, connection->address, sizeof(connection->address)) == NULL)
      return -1;

  } else if (family == AF_INET6) {

    struct sockaddr_in6 *s = (struct sockaddr_in6 *)addr;
    connection->port = ntohs(s->sin6_port);
    if (inet_ntop(AF_INET6, &s->sin6_addr, connection->address, sizeof(connection->address)) == NULL)
      return -1;

  } else {
    return -1;
  }

  return 0;
}

int cdt_connection_create(cdt_connection_t *connection, int fd, struct sockaddr_storage *addr) {
  if (fd < 0)
    return -1;

  memset(connection, 0, sizeof(cdt_connection_t));
  connection->fd = fd;

  if (extract_location_from_addr(addr->ss_family, (struct sockaddr*)addr, connection) != 0) {
    debug_print("Cannot extract address from socket\n");
    cdt_connection_close(connection);
    return -1;
  }

  return 0;
}

int cdt_connection_connect(cdt_connection_t *connection, const char *address, const char *port) {
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

  memset(connection, 0, sizeof(cdt_connection_t));

  struct addrinfo *rp;
  int sfd;
  for (rp = result; rp != NULL; rp = rp->ai_next) {
    sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sfd == -1)
      continue;

    if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
      if (extract_location_from_addr(rp->ai_family, rp->ai_addr, connection) != 0) {
        debug_print("Cannot extract address from socket\n");
        close(sfd);
        freeaddrinfo(result);
        return -1;
      }
      break;
    }
    
    close(sfd);
  }

  freeaddrinfo(result);

  if (rp == NULL) {
    debug_print("Could not connect\n");
    return -1;
  }

  connection->fd = sfd;
  return 0;
}

void cdt_connection_close(cdt_connection_t *connection) {
  shutdown(connection->fd, SHUT_RDWR);
  close(connection->fd);
  connection->fd = -1;
}

int cdt_connection_read(const cdt_connection_t *connection, char *data, size_t length) {
  return read(connection->fd, data, length);
}

int cdt_connection_write(const cdt_connection_t *connection, const char *data, size_t length) {
  return write(connection->fd, data, length);
}
