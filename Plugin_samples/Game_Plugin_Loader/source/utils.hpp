#include <stddef.h>
#include <stdio.h>
#include <sys/_pthreadtypes.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include "dbg.hpp"
#include "dbg/dbg.hpp"
#include "elf/elf.hpp"
#include "hijacker/hijacker.hpp"
#include "notify.hpp"
#include "backtrace.hpp"

#define ORBIS_PAD_PORT_TYPE_STANDARD 0
#define ORBIS_PAD_PORT_TYPE_SPECIAL 2

#define ORBIS_PAD_DEVICE_CLASS_PAD 0
#define ORBIS_PAD_DEVICE_CLASS_GUITAR 1
#define ORBIS_PAD_DEVICE_CLASS_DRUMS 2

#define ORBIS_PAD_CONNECTION_TYPE_STANDARD 0
#define ORBIS_PAD_CONNECTION_TYPE_REMOTE 2

	enum OrbisPadButton
	{
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

	typedef struct vec_float3
	{
		float x;
		float y;
		float z;
	} vec_float3;

	typedef struct vec_float4
	{
		float x;
		float y;
		float z;
		float w;
	} vec_float4;

	typedef struct stick
	{
		uint8_t x;
		uint8_t y;
	} stick;

	typedef struct analog
	{
		uint8_t l2;
		uint8_t r2;
	} analog;

	typedef struct OrbisPadTouch
	{
		uint16_t x, y;
		uint8_t finger;
		uint8_t pad[3];
	} OrbisPadTouch;

	typedef struct OrbisPadTouchData
	{
		uint8_t fingers;
		uint8_t pad1[3];
		uint32_t pad2;
		OrbisPadTouch touch[ORBIS_PAD_MAX_TOUCH_NUM];
	} OrbisPadTouchData;

	// The ScePadData Structure contains data polled from the DS4 controller. This includes button states, analogue
	// positional data, and touchpad related data.
	typedef struct OrbisPadData
	{
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

	// The PadColor structure contains RGBA for the DS4 controller lightbar.
	typedef struct OrbisPadColor
	{
		uint8_t r;
		uint8_t g;
		uint8_t b;
		uint8_t a;
	} OrbisPadColor;

	typedef struct OrbisPadVibeParam
	{
		uint8_t lgMotor;
		uint8_t smMotor;
	} OrbisPadVibeParam;

	// Vendor information about which controller to open for scePadOpenExt
	typedef struct _OrbisPadExtParam
	{
		uint16_t vendorId;
		uint16_t productId;
		uint16_t productId_2; // this is in here twice?
		uint8_t unknown[10];
	} OrbisPadExtParam;

	typedef struct _OrbisPadInformation
	{
		float touchpadDensity;
		uint16_t touchResolutionX;
		uint16_t touchResolutionY;
		uint8_t stickDeadzoneL;
		uint8_t stickDeadzoneR;
		uint8_t connectionType;
		uint8_t count;
		int32_t connected;
		int32_t deviceClass;
		uint8_t unknown[8];
	} OrbisPadInformation;

#if 0
typedef struct {
  int (*scePadReadState)(int32_t handle, OrbisPadData *pData);
  int (*sceKernelDebugOutText)(int channel, const char *txt);
  int (*sceKernelLoadStartModule)(const char *moduleFileName, int args,
                                  const void *argp, int flags, void *opt,
                                  int *pRes);
  int (*sceKernelDlsym)(int handle, const char *symbol, void **addrp);

} GameExtraStuff;
#endif
struct GameStuff {
  uintptr_t scePadReadState;
  uintptr_t debugout;
  uintptr_t sceKernelLoadStartModule;
  uintptr_t sceKernelDlsym;
  uint64_t ASLR_Base = 0;
  char prx_path[256];
  int loaded = 0;

  GameStuff(Hijacker &hijacker) noexcept
      : debugout(hijacker.getLibKernelAddress(nid::sceKernelDebugOutText)), 
        sceKernelLoadStartModule(hijacker.getLibKernelAddress(nid::sceKernelLoadStartModule)),
        sceKernelDlsym(hijacker.getLibKernelAddress(nid::sceKernelDlsym)) {}
};

struct GameBuilder {

  static constexpr size_t SHELLCODE_SIZE = 137;
  static constexpr size_t EXTRA_STUFF_ADDR_OFFSET = 2;

  uint8_t shellcode[SHELLCODE_SIZE];

  void setExtraStuffAddr(uintptr_t addr) noexcept {
    *reinterpret_cast<uintptr_t *>(shellcode + EXTRA_STUFF_ADDR_OFFSET) = addr;
  }
};

static constexpr GameBuilder BUILDER_TEMPLATE {
    0x48, 0xba, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // MOV scePadReadState, RDX

    
    // Additional shellcode0x55, 0x41, 0x57, 0x41, 0x56, 0x41, 0x54, 0x53, 0x48, 0x83, 0xec, 0x60, 0x4c, 0x8b, 0x62, 0x20,0x55, 0x41, 0x57, 0x41, 0x56, 0x41, 0x54, 0x53, 0x48, 0x83, 0xec, 0x30, 0x4c, 0x8b, 0x62, 0x20,
    0x55, 0x41, 0x57, 0x41, 0x56, 0x53, 0x48, 0x83, 0xec, 0x18, 0x48, 0xb8, 0x48, 0x65, 0x6c, 0x6c,
    0x6f, 0x20, 0x66, 0x72, 0x48, 0x89, 0xd3, 0x49, 0x89, 0xf6, 0x41, 0x89, 0xff, 0x48, 0x89, 0x04,
    0x24, 0x48, 0xb8, 0x6f, 0x6d, 0x20, 0x42, 0x4f, 0x36, 0x00, 0x00, 0x48, 0x89, 0x44, 0x24, 0x08,
    0xff, 0x12, 0x89, 0xc5, 0x45, 0x85, 0xff, 0x7e, 0x39, 0x85, 0xed, 0x75, 0x35, 0x41, 0x80, 0x7e,
    0x4c, 0x00, 0x74, 0x2e, 0x83, 0xbb, 0x28, 0x01, 0x00, 0x00, 0x00, 0x75, 0x25, 0x48, 0x8d, 0x7b,
    0x28, 0x31, 0xf6, 0x31, 0xd2, 0x31, 0xc9, 0x45, 0x31, 0xc0, 0x45, 0x31, 0xc9, 0xff, 0x53, 0x10,
    0x48, 0x89, 0xe6, 0x31, 0xff, 0xff, 0x53, 0x08, 0xc7, 0x83, 0x28, 0x01, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x89, 0xe8, 0x48, 0x83, 0xc4, 0x18, 0x5b, 0x41, 0x5e, 0x41, 0x5f, 0x5d, 0xc3
};


extern "C" int sceSystemServiceKillApp(int, int, int, int);
extern "C" int sceSystemServiceGetAppId(const char *);
extern "C" int _sceApplicationGetAppId(int pid, int *appId);
void plugin_log(const char* fmt, ...);
bool Is_Game_Running(int &BigAppid, const char* title_id);
bool HookGame(UniquePtr<Hijacker> &hijacker, uint64_t alsr_b);
