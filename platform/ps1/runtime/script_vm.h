/**************************************************************************/
/*  script_vm.h                                                           */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

struct ScriptVMHost {
	int16_t *rot_x;
	int16_t *rot_y;
	int16_t *rot_z;
	int32_t *pos_x;
	int32_t *pos_y;
	int32_t *pos_z;
};

void script_vm_init(const uint8_t *gdbc, size_t gdbc_size, const uint8_t *luau, size_t luau_size);
// Returns 1 if _process ran (caller should skip IR rot_step).
int script_vm_process(float delta, const ScriptVMHost *host);
