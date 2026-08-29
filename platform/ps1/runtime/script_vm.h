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
#define PS1_MAX_NODES 128
#endif
#ifndef PS1_MAX_HUD
#define PS1_MAX_HUD 32
#endif
#ifndef PS1_MAX_TILES
#define PS1_MAX_TILES 256
#endif
#ifndef PS1_MAX_PACKS
#define PS1_MAX_PACKS 8
#endif
#ifndef PS1_MAX_CLIPS
#define PS1_MAX_CLIPS 8
#endif
#ifndef PS1_MAX_KEYS
#define PS1_MAX_KEYS 32
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

struct ScriptVMHud {
	int16_t x, y, w, h;
	uint8_t kind;
	uint8_t tex;
	uint8_t rgb;
	uint8_t node_id;
	uint8_t flags;
	int16_t value, vmin, vmax;
	char text[32];
	uint8_t nitems;
	char items[8][16];
	uint8_t pressed;
};

struct ScriptVMTile {
	int16_t x, y;
	uint8_t u, v;
	uint8_t tex;
	uint8_t node_id;
};

struct ScriptVMPack {
	char path[64];
	uint16_t node_lo, node_hi;
	uint16_t tri_lo, tri_hi;
	uint16_t hud_lo, hud_hi;
	uint16_t tile_lo, tile_hi;
	uint8_t has_cam;
	int16_t cam_px, cam_py, cam_pz;
	int16_t cam_rx, cam_ry, cam_rz;
};

struct ScriptVMAnimKey {
	uint16_t t_ms;
	uint8_t node_id;
	uint8_t flags;
	int16_t px, py, pz;
	int16_t rx, ry, rz;
};

struct ScriptVMAnimClip {
	char name[32];
	uint8_t nkeys;
	ScriptVMAnimKey keys[PS1_MAX_KEYS];
};

struct ScriptVMHost {
	int16_t *rot_x;
	int16_t *rot_y;
	int16_t *rot_z;
	int32_t *pos_x;
	int32_t *pos_y;
	int32_t *pos_z;
	const uint8_t *pad34;
	int hud_focus_blocks_cam;
	void (*play_vag)(void);
	void (*stop_vag)(void);
	int (*vag_playing)(void);
	void (*play_fmv)(void);
};

void script_vm_init(const uint8_t *gdbc, size_t gdbc_size, const uint8_t *luau, size_t luau_size);
void script_vm_set_nodes(const ScriptVMNode *nodes, int count);
void script_vm_set_hud(const ScriptVMHud *hud, int count);
void script_vm_set_tiles(const ScriptVMTile *tiles, int count);
void script_vm_set_packs(const ScriptVMPack *packs, int count);
void script_vm_set_anims(const ScriptVMAnimClip *clips, int count);
int script_vm_node_count();
const ScriptVMNode *script_vm_nodes();
int script_vm_hud_count();
ScriptVMHud *script_vm_hud();
int script_vm_hud_focus();
int script_vm_tile_count();
const ScriptVMTile *script_vm_tiles();
void script_vm_hud_tick(const ScriptVMHost *host);
int script_vm_process(float delta, const ScriptVMHost *host);
const char *script_vm_last_error();
