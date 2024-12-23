#pragma once

#include <ps5/kernel.h>

#include <stdint.h>

#define UCRED_AUTHID_OFFSET 0x58

static inline uint64_t ucred_get_authid(uintptr_t ucred) {
	uint64_t authid = 0;
	kernel_copyout(ucred + UCRED_AUTHID_OFFSET, &authid, sizeof(authid));
	return authid;
}

static inline void ucred_set_authid(uintptr_t ucred, uint64_t authid) {
	kernel_copyin(&authid, ucred + UCRED_AUTHID_OFFSET, sizeof(authid));
}

uintptr_t get_current_ucred(void);