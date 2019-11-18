#ifndef COORDINATE_PACKET_H
#define COORDINATE_PACKET_H

#define CDT_PACKET_DATA_SIZE 100

enum cdt_packet_type {
  CDT_PACKET_SELF_IDENTIFY,
  CDT_PACKET_PEER_ID_ASSIGN,
  CDT_PACKET_PEER_ID_CONFIRM,

  CDT_PACKET_NEW_PEER,
  CDT_PACKET_EXISTING_PEER,

  CDT_PACKET_READ_REQ,
  CDT_PACKET_READ_RESP,
  CDT_PACKET_READ_INVALIDATE_REQ,
  CDT_PACKET_READ_INVALIDATE_RESP,

  CDT_PACKET_WRITE_REQ,
  CDT_PACKET_WRITE_RESP,
  CDT_PACKET_WRITE_DEMOTE_REQ,
  CDT_PACKET_WRITE_DEMOTE_RESP,
  CDT_PACKET_WRITE_INVALIDATE_REQ,
  CDT_PACKET_WRITE_INVALIDATE_RESP,
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

int cdt_packet_read_req_create(cdt_packet_t *packet, unsigned long page_addr);
int cdt_packet_read_req_parse(cdt_packet_t *packet, unsigned long *page_addr);

int cdt_packet_read_resp_create(cdt_packet_t *packet, void *page);
int cdt_packet_read_resp_parse(cdt_packet_t *packet, void **page);

int cdt_packet_read_invalidate_req_create(cdt_packet_t *packet, unsigned long page_addr);
int cdt_packet_read_invalidate_req_parse(cdt_packet_t *packet, unsigned long *page_addr);

void cdt_packet_read_invalidate_resp_create(cdt_packet_t *packet);

int cdt_packet_write_req_create(cdt_packet_t *packet, unsigned long page_addr);
int cdt_packet_write_req_parse(cdt_packet_t *packet, unsigned long *page_addr);

int cdt_packet_write_resp_create(cdt_packet_t *packet, void *page);
int cdt_packet_write_resp_parse(cdt_packet_t *packet, void **page);

int cdt_packet_write_demote_req_create(cdt_packet_t *packet, unsigned long page_addr);
int cdt_packet_write_demote_req_parse(cdt_packet_t *packet, unsigned long *page_addr);

int cdt_packet_write_demote_resp_create(cdt_packet_t *packet, void *page);
int cdt_packet_write_demote_resp_parse(cdt_packet_t *packet, void **page);

int cdt_packet_write_invalidate_req_create(cdt_packet_t *packet, unsigned long page_addr);
int cdt_packet_write_invalidate_req_parse(cdt_packet_t *packet, unsigned long *page_addr);

int cdt_packet_write_invalidate_resp_create(cdt_packet_t *packet, void *page);
int cdt_packet_write_invalidate_resp_parse(cdt_packet_t *packet, void **page);


#endif