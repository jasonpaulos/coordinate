#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/personality.h>

int main(int argc, char *argv[]) {
  int coordinate_argc = -1;
  for (int i = 1; i < argc; i++) {
    char *arg = argv[i];
    if (arg[0] == '-' && arg[1] == '-') { // arg starts with "--"
      i++; // skip next arg
    } else {
      coordinate_argc = i;
      break;
    }
  }

  if (argc < 2 || coordinate_argc == -1) {
    fprintf(stderr, "Usage: %s --host IP:PORT [--cores CORES --connect IP:PORT] COMMAND [COMMAND_ARGS]\n", argv[0]);
    return -1;
  }

  int stdin_fd_clone = dup(STDIN_FILENO);
  int pipes[2];

  if (close(STDIN_FILENO) != 0 || pipe(pipes) != 0) {
    fprintf(stderr, "%s: Failed to setup pipes\n", argv[0]);
    return -1;
  }

  int read_pipe = pipes[0];
  int write_pipe = pipes[1];

  int result = fork();
  if (result == -1) {
    fprintf(stderr, "%s: Failed to fork\n", argv[0]);
    return -1;
  }

  if (result == 0) { // child process
    if (close(write_pipe) != 0) {
      fprintf(stderr, "%s: Failed to close write pipe\n", argv[0]);
      return -1;
    }

    if (putenv("LD_PRELOAD=libcoordinate.dsm.so") != 0) {
      fprintf(stderr, "%s: Failed to set LD_PRELOAD\n", argv[0]);
      return -1;
    }

    if (personality(ADDR_NO_RANDOMIZE) == -1) {
      // Disable address space layout randomization so that functions will be located at the same
      // addresses across the different machines. This is essential to allowing threads to be
      // started on other machines using just a pointer to a function.
      fprintf(stderr, "%s: Failed to disable address space layout randomization: %s\n", argv[0], strerror(errno));
    }

    execvp(argv[coordinate_argc], argv + coordinate_argc);

    fprintf(stderr, "%s: Failed to exec user program %s: %s\n", argv[0], argv[coordinate_argc], strerror(errno));
    return -1;
  }
  
  // parent process
  if (close(read_pipe) != 0) {
    fprintf(stderr, "%s: Failed to close read pipe\n", argv[0]);
    return -1;
  }

  int size = 0;
  for (int i = 0; i < coordinate_argc; i++) {
    size += strlen(argv[i]) + 1; // include terminating null char
  }

  int header[] = {
    size,
    coordinate_argc,
    stdin_fd_clone
  };

  int left_to_write = sizeof(header);
  void *data = (void*)header;
  while (left_to_write > 0) {
    int n = write(write_pipe, data, left_to_write);
    if (n <= 0) return -1;

    left_to_write -= n;
    data += n;
  }

  for (int i = 0; i < coordinate_argc; i++) {
    left_to_write = strlen(argv[i]) + 1;
    data = (void*)argv[i];

    while (left_to_write > 0) {
      int n = write(write_pipe, data, left_to_write);
      if (n <= 0) return -1;

      left_to_write -= n;
      data += n;
    }
  }

  if (close(write_pipe) != 0) {
    fprintf(stderr, "%s: Failed to close write pipe\n", argv[0]);
    return -1;
  }

  int status;
  wait(&status);
  if (status != 0) {
    fprintf(stderr, "%s: User process exited with status code %d\n", argv[0], status);
  }

  return status;
}
