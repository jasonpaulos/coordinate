#include <arpa/inet.h>
#include "util.h"

uint64_t htonll(uint64_t x) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  return x;
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return ((uint64_t)htonl(x & 0xFFFFFFFF) << 32) | htonl(x >> 32);
#else
#error "Byte order not supported"
#endif
}

uint64_t ntohll(uint64_t x) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  return x;
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return ((uint64_t)ntohl(x & 0xFFFFFFFF) << 32) | ntohl(x >> 32);
#else
#error "Byte order not supported"
#endif
}
