#include "util.h"
#include <stdint.h>

#define MAIN_MANAGER_QUEUE_NAME   "/managerPeerToMainThread"
#define QUEUE_PERMISSIONS 0660
#define MAX_MESSAGES 10 // this is a limit enforced by mqueue (see mq_overview man page)
#define MSG_SIZE sizeof(cdt_message_t)

// Message types
#define ALLOCATE_RESP 0

/**
 * The type for messages passed between threads using a message queue.
 */
typedef struct cdt_message_t {
  int type;
  uint64_t shared_va;
  char page[PAGESIZE + 1];
} cdt_message_t;
