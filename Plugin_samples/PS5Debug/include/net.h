#ifndef _NET_H
#define _NET_H

#include <sys/select.h>
#include "errno.h"

#define NET_MAX_LENGTH      8192

#define	SO_USELOOPBACK	0x0040		/* bypass hardware when possible */
#define	SO_LINGER		0x0080		/* linger on close if data present */
#define	SO_NOSIGPIPE	0x0800		/* no SIGPIPE from EPIPE */
#define	SO_SNDBUF		0x1001		/* send buffer size */
#define	SO_RCVBUF		0x1002		/* receive buffer size */
#define	SO_USELOOPBACK	0x0040		/* bypass hardware when possible */
#define	SO_LINGER		0x0080		/* linger on close if data present */
#define	SO_NOSIGPIPE	0x0800		/* no SIGPIPE from EPIPE */
#define	SO_SNDBUF		0x1001		/* send buffer size */
#define	SO_RCVBUF		0x1002		/* receive buffer size */

#define FD_SETSIZE 1024
typedef unsigned long fd_mask;

int net_select(int fd, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);

int net_send_data(int fd, void *data, int length);
int net_recv_data(int fd, void *data, int length, int force);
int net_send_status(int fd, uint32_t status);

#endif