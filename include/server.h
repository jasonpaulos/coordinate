#ifndef COORDINATE_SERVER_H
#define COORDINATE_SERVER_H

typedef struct cdt_connection_t cdt_connection_t;

/**
 * An object that represents a coordinate server.
 */
typedef struct cdt_server_t {
  /**
   * The file descriptor of the server.
   */
  int fd;
} cdt_server_t;

/**
 * Create a new coordinate server that will listen on the specified address and port.
 * Pass in a null pointer for address to accept incoming messages to any address.
 * Pass in a string containing a numeric port number or service name for port.
 * 
 * Returns 0 on success, -1 on error.
 */
int cdt_server_create(cdt_server_t *server, const char *address, const char *port);

/**
 * Begin listening on the server.
 * queue is the maximum number of connections to keep waiting before new ones are refused.
 * 
 * Returns 0 on success, -1 on error.
 */
int cdt_server_listen(cdt_server_t *server, int queue);

/**
 * Close the server.
 */
void cdt_server_close(cdt_server_t *server);

/**
 * Blocks until a new connection to the server has been established.
 * 
 * Returns 0 on success, -1 on error.
 */
int cdt_server_accept(const cdt_server_t *server, cdt_connection_t *connection);

#endif
