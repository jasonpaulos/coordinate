#include <stdio.h>
#include <unistd.h>
#include <coordinate.h>
#include <stdlib.h>

void* print_message(void* arg) {
  printf("Hello from the second thread, my arg is %p\n", arg);
  void * dest = (void *)((1L << 32) + 5);
  char * src = "a";
  printf("Trying to memcpy dest %p with src %p\n", dest, src);
  cdt_memcpy(dest, src, 1);
  return NULL;
}

int main(int argc, char *argv[]) {
  cdt_init();

  printf("Hello from the main thread\n");
  void * page = cdt_malloc(1);
  char * src = "a";
  
  cdt_memcpy(page, (void *)src, 1);
  
  cdt_thread_t thread;
  cdt_thread_create(&thread, print_message, NULL);

  cdt_thread_join(&thread, NULL);

  sleep(5000);
  return 0;
}
