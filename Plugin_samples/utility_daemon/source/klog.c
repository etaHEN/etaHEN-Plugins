#include "log.h"
#include "tcp.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <unistd.h>

#define KLOG_PORT 9081
#define KLOG_BUF_SIZE 256

static int klog_get_available_size(int fd) {
	int res = 0;
	const int err = ioctl(fd, FIONREAD, &res);
	if (err == -1) {
		LOG_PERROR("klog ioctl FIONREAD failed");
		return 0;
	}
	return res;
}

int send_klog(tcp_socket_t *restrict sock) {
	static char klogbuf[KLOG_BUF_SIZE];
	int fd = open("/dev/klog", O_NONBLOCK, 0);
	if (fd == -1) {
		LOG_PERROR("send_klog open /dev/klog failed");
		exit(0); // NOLINT
		kill(getpid(), SIGKILL);
	}
	while (true) {
		struct pollfd readfds[] = {
			{.fd = fd, .events = POLLRDNORM, .revents = 0},
			{.fd = sock->fd, .events = POLLHUP, .revents = 0}
		};
		int res = poll(readfds, sizeof(readfds) / sizeof(struct pollfd), INFTIM);
		if (res == -1 || res == 0) {
			// error occured
			LOG_PERROR("send_klog poll failed");
			close(fd);
			return -1;
		}

		if (readfds[1].revents & POLLHUP) {
			// connection was closed
			close(fd);
			return 0;
		}

		size_t n = klog_get_available_size(fd);
		ssize_t nread = read(fd, klogbuf, (n >= sizeof(klogbuf)) ? sizeof(klogbuf) : n);
		if (nread == -1) {
			// error occured
			LOG_PERROR("send_klog read failed");
			close(fd);
			return -1;
		}
		if (tcp_write(sock, klogbuf, nread)) {
			LOG_PRINTLN("tcp_write failed");
			close(fd);
			return 0;
		}
	}
}

void *klog(void *args) {
	(void) args;
	tcp_socket_t sock;
	if (tcp_init(&sock, 1, KLOG_PORT)) {
		LOG_PRINTLN("tcp_init failed");
		return NULL;
	}

	for (int done = 0; done == 0;) {
		const int err = tcp_accept(&sock);
		if (err) {
			if (err != REST_MODE_ERR) {
				LOG_PRINTLN("tcp_accept failed");
			}
			return NULL;
		}
		done = send_klog(&sock);
		if (tcp_close_connection(&sock)) {
			LOG_PRINTLN("tcp_close_connection failed");
		}
	}
	return NULL;
}
