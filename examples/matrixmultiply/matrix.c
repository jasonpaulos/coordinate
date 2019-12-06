#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <coordinate.h>
#include "matrix.h"

void print_matrix(int N, double *matrix) {
  for (int i = 0; i < N; i++) {
    double *row = matrix + i * N;
    fprintf(stderr, "[");
    for (int j = 0; j < N; j++) {
      double val;
      cdt_memcpy(&val, row + j, sizeof(val));
      fprintf(stderr, " %f" ,val);
    }
    fprintf(stderr, " ]\n");
  }
  fprintf(stderr, "\n");
}

// arguments type created for pthread arguments
typedef struct arguments{
  int start_row;
  int end_row;
  int N;
  double *A;
  double *B;
  double *C;
}arguments;

// compute the multiplication of matrix rows specified
// by the arguments defined by starting row and ending
// row
void* compute_row(void* args){
  arguments argument;
  cdt_memcpy(&argument, args, sizeof(arguments));

  int start_row = argument.start_row;
  int end_row = argument.end_row;

  printf("Computing rows %d through %d of the matrix\n", start_row, end_row);

  for(int i = start_row; i < end_row; i++){
    for (int j = 0; j < argument.N; j++) {
      double C_i_j = 0;

      printf("Computing (i = %d,j = %d)\n", i, j);

      for (int k = 0; k < argument.N; k++) {
        double A_i_k, B_j_k;
        cdt_memcpy(&A_i_k, argument.A + i * argument.N + k, sizeof(double));
        cdt_memcpy(&B_j_k, argument.B + j * argument.N + k, sizeof(double));

        C_i_j += A_i_k * B_j_k;
      }
      
      cdt_memcpy(argument.C + i * argument.N + j, &C_i_j, sizeof(double));
    }
  }

  printf("Done\n");

  return NULL;
}

void transpose(int N, double *M) {
  double *matrix = malloc(N * N * sizeof(double));

  for(int i = 0; i < N; i++){
    for(int j = 0; j < N; j++){
      cdt_memcpy(matrix + i * N + j, M + j * N + i, sizeof(double));
    }
  }

  cdt_memcpy(M, matrix, N * N * sizeof(double));
  free(matrix);
}

// multiple two square matrix, A and B, with size N
// and return the value into C
int multiply(int N, double *A, double *B, double *C) {

  // initialize pthread attribute value
  
  int num_threads = cdt_get_cores() - 1;

  cdt_thread_t *thread_pool = malloc(num_threads * sizeof(cdt_thread_t));
  arguments *args_pool = (arguments*)cdt_malloc(num_threads * sizeof(arguments));

  transpose(N, B);
  // print_matrix(N, B);

  sleep(1);

  // if the number of matrix row is smaller than the total number of thread
  // then use the number of matrix row as the number of spliting the work
  int n_split = N < num_threads ? N : num_threads;
  // calculate the number of work row in each split
  int n_work = N < num_threads ? 1 : N / num_threads;
  int n_leftover = N < num_threads ? 0 : N % num_threads;

  for (int i = 0; i < n_split; i++) {
    arguments args = {
      .start_row = i * n_work, // calculate the starting and ending index of the row
      .end_row = args.start_row + n_work + (i == n_split - 1 ? n_leftover : 0),
      .N = N,
      .A = A,
      .B = B,
      .C = C
    };

    cdt_memcpy(args_pool + i, &args, sizeof(args));

    // create a thread, store in the thread pool, and assign its work
    // by argments pool
    if (cdt_thread_create(thread_pool + i, compute_row, (void*)(args_pool + i)) != 0) {
      fprintf(stderr, "Failed to create thread %d\n", i);
      return -1;
    }
  }

  for(int i = 0; i < n_split; i++){
    // wait for all threads to finish their jobs
    if (cdt_thread_join(thread_pool + i, NULL) != 0) {
      fprintf(stderr, "Failed to join thread %d\n", i);
      return -1;
    }
  }

  return 0;
}
