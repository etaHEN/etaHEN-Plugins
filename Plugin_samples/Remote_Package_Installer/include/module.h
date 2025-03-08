#pragma once

#include "common.h"

#define DLSYM_MANGLED_NAME 0x1

typedef struct _OrbisKernelEventFlagOptParam {
	size_t sz;
} OrbisKernelEventFlagOptParam;

typedef struct OrbisKernelModuleSegmentInfo
{
    void *address;
    uint32_t size;
    int32_t prot;
} OrbisKernelModuleSegmentInfo;

typedef struct OrbisKernelModuleInfo
{
	size_t size;
	char name[256];
	OrbisKernelModuleSegmentInfo segmentInfo[4];
	uint32_t segmentCount;
	uint8_t fingerprint[20];
} OrbisKernelModuleInfo;

int32_t sceKernelLoadStartModuleFromSandbox(const char* name, size_t args, const void* argp, unsigned int flags, const OrbisKernelEventFlagOptParam* opts, int* res);

// int sceKernelGetModuleInfo(int32_t handle, OrbisKernelModuleInfo* info);
// int sceKernelGetModuleInfoEx(int32_t handle, OrbisKernelModuleInfo* info);

int sceKernelGetModuleInfoByName(const char* name, OrbisKernelModuleInfo* info);
int sceKernelGetModuleInfoExByName(const char* name, OrbisKernelModuleInfo* info);

int sceKernelDlsymEx(int32_t handle, const char* symbol, const char* lib, unsigned int flags, void** addrp);

bool get_module_base(const char* name, uint64_t* base, uint64_t* size);

typedef void module_patch_cb_t(void* arg, uint8_t* base, uint64_t size);
bool patch_module(const char* name, module_patch_cb_t* cb, void* arg);
