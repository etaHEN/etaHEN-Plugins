// Credits to astrelsky: https://github.com/astrelsky/HEN-V/blob/master/spawner/source/memory.h
#pragma once

#include <stdint.h>
#include <unistd.h>

void userland_copyin(int pid, const void *src, uintptr_t dst, size_t length);

void userland_copyout(int pid, uintptr_t src, void *dst, size_t length);