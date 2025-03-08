#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/_stdint.h>
#include <sys/types.h>
#include "util.hpp"
#include "../extern/tiny-json/tiny-json.h"
#include "../extern/cJSON/cJSON.h"
#include "hijacker/hijacker.hpp"
#include <sys/stat.h>
#include <stdatomic.h>
#include "game_patch_xml.hpp"

struct AppMessage;

extern "C" uint32_t sceAppMessagingSendMsg(uint32_t appId, uint32_t msgType, const void *msg, size_t msgLength, uint32_t flags);

extern "C" int sceAppMessagingReceiveMsg(const AppMessage *msg);

struct AppMessage
{
	static constexpr size_t PAYLOAD_SIZE = 8192;
	uint32_t sender;
	uint32_t msgType;
	uint8_t payload[PAYLOAD_SIZE];
	uint32_t payloadSize;
	uint64_t timestamp;
};

enum IpcCommands : uint32_t
{
	TRAINER_ACTIVATE_CHEAT = 0x8000000,
	TRAINER_RETURN_VALUE =   0x8999999,
};

#ifdef __cplusplus
#define restrict // Define restrict as empty for C++
#endif

void startMessageReceiver();
int messageThread(void);
void cheat_log(const char *fmt, ...);
bool Get_Running_App_TID(String &title_id);
bool touch_file(const char *destfile);