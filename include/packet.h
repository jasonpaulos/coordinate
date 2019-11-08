#ifndef COORDINATE_PACKET_H
#define COORDINATE_PACKET_H

#define CDT_PACKET_DATA_SIZE 100

enum cdt_packet_type {
  CDT_PACKET_SELF_IDENTIFY,
  CDT_PACKET_PEER_ID_ASSIGN,
  CDT_PACKET_PEER_ID_CONFIRM,

  CDT_PACKET_NEW_PEER,
  CDT_PACKET_EXISTING_PEER,
};

/**
 * A data type to represent a network packet.
 */
typedef struct cdt_packet_t {
  /* The size of the packet's data. */
  int size;
  /* The type of the packet. */
  int type;
  /* The payload of the packet. */
  char data[CDT_PACKET_DATA_SIZE];
} cdt_packet_t;

int cdt_packet_self_identify_create(cdt_packet_t *packet, const char *address, const char *port);
int cdt_packet_self_identify_parse(cdt_packet_t *packet, char **address, char **port);

int cdt_packet_peer_id_assign_create(cdt_packet_t *packet, int peer_id);
int cdt_packet_peer_id_assign_parse(cdt_packet_t *packet, int *peer_id);

void cdt_packet_peer_id_confim_create(cdt_packet_t *packet);

int cdt_packet_new_peer_create(cdt_packet_t *packet, int peer_id, const char *address, const char *port);
int cdt_packet_new_peer_parse(cdt_packet_t *packet, int *peer_id, char **address, char **port);

int cdt_packet_existing_peer_create(cdt_packet_t *packet, int peer_id);
int cdt_packet_existing_peer_parse(cdt_packet_t *packet, int *peer_id);

#endif
