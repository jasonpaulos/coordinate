#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "server.h"
#include "connection.h"
#include "packet.h"
#include "host.h"

void* cdt_host_thread(void *arg) {
  cdt_host_t *host = (cdt_host_t*)arg;

  printf("Server started\n");

  // Initialize all the locks and virtual addresses for appropriate pagetable
  if (host->manager) {
    for (int i = 0; i < CDT_MAX_SHARED_PAGES; i++) {
      if (pthread_mutex_init(&host->manager_pagetable[i].lock, NULL) != 0) { 
        fprintf(stderr, "Failed to init lock for manager PTE index %d\n", i);
        return NULL;
      } 
      host->manager_pagetable[i].shared_va = i * getpagesize() + CDT_SHARED_VA_START;
    }
  } else {
    for (int i = 0; i < CDT_MAX_SHARED_PAGES; i++) {
      if (pthread_mutex_init(&host->shared_pagetable[i].lock, NULL) != 0) { 
        fprintf(stderr, "Failed to init lock for manager PTE index %d\n", i);
        return NULL;
      } 
      host->shared_pagetable[i].shared_va = i * getpagesize() + CDT_SHARED_VA_START;
    }
  }

  host->num_peers = host->manager ? 1 : 2;

  cdt_connection_t connection;
  while (host->peers_to_be_connected && cdt_server_accept(host->server, &connection) == 0) {
    printf("Client connected from %s:%d\n", connection.address, connection.port);
    
    cdt_packet_t packet;
    if (cdt_connection_receive(&connection, &packet) != 0) {
      fprintf(stderr, "Error reading packet from %s:%d\n", connection.address, connection.port);
      cdt_connection_close(&connection);
      continue;
    }

    cdt_peer_t *peer;

    if (host->manager) {
      if (packet.type != CDT_PACKET_SELF_IDENTIFY) {
        fprintf(stderr, "Unexpected packet type %d from %s:%d\n", packet.type, connection.address, connection.port);
        cdt_connection_close(&connection);
        continue;
      }

      peer = &host->peers[host->num_peers];
      peer->id = host->num_peers;

      char *address, *port;
      if (cdt_packet_self_identify_parse(&packet, &address, &port) != 0) {
        fprintf(stderr, "Unable to parse self identify packet from %s:%d\n", connection.address, connection.port);
        cdt_connection_close(&connection);
        continue;
      }

      cdt_packet_t auxiliary_packet;
      if (cdt_packet_peer_id_assign_create(&auxiliary_packet, peer->id) != 0) {
        fprintf(stderr, "Unable to create peer assign id packet for %s:%d\n", connection.address, connection.port);
        cdt_connection_close(&connection);
        continue;
      }

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

      for (int i = 0; i < host->num_peers; i++) {
        cdt_peer_t *other = &host->peers[i];
        if (host->self_id == i) continue;

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

      int peer_id;
      if (cdt_packet_existing_peer_parse(&packet, &peer_id) != 0) {
        fprintf(stderr, "Unable to parse existing peer packet from %s:%d\n", connection.address, connection.port);
        cdt_connection_close(&connection);
        continue;
      }

      if (peer_id <= 0 || peer_id > CDT_MAX_MACHINES || peer_id == host->self_id) {
        fprintf(stderr, "Invalid peer id %d from %s:%d\n", peer_id, connection.address, connection.port);
        cdt_connection_close(&connection);
        continue;
      }

      if (host->peers[peer_id].id) {
        cdt_connection_t *other = &host->peers[peer_id].connection;
        fprintf(stderr, "Multiple peers with id %d: %s:%d and %s:%d\n", peer_id, other->address, other->port, connection.address, connection.port);
        cdt_connection_close(&connection);
        continue;
      }

      printf("Peer %d has been connected\n", peer_id);

      peer = &host->peers[peer_id];
      peer->id = peer_id;
    }

    peer->connection = connection;
    peer->host = host;
    cdt_peer_start(peer);

    host->peers_to_be_connected &= ~peer->id;
    host->num_peers++;
  }

  return NULL;
}

int cdt_host_start(cdt_host_t *host) {
  return pthread_create(&host->server_thread, NULL, cdt_host_thread, (void*)host);
}

void cdt_host_join(cdt_host_t *host) {
  pthread_join(host->server_thread, NULL);
}
