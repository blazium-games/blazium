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
#define PS1_MAX_PACKS 24
#endif
#ifndef PS1_MAX_TRIS
#define PS1_MAX_TRIS 8192
#endif
#ifndef PS1_RAM_BUDGET
#define PS1_RAM_BUDGET 2097152
#endif
#ifndef PS1_MAX_CLIPS
#define PS1_MAX_CLIPS 16
#endif
#ifndef PS1_MAX_KEYS
#define PS1_MAX_KEYS 64
#endif
#ifndef PS1_MAX_ACTIONS
#define PS1_MAX_ACTIONS 32
#endif
#ifndef PS1_MAX_CONNS
#define PS1_MAX_CONNS 16
#endif
#ifndef PS1_MAX_CAMS
#define PS1_MAX_CAMS 8
#endif
#ifndef PS1_MAX_HITS
#define PS1_MAX_HITS 32
#endif
#ifndef PS1_MAX_SPRITES
#define PS1_MAX_SPRITES 32
#endif
#ifndef PS1_MAX_PARTICLES
#define PS1_MAX_PARTICLES 16
#endif
#ifndef PS1_COOK_ABI
#define PS1_COOK_ABI 20
#endif
#ifndef PS1_MAX_STREAMS
#define PS1_MAX_STREAMS 4
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
	char text[64];
	uint8_t nitems;
	char items[8][16];
	uint8_t pressed;
};

struct ScriptVMTile {
	int16_t x, y;
	uint8_t u, v;
	uint8_t tex;
	uint8_t node_id;
	uint8_t flags;
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
	uint16_t node_count, tri_count, hud_count, tile_count, tim_count;
	uint32_t ram_bytes;
	uint8_t resident;
	uint8_t anim_resident;
	uint8_t sprite_resident;
	uint8_t hit_resident;
	uint8_t cam_resident;
	uint8_t audio_resident;
	uint8_t text_resident;
	uint8_t music_resident;
	uint8_t nav_resident;
	uint8_t path_resident;
	uint8_t way_resident;
	uint8_t tim_lo, tim_hi;
};

struct ScriptVMCam {
	int16_t node_id;
	char name[32];
	uint8_t is_default;
	int16_t px, py, pz;
	int16_t rx, ry, rz;
	uint8_t pack;
	uint8_t dim;
	uint8_t drag;
	uint8_t dead;
	int16_t lim_l, lim_t, lim_r, lim_b;
};

struct ScriptVMHit {
	int16_t node_id;
	uint8_t dim;
	uint8_t kind;
	uint8_t flags;
	uint8_t layer;
	int16_t min_x, min_y, min_z;
	int16_t max_x, max_y, max_z;
	uint8_t pack;
};

struct ScriptVMSprite {
	int16_t x, y;
	uint16_t w, h;
	uint8_t u, v;
	uint16_t tex;
	uint8_t frame;
	uint8_t nframes;
	uint8_t fps;
	uint8_t pack;
	uint8_t billboard;
	uint8_t playing;
	uint8_t flip_h;
	uint8_t flip_v;
	uint8_t rgb;
	float accum;
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
	uint8_t loop;
	ScriptVMAnimKey keys[PS1_MAX_KEYS];
};

struct ScriptVMAction {
	char name[16];
	uint16_t mask;
	uint8_t axis;
};

struct ScriptVMHost {
	int16_t *rot_x;
	int16_t *rot_y;
	int16_t *rot_z;
	int32_t *pos_x;
	int32_t *pos_y;
	int32_t *pos_z;
	const uint8_t *pad34;
	const uint8_t *pad34_1;
	int hud_focus_blocks_cam;
	void (*play_vag)(void);
	void (*stop_vag)(void);
	int (*vag_playing)(void);
	void (*play_fmv)(void);
	int region;
	int *script_drives_cam;
	int *cam_scale;
	void (*set_rumble)(int device, int small, int large);
	int (*upload_tpak)(const uint8_t *blob, int size, int *lo, int *hi);
	void (*evict_tpak)(int lo, int hi);
	int (*load_sfx_bank)(const uint8_t *blob, int size);
	void (*unload_sfx_bank)(void);
	int (*play_sfx)(const char *name);
	void (*stop_sfx)(const char *name);
	void (*set_sfx_volume)(const char *name, int vol);
	void (*set_music_volume)(int vol);
	void (*stop_music)(void);
	void (*set_light)(int index, int dx, int dy, int dz, int r, int g, int b);
	int (*load_music)(const uint8_t *blob, int size);
	void (*unload_music)(void);
	void (*play_fmv_blob)(const uint8_t *blob, int size);
	int (*play_xa)(const char *iso_name, int file, int chan);
	void (*stop_xa)(void);
	int (*play_fmv_cd)(const char *iso_name);
};

struct ScriptVMShot {
	int16_t x, y, z;
	int16_t vx, vy, vz;
	uint8_t used;
	uint8_t life;
	uint8_t tex;
	int16_t owner;
	int16_t hit;
};

struct ScriptVMParticle {
	int16_t x, y, z;
	int16_t vx, vy, vz;
	int16_t px, py, pz;
	uint8_t life;
	uint8_t tex;
	uint8_t r, g, b;
	uint8_t size;
	uint8_t mode;
	uint8_t frame;
	uint8_t nframes;
	uint8_t ftick;
	uint8_t fps;
};

int script_vm_script_cam();
void script_vm_get_fog(int *on, int *start, int *end, uint8_t *r, uint8_t *g, uint8_t *b);
void script_vm_get_fade(int *a, uint8_t *r, uint8_t *g, uint8_t *b);
int script_vm_particle_count();
const ScriptVMParticle *script_vm_particles();
int script_vm_shot_count();
const ScriptVMShot *script_vm_shots();
int script_vm_node_culled(int node);
int script_vm_node_ysort(int node);
void script_vm_get_hud_offset(int *x, int *y);
void script_vm_set_hud_offset(int x, int y);
const ScriptVMSprite *script_vm_sprites();
int script_vm_sprite_count();
void script_vm_set_ysort(const uint8_t *ysort, int count);
void script_vm_set_kinds(const uint8_t *kinds, int count);
void script_vm_set_path_ids(const uint8_t *ids, int count);

void script_vm_init(const uint8_t *gdbc, size_t gdbc_size, const uint8_t *luau, size_t luau_size);
void script_vm_set_nodes(const ScriptVMNode *nodes, int count);
void script_vm_set_group_bits(const uint8_t *bits, int count);
void script_vm_set_group_names(const char names[8][16]);
void script_vm_set_scroll(const uint8_t *scroll, int count);
void script_vm_set_fog(int on, int start, int end, uint8_t r, uint8_t g, uint8_t b);
void script_vm_set_hud(const ScriptVMHud *hud, int count);
void script_vm_set_tiles(const ScriptVMTile *tiles, int count);
void script_vm_set_packs(const ScriptVMPack *packs, int count);
void script_vm_set_anims(const ScriptVMAnimClip *clips, int count);
void script_vm_set_actions(const ScriptVMAction *actions, int count);
void script_vm_set_cams(const ScriptVMCam *cams, int count);
void script_vm_set_hits(const ScriptVMHit *hits, int count);
int script_vm_apply_nav_blob(const uint8_t *blob, int size);
int script_vm_apply_path_blob(const uint8_t *blob, int size);
int script_vm_apply_way_blob(const uint8_t *blob, int size);
void script_vm_set_sprites(const ScriptVMSprite *sprites, int count);
void script_vm_set_mesh(const uint8_t *blob, size_t size);
int script_vm_tri_count();
const uint8_t *script_vm_tris();
int script_vm_node_count();
const ScriptVMNode *script_vm_nodes();
int script_vm_hud_count();
ScriptVMHud *script_vm_hud();
int script_vm_hud_focus();
int script_vm_tile_count();
const ScriptVMTile *script_vm_tiles();
void script_vm_hud_tick(const ScriptVMHost *host);
int script_vm_process(float delta, const ScriptVMHost *host);
int script_vm_should_quit();
const char *script_vm_last_error();
