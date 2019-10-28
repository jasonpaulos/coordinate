#ifndef COORDINATE_SERVER_H
#define COORDINATE_SERVER_H

typedef struct cdt_connection cdt_connection;

/**
 * An object that represents a coordinate server.
 */
typedef struct cdt_server {
  /**
   * The file descriptor of the server.
   */
  int fd;
} cdt_server;

/**
 * Create a new coordinate server with a local address and port.
 * Pass in a null pointer for address to accept incoming messages to any address.
 * 
 * Returns 0 on success, -1 on error.
 */
int cdt_server_create(cdt_server *server, const char *address, int port);

/**
 * Begin listening on the server.
 * queue is the maximum number of connections to keep waiting before new ones are refused.
 * 
 * Returns 0 on success, -1 on error.
 */
int cdt_server_listen(cdt_server *server, int queue);

/**
 * Close the server.
 */
void cdt_server_close(cdt_server *server);

/**
 * Blocks until a new connection to the server has been established.
 * 
 * Returns 0 on success, -1 on error.
 */
int cdt_server_accept(const cdt_server *server, cdt_connection *connection);

#endif
