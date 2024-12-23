#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include "console.h"
#include "utils.h"

int console_reboot_handle(int fd, struct cmd_packet *packet) {
    return syscall(55, 0);
}

int console_print_handle(int fd, struct cmd_packet *packet) {
    struct cmd_console_print_packet *pp;
    void *data;

    pp = (struct cmd_console_print_packet *)packet->data;

    if(pp) {
        data = pfmalloc(pp->length);
        if(!data) {
            net_send_status(fd, CMD_DATA_NULL);
            return 1;
        }

        memset(data, 0, pp->length);
        
        net_recv_data(fd, data, pp->length, 1);
        
        printf("[ps5debug] %s\n", (char*)data);
        net_send_status(fd, CMD_SUCCESS);

        free(data);

        return 0;
    }

    net_send_status(fd, CMD_DATA_NULL);

    return 1;
}

int console_notify_handle(int fd, struct cmd_packet *packet) {
    struct cmd_console_notify_packet *np;
    void *data;

    np = (struct cmd_console_notify_packet *)packet->data;

    if(np) {
        data = pfmalloc(np->length);
        if(!data) {
            net_send_status(fd, CMD_DATA_NULL);
            return 1;
        }

        memset(data, 0, np->length);
        
        net_recv_data(fd, data, np->length, 1);
        

        printf_notification((char*)data);
        net_send_status(fd, CMD_SUCCESS);
        
        free(data);

        return 0;
    }

    net_send_status(fd, CMD_DATA_NULL);

    return 1;
}

int console_info_handle(int fd, struct cmd_packet *packet) {
    struct cmd_console_info_response resp;

    int mib[2];
	size_t len;

	memset((void *)resp.kern_ostype, 0, sizeof(resp.kern_ostype));
	mib[0] = 1; // CTL_KERN
	mib[1] = 1; // KERN_OSTYPE
	syscall(202, mib, 2, 0, &len, 0, 0);
	syscall(202, mib, 2, (void *)resp.kern_ostype, &len, 0, 0);

	memset((void *)resp.kern_osrelease, 0, sizeof(resp.kern_osrelease));
	mib[0] = 1; // CTL_KERN
	mib[1] = 2; // KERN_OSRELEASE
	syscall(202, mib, 2, 0, &len, 0, 0);
	syscall(202, mib, 2, (void *)resp.kern_osrelease, &len, 0, 0);

	len = sizeof(resp.kern_osrev);
	mib[0] = 1; // CTL_KERN
	mib[1] = 3; // KERN_OSREV
    int kern_osrev = 0;
	syscall(202, mib, 2, &kern_osrev, &len, 0, 0);
    resp.kern_osrev = kern_osrev;

	memset((void *)resp.kern_version, 0, sizeof(resp.kern_version));
	mib[0] = 1; // CTL_KERN
	mib[1] = 4; // KERN_VERSION
	syscall(202, mib, 2, 0, &len, 0, 0);
	syscall(202, mib, 2, (void *)resp.kern_version, &len, 0, 0);

	memset((void *)resp.hw_model, 0, sizeof(resp.hw_model));
	mib[0] = 6; // CTL_HW
	mib[1] = 2; // HW_MODEL
	syscall(202, mib, 2, 0, &len, 0, 0);
	syscall(202, mib, 2, (void *)resp.hw_model, &len, 0, 0);

	len = sizeof(resp.hw_ncpu);
	mib[0] = 6; // CTL_HW
	mib[1] = 3; // HW_NCPU
	int hw_ncpu = 0;
	syscall(202, mib, 2, &hw_ncpu, &len, 0, 0);
    resp.hw_ncpu = hw_ncpu;

	net_send_status(fd, CMD_SUCCESS);
	net_send_data(fd, &resp, CMD_CONSOLE_INFO_RESPONSE_SIZE);

    return 0;
}

int console_handle(int fd, struct cmd_packet *packet) {
    switch(packet->cmd) {
        case CMD_CONSOLE_REBOOT:
            return console_reboot_handle(fd, packet);
        case CMD_CONSOLE_END:
            return 1;
        case CMD_CONSOLE_PRINT:
            return console_print_handle(fd, packet);
        case CMD_CONSOLE_NOTIFY:
            return console_notify_handle(fd, packet);
        case CMD_CONSOLE_INFO:
            return console_info_handle(fd, packet);
    }

    return 0;
}