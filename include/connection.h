#ifndef COORDINATE_CONNECTION_H
#define COORDINATE_CONNECTION_H
#include <arpa/inet.h>

typedef struct cdt_packet_t cdt_packet_t;

/**
 * An object that represents a coodinate connection.
 */
typedef struct cdt_connection_t {
  int fd;
  int port;
  char address[INET6_ADDRSTRLEN];
} cdt_connection_t;

/**
 * Create a new connection from a socket file descriptor and peer address.
 * 
 * Returns 0 on success, -1 if the descriptor is invalid.
 */
int cdt_connection_create(cdt_connection_t *connection, int fd, struct sockaddr_storage *addr);

/**
 * Create a new connection by attempting to connect to a given IP address and port.
 * 
 * Returns 0 on success, -1 on error.
 */
int cdt_connection_connect(cdt_connection_t *connection, const char *address, const char *port);

/**
 * Close the connection.
 */
void cdt_connection_close(cdt_connection_t *connection);

/**
 * Receive a packet from this connection. This operation will block until the packet is completely received, or an error occurs.
 * 
 * Returns 0 on success, or -1 on error.
 */
int cdt_connection_receive(const cdt_connection_t *connection, cdt_packet_t *packet);

/**
 * Send a packet along this connection. This operation will block until the packet is completely sent, or an error occurs.
 * 
 * Returns 0 on success, or -1 on error.
 */
int cdt_connection_send(const cdt_connection_t *connection, const cdt_packet_t *packet);

#endif
