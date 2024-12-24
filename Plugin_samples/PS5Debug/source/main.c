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


int main() {
    printf_notification("PS5Debug v0.0.1 Loaded\nBy Dizz");
    ScePthread broadcastThread;
    scePthreadCreate(&broadcastThread, 0, (void*)broadcast_thread, 0, "broadcast_thread");
    start_server();
    return 0;
}