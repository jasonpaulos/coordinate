#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include "connection.h"
#include "server.h"

int cdt_server_create(cdt_server *server, const char *address, int port) {
  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd < 0)
    return -1;

  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = address == 0 ? htonl(INADDR_ANY) : inet_addr(address);
  serv_addr.sin_port = htons(port);

  if (bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
    return -1;

  server->fd = listenfd;

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
  int connfd = accept(server->fd, (struct sockaddr*)0, 0);
  return cdt_connection_create(connection, connfd);
}
