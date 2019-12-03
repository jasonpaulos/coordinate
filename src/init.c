#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <mqueue.h>
#include "server.h"
#include "connection.h"
#include "packet.h"
#include "host.h"
#include "init.h"

int cdt_init_get_args(char ***_argv) {
  int header[3];

  int left_to_read = sizeof(header);
  void *data = (void*)&header;
  while (left_to_read > 0) {
    int n = read(STDIN_FILENO, data, left_to_read);
    if (n <= 0) return -1;

    left_to_read -= n;
    data += n;
  }

  int size = header[0];
  int argc = header[1];
  int stdin_fd_clone = header[2];
  
  if (size <= 0 || argc <= 0)
    return -1;
  
  void *args = malloc(argc * sizeof(char*) + size * sizeof(char));
  if (!args)
    return -1;

  char **argv = (char**)args;
  char *arg_buffer = (char*)(args + argc * sizeof(char*));

  left_to_read = size;
  data = arg_buffer;
  while (left_to_read > 0) {
    int n = read(STDIN_FILENO, data, left_to_read);
    if (n <= 0) return -1;

    left_to_read -= n;
    data += n;
  }

  int i = 0;
  argv[0] = arg_buffer;
  for (int j = 0; j < size - 1; j++) {
    char *c = arg_buffer + j;
    if (*c == 0) {
      if (++i == argc) {
        return -1;
      }

      argv[i] = c + 1;
    }
  }

  *_argv = argv;

  if (stdin_fd_clone != STDIN_FILENO) {
    if (close(STDIN_FILENO) != 0) {
      free(argv);
      return -1;
    }

    if (dup2(stdin_fd_clone, STDIN_FILENO) != 0) {
      free(argv);
      return -1;
    }
  }

  return argc;
}

int cdt_main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s --host IP:PORT [--cores CORES --connect IP:PORT] COMMAND [COMMAND_ARGS]\n", argv[0]);
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

  int cores_index = 0;
  for (int i = 1; i < argc - 1; i++) {
    if (strcmp(argv[i], "--cores") == 0) {
      cores_index = i + 1;
      break;
    }
  }

  if (!host_index) {
    fprintf(stderr, "Missing --host option\n");
    return -1;
  }

  if (cores_index == connection_index) {
    fprintf(stderr, "Must specify one of --cores or --connection options\n");
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

  int cores = 0;
  if (cores_index) {
    cores = atoi(argv[cores_index]);
    if (cores < 1 || cores > CDT_MAX_MACHINES) {
      fprintf(stderr, "Invalid value for cores: %s\n", argv[cores_index]);
      return -1;
    }
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
  
  uint32_t core_set = (1 << cores) - 2; // create a set where each bit from 1 to cores is 1, and the 0th bit is 0
  cdt_host_t *host = cdt_host_init(connection_index == 0, &server, core_set);

  if (connection_index) {
    cdt_connection_t *manager_connection = &host->peers[0].connection;

    if (cdt_connection_connect(manager_connection, connection_address, connection_port) == -1) {
      fprintf(stderr, "Error connecting to server\n");
      return -1;
    }
    printf("Connected to %s:%s\n", connection_address, connection_port);

    cdt_packet_t packet;
    if (cdt_packet_self_identify_create(&packet, host_address, host_port) != 0) {
      fprintf(stderr, "Cannot create self identify packet\n");
      return -1;
    }

    if (cdt_connection_send(manager_connection, &packet) != 0) {
      fprintf(stderr, "Failed to send self identify packet\n");
      return -1;
    }

    if (cdt_connection_receive(manager_connection, &packet) != 0) {
      fprintf(stderr, "Failed to get peer id assign packet\n");
      return -1;
    }

    cdt_packet_peer_id_assign_parse(&packet, &host->self_id);

    printf("Assigned machine id %d\n", host->self_id);

    cdt_packet_peer_id_confim_create(&packet);
    if (cdt_connection_send(manager_connection, &packet) != 0) {
      fprintf(stderr, "Failed to send peer id confirmation packet\n");
      return -1;
    }

    host->peers_to_be_connected &= ((1 << host->self_id) - 1); // initialize to 1s between 0 (exclusive) and host.self_id (exclusive)
    host->peers[host->self_id].id = host->self_id;

    cdt_peer_start(&host->peers[0]);

    printf("Connection successful\n");
  }

  cdt_peer_setup_task_queue(&host->peers[host->self_id]);

  if (cdt_host_start() != 0) {
    fprintf(stderr, "Cannot start host thread\n");
    return -1;
  }

  cdt_host_join(&host);

  printf("Peers done connecting\n");

  if (!host->manager) {
    for (int i = 0; i < host->num_peers; i++) {
      cdt_peer_t *peer = &host->peers[i];
      if (peer->id == host->self_id) continue;

      cdt_peer_join(peer);
      cdt_connection_close(&peer->connection);
    }

    cdt_server_close(&server);
    exit(0);
  }

  return 0;
}

void cdt_cleanup() {
  cdt_host_t *host = cdt_get_host();
  if (host) {
    cdt_server_close(host->server);
    // TODO: cleanup everything else, including message queues
  }
}

void cdt_init() {
#ifdef COORDINATE_LOCAL
  printf("coordinate started under local mode\n");
#else
  printf("coordinate started under dsm mode\n");

  char **argv;
  int argc = cdt_init_get_args(&argv);
  if (argc < 0) {
    fprintf(stderr, "Failed to get coordinate args\n");
    exit(-1);
  }

  int status = cdt_main(argc, argv);

  atexit(cdt_cleanup);

  if (status != 0) {
    free(argv);
    exit(status);
  }
#endif
}
