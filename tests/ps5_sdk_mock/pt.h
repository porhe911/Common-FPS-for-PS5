/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdint.h>
#include <sys/types.h>
// Signature of the pinned shsrv v0.20 helper used by the production writer.
int pt_copyin(pid_t pid, const void* buffer, intptr_t address, size_t length);
