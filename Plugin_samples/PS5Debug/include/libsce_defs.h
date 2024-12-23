#pragma once
#include <sys/socket.h>
#include <stdint.h>
#include "types.h"

int sceNetSocket(const char*, int, int, int);
int sceNetSocketClose(int);
int sceNetConnect(int, struct sockaddr *, int);
int sceNetSend(int, const void *, size_t, int);
int sceNetBind(int, struct sockaddr *, int);
int sceNetListen(int, int);
int sceNetAccept(int, struct sockaddr *, unsigned int *);
int sceNetRecv(int, void *, size_t, int);
int sceNetSocketAbort(int, int);
int sceNetSetsockopt(int s, int level, int optname, const void *optval, socklen_t optlen);
int sceNetRecvfrom(int s, void *buf, unsigned int len, int flags, struct sockaddr *from, unsigned int *fromlen);
int sceNetSendto(int s, void *msg, unsigned int len, int flags, struct sockaddr *to, unsigned int tolen);
uint16_t sceNetHtons(uint16_t host16);
unsigned int sceKernelSleep(unsigned int seconds);
int sceKernelUsleep(unsigned int microseconds);
int scePthreadCreate(ScePthread *thread, const ScePthreadAttr *attr, void *entry, void *arg, const char *name);
void scePthreadYield(void);
int sceKernelSendNotificationRequest(int, notify_request_t*, size_t, int);