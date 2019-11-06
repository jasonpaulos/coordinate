#ifndef COORDINATE_PEER_H
#define COORDINATE_PEER_H

#include "connection.h"

typedef struct cdt_peer_t {
  int self;
  cdt_connection_t connection;
} cdt_peer_t;

#endif
