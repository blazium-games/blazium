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

#ifndef PS1_MAX_NODES
#define PS1_MAX_NODES 32
#endif

struct ScriptVMNode {
	int16_t parent;
	char name[32];
	uint8_t type;
	uint8_t flags;
	int16_t px, py, pz;
	int16_t rx, ry, rz;
	uint16_t tri_lo, tri_hi;
	int16_t sprite;
	int16_t script;
};

struct ScriptVMHost {
	int16_t *rot_x;
	int16_t *rot_y;
	int16_t *rot_z;
	int32_t *pos_x;
	int32_t *pos_y;
	int32_t *pos_z;
	const uint8_t *pad34;
};

void script_vm_init(const uint8_t *gdbc, size_t gdbc_size, const uint8_t *luau, size_t luau_size);
void script_vm_set_nodes(const ScriptVMNode *nodes, int count);
int script_vm_node_count();
const ScriptVMNode *script_vm_nodes();
int script_vm_process(float delta, const ScriptVMHost *host);
const char *script_vm_last_error();
