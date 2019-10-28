#ifndef COORDINATE_CONNECTION_H
#define COORDINATE_CONNECTION_H

/**
 * An object that represents a coodinate connection.
 */
typedef struct cdt_connection {
  int fd;
} cdt_connection;

/**
 * Create a new connection from a socket file descriptor.
 * 
 * Returns 0 on success, -1 if the descriptor is invalid.
 */
int cdt_connection_create(cdt_connection *connection, int fd);

/**
 * Create a new connection by attempting to connect to a given IP address and port.
 * 
 * Returns 0 on success, -1 on error.
 */
int cdt_connection_connect(cdt_connection *connection, const char *address, int port);

/**
 * Close the connection.
 */
void cdt_connection_close(cdt_connection *connection);

/**
 * Perform a blocking read on the connection.
 * 
 * Returns the number of bytes read, -1 on error, and 0 on connection close.
 */
int cdt_connection_read(const cdt_connection *connection, char *data, size_t length);

/**
 * Perform a blocking write on the connection.
 * 
 * Returns the number of bytes written, or -1 on error.
 */
int cdt_connection_write(const cdt_connection *connection, const char *data, size_t length);

#endif
