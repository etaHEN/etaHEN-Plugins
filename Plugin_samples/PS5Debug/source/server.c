#include <netinet/in.h>
#include <sys/ptrace.h>
#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <x86/reg.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <stdlib.h>
#include "server.h"
#include "utils.h"
#include "console.h"
#include "kern.h"
#include "proc.h"
#include "debug.h"
#include "mdbg.h"
#include "tracer.h"
#include "libsce_defs.h"

bool unload_cmd_sent = false;

struct server_client servclients[SERVER_MAXCLIENTS];

struct server_client *alloc_client() {
    for(int i = 0; i < SERVER_MAXCLIENTS; i++) {
        if(servclients[i].id == 0) {
            servclients[i].id = i + 1;
            return &servclients[i];
        }
    }

    return 0;
}

void free_client(struct server_client *svc) {
    svc->id = 0;
    sceNetSocketClose(svc->fd);

    if(svc->debugging) {
        debug_cleanup(&svc->dbgctx);
    }

    memset(svc, 0, sizeof(struct server_client));
}

int handle_version(int fd, struct cmd_packet *packet) {
    uint32_t len = strlen(PACKET_VERSION);
    net_send_data(fd, &len, sizeof(uint32_t));
    net_send_data(fd, PACKET_VERSION, len);
    return 0;
}

int unload_handle(int fd, struct cmd_packet *packet) {
    unload_cmd_sent = true;
    sceKernelSleep(5);
    return 0;
}

int cmd_handler(int fd, struct cmd_packet *packet) {
    if (!VALID_CMD(packet->cmd)) {
        return 1;
    }

    printf("cmd_handler %X\n", packet->cmd);

    if (packet->cmd == CMD_UNLOAD)
        return unload_handle(fd, packet);

    if(packet->cmd == CMD_VERSION) {
        return handle_version(fd, packet);
    }

    if(VALID_PROC_CMD(packet->cmd)) {
        return proc_handle(fd, packet);
    } else if(VALID_DEBUG_CMD(packet->cmd)) {
        return debug_handle(fd, packet);
    } else if(VALID_KERN_CMD(packet->cmd)) {
        return kern_handle(fd, packet);
    } else if(VALID_CONSOLE_CMD(packet->cmd)) {
        return console_handle(fd, packet);
    }

    return 0;
}

int check_debug_interrupt() {
    struct debug_interrupt_packet resp;
    struct debug_breakpoint *breakpoint;
    struct ptrace_lwpinfo *lwpinfo;
    uint8_t int3;
    int status;
    int signal;
    int r;

    r = wait4(curdbgctx->pid, &status, WNOHANG, NULL);
    if(!r) {
        return 0;
    }

    signal = WSTOPSIG(status);
    printf("check_debug_interrupt signal %i", signal);

    if(signal == SIGSTOP) {
        printf("passed on a SIGSTOP");
        return 0;
    } else if(signal == SIGKILL) {
        debug_cleanup(curdbgctx);

        // the process will die
        trace(PT_CONTINUE, curdbgctx->pid, (caddr_t)1, SIGKILL);

        printf("sent final SIGKILL");
        return 0;
    }

    // If lwpinfo is on the stack it fails, maybe I should patch ptrace? idk
    lwpinfo = (struct ptrace_lwpinfo *)pfmalloc(sizeof(struct ptrace_lwpinfo));
    if(!lwpinfo) {
        printf("could not allocate memory for thread information");
        return 1;
    }

    // grab interrupt data
    r = trace(PT_LWPINFO, curdbgctx->pid, (caddr_t)lwpinfo, sizeof(struct ptrace_lwpinfo));
    if(r) {
        printf("could not get lwpinfo errno %i", errno);
    }

    // fill response
    memset(&resp, 0, DEBUG_INTERRUPT_PACKET_SIZE);
    resp.lwpid = lwpinfo->pl_lwpid;
    resp.status = status;

    // TODO: fix size mismatch with these fields
    memcpy(resp.tdname, lwpinfo->pl_tdname, sizeof(lwpinfo->pl_tdname));

    r = trace(PT_GETREGS, resp.lwpid, (caddr_t)&resp.reg64, 0);
    if(r) {
        printf("could not get registers errno %i", errno);
    }

    r = trace(PT_GETFPREGS, resp.lwpid, (caddr_t)&resp.savefpu, 0);
    if(r) {
        printf("could not get float registers errno %i", errno);
    }
    
    r = trace(PT_GETDBREGS, resp.lwpid, (caddr_t)&resp.dbreg64, 0);
    if(r) {
        printf("could not get debug registers errno %i", errno);
    }

    // if it is a software breakpoint we need to handle it accordingly
    breakpoint = NULL;
    for(int i = 0; i < MAX_BREAKPOINTS; i++) {
        if(curdbgctx->breakpoints[i].address == resp.reg64.r_rip - 1) {
            breakpoint = &curdbgctx->breakpoints[i];
            break;
        }
    }

    if(breakpoint) {
        printf("dealing with software breakpoint");
        printf("breakpoint: %llX %X", (unsigned long long)breakpoint->address, breakpoint->original);

        // write old instruction
        userland_copyin(curdbgctx->pid, &breakpoint->original, breakpoint->address, 1);

        // backstep 1 instruction
        resp.reg64.r_rip -= 1;
        trace(PT_SETREGS, resp.lwpid, (caddr_t)&resp.reg64, 0);

        // single step over the instruction
        trace(PT_STEP, resp.lwpid, (caddr_t)1, 0);
        while(!wait4(curdbgctx->pid, &status, WNOHANG, 0)) {
            sceKernelUsleep(4000);
        }

        printf("waited for signal %i", WSTOPSIG(status));

        // set breakpoint again
        int3 = 0xCC;
        userland_copyin(curdbgctx->pid, &int3, breakpoint->address, 1);
    } else {
        printf("dealing with hardware breakpoint");
    }

    r = net_send_data(curdbgctx->dbgfd, &resp, DEBUG_INTERRUPT_PACKET_SIZE);
    if(r != DEBUG_INTERRUPT_PACKET_SIZE) {
        printf("net_send_data failed %i %i", r, errno);
    }

    printf("check_debug_interrupt interrupt data sent");

    free(lwpinfo);

    return 0;
}

int handle_client(struct server_client *svc) {
    struct cmd_packet packet;
    uint32_t rsize;
    uint32_t length;
    void *data;
    int fd;
    int r;

    fd = svc->fd;

    // setup time val for select
    struct timeval tv;
    memset(&tv, 0, sizeof(tv));
    tv.tv_usec = 1000;

    while (1) {
        if (unload_cmd_sent)
            break;
        // do a select
        fd_set sfd;
        FD_ZERO(&sfd);
        FD_SET(fd, &sfd);
        errno = 0;
        net_select(FD_SETSIZE, &sfd, NULL, NULL, &tv);

        // check if we can recieve
        if(FD_ISSET(fd, &sfd)) {
            // zero out
            memset(&packet, 0, CMD_PACKET_SIZE);

            // recieve our data
            rsize = net_recv_data(fd, &packet, CMD_PACKET_SIZE, 0);

            // if we didnt recieve hmm
            if (rsize <= 0) {
                goto error;
            }

            // check if disconnected
            if (errno == ECONNRESET) {
                goto error;
            }
        } else {
            // if we have a valid debugger context then check for interrupt
            // this does not block, as wait is called with option WNOHANG
            if(svc->debugging) {
                if(check_debug_interrupt()) {
                    goto error;
                }
            }

            // check if disconnected
            if (errno == ECONNRESET) {
                goto error;
            }

            sceKernelUsleep(25000);
            continue;
        }

        printf("client packet recieved\n");

        // invalid packet
        if (packet.magic != PACKET_MAGIC) {
            printf("invalid packet magic %X!\n", packet.magic);
            continue;
        }

        // mismatch received size
        if (rsize != CMD_PACKET_SIZE) {
            printf("invalid recieve size %i!\n", rsize);
            continue;
        }

        length = packet.datalen;
        if (length) {
            // allocate data
            data = pfmalloc(length);
            if (!data) {
                goto error;
            }

            printf("recieving data length %i\n", length);

            // recv data
            r = net_recv_data(fd, data, length, 1);
            if (!r) {
                goto error;
            }

            // set data
            packet.data = data;
        } else {
            packet.data = NULL;
        }

        // special case when attaching
        // if we are debugging then the handler for CMD_DEBUG_ATTACH will send back the right error
        if(!g_debugging && packet.cmd == CMD_DEBUG_ATTACH) {
            curdbgcli = svc;
            curdbgctx = &svc->dbgctx;
        }

        // handle the packet
        r = cmd_handler(fd, &packet);

        if (data) {
            free(data);
            data = NULL;
        }

        // check cmd handler error
        if (r) {
            goto error;
        }
    }

error:
    printf("client disconnected\n");
    free_client(svc);

    return 0;
}

void configure_socket(int fd) {
    int flag;

    flag = 1;
    sceNetSetsockopt(fd, SOL_SOCKET, 0x1200, (char *)&flag, sizeof(flag));

    flag = 1;
    sceNetSetsockopt(fd, IPPROTO_TCP, 1, (char *)&flag, sizeof(flag));

    flag = 1;
    sceNetSetsockopt(fd, SOL_SOCKET, 0x0800, (char *)&flag, sizeof(flag));
}

void *broadcast_thread(void *arg) {
    struct sockaddr_in server;
    struct sockaddr_in client;
    unsigned int clisize;
    int serv;
    int flag;
    int r;
    uint32_t magic;

    printf("broadcast server started\n");

    // setup server
    server.sin_len = sizeof(server);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = 0;
    server.sin_port = sceNetHtons(BROADCAST_PORT);
    memset(server.sin_zero, 0, sizeof(server.sin_zero));

    serv = sceNetSocket("broadsock", AF_INET, SOCK_DGRAM, 0);
    if(serv < 0) {
        printf("failed to create broadcast server\n");
        return NULL;
    }

    flag = 1;
    sceNetSetsockopt(serv, SOL_SOCKET, SO_BROADCAST, (char *)&flag, sizeof(flag));

    r = sceNetBind(serv, (struct sockaddr *)&server, sizeof(server));
    if(r) {
        printf("failed to bind broadcast server\n");
        return NULL;
    }

    while(!unload_cmd_sent) {
        scePthreadYield();

        magic = 0;
        clisize = sizeof(client);
        r = sceNetRecvfrom(serv, &magic, sizeof(uint32_t), 0, (struct sockaddr *)&client, &clisize);

        if(r >= 0) {
            printf("broadcast server received a message\n");
            if(magic == BROADCAST_MAGIC) {
                sceNetSendto(serv, &magic, sizeof(uint32_t), 0, (struct sockaddr *)&client, clisize);
            }
        } else {
            printf("sceNetRecvfrom failed\n");
        }

        sceKernelSleep(1);
    }

    return NULL;
}

int start_server() {
    struct sockaddr_in server;
    struct sockaddr_in client;
    struct server_client *svc;
    unsigned int len = sizeof(client);
    int serv, fd;
    int r;

    printf("ps4debug " PACKET_VERSION " server started\n");

    ScePthread broadcast;
    scePthreadCreate(&broadcast, NULL, broadcast_thread, NULL, "broadcast");

    // server structure
    server.sin_len = sizeof(server);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = 0;
    server.sin_port = sceNetHtons(SERVER_PORT);
    memset(server.sin_zero, 0, sizeof(server.sin_zero));

    // start up server
    serv = sceNetSocket("debugserver", AF_INET, SOCK_STREAM, 0);
    if(serv < 0) {
        printf("could not create socket!\n");
        return 1;
    }

    configure_socket(serv);

    r = sceNetBind(serv, (struct sockaddr *)&server, sizeof(server));
    if(r) {
        printf("bind failed!\n");
        return 1;
    }

    r = sceNetListen(serv, SERVER_MAXCLIENTS * 2);
    if(r) {
        printf("bind failed!\n");
        return 1;
    }

    // reset clients
    memset(servclients, 0, sizeof(struct server_client) * SERVER_MAXCLIENTS);

    // reset debugging stuff
    g_debugging = 0;
    curdbgcli = 0;
    curdbgctx = 0;

    while (1) {
        if (unload_cmd_sent)
            break;
            
        scePthreadYield();

        errno = 0;
        fd = sceNetAccept(serv, (struct sockaddr *)&client, &len);
        if(fd > -1 && !errno) {
            printf("accepted a new client\n");

            svc = alloc_client();
            if(!svc) {
                printf("server can not accept anymore clients\n");
                continue;
            }

            configure_socket(fd);

            svc->fd = fd;
            svc->debugging = 0;
            memcpy(&svc->client, &client, sizeof(svc->client));
            memset(&svc->dbgctx, 0, sizeof(svc->dbgctx));

            ScePthread thread;
            scePthreadCreate(&thread, NULL, (void * (*)(void *))handle_client, (void *)svc, "clienthandler");
        }

        sceKernelSleep(2);
    }
    sceNetSocketAbort(0, serv);
    sceNetSocketClose(serv);
    printf("server thread has ended!\n");

    return 0;
}