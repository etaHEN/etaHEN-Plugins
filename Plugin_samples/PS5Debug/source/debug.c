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
#include "utils.h"
#include "mdbg.h"
#include "debug.h"
#include "libsce_defs.h"

int g_debugging;
struct server_client *curdbgcli;
struct debug_context *curdbgctx;

int debug_attach_handle(int fd, struct cmd_packet *packet) {
    struct cmd_debug_attach_packet *ap;
    int r;

    if(g_debugging) {
        net_send_status(fd, CMD_ALREADY_DEBUG);
        return 1;
    }

    ap = (struct cmd_debug_attach_packet *)packet->data;

    if(ap) {
        r = trace(PT_ATTACH, ap->pid, (caddr_t)0, 0);
        if(r) {
            net_send_status(fd, CMD_ERROR);
            return 1;
        }

        r = connect_debugger(curdbgctx, &curdbgcli->client);
        if(r) {
            net_send_status(fd, CMD_ERROR);
            return 1;
        }

        curdbgcli->debugging = 1;
        curdbgctx->pid = ap->pid;

        net_send_status(fd, CMD_SUCCESS);

        return 0;
    }

    net_send_status(fd, CMD_DATA_NULL);

    return 1;
}

int debug_detach_handle(int fd, struct cmd_packet *packet) {
    debug_cleanup(curdbgctx);

    net_send_status(fd, CMD_SUCCESS);

    return 0;
}

int debug_breakpt_handle(int fd, struct cmd_packet *packet) {
    struct cmd_debug_breakpt_packet *bp;
    uint8_t int3;

    bp = (struct cmd_debug_breakpt_packet *)packet->data;

    if (curdbgctx->pid == 0) {
        net_send_status(fd, CMD_ERROR);
        return 1;
    }

    if(!bp) {
        net_send_status(fd, CMD_DATA_NULL);
        return 1;
    }

    if(bp->index >= MAX_BREAKPOINTS) {
        net_send_status(fd, CMD_INVALID_INDEX);
        return 1;
    }

    struct debug_breakpoint *breakpoint = &curdbgctx->breakpoints[bp->index];

    if(bp->enabled) {
        breakpoint->enabled = 1;
        breakpoint->address = bp->address;

        userland_copyout(curdbgctx->pid, breakpoint->address, &breakpoint->original, 1);

        int3 = 0xCC;
        userland_copyin(curdbgctx->pid, &int3, breakpoint->address, 1);
    } else {
        userland_copyin(curdbgctx->pid, &breakpoint->original, breakpoint->address, 1);

        breakpoint->enabled = 0;
        breakpoint->address = 0;
    }

    net_send_status(fd, CMD_SUCCESS);

    return 0;
}

int debug_watchpt_handle(int fd, struct cmd_packet *packet) {
    struct cmd_debug_watchpt_packet *wp;
    struct __dbreg64 *dbreg64;
    uint32_t *lwpids;
    int nlwps;
    int r;
    int size;
    //printf("[ProsperoAPI] (debug_watchpt_handle) Step 1\n");

    wp = (struct cmd_debug_watchpt_packet *)packet->data;

    if (curdbgctx->pid == 0) {
        net_send_status(fd, CMD_ERROR);
        return 1;
    }
    //printf("[ProsperoAPI] (debug_watchpt_handle) Step 2\n");

    if(!wp) {
        net_send_status(fd, CMD_DATA_NULL);
        return 1;
    }
    //printf("[ProsperoAPI] (debug_watchpt_handle) Step 3\n");

    if(wp->index >= MAX_WATCHPOINTS) {
        net_send_status(fd, CMD_INVALID_INDEX);
        return 1;
    }
    //printf("[ProsperoAPI] (debug_watchpt_handle) Step 4\n");

    kill(curdbgctx->pid, SIGSTOP);

    // get the threads
    nlwps = trace(PT_GETNUMLWPS, curdbgctx->pid, NULL, 0);
    int status = 0;
    waitpid(curdbgctx->pid, &status, WNOHANG);
    //printf("[ProsperoAPI] (debug_watchpt_handle) PT_GETNUMLWPS state: %i\n", status);
    if (nlwps == -1 && errno) {
        perror("PT_GETNUMLWPS");
        net_send_status(fd, CMD_ERROR);
        return 1;
    }
    //printf("[ProsperoAPI] (debug_watchpt_handle) Step 5\n");
    size = nlwps * sizeof(uint32_t);
    lwpids = (uint32_t *)pfmalloc(size);
    if(!lwpids) {
        net_send_status(fd, CMD_DATA_NULL);
        return 1;
    }
    //printf("[ProsperoAPI] (debug_watchpt_handle) Step 6\n");

    r = trace(PT_GETLWPLIST, curdbgctx->pid, (void *)lwpids, nlwps);
    if (r == -1 && errno) {
        perror("PT_GETLWPLIST");
        net_send_status(fd, CMD_ERROR);
        goto finish;
    }
    //printf("[ProsperoAPI] (debug_watchpt_handle) Step 7\n");

    dbreg64 = (struct __dbreg64 *)&curdbgctx->watchdata;

    // setup the watchpoint
    dbreg64->dr[7] &= ~DBREG_DR7_MASK(wp->index);
    if(wp->enabled) {
        dbreg64->dr[wp->index] = wp->address;
        dbreg64->dr[7] |= DBREG_DR7_SET(wp->index, wp->length, wp->breaktype, DBREG_DR7_LOCAL_ENABLE | DBREG_DR7_GLOBAL_ENABLE);
    } else {
        dbreg64->dr[wp->index] = 0;
        dbreg64->dr[7] |= DBREG_DR7_SET(wp->index, 0, 0, DBREG_DR7_DISABLE);
    }
    //printf("[ProsperoAPI] (debug_watchpt_handle) Step 8\n");

    ////printf("dr%i: %llX dr7: %llX", wp->index, wp->address, dbreg64->dr[7]);

    // for each current lwpid edit the watchpoint
    for(int i = 0; i < nlwps; i++) {
        r = trace(PT_SETDBREGS, lwpids[i], (caddr_t)dbreg64, 0);
        if (r == -1 && errno) {
            perror("PT_SETDBREGS");
            net_send_status(fd, CMD_ERROR);
            goto finish;
        }
    }
    //printf("[ProsperoAPI] (debug_watchpt_handle) Step 9\n");

    trace(PT_CONTINUE, curdbgctx->pid, (caddr_t)1, 0);
    net_send_status(fd, CMD_SUCCESS);
    //printf("[ProsperoAPI] (debug_watchpt_handle) Step 10 - All done\n");

finish:
    free(lwpids);

    return r;
}

int debug_threads_handle(int fd, struct cmd_packet *packet) {
    void *lwpids;
    int nlwps;
    int r;
    int size;

    if (curdbgctx->pid == 0) {
        net_send_status(fd, CMD_ERROR);
        return 1;
    }

    nlwps = trace(PT_GETNUMLWPS, curdbgctx->pid, NULL, 0);

    if(nlwps == -1) {
        net_send_status(fd, CMD_ERROR);
        return 0;
    }

    // i assume the lwpid_t is 32 bits wide
    size = nlwps * sizeof(uint32_t);
    lwpids = pfmalloc(size);
    if(!lwpids) {
        net_send_status(fd, CMD_DATA_NULL);
        return 1;
    }

    r = trace(PT_GETLWPLIST, curdbgctx->pid, (caddr_t)lwpids, nlwps);

    if(r == -1) {
        net_send_status(fd, CMD_ERROR);
        return 0;
    }

    net_send_status(fd, CMD_SUCCESS);
    net_send_data(fd, &nlwps, sizeof(nlwps));
    net_send_data(fd, lwpids, size);

    free(lwpids);

    return 0;
}

int debug_stopthr_handle(int fd, struct cmd_packet *packet) {
    struct cmd_debug_stopthr_packet *sp;
    int r;

    sp = (struct cmd_debug_stopthr_packet *)packet->data;

    if(curdbgctx->pid == 0) {
        net_send_status(fd, CMD_ERROR);
        return 1;
    }

    if(!sp) {
        net_send_status(fd, CMD_DATA_NULL);
        return 1;
    }

    r = trace(PT_SUSPEND, sp->lwpid, NULL, 0);
    if(r == -1) {
        net_send_status(fd, CMD_ERROR);
        return 0;
    }

    net_send_status(fd, CMD_SUCCESS);

    return 0;
}

int debug_resumethr_handle(int fd, struct cmd_packet *packet) {
    struct cmd_debug_resumethr_packet *rp;
    int r;

    rp = (struct cmd_debug_resumethr_packet *)packet->data;

    if(curdbgctx->pid == 0) {
        net_send_status(fd, CMD_ERROR);
        return 1;
    }

    if(!rp) {
        net_send_status(fd, CMD_DATA_NULL);
        return 1;
    }

    r = trace(PT_RESUME, rp->lwpid, NULL, 0);
    if(r == -1) {
        net_send_status(fd, CMD_ERROR);
        return 0;
    }

    net_send_status(fd, CMD_SUCCESS);

    return 0;
}

int debug_getregs_handle(int fd, struct cmd_packet *packet) {
    struct cmd_debug_getregs_packet *rp;
    struct __reg64 reg64;
    int r;

    rp = (struct cmd_debug_getregs_packet *)packet->data;

    if (curdbgctx->pid == 0) {
        net_send_status(fd, CMD_ERROR);
        return 1;
    }

    if(!rp) {
        net_send_status(fd, CMD_DATA_NULL);
        return 1;
    }

    r = trace(PT_GETREGS, rp->lwpid, (caddr_t)&reg64, 0);
    if (r == -1 && errno) {
        net_send_status(fd, CMD_ERROR);
        return 1;
    }

    net_send_status(fd, CMD_SUCCESS);
    net_send_data(fd, &reg64, sizeof(struct __reg64));

    return 0;
}

int debug_getfpregs_handle(int fd, struct cmd_packet *packet) {
    struct cmd_debug_getregs_packet *rp;
    struct savefpu_ymm savefpu;
    int r;

    rp = (struct cmd_debug_getregs_packet *)packet->data;

    if (curdbgctx->pid == 0) {
        net_send_status(fd, CMD_ERROR);
        return 1;
    }

    if(!rp) {
        net_send_status(fd, CMD_DATA_NULL);
        return 1;
    }

    r = trace(PT_GETFPREGS, rp->lwpid, (caddr_t)&savefpu, 0);
    if (r == -1 && errno) {
        net_send_status(fd, CMD_ERROR);
        return 1;
    }

    net_send_status(fd, CMD_SUCCESS);
    net_send_data(fd, &savefpu, sizeof(struct savefpu_ymm));

    return 0;
}

int debug_getdbregs_handle(int fd, struct cmd_packet *packet) {
    struct cmd_debug_getregs_packet *rp;
    struct __dbreg64 dbreg64;
    int r;

    rp = (struct cmd_debug_getregs_packet *)packet->data;

    if (curdbgctx->pid == 0) {
        net_send_status(fd, CMD_ERROR);
        return 1;
    }

    if(!rp) {
        net_send_status(fd, CMD_DATA_NULL);
        return 1;
    }

    r = trace(PT_GETDBREGS, rp->lwpid, (caddr_t)&dbreg64, 0);
    if (r == -1 && errno) {
        net_send_status(fd, CMD_ERROR);
        return 1;
    }

    net_send_status(fd, CMD_SUCCESS);
    net_send_data(fd, &dbreg64, sizeof(struct __dbreg64));

    return 0;
}

int debug_setregs_handle(int fd, struct cmd_packet *packet) {
    struct cmd_debug_setregs_packet *sp;
    struct __reg64 reg64;
    int r;

    if (curdbgctx->pid == 0) {
        net_send_status(fd, CMD_ERROR);
        return 1;
    }

    sp = (struct cmd_debug_setregs_packet *)packet->data;
    if (!sp) {
        net_send_status(fd, CMD_ERROR);
        return 1;
    }

    net_send_status(fd, CMD_SUCCESS);
    net_recv_data(fd, &reg64, sp->length, 1);

    r = trace(PT_SETREGS, curdbgctx->pid, (caddr_t)&reg64, 0);
    if (r == -1 && errno) {
        net_send_status(fd, CMD_ERROR);
        return 1;
    }

    net_send_status(fd, CMD_SUCCESS);

    return 0;
}

int debug_setfpregs_handle(int fd, struct cmd_packet *packet) {
    struct cmd_debug_setregs_packet *sp;
    struct savefpu_ymm *fpregs;
    int r;

    if (curdbgctx->pid == 0) {
        net_send_status(fd, CMD_ERROR);
        return 1;
    }

    sp = (struct cmd_debug_setregs_packet *)packet->data;
    if (!sp) {
        net_send_status(fd, CMD_ERROR);
        return 1;
    }

    net_send_status(fd, CMD_SUCCESS);
    net_recv_data(fd, &fpregs, sp->length, 1);

    r = trace(PT_SETFPREGS, curdbgctx->pid, (caddr_t)fpregs, 0);
    if (r == -1 && errno) {
        net_send_status(fd, CMD_ERROR);
        return 1;
    }

    net_send_status(fd, CMD_SUCCESS);

    return 0;
}

int debug_setdbregs_handle(int fd, struct cmd_packet *packet) {
    struct cmd_debug_setregs_packet *sp;
    struct __dbreg64 dbreg64;
    int r;

    if (curdbgctx->pid == 0) {
        net_send_status(fd, CMD_ERROR);
        return 1;
    }

    sp = (struct cmd_debug_setregs_packet *)packet->data;

    if (!sp) {
        net_send_status(fd, CMD_ERROR);
        return 1;
    }

    net_send_status(fd, CMD_SUCCESS);
    net_recv_data(fd, &dbreg64, sp->length, 1);

    r = trace(PT_SETDBREGS, curdbgctx->pid, (caddr_t)&dbreg64, 0);
    if (r == -1 && errno) {
        net_send_status(fd, CMD_ERROR);
        return 1;
    }

    net_send_status(fd, CMD_SUCCESS);

    return 0;
}

int debug_stopgo_handle(int fd, struct cmd_packet *packet) {
    struct cmd_debug_stopgo_packet *sp;
    int signal;
    int r;

    if (curdbgctx->pid == 0) {
        net_send_status(fd, CMD_ERROR);
        return 1;
    }

    sp = (struct cmd_debug_stopgo_packet *)packet->data;

    if(!sp) {
        net_send_status(fd, CMD_DATA_NULL);
        return 1;
    }

    signal = 0;

    if(sp->stop == 1) {
        kill(curdbgctx->pid, SIGSTOP);
        net_send_status(fd, CMD_SUCCESS);
        return 0;
    } else if(sp->stop == 2) {
        kill(curdbgctx->pid, SIGKILL);
        net_send_status(fd, CMD_SUCCESS);
        return 0;
    }

    r = trace(PT_CONTINUE, curdbgctx->pid, (caddr_t)1, signal);
    if (r == -1 && errno) {
        perror("PT_CONTINUE");
        net_send_status(fd, CMD_ERROR);
        return 1;
    }

    net_send_status(fd, CMD_SUCCESS);

    return 0;
}

// int debug_thrinfo_handle(int fd, struct cmd_packet *packet) {
//     struct cmd_debug_thrinfo_packet *tp;
//     struct cmd_debug_thrinfo_response resp;
//     struct sys_proc_thrinfo_args args;

//     if (curdbgctx->pid == 0) {
//         net_send_status(fd, CMD_ERROR);
//         return 1;
//     }

//     tp = (struct cmd_debug_thrinfo_packet *)packet->data;

//     if(!tp) {
//         net_send_status(fd, CMD_DATA_NULL);
//         return 1;
//     }

//     args.lwpid = tp->lwpid;
//     sys_proc_cmd(curdbgctx->pid, SYS_PROC_THRINFO, &args);
    
//     resp.lwpid = args.lwpid;
//     resp.priority = args.priority;
//     memcpy(resp.name, args.name, sizeof(resp.name));

//     net_send_status(fd, CMD_SUCCESS);
//     net_send_data(fd, &resp, CMD_DEBUG_THRINFO_RESPONSE_SIZE);

//     return 0;
// }

int debug_singlestep_handle(int fd, struct cmd_packet *packet) {
    int r;

    if (curdbgctx->pid == 0) {
        net_send_status(fd, CMD_ERROR);
        return 1;
    }

    r = trace(PT_STEP, curdbgctx->pid, (caddr_t)1, 0);
    if(r) {
        net_send_status(fd, CMD_ERROR);
        return 1;
    }
    
    net_send_status(fd, CMD_SUCCESS);
    
    return 0;
}

int connect_debugger(struct debug_context *dbgctx, struct sockaddr_in *client) {
    struct sockaddr_in server;
    int r;

    // we are now debugging
    g_debugging = 1;

    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client->sin_addr.s_addr, str, INET_ADDRSTRLEN);

    //printf("Client to connect to is: %s\n", str);

    // connect to server
    inet_pton(AF_INET, str, &server.sin_addr);
    server.sin_family = AF_INET;
    server.sin_port = sceNetHtons(DEBUG_PORT);
    memset(server.sin_zero, 0, sizeof(server.sin_zero));

    dbgctx->dbgfd = sceNetSocket("interrupt", AF_INET, SOCK_STREAM, 0);
    if(dbgctx->dbgfd <= 0) {
        return 1;
    }

    r = sceNetConnect(dbgctx->dbgfd, (struct sockaddr *)&server, sizeof(server));
    if(r) {
        return 1;
    }

    return 0;
}

void debug_cleanup(struct debug_context *dbgctx) {
    struct __dbreg64 dbreg64;
    uint32_t *lwpids;
    int nlwps;
    int r;

    // clean up stuff
    curdbgcli->debugging = 0;

    // delete references
    g_debugging = 0;
    curdbgcli = NULL;
    curdbgctx = NULL;

    // disable all breakpoints
    for(int i = 0; i < MAX_BREAKPOINTS; i++) {
        userland_copyin(dbgctx->pid, &dbgctx->breakpoints[i].original, dbgctx->breakpoints[i].address, 1);
    }

    // reset all debug registers
    nlwps = trace(PT_GETNUMLWPS, dbgctx->pid, NULL, 0);
    lwpids = (uint32_t *)pfmalloc(nlwps * sizeof(uint32_t));
    if(lwpids) {
        memset(&dbreg64, 0, sizeof(struct __dbreg64));

        r = trace(PT_GETLWPLIST, dbgctx->pid, (caddr_t)lwpids, nlwps);
        if(!r) {
            for(int i = 0; i < nlwps; i++) {
                trace(PT_SETDBREGS, lwpids[i], (caddr_t)&dbreg64, 0);
            }
        }

        free(lwpids);
    }

    trace(PT_CONTINUE, dbgctx->pid, (caddr_t)1, 0);
    trace(PT_DETACH, dbgctx->pid, NULL, 0);

    sceNetSocketClose(dbgctx->dbgfd);
}

int debug_handle(int fd, struct cmd_packet *packet) {
    switch(packet->cmd) {
        case CMD_DEBUG_ATTACH:
            return debug_attach_handle(fd, packet);
        case CMD_DEBUG_DETACH:
            return debug_detach_handle(fd, packet);
        case CMD_DEBUG_BREAKPT:
            return debug_breakpt_handle(fd, packet);
        case CMD_DEBUG_WATCHPT:
            return debug_watchpt_handle(fd, packet);
        case CMD_DEBUG_THREADS:
            return debug_threads_handle(fd, packet);
        case CMD_DEBUG_STOPTHR:
            return debug_stopthr_handle(fd, packet);
        case CMD_DEBUG_RESUMETHR:
            return debug_resumethr_handle(fd, packet);
        case CMD_DEBUG_GETREGS:
            return debug_getregs_handle(fd, packet);
        case CMD_DEBUG_SETREGS:
            return debug_setregs_handle(fd, packet);
        case CMD_DEBUG_GETFPREGS:
            return debug_getfpregs_handle(fd, packet);
        case CMD_DEBUG_SETFPREGS:
            return debug_setfpregs_handle(fd, packet);
        case CMD_DEBUG_GETDBGREGS:
            return debug_getdbregs_handle(fd, packet);
        case CMD_DEBUG_SETDBGREGS:
            return debug_setdbregs_handle(fd, packet);
        case CMD_DEBUG_STOPGO:
            return debug_stopgo_handle(fd, packet);
        // case CMD_DEBUG_THRINFO:
        //     return debug_thrinfo_handle(fd, packet);
        case CMD_DEBUG_SINGLESTEP:
            return debug_singlestep_handle(fd, packet);
        default:
            return 1;
    }
}