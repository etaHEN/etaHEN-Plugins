#ifndef _KERN_H
#define _KERN_H

#include "protocol.h"
#include "net.h"

int kern_base_handle(int fd, struct cmd_packet *packet);
int kern_read_handle(int fd, struct cmd_packet *packet);
int kern_write_handle(int fd, struct cmd_packet *packet);

int kern_handle(int fd, struct cmd_packet *packet);

#endif