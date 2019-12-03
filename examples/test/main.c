#include <stdio.h>
#include <unistd.h>
#include <coordinate.h>

void* print_message(void* arg) {
  printf("Hello from the second thread, my arg is %p\n", arg);

  sleep(5);

  printf("Second thread done\n");

  return (void*)17;
}

int main(int argc, char *argv[]) {
  cdt_init();

  printf("Hello from the main thread\n");
  
  cdt_thread_t thread;
  cdt_thread_create(&thread, print_message, NULL);
  
  void *return_value;
  cdt_thread_join(&thread, &return_value);

  printf("Got %ld from other thread, done!\n", (long)return_value);

  return 0;
}
