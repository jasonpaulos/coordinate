#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mqueue.h>
#include <errno.h>
#include "server.h"
#include "connection.h"
#include "packet.h"
#include "host.h"
#include "message.h"

cdt_host_t host;
cdt_connection_t manager_connection;
mqd_t qd_manager_peer_thread;   // queue descriptor for message queue TO the manager peer-thread (R/O)

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s --host IP:PORT [--connect IP:PORT] COMMAND [INITIAL_ARGS]...\n", argv[0]);
    return -1;
  }

  int host_index = 0;
  for (int i = 1; i < argc - 1; i++) {
    if (strcmp(argv[i], "--host") == 0) {
      host_index = i + 1;
      break;
    }
  }

  int connection_index = 0;
  for (int i = 1; i < argc - 1; i++) {
    if (strcmp(argv[i], "--connect") == 0) {
      connection_index = i + 1;
      break;
    }
  }

  if (!host_index) {
    fprintf(stderr, "Missing --host option\n");
    return -1;
  }

  char *host_port;
  char *host_address = argv[host_index];
  char *lastColon = strrchr(host_address, ':');
  if (lastColon == NULL) {
    fprintf(stderr, "Unable to parse address: %s\n", host_address);
    return -1;
  }
  host_port = lastColon + 1;
  *lastColon = 0;

  char *connection_port = NULL;
  char *connection_address = NULL;
  if (connection_index) {
    connection_address = argv[connection_index];
    lastColon = strrchr(connection_address, ':');
    if (lastColon == NULL) {
      fprintf(stderr, "Unable to parse address: %s\n", connection_address);
      return -1;
    }

    connection_port = lastColon + 1;
    *lastColon = 0;
  }

  cdt_server_t server;
  if (cdt_server_create(&server, host_address, host_port) == -1) {
    fprintf(stderr, "Cannot create server\n");
    return -1;
  }
  if (cdt_server_listen(&server, 10) == -1) {
    fprintf(stderr, "Server cannot listen\n");
    return -1;
  }

  printf("Listening at %s:%s\n", host_address == NULL ? "*" : host_address, host_port);

  memset(&host, 0, sizeof(host));
  host.manager = connection_index == 0;
  host.server = &server;
  host.peers_to_be_connected = ~1;

  if (connection_index) {
    if (cdt_connection_connect(&manager_connection, connection_address, connection_port) == -1) {
      fprintf(stderr, "Error connecting to server\n");
      return -1;
    }
    printf("Connected to %s:%s\n", connection_address, connection_port);

    cdt_packet_t packet;
    if (cdt_packet_self_identify_create(&packet, host_address, host_port) != 0) {
      fprintf(stderr, "Cannot create self identify packet\n");
      return -1;
    }

    if (cdt_connection_send(&manager_connection, &packet) != 0) {
      fprintf(stderr, "Failed to send self identify packet\n");
      return -1;
    }

    if (cdt_connection_receive(&manager_connection, &packet) != 0) {
      fprintf(stderr, "Failed to get peer id assign packet\n");
      return -1;
    }

    cdt_packet_peer_id_assign_parse(&packet, &host.self_id);

    printf("Assigned machine id %d\n", host.self_id);

    cdt_packet_peer_id_confim_create(&packet);
    if (cdt_connection_send(&manager_connection, &packet) != 0) {
      fprintf(stderr, "Failed to send peer id confirmation packet\n");
      return -1;
    }
    struct mq_attr attr;

    attr.mq_flags = 0;
    attr.mq_maxmsg = MAX_MESSAGES;
    attr.mq_msgsize = MSG_SIZE;
    attr.mq_curmsgs = 0;

    if ((qd_manager_peer_thread = mq_open (MAIN_MANAGER_QUEUE_NAME, O_RDONLY | O_CREAT, QUEUE_PERMISSIONS, &attr)) == -1) {
      debug_print("Main thread failed to create message queue to manager peer-thread, \n");
      perror("Error num");
      return -1;
    }

    host.peers[0].connection = manager_connection;
    host.peers[0].host = &host;
    host.peers_to_be_connected &= ((1 << host.self_id) - 1); // initialize to 1s between 0 (exclusive) and host.self_id (exclusive)
    cdt_peer_start(&host.peers[0]);

    printf("Connection successful\n");
  }

  if (cdt_host_start(&host) == -1) {
    fprintf(stderr, "Cannot start host thread\n");
    return -1;
  }

  cdt_host_join(&host);

  printf("Peers done connecting\n");

  for (int i = 0; i < host.num_peers; i++) {
    cdt_peer_t *peer = &host.peers[i];
    if (peer->id == host.self_id) continue;

    cdt_peer_join(peer);
    cdt_connection_close(&peer->connection);
  }

  cdt_server_close(&server);

  return 0;
}
