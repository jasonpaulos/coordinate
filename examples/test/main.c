#include <stdio.h>
#include <unistd.h>
#include <coordinate.h>
#include <stdlib.h>

void* test0_helper(void* arg) {
  printf("Hello from the second thread, my arg is %p\n", arg);
  void * page = cdt_malloc(5 * 4096);
  char * src = "a";
  
  cdt_memcpy(page, (void *)src, 1);
  sleep(5000);
  return NULL;
}

void test0_malloc2_on_peer(void) {
  printf("Hello from the main thread\n");
  
  cdt_thread_t thread;
  cdt_thread_create(&thread, test0_helper, NULL);

  sleep(5000);
}

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
  sleep(500);
  return NULL;
}

int test2_malloc_on_mngr_write_on_peer(void) {
  printf("Hello from the main thread\n");
  void * page = cdt_malloc(4 * 4096 + 3);
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
void* test4and5_helper(void* arg) {
  printf("Hello from the second thread, my arg is %p\n", arg);
  void * src = (void *)(1L << 32);
  char dest[2];
  
  cdt_memcpy(dest, (void *)src, 1);
  printf("Read into local buf: %s\n", dest);
  return NULL;
}

void test4_malloc_on_mngr_read_on_peer(void) {
  printf("Hello from the main thread\n");
  void * page = cdt_malloc(1);
  char * src = "a";
  
  cdt_memcpy(page, (void *)src, 1);
  
  cdt_thread_t thread;
  cdt_thread_create(&thread, test4and5_helper, NULL);

  sleep(500);
}

void test5_malloc_on_mngr_read_on_peer_write_on_mngr(void) {
  printf("Hello from the main thread\n");
  void * page = cdt_malloc(1);
  char * src = "a";
  
  cdt_memcpy(page, (void *)src, 1);
  
  cdt_thread_t thread;
  cdt_thread_create(&thread, test4and5_helper, NULL);

  sleep(5);

  void * dest = (void *)(1L << 32);
  char * new_src = "b";
  cdt_memcpy(dest, (void *)new_src, 1);
}

void* test6_helper1(void* arg) {
  printf("Hello from the second thread, my arg is %p\n", arg);
  void * src = (void *)(1L << 32);
  char dest[2];
  
  cdt_memcpy(dest, (void *)src, 1);
  printf("Read into local buf: %s\n", dest);
  sleep(500);
  return NULL;
}

void* test6_helper2(void* arg) {
  sleep(10);
  printf("Hello from the third thread, my arg is %p\n", arg);

  void * dest = (void *)((1L << 32) + 1);
  char * new_src = "b";
  cdt_memcpy(dest, (void *)new_src, 1);
  
  return NULL;
}

void test6_malloc_on_mngr_read_on_peer1_write_on_peer2(void) {
  printf("Hello from the main thread\n");
  void * page = cdt_malloc(1);
  char * src = "a";
  
  cdt_memcpy(page, (void *)src, 1);
  
  cdt_thread_t thread;
  cdt_thread_create(&thread, test6_helper1, NULL);
  cdt_thread_create(&thread, test6_helper2, NULL);

  sleep(500);
}

void* print_message(void* arg) {
  printf("Hello from the second thread, my arg is %p\n", arg);

  sleep(5);

  printf("Second thread done\n");

  return (void*)17;
}

typedef struct worker_assignment {
  void * vec_a_start;
  void * vec_b_start;
  int section_len; 
} worker_assignment;

void* dot_product_worker(void* arg) {
  // Copy out the assignment
  worker_assignment assignment;
  cdt_memcpy((void *)&assignment, arg, sizeof(assignment));
  printf("Vec a start: %p, vec b start: %p, section len: %d\n", assignment.vec_a_start, assignment.vec_b_start, assignment.section_len);

  // Iterate over each byte of both vec_a and vec_b for the section, multiplying
  intptr_t result = 0;
  uint8_t buf_a[1];
  uint8_t buf_b[1];
  for (int i = 0; i < assignment.section_len; i++) {
    cdt_memcpy(buf_a, (void *)((uint64_t)assignment.vec_a_start + i), 1);
    cdt_memcpy(buf_b, (void *)((uint64_t)assignment.vec_b_start + i), 1);
    result += buf_a[0] * buf_b[0];
  }
  return (void *)result;
}

void fill_random_vector(uint8_t * vector, int vector_size) {
  for (int i = 0; i < vector_size; i++) {
    *(vector + i) = rand();
  }
}

// Vector size is the length in bytes for each vector
// Assume it is divisible by the number of cores.
void dot_product_test(int vector_size, int num_peers) {
  printf("Starting dot product test\n");
  void * vec_a = cdt_malloc(vector_size);
  void * vec_b = cdt_malloc(vector_size);

  uint8_t local_vec_a[vector_size];
  uint8_t local_vec_b[vector_size];
  fill_random_vector(local_vec_a, vector_size);
  fill_random_vector(local_vec_b, vector_size);

  cdt_memcpy(vec_a, (void *)local_vec_a, vector_size);
  cdt_memcpy(vec_b, (void *)local_vec_b, vector_size);

  cdt_thread_t threads[num_peers];
  int section_size = vector_size / (cdt_get_cores() - 1);
  for (intptr_t i = 0; i < num_peers; i++) {
    void * assignment = cdt_malloc(4096);

    worker_assignment local_assignment = {
      .vec_a_start = (void *)((uint64_t)vec_a + section_size * i),
      .vec_b_start = (void *)((uint64_t)vec_b + section_size * i),
      .section_len = section_size,
    };
    if (i == num_peers - 1) {
      local_assignment.section_len = ((uint64_t)vec_a + vector_size) - ((uint64_t)vec_a + section_size * i);
    }
    cdt_memcpy(assignment, (void *)&local_assignment, sizeof(local_assignment));
    cdt_thread_create(&threads[i], dot_product_worker, (void *)assignment);
  }

  intptr_t result = 0;
  for (int i = 0; i < num_peers; i++) {
    void *return_value;
    cdt_thread_join(&threads[i], &return_value);
    result += (intptr_t)return_value;
  }
  printf("Dot product example result: %ld\n", result);
}

int main(int argc, char *argv[]) {
  cdt_init();

  // printf("Hello from the main thread\n");
  
  // cdt_thread_t thread;
  // cdt_thread_create(&thread, print_message, NULL);
  
  // void *return_value;
  // cdt_thread_join(&thread, &return_value);

  // printf("Got %ld from other thread, done!\n", (long)return_value);
  int num_peers = cdt_get_cores() - 1;
  
  dot_product_test(4096 * 7, num_peers);
  return 0;
}