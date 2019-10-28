#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include "connection.h"

int cdt_connection_create(cdt_connection *connection, int fd) {
  if (fd < 0)
    return -1;

  connection->fd = fd;

  return 0;
}

int cdt_connection_connect(cdt_connection *connection, const char *address, int port) {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    return -1;

  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(address);
  serv_addr.sin_port = htons(port);

  if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    return -1;

  connection->fd = sockfd;

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
