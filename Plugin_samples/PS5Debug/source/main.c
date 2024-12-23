#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include "jailbreak.h"
#include "server.h"
#include "libsce_defs.h"
#include "utils.h"
#include <signal.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <stdlib.h>


static pid_t
find_pid(const char* name) {
  int mib[4] = {1, 14, 8, 0};
  pid_t mypid = getpid();
  pid_t pid = -1;
  size_t buf_size;
  uint8_t *buf;

  if(sysctl(mib, 4, 0, &buf_size, 0, 0)) {
    return -1;
  }

  if(!(buf=malloc(buf_size))) {
    return -1;
  }

  if(sysctl(mib, 4, buf, &buf_size, 0, 0)) {
    free(buf);
    return -1;
  }

  for(uint8_t *ptr=buf; ptr<(buf+buf_size);) {
    int ki_structsize = *(int*)ptr;
    pid_t ki_pid = *(pid_t*)&ptr[72];
    char *ki_tdname = (char*)&ptr[447];

    ptr += ki_structsize;
    if(!strcmp(name, ki_tdname) && ki_pid != mypid) {
      pid = ki_pid;
    }
  }

  free(buf);

  return pid;
}

int main() {
    syscall(SYS_thr_set_name, -1, "ps5debug_dizz.elf");

    int pid;
    while((pid=find_pid("ps5debug_dizz.elf")) > 0) {
        if(kill(pid, SIGKILL)) {
            return -1;
        }
        printf_notification("Killed old instance");
        sleep(1);
    }

    printf_notification("PS5Debug v0.0.1 Loaded\nBy Dizz");
    ScePthread broadcastThread;
    scePthreadCreate(&broadcastThread, 0, (void*)broadcast_thread, 0, "broadcast_thread");
    start_server();
    return 0;
}