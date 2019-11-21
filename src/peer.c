#include <stdio.h>
#include "util.h"
#include "packet.h"
#include "host.h"

extern cdt_host_t host;

int cdt_peer_greet_existing_peer(cdt_host_t *host, int peer_id, const char *peer_address, const char *peer_port) {
  cdt_peer_t *peer = &host->peers[peer_id];

  if (cdt_connection_connect(&peer->connection, peer_address, peer_port) == -1) {
    debug_print("Error connecting to peer at %s:%s\n", peer_address, peer_port);
    return -1;
  }

  cdt_packet_t packet;
  if (cdt_packet_existing_peer_create(&packet, host->self_id) != 0 || cdt_connection_send(&peer->connection, &packet) != 0) {
    debug_print("Error greeting peer %d at %s:%s\n", peer_id, peer_address, peer_port);
    cdt_connection_close(&peer->connection);
    return -1;
  }

  peer->id = peer_id;
  host->num_peers++;
  
  if (cdt_peer_start(peer) != 0) {
    peer->id = 0;
    host->num_peers--;
    debug_print("Error starting thread for peer %d (%s:%s)\n", peer_id, peer_address, peer_port);
    cdt_connection_close(&peer->connection);
    return -1;
  }

  return 0;
}

void* cdt_peer_thread(void *arg) {
  cdt_peer_t *peer = (cdt_peer_t*)arg;

  while(1) {
    cdt_packet_t packet;
    if (cdt_connection_receive(&peer->connection, &packet) != 0) {
      fprintf(stderr, "Error reading connection from %s:%d\n", peer->connection.address, peer->connection.port);
      break;
    }

    if (packet.type == CDT_PACKET_NEW_PEER && peer->id == 0) { // only the manager (peer 0) can notify of new peers
      int peer_id;
      char *address, *port;
      if (cdt_packet_new_peer_parse(&packet, &peer_id, &address, &port) != 0) {
        fprintf(stderr, "Failed to parse new peer packet from %s:%d\n", peer->connection.address, peer->connection.port);
        break;
      }
      
      if (cdt_peer_greet_existing_peer(peer->host, peer_id, address, port) != 0) {
        fprintf(stderr, "Failed to greet new peer at %s:%s\n", address, port);
        break;
      }

      printf("Greeted new peer %d at %s:%s\n", peer_id, address, port);
    }
    
    if (packet.type == CDT_PACKET_ALLOC_REQ && host.manager == 1) { // only the manager can allocate a page
      int peer_id;
      if (cdt_packet_alloc_req_parse(&packet, &peer_id) != 0) {
        fprintf(stderr, "Failed to parse allocation request packet from %s:%d\n", peer->connection.address, peer->connection.port);
        break;
      }

      printf("Received allocation request from peer %d\n", peer_id);
    }

    // TODO: handle other packets
  }
  
  return NULL;
}

int cdt_peer_start(cdt_peer_t *peer) {
  return pthread_create(&peer->read_thread, NULL, cdt_peer_thread, (void*)peer);
}

void cdt_peer_join(cdt_peer_t *peer) {
  pthread_join(peer->read_thread, NULL);
}
