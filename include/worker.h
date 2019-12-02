#ifndef COORDINATE_WORKER_H
#define COORDINATE_WORKER_H

/**
 * Handle CDT_PACKET_THREAD_CREATE_REQ
 * 
 * arg should be a pointer to a cdt_packet_t that has type CDT_PACKET_THREAD_CREATE_REQ
 */
void* cdt_worker_thread_create(void *arg);

#endif
