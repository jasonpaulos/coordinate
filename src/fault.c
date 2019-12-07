#define _GNU_SOURCE
#include <ucontext.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include "coordinate.h"
#include "util.h"
#include "host.h"
#include "fault.h"

pthread_t cdt_signal_thread_id;
int cdt_signal_notify_pipe[2];

typedef struct cdt_fault_info {
  char write_fault;
  void *addr;
} cdt_fault_info;

void sigaction_handler(int signum, siginfo_t *info, void *context) {
  cdt_fault_info fi = {
    .write_fault = (((ucontext_t*)context)->uc_mcontext.gregs[REG_ERR] & 0x2) != 0,
    .addr = info->si_addr
  };
  int err = errno;
  if (write(cdt_signal_notify_pipe[1], &fi, sizeof(cdt_fault_info)) == -1) {
    char message[] = "Error writing in sigaction_handler\n";
    write(2, message, sizeof(message));
  }
  errno = err;
}

void* cdt_signal_thread(void *arg) {
  while (1) {
    cdt_fault_info info;
    int n = read(cdt_signal_notify_pipe[0], &info, sizeof(cdt_fault_info));
    if (n < sizeof(cdt_fault_info))
      break;

    printf("Got %d bytes from signal handler: write_fault = %d at location %p\n", n, info.write_fault, info.addr);

    if (info.write_fault) {
      char test_value = 0;
      cdt_memcpy(info.addr, &test_value, 1);
      printf("fault handler: wrote to address %p\n", info.addr);
    } else {
      char pagebuf[PAGESIZE];
      cdt_memcpy(pagebuf, info.addr, PAGESIZE);
      printf("fault handler: read address %p\n", info.addr);
    }

    sleep(1);
  }
  
  return NULL;
}

int cdt_setup_fault_handler() {
  if (pipe(cdt_signal_notify_pipe) != 0)
    return -1;
  
  if (pthread_create(&cdt_signal_thread_id, NULL, cdt_signal_thread, NULL) != 0)
    return -1;
  
  struct sigaction action = {
    .sa_flags = SA_SIGINFO,
    .sa_sigaction = sigaction_handler
  };
  sigfillset(&action.sa_mask);
  
  if (sigaction(SIGSEGV, &action, NULL) != 0)
    return -1;
  
  return 0;
}
