#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "server.h"
#include "connection.h"
#include "packet.h"
#include "host.h"

cdt_host_t cdt_host = {
  .manager = -1
};

void* cdt_host_thread(void *arg) {
  printf("Host started\n");

  cdt_connection_t connection;
  while (cdt_host.peers_to_be_connected && cdt_server_accept(cdt_host.server, &connection) == 0) {
    printf("Client connected from %s:%d\n", connection.address, connection.port);
    
    cdt_packet_t packet;
    if (cdt_connection_receive(&connection, &packet) != 0) {
      fprintf(stderr, "Error reading packet from %s:%d\n", connection.address, connection.port);
      cdt_connection_close(&connection);
      continue;
    }

    cdt_peer_t *peer;

    if (cdt_host.manager) {
      if (packet.type != CDT_PACKET_SELF_IDENTIFY) {
        fprintf(stderr, "Unexpected packet type %d from %s:%d\n", packet.type, connection.address, connection.port);
        cdt_connection_close(&connection);
        continue;
      }

      peer = &cdt_host.peers[cdt_host.num_peers];
      peer->id = cdt_host.num_peers;

      char *address, *port;
      cdt_packet_self_identify_parse(&packet, &address, &port);

      cdt_packet_t auxiliary_packet;
      cdt_packet_peer_id_assign_create(&auxiliary_packet, peer->id);

      if (cdt_connection_send(&connection, &auxiliary_packet) != 0) {
        fprintf(stderr, "Failed to send assign id packet to %s:%d\n", connection.address, connection.port);
        cdt_connection_close(&connection);
        continue;
      }

      // TODO: it would be good to enforce a read timeout here
      if (cdt_connection_receive(&connection, &auxiliary_packet) != 0 || auxiliary_packet.type != CDT_PACKET_PEER_ID_CONFIRM) {
        fprintf(stderr, "Failed to receive confirmation packet from %s:%d\n", connection.address, connection.port);
        cdt_connection_close(&connection);
        continue;
      }

      if (cdt_packet_new_peer_create(&auxiliary_packet, peer->id, address, port) != 0) {
        fprintf(stderr, "Unable to create new peer packet for %s:%d\n", connection.address, connection.port);
        cdt_connection_close(&connection);
        continue;
      }

      for (int i = 0; i < cdt_host.num_peers; i++) {
        cdt_peer_t *other = &cdt_host.peers[i];
        if (cdt_host.self_id == i) continue;

        printf("Connecting peer %d to new peer %d (%s:%s)\n", i, peer->id, address, port);

        if (cdt_connection_send(&other->connection, &auxiliary_packet) != 0) {
          fprintf(stderr, "Error connecting peer %d to %s:%s\n", i, address, port);
        }
      }
    } else {
      if (packet.type != CDT_PACKET_EXISTING_PEER) {
        fprintf(stderr, "Unexpected packet type %d from %s:%d\n", packet.type, connection.address, connection.port);
        cdt_connection_close(&connection);
        continue;
      }

      uint32_t peer_id;
      cdt_packet_existing_peer_parse(&packet, &peer_id);

      if (peer_id <= 0 || peer_id > CDT_MAX_MACHINES || peer_id == cdt_host.self_id) {
        fprintf(stderr, "Invalid peer id %d from %s:%d\n", peer_id, connection.address, connection.port);
        cdt_connection_close(&connection);
        continue;
      }

      if (cdt_host.peers[peer_id].id) {
        cdt_connection_t *other = &cdt_host.peers[peer_id].connection;
        fprintf(stderr, "Multiple peers with id %d: %s:%d and %s:%d\n", peer_id, other->address, other->port, connection.address, connection.port);
        cdt_connection_close(&connection);
        continue;
      }

      printf("Peer %d has been connected\n", peer_id);

      peer = &cdt_host.peers[peer_id];
      peer->id = peer_id;
    }

    peer->connection = connection;
    cdt_peer_start(peer);

    cdt_host.peers_to_be_connected &= ~peer->id;
    cdt_host.num_peers++;
  }

  return NULL;
}

cdt_host_t* cdt_host_init(int manager, cdt_server_t *server, uint32_t peers_to_be_connected) {
  if (cdt_host.manager != -1)
    return NULL;

  cdt_host.manager = manager;
  cdt_host.server = server;
  cdt_host.peers_to_be_connected = peers_to_be_connected;
  cdt_host.num_peers = manager ? 1 : 2;

  // Initialize all the locks and virtual addresses for appropriate pagetable
  if (cdt_host.manager) {
    for (int i = 0; i < CDT_MAX_SHARED_PAGES; i++) {
      if (pthread_mutex_init(&cdt_host.manager_pagetable[i].lock, NULL) != 0) { 
        debug_print("Failed to init lock for manager PTE index %d\n", i);
        return NULL;
      } 
      cdt_host.manager_pagetable[i].shared_va = i * PAGESIZE + CDT_SHARED_VA_START;
    }
  } else {
    for (int i = 0; i < CDT_MAX_SHARED_PAGES; i++) {
      if (pthread_mutex_init(&cdt_host.shared_pagetable[i].lock, NULL) != 0) { 
        debug_print("Failed to init lock for manager PTE index %d\n", i);
        return NULL;
      } 
      cdt_host.shared_pagetable[i].shared_va = i * PAGESIZE + CDT_SHARED_VA_START;
    }
  }

  return &cdt_host;
}

int cdt_host_start() {
  if (cdt_host.manager == -1)
    return -1;

  return pthread_create(&cdt_host.server_thread, NULL, cdt_host_thread, NULL);
}

cdt_host_t *cdt_get_host() {
  if (cdt_host.manager == -1)
    return NULL;

  return &cdt_host;
}

int cdt_host_join() {
  if (cdt_host.manager == -1)
    return -1;

  if (pthread_join(cdt_host.server_thread, NULL) != 0) {
    return -1;
  }

  return 0;
}
