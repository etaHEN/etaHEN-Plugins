#pragma once

enum AppType : uint32_t
{
	PS4_APP = 1 << 4,
	PS5_APP = 1 << 5,
};

typedef struct GamePatchInfo_t
{
	uint64_t image_base;
	uint64_t image_size;
	int image_pid;
	char titleID[10];
	char titleVersion[16];
	char ImageSelf[64];
	AppType app_mode;
} GamePatchInfo;

int makeDefaultXml_List();
int Xml_parseTitleID_FliprateList(const char* titleId);
int Xml_ParseGamePatch(GamePatchInfo* info);
