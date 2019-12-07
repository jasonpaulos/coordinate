// Taken from https://ernie55ernie.github.io/parallel%20programming/2017/03/03/pthread-fast-matrix-multiplication.html

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <coordinate.h>
#include "matrix.h"

/* generate a random floating point number from min to max */
double randfrom(double min, double max) {
  double range = (max - min);
  double div = RAND_MAX / range;
  return min + (rand() / div);
}

double *create_matrix(size_t size) {
  size_t row_size = size * sizeof(double);
  void *matrix = cdt_malloc(size * row_size);

  if (!matrix)
    return NULL;

  double *row = malloc(row_size);
  for (int i = 0; i < size; i++) {
    for (int j = 0; j < size; j++) {
      row[j] = randfrom(-1, 1);
    }
    cdt_memcpy(matrix + i * row_size, row, row_size);
  }
  free(row);

  return (double*)matrix;
}

int main(int argc, char *argv[]) {
  cdt_init();

  if (argc < 2) {
    fprintf(stderr, "Correct usage: matrixmultiply n\n");
    return -1;
  }

  struct timeval start, stop;
  gettimeofday(&start, NULL);

  int N = atoi(argv[1]);

  int fallback = 0;
  if (argc >= 3) {
    fallback = atoi(argv[2]);
  }

  int num_threads = cdt_get_cores(fallback) - 1;

  double *A = create_matrix(N);
  double *B = create_matrix(N);
  double *C = (double*)cdt_malloc(N * N * sizeof(double));

  if (!A || !B || !C) {
    fprintf(stderr, "cdt_malloc failed\n");
    return -1;
  }

  // print_matrix(N, A);
  // print_matrix(N, B);

  printf("%d\n", N);
  int res = multiply(N, A, B, C, num_threads);

  if (res != 0) {
    return res;
  }

  // print_matrix(N, C);

  gettimeofday(&stop, NULL);
  double time_taken = (double)(stop.tv_sec - start.tv_sec) + (double)(stop.tv_usec - start.tv_usec) / 1e6;

  printf("Time taken: %f seconds\n", time_taken);

  return 0;
}
