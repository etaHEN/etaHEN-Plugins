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

int main() {
    pid_t pid = getpid();
    jailbreak_process(pid, true);
    printf_notification("PS5Debug v0.0.1 Loaded\nBy Dizz");//DIZZ
    ScePthread broadcastThread;
    scePthreadCreate(&broadcastThread, 0, (void*)broadcast_thread, 0, "broadcast_thread");
    start_server();
    return 0;
}
