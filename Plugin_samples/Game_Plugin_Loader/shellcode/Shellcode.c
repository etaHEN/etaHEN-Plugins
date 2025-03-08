#include <stdint.h>

#define ORBIS_PAD_PORT_TYPE_STANDARD 0
#define ORBIS_PAD_PORT_TYPE_SPECIAL 2

#define ORBIS_PAD_DEVICE_CLASS_PAD 0
#define ORBIS_PAD_DEVICE_CLASS_GUITAR 1
#define ORBIS_PAD_DEVICE_CLASS_DRUMS 2

#define ORBIS_PAD_CONNECTION_TYPE_STANDARD 0
#define ORBIS_PAD_CONNECTION_TYPE_REMOTE 2

enum OrbisPadButton {
  ORBIS_PAD_BUTTON_L3 = 0x0002,
    ORBIS_PAD_BUTTON_R3 = 0x0004,
    ORBIS_PAD_BUTTON_OPTIONS = 0x0008,
    ORBIS_PAD_BUTTON_UP = 0x0010,
    ORBIS_PAD_BUTTON_RIGHT = 0x0020,
    ORBIS_PAD_BUTTON_DOWN = 0x0040,
    ORBIS_PAD_BUTTON_LEFT = 0x0080,

    ORBIS_PAD_BUTTON_L2 = 0x0100,
    ORBIS_PAD_BUTTON_R2 = 0x0200,
    ORBIS_PAD_BUTTON_L1 = 0x0400,
    ORBIS_PAD_BUTTON_R1 = 0x0800,

    ORBIS_PAD_BUTTON_TRIANGLE = 0x1000,
    ORBIS_PAD_BUTTON_CIRCLE = 0x2000,
    ORBIS_PAD_BUTTON_CROSS = 0x4000,
    ORBIS_PAD_BUTTON_SQUARE = 0x8000,

    ORBIS_PAD_BUTTON_TOUCH_PAD = 0x100000
};

#define ORBIS_PAD_MAX_TOUCH_NUM 2
#define ORBIS_PAD_MAX_DATA_NUM 0x40

typedef struct vec_float3 {
  float x;
  float y;
  float z;
}
vec_float3;

typedef struct vec_float4 {
  float x;
  float y;
  float z;
  float w;
}
vec_float4;

typedef struct stick {
  uint8_t x;
  uint8_t y;
}
stick;

typedef struct analog {
  uint8_t l2;
  uint8_t r2;
}
analog;

typedef struct OrbisPadTouch {
  uint16_t x, y;
  uint8_t finger;
  uint8_t pad[3];
}
OrbisPadTouch;

typedef struct OrbisPadTouchData {
  uint8_t fingers;
  uint8_t pad1[3];
  uint32_t pad2;
  OrbisPadTouch touch[ORBIS_PAD_MAX_TOUCH_NUM];
}
OrbisPadTouchData;

// The ScePadData Structure contains data polled from the DS4 controller. This includes button states, analogue
// positional data, and touchpad related data.
typedef struct OrbisPadData {
  uint32_t buttons;
  stick leftStick;
  stick rightStick;
  analog analogButtons;
  uint16_t padding;
  vec_float4 quat;
  vec_float3 vel;
  vec_float3 acell;
  OrbisPadTouchData touch;
  uint8_t connected;
  uint64_t timestamp;
  uint8_t ext[16];
  uint8_t count;
  uint8_t unknown[15];
} OrbisPadData;

typedef struct {
  int (*scePadReadState)(int handle, OrbisPadData *pData);
  int (*sceKernelDebugOutText)(int channel, const char *txt);
  int (*sceKernelLoadStartModule)(const char *moduleFileName, int args, const void *argp, int flags, void *opt, int *pRes);
  int (*sceKernelDlsym)(int handle, const char *symbol, void **addrp);
  uint64_t ASLR_Base;
  char prx_path[256];
  int loaded;

} GameExtraStuff;


static int __attribute__((used)) scePadReadState_Hook(int handle, OrbisPadData *pData, GameExtraStuff *restrict stuff){

    volatile unsigned long long Hello_Game[2];
    Hello_Game[0] = 0x7266206f6c6c6548;
    Hello_Game[1] = 0x0000364f42206d6f;

    int ret = stuff->scePadReadState(handle, pData);
    int is_connected = (ret == 0 && handle > 0 && pData->connected);

    if (is_connected && !stuff->loaded)
    {
        int res = stuff->sceKernelLoadStartModule(stuff->prx_path, 0, 0, 0, 0, 0);
        (void)res;
        stuff->sceKernelDebugOutText(0, (const char*)Hello_Game);
        stuff->loaded = 1;
    }
    
    return ret;
}
