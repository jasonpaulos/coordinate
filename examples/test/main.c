#include <stdio.h>
#include <unistd.h>
#include <coordinate.h>

void* print_message(void* arg) {
  printf("Hello from the second thread, my arg is %p\n", arg);
  return NULL;
}

int main(int argc, char *argv[]) {
  cdt_init();

  printf("Hello from the main thread\n");
  
  cdt_thread_t thread;
  cdt_thread_create(&thread, print_message, NULL);

  cdt_thread_join(&thread, NULL);

  sleep(5);

  return 0;
}
