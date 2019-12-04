#include <stdio.h>
#include <unistd.h>
#include <coordinate.h>
#include <stdlib.h>

void* test1_helper(void* arg) {
  printf("Hello from the second thread, my arg is %p\n", arg);
  void * page = cdt_malloc(1);
  char * src = "a";
  
  cdt_memcpy(page, (void *)src, 1);
  sleep(500000);
  return NULL;
}

void test1_malloc_on_peer_write_on_mngr(void) {
  printf("Hello from the main thread\n");
  
  cdt_thread_t thread;
  cdt_thread_create(&thread, test1_helper, NULL);

  sleep(5);

  void * dest = (void *)((1L << 32) + 1);
  char * src = "a";
  printf("Trying to memcpy dest %p with src %p\n", dest, src);
  cdt_memcpy(dest, src, 1);
}

void* test2_helper(void* arg) {
  printf("Hello from the second thread, my arg is %p\n", arg);
  void * dest = (void *)((1L << 32) + 1);
  char * src = "a";
  printf("Trying to memcpy dest %p with src %p\n", dest, src);
  cdt_memcpy(dest, src, 1);
  return NULL;
}

int test2_malloc_on_mngr_write_on_peer(void) {
  printf("Hello from the main thread\n");
  void * page = cdt_malloc(1);
  char * src = "a";
  
  cdt_memcpy(page, (void *)src, 1);
  
  cdt_thread_t thread;
  cdt_thread_create(&thread, test2_helper, NULL);

  cdt_thread_join(&thread, NULL);

  sleep(5000);
  return 0;
}

void* test3_helper1(void* arg) {
  printf("Hello from the second thread, my arg is %p\n", arg);
  void * page = cdt_malloc(1);
  char * src = "a";
  
  cdt_memcpy(page, (void *)src, 1);
  sleep(500);
  return NULL;
}

void* test3_helper2(void* arg) {
  printf("Hello from the third thread, my arg is %p\n", arg);
  sleep(10);
  void * dest = (void *)((1L << 32) + 1);
  char * src = "b";
  
  cdt_memcpy(dest, (void *)src, 1);
  return NULL;
}

void test3_malloc_on_peer1_write_on_peer2(void) {
  printf("Hello from the main thread\n");
  
  cdt_thread_t thread;
  cdt_thread_create(&thread, test3_helper1, NULL);
  cdt_thread_create(&thread, test3_helper2, NULL);

  sleep(500);
}

void* print_message(void* arg) {
  printf("Hello from the second thread, my arg is %p\n", arg);

  sleep(5);

  printf("Second thread done\n");

  return (void*)17;
}

int main(int argc, char *argv[]) {
  cdt_init();

  // printf("Hello from the main thread\n");
  
  // cdt_thread_t thread;
  // cdt_thread_create(&thread, print_message, NULL);
  
  // void *return_value;
  // cdt_thread_join(&thread, &return_value);

  // printf("Got %ld from other thread, done!\n", (long)return_value);
  test3_malloc_on_peer1_write_on_peer2();
  return 0;
}