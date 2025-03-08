#include <orbis/libkernel.h>

#include "module.h"
#include "util.h"

int32_t sceKernelLoadStartModuleFromSandbox(const char* name, size_t args, const void* argp, unsigned int flags, const OrbisKernelEventFlagOptParam* opts, int* res) {
	static const char* sandboxWord = NULL;
	char filePath[1024];
	int32_t handle;

	if (!sandboxWord) {
		sandboxWord = sceKernelGetFsSandboxRandomWord();
		if (!sandboxWord) {
			return 0x8002000E;
		}
	}

	snprintf(filePath, sizeof(filePath), "/%s/common/lib/%s", sandboxWord, name);

	handle = sceKernelLoadStartModule(filePath, args, argp, flags, opts, res);

	return handle;
}


// int sceKernelGetModuleInfo(int32_t handle, OrbisKernelModuleInfo* info) {
// 	int ret;
// 
// 	if (!info) {
// 		ret = 0x8002000E;
// 		goto err;
// 	}
// 
// 	memset(info, 0, sizeof(*info));
// 	{
// 		info->size = sizeof(*info);
// 	}
// 
// 	ret = syscall(SYS_dynlib_get_info, handle, info); /* TODO: make proper error code */
// 
// err:
// 	return ret;
// }


int sceKernelGetModuleInfoByName(const char* name, OrbisKernelModuleInfo* info) {
	OrbisKernelModuleInfo tmpInfo;
	int32_t handles[255];
	size_t numModules;
	size_t i;
	int ret;

	if (!name) {
		ret = 0x8002000E;
		goto err;
	}
	if (!info) {
		ret = 0x8002000E;
		goto err;
	}

	memset(handles, 0, sizeof(handles));

	ret = sceKernelGetModuleList(handles, ARRAY_SIZE(handles), &numModules);
	if (ret) {
		goto err;
	}

	for (i = 0; i < numModules; ++i) {
		ret = sceKernelGetModuleInfo(handles[i], &tmpInfo);
		if (ret) {
			goto err;
		}

		if (strcmp(tmpInfo.name, name) == 0) {
			memcpy(info, &tmpInfo, sizeof(tmpInfo));
			ret = 0;
			goto err;
		}
	}

	ret = 0x80020002;

err:
	return ret;
}

// int sceKernelGetModuleInfoEx(int32_t handle, OrbisKernelModuleInfo* info) {
// 	int ret;
// 
// 	if (!info) {
// 		ret = 0x8002000E;
// 		goto err;
// 	}
// 
// 	memset(info, 0, sizeof(*info));
// 	{
// 		info->size = sizeof(*info);
// 	}
// 
// 	ret = syscall(SYS_dynlib_get_info_ex, handle, info); /* TODO: make proper error code */
// 
// err:
// 	return ret;
// }

// int sceKernelGetModuleInfoExByName(const char* name, OrbisKernelModuleInfo* info) {
// 	OrbisKernelModuleInfo tmpInfo;
// 	int32_t handles[255];
// 	size_t numModules;
// 	size_t i;
// 	int ret;
// 
// 	if (!name) {
// 		ret = 0x8002000E;
// 		goto err;
// 	}
// 	if (!info) {
// 		ret = 0x8002000E;
// 		goto err;
// 	}
// 
// 	memset(handles, 0, sizeof(handles));
// 
// 	ret = sceKernelGetModuleList(handles, ARRAY_SIZE(handles), &numModules);
// 	if (ret) {
// 		goto err;
// 	}
// 
// 	for (i = 0; i < numModules; ++i) {
// 		ret = sceKernelGetModuleInfoEx(handles[i], &tmpInfo);
// 		if (ret) {
// 			goto err;
// 		}
// 
// 		if (strcmp(tmpInfo.name, name) == 0) {
// 			memcpy(info, &tmpInfo, sizeof(tmpInfo));
// 			ret = 0;
// 			goto err;
// 		}
// 	}
// 
// 	ret = 0x80020002;
// 
// err:
// 	return ret;
// }

int sceKernelDlsymEx(int32_t handle, const char* symbol, const char* lib, unsigned int flags, void** addrp) {
	int ret;

	if (!symbol) {
		ret = 0x8002000E;
		goto err;
	}
	if (!lib) {
		ret = 0x8002000E;
		goto err;
	}
	if (!addrp) {
		ret = 0x8002000E;
		goto err;
	}

	ret = syscall(SYS_supercall, SUPERCALL_DLSYM, handle, symbol, lib, flags, addrp); /* TODO: make proper error code */

err:
	return ret;
}

bool get_module_base(const char* name, uint64_t* base, uint64_t* size) 
{
	OrbisKernelModuleInfo moduleInfo;
	int ret;

	// FIX

	ret = sceKernelGetModuleInfoByName(name, &moduleInfo);
	if (ret) {
		EPRINTF("sceKernelGetModuleInfoByName(%s) failed: 0x%08X\n", name, ret);
		goto err;
	}

	if (base)
		*base = (uint64_t)moduleInfo.segmentInfo[0].address;
	if (size)
		*size = moduleInfo.segmentInfo[0].size;

	return true;

err:
	return false;
}

bool patch_module(const char* name, module_patch_cb_t* cb, void* arg) {
	uint64_t base, size;
	int ret;

	if (!get_module_base(name, &base, &size)) {
		goto err;
	}
	printf("%s: base:0x%" PRIX64 " size:0x%" PRIX64 "\n", name, base, size);

	ret = sceKernelMprotect((void*)base, size, ORBIS_KERNEL_PROT_CPU_READ | ORBIS_KERNEL_PROT_CPU_WRITE | ORBIS_KERNEL_PROT_CPU_EXEC);
	if (ret) {
		EPRINTF("sceKernelMprotect(%s) failed: 0x%08X\n", name, ret);
		goto err;
	}

	if (cb) {
		(*cb)(arg, (uint8_t*)base, size);
	}

	return true;

err:
	return false;
}
