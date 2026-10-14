/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdint.h>
#include <sys/types.h>
uint64_t kernel_get_ucred_authid(pid_t pid);
int32_t kernel_set_ucred_authid(pid_t pid, uint64_t authid);
int32_t kernel_get_ucred_caps(pid_t pid, uint8_t caps[16]);
int32_t kernel_set_ucred_caps(pid_t pid, const uint8_t caps[16]);
int kernel_get_vmem_protection(pid_t pid, intptr_t address, size_t length);
int kernel_mprotect(pid_t pid, intptr_t address, size_t length, int protection);
