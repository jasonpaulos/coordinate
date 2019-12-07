#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <coordinate.h>

typedef struct worker_assignment {
  void * vec_a_start;
  void * vec_b_start;
  int section_len; 
} worker_assignment;

void* dot_product_worker(void* arg) {
  // Copy out the assignment
  worker_assignment assignment;
  cdt_memcpy((void *)&assignment, arg, sizeof(assignment));

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
void dot_product_test(int vector_size, int num_peers, int num_cores) {
  printf("Starting dot product test\n");

  struct timeval start, stop;
  gettimeofday(&start, NULL);
  
  void * vec_a = cdt_malloc(vector_size);
  void * vec_b = cdt_malloc(vector_size);

  uint8_t *vector = malloc(vector_size);
  fill_random_vector(vector, vector_size);
  cdt_memcpy(vec_a, (void *)vector, vector_size);

  fill_random_vector(vector, vector_size);
  cdt_memcpy(vec_b, (void *)vector, vector_size);

  free(vector);

  cdt_thread_t threads[num_peers];
  int section_size = vector_size / (num_cores - 1);
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

  gettimeofday(&stop, NULL);
  double time_taken = (double)(stop.tv_sec - start.tv_sec) + (double)(stop.tv_usec - start.tv_usec) / 1e6;

  printf("Dot product example result: %ld with time %f\n", result, time_taken);
}

int main(int argc, char *argv[]) {
  cdt_init();

  if (argc < 3) {
    fprintf(stderr, "Correct usage: dotproduct length_in_pages\n");
    return -1;
  }

  int fallback = 0;
  if (argc >= 3) {
    fallback = atoi(argv[2]);
  }

  int num_cores = cdt_get_cores(fallback);
  int num_pages = atoi(argv[1]);
  int num_peers = num_cores - 1;
  
  dot_product_test(4096 * num_pages, num_peers, num_cores);
  return 0;
}