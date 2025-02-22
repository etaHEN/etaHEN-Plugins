#include <stdint.h>

typedef struct {
  int (*sceKernelSendNotificationRequest)(int handle, void* noti_struct, int arg);
  int (*sceKernelDebugOutText)(int channel, const char *txt);

} HookExtraStuff;


static int __attribute__((used)) sceKernelSendNotificationRequest_Hook(int handle, void* noti_struct, int arg, HookExtraStuff *restrict stuff){

    volatile unsigned long long called_msg[7];
    called_msg[0] = 0x656e72654b656373;
    called_msg[1] = 0x746f4e646e65536c; 
    called_msg[2] = 0x6f69746163696669;
    called_msg[3] = 0x747365757165526e;
    called_msg[4] = 0x6163206b6f6f485f;
    called_msg[5] = 0x6f72662064656c6c;
    called_msg[6] = 0x0000656d6147206d;

    stuff->sceKernelDebugOutText(0, (const char*)called_msg);
    
    return 0;
}
