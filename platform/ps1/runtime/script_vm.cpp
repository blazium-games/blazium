/**************************************************************************/
/*  script_vm.cpp                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/

#include "script_vm.h"

#include <psxpad.h>
#include <psxsn.h>
#include <string.h>

static const uint8_t *g_gdbc = nullptr;
static size_t g_gdbc_size = 0;
static const uint8_t *g_luau = nullptr;
static size_t g_luau_size = 0;
static int g_ready = 0;
static int g_did_ready = 0;
static char g_err[8] = "";
static ScriptVMNode g_nodes[PS1_MAX_NODES];
static int g_nnode = 0;
static ScriptVMHud g_hud[PS1_MAX_HUD];
static int g_nhud = 0;
static int g_focus = -1;
static ScriptVMTile g_tiles[PS1_MAX_TILES];
static int g_ntile = 0;
static uint16_t g_prev_btn[2] = { 0xffff, 0xffff };
static ScriptVMCam g_cams[PS1_MAX_CAMS];
static int g_ncam = 0;
static int g_cam_cur = -1;
static int g_cam_def = 0;
static ScriptVMHit g_hits[PS1_MAX_HITS];
static uint8_t g_hit_used[PS1_MAX_HITS];
static int g_nhit = 0;
static ScriptVMSprite g_sprs[PS1_MAX_SPRITES];
static int g_nspr = 0;
static float g_ticks_ms = 0.0f;
static float g_timer_end[8];
static uint8_t g_timer_used[8];
static uint32_t g_rng = 1;
static ScriptVMPack g_packs[PS1_MAX_PACKS];
static int g_npack = 0;
static ScriptVMAnimClip g_clips[PS1_MAX_CLIPS];
static int g_nclip = 0;
static int g_anim_playing = 0;
static int g_anim_clip = -1;
static float g_anim_ms = 0.0f;
static int g_anim_just_finished = 0;
static uint8_t g_node_used[PS1_MAX_NODES];
static uint8_t g_node_ready[PS1_MAX_NODES];
static uint8_t g_inst_group[PS1_MAX_NODES];
static uint8_t g_next_group = 1;
static uint8_t g_hud_used[PS1_MAX_HUD];
static uint8_t g_tile_used[PS1_MAX_TILES];
static ScriptVMAction g_actions[PS1_MAX_ACTIONS];
static int g_naction = 0;
static uint8_t g_mesh[(4 + PS1_MAX_TRIS * 32)];
static int g_ntri = 0;
static uint32_t g_ram_used = 0;
static uint32_t g_ram_peak = 0;
static int g_ntim_used = 0;
static const int kTreeId = -2;
static const int kPS1Id = -3;
static const int kPackBase = 1000;
static const int kTimerBase = 2000;
static const int kTweenBase = 3000;
enum { SIG_TIMEOUT = 1, SIG_PRESSED = 2, SIG_ANIM = 3 };
struct ScriptVMConn {
	int src;
	uint8_t sig;
	int dest;
	char method[24];
	uint8_t used;
};
static ScriptVMConn g_conn[PS1_MAX_CONNS];
static int g_script_cam = 0;
static int g_cam_attach = -1;
static int16_t g_cam_offx = 0, g_cam_offy = 0, g_cam_offz = 0;
static int g_cam_scale = 0;
static int g_fog_on = 1, g_fog_start = 0, g_fog_end = 2048;
static uint8_t g_fog_r = 32, g_fog_g = 0, g_fog_b = 48;
static int g_fade_a = 0;
static uint8_t g_fade_r = 0, g_fade_g = 0, g_fade_b = 0;
static ScriptVMParticle g_parts[PS1_MAX_PARTICLES];
static int g_npart = 0;
static const ScriptVMHost *g_host = nullptr;
static int g_paused = 0;
static int g_on_floor = 0, g_on_wall = 0, g_on_ceiling = 0;
static float g_shake_amp = 0, g_shake_ms = 0;
static float g_ray_hx = 0, g_ray_hy = 0, g_ray_hz = 0;
static int g_ray_hit = 0;
static int g_ray_node = -1;
static int g_deadzone = 16;
static float g_vel_x = 0, g_vel_y = 0, g_vel_z = 0;
static float g_nvel[PS1_MAX_NODES][3];
static int16_t g_meta[PS1_MAX_NODES];
static float g_ang[PS1_MAX_NODES][3];
static float g_drive_fric[PS1_MAX_NODES];
static uint8_t g_group[PS1_MAX_NODES];
static char g_gname[8][16];
static uint8_t g_scroll[PS1_MAX_NODES];
static int g_floor_node = -1;
static float g_floor_nx = 0, g_floor_ny = 1, g_floor_nz = 0;
static int g_mc_open = 0;
static uint8_t g_parse_groups[PS1_MAX_NODES];
static uint8_t g_parse_scroll[PS1_MAX_NODES];
static uint32_t g_hit_prev[PS1_MAX_HITS];
static int g_cam_drag_live = -1;
static int16_t g_cam_lim_live[4];
static int g_cam_lim_set = 0;
struct PartStream {
	uint8_t used;
	int16_t node;
	uint8_t rate;
	uint8_t acc;
	uint8_t tex;
	uint8_t mode;
	uint8_t life;
	uint8_t nframes;
	uint8_t fps;
	int16_t vx, vy, vz;
	uint8_t spread;
	int16_t ox, oy, oz;
};
static PartStream g_streams[PS1_MAX_STREAMS];
static uint8_t g_part_mode = 0;
static uint8_t g_part_r = 255, g_part_g = 220, g_part_b = 80;
static uint8_t g_part_size = 4;
static uint8_t g_part_nframes = 1;
static uint8_t g_part_fps = 8;
static uint8_t g_part_life = 20;
static float g_grav_x = 0, g_grav_y = 0, g_grav_z = 0;
static float g_anim_speed = 1.0f;
static int g_fmv_playing = 0;
static int g_fmv_pack = -1;
#define PS1_MAX_TXT 32
struct ScriptVMTextLine {
	char name[32];
	char text[64];
};
static ScriptVMTextLine g_txt[PS1_MAX_TXT];
static int g_ntxt = 0;
static uint8_t g_txt_pack[PS1_MAX_TXT];
static uint8_t g_floor_n[PS1_MAX_NODES];
static uint8_t g_wall_n[PS1_MAX_NODES];
static uint8_t g_ceil_n[PS1_MAX_NODES];
static uint8_t g_ysort[PS1_MAX_NODES];
static uint8_t g_kind[PS1_MAX_NODES];
static uint8_t g_path_id[PS1_MAX_NODES];
static uint8_t g_parse_ysort[PS1_MAX_NODES];
static uint8_t g_parse_kind[PS1_MAX_NODES];
static uint8_t g_parse_path[PS1_MAX_NODES];
static uint8_t g_node_cull[PS1_MAX_NODES];
static float g_cull_dist = 0;
static int g_orbit_clip = 1;
static float g_body_gx = 0, g_body_gy = -60, g_body_gz = 0;
static float g_coyote_ms = 0;
static float g_jump_buf_ms = 0;
static float g_coyote_until[PS1_MAX_NODES];
static float g_jump_buf_until[PS1_MAX_NODES];
static uint8_t g_one_way_pass[PS1_MAX_NODES];
static uint8_t g_was_floor[PS1_MAX_NODES];
static uint8_t g_auto_jump[PS1_MAX_NODES];
static uint8_t g_air_jumps = 0;
static uint8_t g_air_left[PS1_MAX_NODES];
static float g_wall_jump_ms = 0;
static float g_wall_until[PS1_MAX_NODES];
static uint8_t g_was_wall[PS1_MAX_NODES];
static float g_invuln_until[PS1_MAX_NODES];
static int16_t g_cp_x[PS1_MAX_NODES], g_cp_y[PS1_MAX_NODES], g_cp_z[PS1_MAX_NODES];
static uint8_t g_cp_set[PS1_MAX_NODES];
static float g_fade_out_left = 0;
static float g_fade_out_dur = 0;
static int16_t g_hud_ox = 0, g_hud_oy = 0;
static ScriptVMShot g_shot[32];
static int g_nshot = 32;
enum { NK_NONE = 0, NK_PATH = 1, NK_RAY = 2, NK_SHAPE = 3, NK_NOTE = 4, NK_ENAB = 5, NK_REMOTE = 6, NK_TIMER = 7, NK_AGENT = 8, NK_ARM = 9, NK_LOOK = 10 };
struct TweenSlot {
	uint8_t used;
	int16_t node;
	uint8_t prop;
	float from[3];
	float to[3];
	float t;
	float dur;
	uint8_t loop;
};
static TweenSlot g_tween[8];
static float g_path_prog[PS1_MAX_NODES];
static float g_path_spd[PS1_MAX_NODES];
static int16_t g_remote_tgt[PS1_MAX_NODES];
static int16_t g_look_tgt[PS1_MAX_NODES];
static float g_agent_tx[PS1_MAX_NODES];
static float g_agent_ty[PS1_MAX_NODES];
static float g_agent_tz[PS1_MAX_NODES];
static float g_agent_spd[PS1_MAX_NODES];
static uint8_t g_cast_on[PS1_MAX_NODES];
static int16_t g_cast_hit[PS1_MAX_NODES];
static uint8_t g_on_screen[PS1_MAX_NODES];
static uint8_t g_timer_node[8];
#define PS1_MAX_NAV_V 32
#define PS1_MAX_NAV_E 48
#define PS1_MAX_PATHS 8
#define PS1_MAX_PPTS 16
#define PS1_MAX_WAYS 32
static int16_t g_nav_x[PS1_MAX_NAV_V], g_nav_y[PS1_MAX_NAV_V], g_nav_z[PS1_MAX_NAV_V];
static int16_t g_nav_node[PS1_MAX_NAV_V];
static uint8_t g_nav_a[PS1_MAX_NAV_E], g_nav_b[PS1_MAX_NAV_E];
static int g_nvert = 0, g_nedge = 0, g_nav_loaded = 0;
static int16_t g_ppx[PS1_MAX_PATHS][PS1_MAX_PPTS];
static int16_t g_ppy[PS1_MAX_PATHS][PS1_MAX_PPTS];
static int16_t g_ppz[PS1_MAX_PATHS][PS1_MAX_PPTS];
static uint8_t g_pnpt[PS1_MAX_PATHS];
static uint8_t g_pclosed[PS1_MAX_PATHS];
static int g_npath = 0, g_path_loaded = 0;
static uint8_t g_way_n[PS1_MAX_WAYS];
static uint8_t g_way_p[PS1_MAX_WAYS];
static int g_nway = 0, g_way_loaded = 0;
static int g_boot_slices = 0;

static int node_ok(int id);
static int pad_pressed_on(const ScriptVMHost *host, const char *action, int just, int device);
static int hud_index_for_node(int node);
static int read_host_file(const char *path, uint8_t *dst, int max);
static int write_host_file(const char *path, const uint8_t *src, int n);
static void copy_str(char *dst, int max, const char *src);

enum {
	kTapeReturn = 0,
	kTapeLoadConstF = 1,
	kTapeLoadArg = 2,
	kTapeMul = 3,
	kTapeAdd = 4,
	kTapeCallSelf = 5,
	kTapeCallGetNode = 6,
	kTapeCallNamed = 7
};

// Must match GDScriptFunction::Opcode numeric values.
enum {
	OP_OPERATOR = 0,
	OP_SET_KEYED = 6,
	OP_SET_KEYED_VALIDATED = 7,
	OP_SET_INDEXED_VALIDATED = 8,
	OP_GET_KEYED = 9,
	OP_GET_KEYED_VALIDATED = 10,
	OP_GET_INDEXED_VALIDATED = 11,
	OP_SET_NAMED = 12,
	OP_GET_NAMED = 14,
	OP_SET_MEMBER = 16,
	OP_GET_MEMBER = 17,
	OP_ASSIGN = 20,
	OP_ASSIGN_NULL = 21,
	OP_ASSIGN_TRUE = 22,
	OP_ASSIGN_FALSE = 23,
	OP_ASSIGN_TYPED_BUILTIN = 24,
	OP_CAST_TO_BUILTIN = 28,
	OP_CONSTRUCT = 31,
	OP_CONSTRUCT_VALIDATED = 32,
	OP_CALL = 36,
	OP_CALL_RETURN = 37,
	OP_CALL_UTILITY = 39,
	OP_CALL_UTILITY_VALIDATED = 40,
	OP_CALL_GDSCRIPT_UTILITY = 41,
	OP_CALL_METHOD_BIND = 44,
	OP_CALL_METHOD_BIND_RET = 45,
	OP_CALL_NATIVE_STATIC = 47,
	OP_CALL_NATIVE_STATIC_VALIDATED_RETURN = 48,
	OP_CALL_NATIVE_STATIC_VALIDATED_NO_RETURN = 49,
	OP_CALL_METHOD_BIND_VALIDATED_RETURN = 50,
	OP_CALL_METHOD_BIND_VALIDATED_NO_RETURN = 51,
	OP_JUMP = 56,
	OP_JUMP_IF = 57,
	OP_JUMP_IF_NOT = 58,
	OP_JUMP_TO_DEF_ARGUMENT = 59,
	OP_JUMP_IF_SHARED = 60,
	OP_RETURN = 61,
	OP_RETURN_TYPED_BUILTIN = 62,
	OP_ITERATE_BEGIN = 66,
	OP_ITERATE_BEGIN_INT = 67,
	OP_ITERATE = 87,
	OP_ITERATE_INT = 88,
	OP_TYPE_ADJUST_BOOL = 110,
	OP_TYPE_ADJUST_INT = 111,
	OP_TYPE_ADJUST_FLOAT = 112,
	OP_TYPE_ADJUST_STRING = 113,
	OP_TYPE_ADJUST_VECTOR2 = 114,
	OP_TYPE_ADJUST_VECTOR3 = 118,
	OP_TYPE_ADJUST_NODE_PATH = 131,
	OP_TYPE_ADJUST_OBJECT = 133,
	OP_ASSERT = 148,
	OP_BREAKPOINT = 149,
	OP_LINE = 150,
	OP_END = 151
};

enum { V_NIL, V_BOOL, V_INT, V_FLOAT, V_STR, V_V2, V_V3, V_OBJ };

struct GVar {
	uint8_t type;
	int32_t i;
	float f;
	float x, y, z;
	char s[32];
};

static GVar g_mc_var;
static uint8_t g_mc_payload[24576];
static int g_mc_len = 0;
static char g_mc_title[32] = "BLAZIUM";

static uint16_t ru16(const uint8_t *p) {
	return uint16_t(p[0] | (p[1] << 8));
}

static int32_t ri32(const uint8_t *p) {
	return int32_t(uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24));
}

static float rf32(const uint8_t *p) {
	union {
		float f;
		uint32_t u;
	} x;
	x.u = uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
	return x.f;
}

static int owner_node_visible(int owner) {
	const int id = (owner == 0xffff) ? 0 : owner;
	if (id < 0 || id >= g_nnode) {
		return 1;
	}
	return (g_nodes[id].flags & 1) != 0;
}

void script_vm_init(const uint8_t *gdbc, size_t gdbc_size, const uint8_t *luau, size_t luau_size) {
	g_gdbc = gdbc;
	g_gdbc_size = gdbc_size;
	g_luau = luau;
	g_luau_size = luau_size;
	g_ready = 1;
	g_did_ready = 0;
	g_err[0] = 0;
	g_prev_btn[0] = 0xffff;
	g_prev_btn[1] = 0xffff;
	g_ticks_ms = 0.0f;
	g_rng = 1;
	g_anim_playing = 0;
	g_anim_clip = -1;
	g_anim_ms = 0.0f;
	for (int i = 0; i < 8; i++) {
		g_timer_used[i] = 0;
		g_timer_end[i] = 0.0f;
	}
	for (int i = 0; i < PS1_MAX_NODES; i++) {
		g_node_used[i] = 0;
		g_node_ready[i] = 0;
		g_inst_group[i] = 0;
	}
	for (int i = 0; i < PS1_MAX_HUD; i++) {
		g_hud_used[i] = 0;
	}
	for (int i = 0; i < PS1_MAX_TILES; i++) {
		g_tile_used[i] = 0;
	}
	for (int i = 0; i < PS1_MAX_CONNS; i++) {
		g_conn[i].used = 0;
	}
	g_naction = 0;
	g_next_group = 1;
	g_anim_speed = 1.0f;
	g_deadzone = 16;
	g_vel_x = g_vel_y = g_vel_z = 0;
	g_cam_drag_live = -1;
	g_cam_lim_set = 0;
	g_part_mode = 0;
	g_part_r = 255;
	g_part_g = 220;
	g_part_b = 80;
	g_part_size = 4;
	g_part_nframes = 1;
	g_part_fps = 8;
	g_part_life = 20;
	g_grav_x = g_grav_y = g_grav_z = 0;
	g_floor_node = -1;
	g_floor_nx = 0;
	g_floor_ny = 1;
	g_floor_nz = 0;
	g_mc_open = 0;
	for (int i = 0; i < 8; i++) {
		g_gname[i][0] = 0;
	}
	for (int i = 0; i < PS1_MAX_NODES; i++) {
		g_nvel[i][0] = g_nvel[i][1] = g_nvel[i][2] = 0;
		g_meta[i] = 0;
		g_ang[i][0] = g_ang[i][1] = g_ang[i][2] = 0;
		g_drive_fric[i] = 0;
		g_group[i] = 0;
		g_scroll[i] = 0;
		g_parse_groups[i] = 0;
		g_parse_scroll[i] = 0;
		g_parse_ysort[i] = 0;
		g_parse_kind[i] = 0;
		g_parse_path[i] = 0;
		g_floor_n[i] = 0;
		g_wall_n[i] = 0;
		g_ceil_n[i] = 0;
		g_coyote_until[i] = 0;
		g_jump_buf_until[i] = 0;
		g_one_way_pass[i] = 0;
		g_was_floor[i] = 0;
		g_auto_jump[i] = 0;
		g_air_left[i] = 0;
		g_wall_until[i] = 0;
		g_was_wall[i] = 0;
		g_invuln_until[i] = 0;
		g_cp_x[i] = g_cp_y[i] = g_cp_z[i] = 0;
		g_cp_set[i] = 0;
		g_ysort[i] = 0;
		g_kind[i] = 0;
		g_path_id[i] = 0;
		g_node_cull[i] = 0;
		g_path_prog[i] = 0;
		g_path_spd[i] = 0;
		g_remote_tgt[i] = -1;
		g_look_tgt[i] = -1;
		g_agent_tx[i] = 0;
		g_agent_ty[i] = 0;
		g_agent_tz[i] = 0;
		g_agent_spd[i] = 4;
		g_cast_on[i] = 1;
		g_cast_hit[i] = -1;
		g_on_screen[i] = 1;
	}
	g_cull_dist = 0;
	g_orbit_clip = 1;
	g_body_gx = 0;
	g_body_gy = -60;
	g_body_gz = 0;
	g_coyote_ms = 0;
	g_jump_buf_ms = 0;
	g_air_jumps = 0;
	g_wall_jump_ms = 0;
	g_fade_out_left = 0;
	g_fade_out_dur = 0;
	g_hud_ox = 0;
	g_hud_oy = 0;
	g_nvert = g_nedge = g_nav_loaded = 0;
	g_npath = g_path_loaded = 0;
	g_nway = g_way_loaded = 0;
	g_boot_slices = 0;
	for (int i = 0; i < 8; i++) {
		g_tween[i].used = 0;
		g_timer_node[i] = 0xff;
	}
	for (int i = 0; i < 32; i++) {
		g_shot[i].used = 0;
		g_shot[i].hit = -1;
	}
	for (int i = 0; i < PS1_MAX_HITS; i++) {
		g_hit_prev[i] = 0;
	}
	for (int i = 0; i < PS1_MAX_STREAMS; i++) {
		g_streams[i].used = 0;
	}
	for (int i = 0; i < PS1_MAX_PARTICLES; i++) {
		g_parts[i].life = 0;
	}
	g_npart = PS1_MAX_PARTICLES;
	g_fmv_playing = 0;
	g_fmv_pack = -1;
	g_mc_var.type = V_NIL;
	g_mc_var.s[0] = 0;
	{
		uint8_t hdr[64];
		const int n = read_host_file("SAVE.MCD", hdr, int(sizeof(hdr)));
		if (n >= 8 && hdr[0] == 'M' && hdr[1] == 'C' && ((hdr[2] == '1' && hdr[3] == '9') || (hdr[2] == '2' && hdr[3] == '0'))) {
			g_mc_var.type = hdr[4];
			g_mc_len = int(hdr[5] | (hdr[6] << 8));
			if (g_mc_len > int(sizeof(g_mc_payload))) {
				g_mc_len = int(sizeof(g_mc_payload));
			}
			const int got = read_host_file("SAVE.BIN", g_mc_payload, g_mc_len);
			if (got > 0) {
				g_mc_len = got;
			}
			if (g_mc_var.type == V_STR) {
				copy_str(g_mc_var.s, 32, (const char *)g_mc_payload);
			} else if (g_mc_var.type == V_INT) {
				g_mc_var.i = int(g_mc_payload[0] | (g_mc_payload[1] << 8) | (g_mc_payload[2] << 16) | (g_mc_payload[3] << 24));
			} else if (g_mc_var.type == V_FLOAT) {
				union {
					float f;
					uint32_t u;
				} x;
				x.u = uint32_t(g_mc_payload[0] | (g_mc_payload[1] << 8) | (g_mc_payload[2] << 16) | (g_mc_payload[3] << 24));
				g_mc_var.f = x.f;
			} else if (g_mc_var.type == V_V2 || g_mc_var.type == V_V3) {
				union {
					float f;
					uint32_t u;
				} x[3];
				for (int i = 0; i < 3; i++) {
					x[i].u = uint32_t(g_mc_payload[i * 4] | (g_mc_payload[i * 4 + 1] << 8) | (g_mc_payload[i * 4 + 2] << 16) | (g_mc_payload[i * 4 + 3] << 24));
				}
				g_mc_var.x = x[0].f;
				g_mc_var.y = x[1].f;
				g_mc_var.z = x[2].f;
			}
		}
	}
}

void script_vm_set_nodes(const ScriptVMNode *nodes, int count) {
	g_nnode = 0;
	if (!nodes || count <= 0) {
		return;
	}
	g_nnode = count > PS1_MAX_NODES ? PS1_MAX_NODES : count;
	for (int i = 0; i < PS1_MAX_NODES; i++) {
		g_node_used[i] = 0;
		g_node_ready[i] = 0;
		g_inst_group[i] = 0;
	}
	for (int i = 0; i < g_nnode; i++) {
		g_nodes[i] = nodes[i];
		g_node_used[i] = 1;
		g_group[i] = g_parse_groups[i];
		g_scroll[i] = g_parse_scroll[i];
		g_ysort[i] = g_parse_ysort[i];
		g_kind[i] = g_parse_kind[i];
		g_path_id[i] = g_parse_path[i];
		if (g_kind[i] == NK_REMOTE || g_kind[i] == NK_LOOK) {
			g_remote_tgt[i] = int16_t(g_path_id[i]);
			g_look_tgt[i] = int16_t(g_path_id[i]);
		}
		if (g_kind[i] == NK_PATH) {
			g_path_spd[i] = 0.25f;
		}
		if (g_kind[i] == NK_TIMER) {
			const int slot = int(g_path_id[i]);
			if (slot >= 0 && slot < 8) {
				g_timer_node[slot] = uint8_t(i);
			}
		}
	}
}

void script_vm_set_group_bits(const uint8_t *bits, int count) {
	for (int i = 0; i < PS1_MAX_NODES; i++) {
		g_group[i] = (bits && i < count) ? bits[i] : 0;
	}
}

void script_vm_set_group_names(const char names[8][16]) {
	for (int i = 0; i < 8; i++) {
		g_gname[i][0] = 0;
		if (!names) {
			continue;
		}
		int k = 0;
		while (k < 15 && names[i][k]) {
			g_gname[i][k] = names[i][k];
			k++;
		}
		g_gname[i][k] = 0;
	}
}

void script_vm_set_scroll(const uint8_t *scroll, int count) {
	for (int i = 0; i < PS1_MAX_NODES; i++) {
		g_scroll[i] = (scroll && i < count) ? scroll[i] : 0;
	}
}

void script_vm_set_fog(int on, int start, int end, uint8_t r, uint8_t g, uint8_t b) {
	g_fog_on = on ? 1 : 0;
	g_fog_start = start;
	g_fog_end = end;
	g_fog_r = r;
	g_fog_g = g;
	g_fog_b = b;
}

void script_vm_set_ysort(const uint8_t *ysort, int count) {
	for (int i = 0; i < PS1_MAX_NODES; i++) {
		g_ysort[i] = (ysort && i < count) ? ysort[i] : 0;
	}
}

void script_vm_set_kinds(const uint8_t *kinds, int count) {
	for (int i = 0; i < PS1_MAX_NODES; i++) {
		g_kind[i] = (kinds && i < count) ? kinds[i] : 0;
	}
}

void script_vm_set_path_ids(const uint8_t *ids, int count) {
	for (int i = 0; i < PS1_MAX_NODES; i++) {
		g_path_id[i] = (ids && i < count) ? ids[i] : 0;
	}
}

int script_vm_apply_nav_blob(const uint8_t *blob, int size) {
	if (!blob || size < 8 || blob[0] != 'N' || blob[1] != 'A' || blob[2] != 'V' || blob[3] != '0' || ru16(blob + 4) != PS1_COOK_ABI) {
		return 0;
	}
	g_nvert = int(blob[6]);
	g_nedge = int(blob[7]);
	if (g_nvert > PS1_MAX_NAV_V) {
		g_nvert = PS1_MAX_NAV_V;
	}
	if (g_nedge > PS1_MAX_NAV_E) {
		g_nedge = PS1_MAX_NAV_E;
	}
	const uint8_t *p = blob + 8;
	for (int i = 0; i < g_nvert && p + 8 <= blob + size; i++) {
		g_nav_x[i] = int16_t(p[0] | (p[1] << 8));
		g_nav_y[i] = int16_t(p[2] | (p[3] << 8));
		g_nav_z[i] = int16_t(p[4] | (p[5] << 8));
		g_nav_node[i] = int16_t(p[6] | (p[7] << 8));
		p += 8;
	}
	for (int i = 0; i < g_nedge && p + 2 <= blob + size; i++) {
		g_nav_a[i] = p[0];
		g_nav_b[i] = p[1];
		p += 2;
	}
	g_nav_loaded = 1;
	if (g_npack > 0) {
		g_packs[0].nav_resident = 1;
	}
	return 1;
}

int script_vm_apply_path_blob(const uint8_t *blob, int size) {
	if (!blob || size < 8 || blob[0] != 'P' || blob[1] != 'A' || blob[2] != 'T' || blob[3] != 'H' || ru16(blob + 4) != PS1_COOK_ABI) {
		return 0;
	}
	g_npath = int(blob[6]);
	if (g_npath > PS1_MAX_PATHS) {
		g_npath = PS1_MAX_PATHS;
	}
	const uint8_t *p = blob + 8;
	for (int i = 0; i < g_npath && p + 4 <= blob + size; i++) {
		g_pnpt[i] = p[1];
		g_pclosed[i] = p[2];
		if (g_pnpt[i] > PS1_MAX_PPTS) {
			g_pnpt[i] = PS1_MAX_PPTS;
		}
		p += 4;
		for (int k = 0; k < PS1_MAX_PPTS && p + 6 <= blob + size; k++) {
			g_ppx[i][k] = int16_t(p[0] | (p[1] << 8));
			g_ppy[i][k] = int16_t(p[2] | (p[3] << 8));
			g_ppz[i][k] = int16_t(p[4] | (p[5] << 8));
			p += 6;
		}
	}
	g_path_loaded = 1;
	if (g_npack > 0) {
		g_packs[0].path_resident = 1;
	}
	return 1;
}

int script_vm_apply_way_blob(const uint8_t *blob, int size) {
	if (!blob || size < 8 || blob[0] != 'W' || blob[1] != 'A' || blob[2] != 'Y' || blob[3] != '0' || ru16(blob + 4) != PS1_COOK_ABI) {
		return 0;
	}
	g_nway = int(blob[6]);
	if (g_nway > PS1_MAX_WAYS) {
		g_nway = PS1_MAX_WAYS;
	}
	const uint8_t *p = blob + 8;
	for (int i = 0; i < g_nway && p + 4 <= blob + size; i++) {
		g_way_n[i] = p[0];
		g_way_p[i] = p[1];
		p += 4;
	}
	g_way_loaded = 1;
	if (g_npack > 0) {
		g_packs[0].way_resident = 1;
	}
	return 1;
}

int script_vm_shot_count() {
	return g_nshot;
}

const ScriptVMShot *script_vm_shots() {
	return g_shot;
}

int script_vm_node_culled(int node) {
	return node_ok(node) ? int(g_node_cull[node]) : 0;
}

int script_vm_node_ysort(int node) {
	return node_ok(node) ? int(g_ysort[node]) : 0;
}

void script_vm_set_hud_offset(int x, int y) {
	g_hud_ox = int16_t(x);
	g_hud_oy = int16_t(y);
}

void script_vm_get_hud_offset(int *x, int *y) {
	if (x) {
		*x = int(g_hud_ox);
	}
	if (y) {
		*y = int(g_hud_oy);
	}
}

void script_vm_set_hud(const ScriptVMHud *hud, int count) {
	g_nhud = 0;
	g_focus = -1;
	if (!hud || count <= 0) {
		return;
	}
	g_nhud = count > PS1_MAX_HUD ? PS1_MAX_HUD : count;
	for (int i = 0; i < PS1_MAX_HUD; i++) {
		g_hud_used[i] = 0;
	}
	for (int i = 0; i < g_nhud; i++) {
		g_hud[i] = hud[i];
		g_hud_used[i] = 1;
	}
}

void script_vm_set_tiles(const ScriptVMTile *tiles, int count) {
	g_ntile = 0;
	if (!tiles || count <= 0) {
		return;
	}
	g_ntile = count > PS1_MAX_TILES ? PS1_MAX_TILES : count;
	for (int i = 0; i < PS1_MAX_TILES; i++) {
		g_tile_used[i] = 0;
	}
	for (int i = 0; i < g_ntile; i++) {
		g_tiles[i] = tiles[i];
		g_tile_used[i] = 1;
	}
}

void script_vm_set_packs(const ScriptVMPack *packs, int count) {
	g_npack = 0;
	g_ram_used = 0;
	g_ram_peak = 0;
	g_ntim_used = 0;
	if (!packs || count <= 0) {
		return;
	}
	g_npack = count > PS1_MAX_PACKS ? PS1_MAX_PACKS : count;
	for (int i = 0; i < g_npack; i++) {
		g_packs[i] = packs[i];
		g_packs[i].resident = 0;
	}
	if (g_npack > 0) {
		g_packs[0].resident = 1;
		if (g_packs[0].node_count == 0) {
			g_packs[0].node_count = uint16_t(g_packs[0].node_hi > g_packs[0].node_lo ? g_packs[0].node_hi - g_packs[0].node_lo : 0);
		}
		if (g_packs[0].tri_count == 0) {
			g_packs[0].tri_count = uint16_t(g_packs[0].tri_hi > g_packs[0].tri_lo ? g_packs[0].tri_hi - g_packs[0].tri_lo : 0);
		}
		if (g_packs[0].hud_count == 0) {
			g_packs[0].hud_count = uint16_t(g_packs[0].hud_hi > g_packs[0].hud_lo ? g_packs[0].hud_hi - g_packs[0].hud_lo : 0);
		}
		if (g_packs[0].tile_count == 0) {
			g_packs[0].tile_count = uint16_t(g_packs[0].tile_hi > g_packs[0].tile_lo ? g_packs[0].tile_hi - g_packs[0].tile_lo : 0);
		}
		g_ram_used = g_packs[0].ram_bytes;
		g_ram_peak = g_ram_used;
		g_ntim_used = int(g_packs[0].tim_count);
	}
}

void script_vm_set_mesh(const uint8_t *blob, size_t size) {
	g_ntri = 0;
	if (!blob || size < 4) {
		return;
	}
	const int n = int(blob[0] | (blob[1] << 8));
	const int take = n > PS1_MAX_TRIS ? PS1_MAX_TRIS : n;
	const size_t need = 4 + size_t(take) * 32;
	if (need > size) {
		return;
	}
	for (size_t i = 0; i < need && i < sizeof(g_mesh); i++) {
		g_mesh[i] = blob[i];
	}
	g_mesh[0] = uint8_t(take & 0xff);
	g_mesh[1] = uint8_t(take >> 8);
	g_ntri = take;
}

int script_vm_tri_count() {
	return g_ntri;
}

const uint8_t *script_vm_tris() {
	return g_ntri > 0 ? g_mesh : nullptr;
}

void script_vm_set_anims(const ScriptVMAnimClip *clips, int count) {
	g_nclip = 0;
	if (!clips || count <= 0) {
		return;
	}
	g_nclip = count > PS1_MAX_CLIPS ? PS1_MAX_CLIPS : count;
	for (int i = 0; i < g_nclip; i++) {
		g_clips[i] = clips[i];
	}
}

void script_vm_set_cams(const ScriptVMCam *cams, int count) {
	g_ncam = 0;
	g_cam_cur = -1;
	g_cam_def = 0;
	if (!cams || count <= 0) {
		return;
	}
	g_ncam = count > PS1_MAX_CAMS ? PS1_MAX_CAMS : count;
	for (int i = 0; i < g_ncam; i++) {
		g_cams[i] = cams[i];
		if (g_cams[i].is_default) {
			g_cam_def = i;
		}
	}
	g_cam_cur = g_cam_def;
}

void script_vm_set_hits(const ScriptVMHit *hits, int count) {
	g_nhit = 0;
	for (int i = 0; i < PS1_MAX_HITS; i++) {
		g_hit_used[i] = 0;
	}
	if (!hits || count <= 0) {
		return;
	}
	g_nhit = count > PS1_MAX_HITS ? PS1_MAX_HITS : count;
	for (int i = 0; i < g_nhit; i++) {
		g_hits[i] = hits[i];
		if (!g_hits[i].layer) {
			g_hits[i].layer = 1;
		}
		g_hit_used[i] = 1;
	}
}

void script_vm_set_sprites(const ScriptVMSprite *sprites, int count) {
	g_nspr = 0;
	if (!sprites || count <= 0) {
		return;
	}
	g_nspr = count > PS1_MAX_SPRITES ? PS1_MAX_SPRITES : count;
	for (int i = 0; i < g_nspr; i++) {
		g_sprs[i] = sprites[i];
		if (!g_sprs[i].rgb) {
			g_sprs[i].rgb = 255;
		}
	}
}

void script_vm_set_actions(const ScriptVMAction *actions, int count) {
	g_naction = 0;
	if (!actions || count <= 0) {
		return;
	}
	g_naction = count > PS1_MAX_ACTIONS ? PS1_MAX_ACTIONS : count;
	for (int i = 0; i < g_naction; i++) {
		g_actions[i] = actions[i];
	}
}

int script_vm_node_count() {
	return g_nnode;
}

const ScriptVMNode *script_vm_nodes() {
	return g_nodes;
}

int script_vm_hud_count() {
	return g_nhud;
}

ScriptVMHud *script_vm_hud() {
	return g_hud;
}

int script_vm_hud_focus() {
	return g_focus;
}

int script_vm_tile_count() {
	return g_ntile;
}

const ScriptVMTile *script_vm_tiles() {
	return g_tiles;
}

const char *script_vm_last_error() {
	return g_err;
}

static void set_vm_err() {
	g_err[0] = 'V';
	g_err[1] = 'M';
	g_err[2] = ' ';
	g_err[3] = 'E';
	g_err[4] = 'R';
	g_err[5] = 'R';
	g_err[6] = 0;
}

static GVar gv_nil() {
	GVar v{};
	v.type = V_NIL;
	return v;
}

static GVar gv_float(float f) {
	GVar v = gv_nil();
	v.type = V_FLOAT;
	v.f = f;
	return v;
}

static GVar gv_int(int32_t i) {
	GVar v = gv_nil();
	v.type = V_INT;
	v.i = i;
	return v;
}

static GVar gv_bool(int b) {
	GVar v = gv_nil();
	v.type = V_BOOL;
	v.i = b ? 1 : 0;
	return v;
}

static GVar gv_obj(int id) {
	GVar v = gv_nil();
	v.type = V_OBJ;
	v.i = id;
	return v;
}

static GVar gv_v2(float x, float y) {
	GVar v = gv_nil();
	v.type = V_V2;
	v.x = x;
	v.y = y;
	return v;
}

static GVar gv_v3(float x, float y, float z) {
	GVar v = gv_nil();
	v.type = V_V3;
	v.x = x;
	v.y = y;
	v.z = z;
	return v;
}

static float as_float(const GVar &v) {
	if (v.type == V_FLOAT) {
		return v.f;
	}
	if (v.type == V_INT || v.type == V_BOOL) {
		return float(v.i);
	}
	return 0.0f;
}

static int as_truth(const GVar &v) {
	if (v.type == V_NIL) {
		return 0;
	}
	if (v.type == V_BOOL || v.type == V_INT || v.type == V_OBJ) {
		return v.i != 0;
	}
	if (v.type == V_FLOAT) {
		return v.f != 0.0f;
	}
	return 1;
}

static void copy_str(char *dst, int cap, const char *src);

static int name_is(const char *n, const char *w) {
	int i = 0;
	for (; w[i]; i++) {
		if (n[i] != w[i]) {
			return 0;
		}
	}
	return n[i] == 0;
}

static int name_eq_n(const char *a, const char *b, int n) {
	for (int i = 0; i < n; i++) {
		if (a[i] != b[i]) {
			return 0;
		}
	}
	return a[n] == 0;
}

static int node_ok(int id) {
	return id >= 0 && id < g_nnode && g_node_used[id];
}

static int kit_nid(int node, const GVar *argv, int argc) {
	if (argv && argc > 0) {
		if (argv[0].type == V_OBJ) {
			return argv[0].i;
		}
		if (argv[0].type != V_STR) {
			return int(as_float(argv[0]));
		}
	}
	return node;
}

static int kit_invuln(int nid) {
	return node_ok(nid) && g_invuln_until[nid] > g_ticks_ms;
}

static int kit_can_jump(int nid) {
	if (!node_ok(nid)) {
		return 0;
	}
	if (g_auto_jump[nid]) {
		return 1;
	}
	if (g_floor_n[nid]) {
		return 1;
	}
	if (g_coyote_until[nid] > g_ticks_ms) {
		return 1;
	}
	if (g_wall_n[nid] || g_wall_until[nid] > g_ticks_ms) {
		return 1;
	}
	return g_air_left[nid] > 0;
}

static int find_child(int parent, const char *name, int nlen) {
	for (int i = 0; i < g_nnode; i++) {
		if (g_nodes[i].parent != parent) {
			continue;
		}
		if (name_eq_n(g_nodes[i].name, name, nlen)) {
			return i;
		}
	}
	return -1;
}

static int walk_path(int from, const char *path) {
	int cur = from;
	int i = 0;
	if (path[0] == '/' || path[0] == '.') {
		cur = 0;
		if (path[0] == '/' || (path[0] == '.' && path[1] == '/')) {
			i = path[0] == '.' ? 2 : 1;
		} else if (path[0] == '.' && path[1] == 0) {
			return from;
		}
	}
	while (path[i]) {
		if (path[i] == '/') {
			i++;
			continue;
		}
		int n = 0;
		char buf[32];
		while (path[i] && path[i] != '/' && n < 31) {
			buf[n++] = path[i++];
		}
		buf[n] = 0;
		if (n == 1 && buf[0] == '.') {
			continue;
		}
		if (n == 2 && buf[0] == '.' && buf[1] == '.') {
			if (node_ok(cur)) {
				cur = g_nodes[cur].parent;
			}
			continue;
		}
		const int next = find_child(cur, buf, n);
		if (next < 0) {
			return -1;
		}
		cur = next;
	}
	return cur;
}

static float util_atan2(float y, float x);
static float util_sin(float x);
static int rad_to_ps1(float arg) {
	int step = int(arg * 4096.0f / (2.0f * 3.14159265f));
	if (step == 0 && arg != 0.0f) {
		step = arg > 0.0f ? 1 : -1;
	}
	return step;
}

static void write_host_rot(const ScriptVMHost *host, const char *name, float arg) {
	const int step = rad_to_ps1(arg);
	if (name_is(name, "rotate_x") && host && host->rot_x) {
		*host->rot_x += int16_t(step);
	} else if (name_is(name, "rotate_z") && host && host->rot_z) {
		*host->rot_z += int16_t(step);
	} else if (name_is(name, "translate")) {
		if (host && host->pos_z) {
			*host->pos_z += int32_t(arg);
		}
	} else if (host && host->rot_y) {
		*host->rot_y += int16_t(step);
	}
}

static int pad_pressed(const ScriptVMHost *host, const char *action, int just);

static void apply_method(const ScriptVMHost *host, int node, const char *name, float arg, const GVar *argv, int argc) {
	if (name_is(name, "print") || name_is(name, "push_warning") || name_is(name, "push_error")) {
		return;
	}
	if (name_is(name, "hide") && node_ok(node)) {
		g_nodes[node].flags = uint8_t(g_nodes[node].flags & ~uint8_t(1));
		return;
	}
	if (name_is(name, "show") && node_ok(node)) {
		g_nodes[node].flags = uint8_t(g_nodes[node].flags | uint8_t(1));
		return;
	}
	if (name_is(name, "set_visible") && node_ok(node)) {
		int on = 1;
		if (argv && argc > 0) {
			on = as_truth(argv[0]);
		}
		if (on) {
			g_nodes[node].flags = uint8_t(g_nodes[node].flags | uint8_t(1));
		} else {
			g_nodes[node].flags = uint8_t(g_nodes[node].flags & ~uint8_t(1));
		}
		return;
	}
	if ((name_is(name, "set_process") || name_is(name, "set_physics_process")) && node_ok(node)) {
		int on = 1;
		if (argv && argc > 0) {
			on = as_truth(argv[0]);
		} else {
			on = arg != 0.0f;
		}
		const uint8_t bit = name_is(name, "set_process") ? uint8_t(2) : uint8_t(8);
		if (on) {
			g_nodes[node].flags = uint8_t(g_nodes[node].flags & ~bit);
		} else {
			g_nodes[node].flags = uint8_t(g_nodes[node].flags | bit);
		}
		return;
	}
	if (name_is(name, "look_at") && node_ok(node)) {
		float tx = arg;
		float ty = float(-g_nodes[node].py);
		float tz = 0.0f;
		int look2 = (g_nodes[node].type == 3 || g_nodes[node].type == 5 || g_nodes[node].type == 6);
		if (argv && argc > 0 && argv[0].type == V_V2) {
			tx = argv[0].x;
			ty = argv[0].y;
			tz = float(g_nodes[node].pz);
			look2 = 1;
		} else if (argv && argc > 0 && argv[0].type == V_V3) {
			tx = argv[0].x;
			ty = argv[0].y;
			tz = argv[0].z;
		}
		const float dx = tx - float(g_nodes[node].px);
		const float dy = ty - float(-g_nodes[node].py);
		const float dz = tz - float(g_nodes[node].pz);
		float yaw = 0.0f;
		if (dx != 0.0f || dz != 0.0f) {
			if (dz == 0.0f) {
				yaw = dx > 0.0f ? 1.5707963f : -1.5707963f;
			} else {
				const float t = dx / dz;
				float a = t;
				if (t > 1.0f) {
					a = 1.5707963f - 1.0f / t;
				} else if (t < -1.0f) {
					a = -1.5707963f - 1.0f / t;
				}
				if (dz < 0.0f) {
					a += (dx >= 0.0f) ? 3.14159265f : -3.14159265f;
				}
				yaw = a;
			}
		}
		float pitch = 0.0f;
		const float horiz = dx * dx + dz * dz;
		if (horiz > 0.0001f || dy != 0.0f) {
			float h = horiz;
			if (h < 0.0001f) {
				h = 0.0001f;
			}
			// sqrt approx
			float s = h;
			for (int i = 0; i < 4; i++) {
				s = 0.5f * (s + h / s);
			}
			const float t = dy / s;
			float a = t;
			if (t > 1.0f) {
				a = 1.5707963f - 1.0f / t;
			} else if (t < -1.0f) {
				a = -1.5707963f - 1.0f / t;
			}
			pitch = a;
		}
		if (look2) {
			g_nodes[node].rz = int16_t(rad_to_ps1(util_atan2(ty - float(-g_nodes[node].py), tx - float(g_nodes[node].px))));
		} else {
			g_nodes[node].ry = int16_t(rad_to_ps1(yaw));
			g_nodes[node].rx = int16_t(rad_to_ps1(pitch));
		}
		return;
	}
	if (name_is(name, "set_position") && node_ok(node)) {
		if (argv && argc > 0 && argv[0].type == V_V3) {
			g_nodes[node].px = int16_t(argv[0].x);
			g_nodes[node].py = int16_t(-argv[0].y);
			g_nodes[node].pz = int16_t(argv[0].z);
		} else {
			g_nodes[node].pz += int16_t(arg);
		}
		return;
	}
	if (name_is(name, "grab_focus")) {
		for (int i = 0; i < g_nhud; i++) {
			if (int(g_hud[i].node_id) == node) {
				g_focus = i;
				return;
			}
		}
		return;
	}
	if (name_is(name, "release_focus")) {
		if (g_focus >= 0 && g_focus < g_nhud && int(g_hud[g_focus].node_id) == node) {
			g_focus = -1;
		}
		return;
	}
	if (name_is(name, "set_rotation") && node_ok(node)) {
		if (argv && argc > 0 && argv[0].type == V_V3) {
			g_nodes[node].rx = int16_t(rad_to_ps1(argv[0].x));
			g_nodes[node].ry = int16_t(rad_to_ps1(argv[0].y));
			g_nodes[node].rz = int16_t(rad_to_ps1(argv[0].z));
		}
		return;
	}
	if (node_ok(node)) {
		const int step = rad_to_ps1(arg);
		if (name_is(name, "rotate_x")) {
			g_nodes[node].rx = int16_t(g_nodes[node].rx + step);
		} else if (name_is(name, "rotate_z")) {
			g_nodes[node].rz = int16_t(g_nodes[node].rz + step);
		} else if (name_is(name, "translate")) {
			if (argv && argc > 0 && argv[0].type == V_V3) {
				g_nodes[node].px = int16_t(g_nodes[node].px + int(argv[0].x));
				g_nodes[node].py = int16_t(g_nodes[node].py + int(-argv[0].y));
				g_nodes[node].pz = int16_t(g_nodes[node].pz + int(argv[0].z));
			} else {
				g_nodes[node].pz = int16_t(g_nodes[node].pz + int(arg));
			}
		} else {
			g_nodes[node].ry = int16_t(g_nodes[node].ry + step);
		}
		return;
	}
	write_host_rot(host, name, arg);
}

static float util_abs(float x) {
	return x < 0.0f ? -x : x;
}

static float util_floor(float x) {
	int i = int(x);
	if (x < 0.0f && float(i) != x) {
		i--;
	}
	return float(i);
}

static float util_ceil(float x) {
	int i = int(x);
	if (x > 0.0f && float(i) != x) {
		i++;
	}
	return float(i);
}

static float util_sin(float x) {
	const float pi = 3.14159265f;
	const float twopi = 6.2831853f;
	while (x > pi) {
		x -= twopi;
	}
	while (x < -pi) {
		x += twopi;
	}
	const float x2 = x * x;
	return x * (1.0f - x2 * (1.0f / 6.0f) * (1.0f - x2 * (1.0f / 20.0f)));
}

static float util_sqrt(float x) {
	if (x <= 0.0f) {
		return 0.0f;
	}
	float g = x;
	for (int i = 0; i < 8; i++) {
		g = 0.5f * (g + x / g);
	}
	return g;
}

static float util_pow(float a, float b) {
	const int n = int(b);
	if (float(n) == b && n >= 0 && n < 16) {
		float r = 1.0f;
		for (int i = 0; i < n; i++) {
			r *= a;
		}
		return r;
	}
	if (a <= 0.0f) {
		return 0.0f;
	}
	float ln = 0.0f;
	float t = (a - 1.0f) / (a + 1.0f);
	float tp = t;
	for (int i = 0; i < 8; i++) {
		ln += tp / float(2 * i + 1);
		tp *= t * t;
	}
	ln *= 2.0f;
	const float y = b * ln;
	float exp = 1.0f + y;
	float term = y;
	for (int i = 2; i < 8; i++) {
		term *= y / float(i);
		exp += term;
	}
	return exp;
}

static float util_sign(float x) {
	if (x < 0.0f) {
		return -1.0f;
	}
	if (x > 0.0f) {
		return 1.0f;
	}
	return 0.0f;
}

static float util_atan2(float y, float x) {
	if (x == 0.0f && y == 0.0f) {
		return 0.0f;
	}
	const float ax = util_abs(x);
	const float ay = util_abs(y);
	float a = (ax > ay) ? (ay / ax) : (ax / ay);
	const float s = a * a;
	float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
	if (ay > ax) {
		r = 1.5707963f - r;
	}
	if (x < 0.0f) {
		r = 3.14159265f - r;
	}
	if (y < 0.0f) {
		r = -r;
	}
	return r;
}

static void node_world(int n, float *px, float *py, float *pz, float *rx, float *ry, float *rz) {
	*px = float(g_nodes[n].px);
	*py = float(g_nodes[n].py);
	*pz = float(g_nodes[n].pz);
	*rx = float(g_nodes[n].rx);
	*ry = float(g_nodes[n].ry);
	*rz = float(g_nodes[n].rz);
	int p = int(g_nodes[n].parent);
	int guard = 0;
	while (p >= 0 && p < g_nnode && g_node_used[p] && guard++ < 16) {
		*px += float(g_nodes[p].px);
		*py += float(g_nodes[p].py);
		*pz += float(g_nodes[p].pz);
		*rx += float(g_nodes[p].rx);
		*ry += float(g_nodes[p].ry);
		*rz += float(g_nodes[p].rz);
		p = int(g_nodes[p].parent);
	}
}

static int node_is_2d(int n) {
	if (!node_ok(n)) {
		return 0;
	}
	const uint8_t t = g_nodes[n].type;
	return t == 1 || t == 3 || t == 5 || t == 6 || t == 11;
}

static void node_basis(int n, float *fx, float *fy, float *fz, float *rx, float *ry, float *rz, float *ux, float *uy, float *uz) {
	float px, py, pz, erx, ery, erz;
	if (node_ok(n)) {
		node_world(n, &px, &py, &pz, &erx, &ery, &erz);
	} else {
		erx = ery = erz = 0;
	}
	const float s = (2.0f * 3.14159265f) / 4096.0f;
	const float pitch = erx * s;
	const float yaw = ery * s;
	const float roll = erz * s;
	if (node_is_2d(n)) {
		*fx = util_sin(roll + 1.5707963f);
		*fy = util_sin(roll);
		*fz = 0;
		*rx = -*fy;
		*ry = *fx;
		*rz = 0;
		*ux = 0;
		*uy = 0;
		*uz = 1;
		return;
	}
	const float cp = util_sin(pitch + 1.5707963f);
	const float sp = util_sin(pitch);
	*fx = util_sin(yaw + 1.5707963f) * cp;
	*fy = sp;
	*fz = util_sin(yaw) * cp;
	*rx = util_sin(yaw);
	*ry = 0;
	*rz = -util_sin(yaw + 1.5707963f);
	*ux = -*fx * sp;
	*uy = cp;
	*uz = -*fz * sp;
}

static int do_raycast(float ox, float oy, float oz, float dx, float dy, float dz, float dist, int dim, int mask, int skip, int group_bit) {
	int hitn = -1;
	float best = dist;
	g_ray_hit = 0;
	g_ray_node = -1;
	for (int i = 0; i < g_nhit; i++) {
		if (!g_hit_used[i] || !(g_hits[i].flags & 1)) {
			continue;
		}
		if (skip >= 0 && int(g_hits[i].node_id) == skip) {
			continue;
		}
		if (!(int(g_hits[i].layer) & mask)) {
			continue;
		}
		if (int(g_hits[i].dim) != dim) {
			continue;
		}
		if (group_bit >= 0) {
			const int nid = int(g_hits[i].node_id);
			if (!node_ok(nid) || !(g_group[nid] & uint8_t(1 << group_bit))) {
				continue;
			}
		}
		const float minx = float(g_hits[i].min_x);
		const float maxx = float(g_hits[i].max_x);
		const float miny = float(-g_hits[i].max_y);
		const float maxy = float(-g_hits[i].min_y);
		const float minz = dim == 2 ? -1 : float(g_hits[i].min_z);
		const float maxz = dim == 2 ? 1 : float(g_hits[i].max_z);
		float tmin = 0, tmax = dist;
		const float bmin[3] = { minx, miny, minz };
		const float bmax[3] = { maxx, maxy, maxz };
		const float o[3] = { ox, oy, oz };
		const float d[3] = { dx, dy, dz };
		int miss = 0;
		for (int a = 0; a < 3; a++) {
			if (d[a] == 0.0f) {
				if (o[a] < bmin[a] || o[a] > bmax[a]) {
					miss = 1;
				}
				continue;
			}
			float inv = 1.0f / d[a];
			float t0 = (bmin[a] - o[a]) * inv;
			float t1 = (bmax[a] - o[a]) * inv;
			if (t0 > t1) {
				const float tmp = t0;
				t0 = t1;
				t1 = tmp;
			}
			if (t0 > tmin) {
				tmin = t0;
			}
			if (t1 < tmax) {
				tmax = t1;
			}
			if (tmin > tmax) {
				miss = 1;
			}
		}
		if (!miss && tmin >= 0 && tmin < best) {
			best = tmin;
			hitn = int(g_hits[i].node_id);
			g_ray_hx = ox + dx * tmin;
			g_ray_hy = oy + dy * tmin;
			g_ray_hz = oz + dz * tmin;
			g_ray_hit = 1;
			g_ray_node = hitn;
		}
	}
	return hitn;
}

static int pack_id_of(int node) {
	if (node >= kPackBase && node < kPackBase + g_npack) {
		return node - kPackBase;
	}
	return -1;
}

static int find_pack_path(const char *path) {
	for (int i = 0; i < g_npack; i++) {
		if (name_is(g_packs[i].path, path)) {
			return i;
		}
	}
	return -1;
}

static int find_clip_name(const char *name) {
	for (int i = 0; i < g_nclip; i++) {
		if (name_is(g_clips[i].name, name)) {
			return i;
		}
	}
	return -1;
}

static int count_used_flags(const uint8_t *used, int n) {
	int c = 0;
	for (int i = 0; i < n; i++) {
		if (used[i]) {
			c++;
		}
	}
	return c;
}

static int inst_row_ram(int nodes, int hud, int tiles) {
	return nodes * int(sizeof(ScriptVMNode)) + hud * int(sizeof(ScriptVMHud)) + tiles * int(sizeof(ScriptVMTile));
}

static void bump_ram(int delta) {
	if (delta < 0) {
		const uint32_t sub = uint32_t(-delta);
		g_ram_used = g_ram_used > sub ? g_ram_used - sub : 0;
	} else {
		g_ram_used += uint32_t(delta);
		if (g_ram_used > g_ram_peak) {
			g_ram_peak = g_ram_used;
		}
	}
}

static uint8_t g_load_buf[262208];

static int read_host_file(const char *path, uint8_t *dst, int max) {
	if (!path || !dst || max <= 0) {
		return -1;
	}
	if (PCinit() != 0) {
		return -1;
	}
	const int fd = PCopen(path, PCDRV_MODE_READ);
	if (fd < 0) {
		return -1;
	}
	const int n = PCread(fd, dst, max);
	PCclose(fd);
	return n;
}

static int write_host_file(const char *path, const uint8_t *src, int n) {
	if (!path || !src || n < 0) {
		return 0;
	}
	if (PCinit() != 0) {
		return 0;
	}
	int fd = PCopen(path, PCDRV_MODE_WRITE);
	if (fd < 0) {
		fd = PCcreat(path);
	}
	if (fd < 0) {
		return 0;
	}
	const int w = PCwrite(fd, src, size_t(n));
	PCclose(fd);
	return w == n;
}

static int try_read_pack_blob(int pack, const char *kind, uint8_t *dst, int max) {
	char a[64];
	char b[64];
	char c[64];
	const int n = pack;
	int i = 0;
	const char *pre = "packs/";
	while (pre[i] && i < 16) {
		a[i] = pre[i];
		i++;
	}
	a[i++] = char('0' + (n / 10) % 10);
	a[i++] = char('0' + n % 10);
	a[i++] = '/';
	int k = 0;
	while (kind[k] && i < 48) {
		a[i++] = kind[k++];
	}
	a[i++] = char('0' + (n / 10) % 10);
	a[i++] = char('0' + n % 10);
	a[i++] = '.';
	a[i++] = 'b';
	a[i++] = 'i';
	a[i++] = 'n';
	a[i] = 0;
	for (int j = 0; j < 64; j++) {
		b[j] = a[j];
		if (a[j] >= 'a' && a[j] <= 'z') {
			b[j] = char(a[j] - 32);
		}
		if (!a[j]) {
			break;
		}
	}
	int ci = 0;
	while (kind[ci] && ci < 48) {
		c[ci] = kind[ci];
		ci++;
	}
	c[ci++] = char('0' + (n / 10) % 10);
	c[ci++] = char('0' + n % 10);
	c[ci++] = '.';
	c[ci++] = 'b';
	c[ci++] = 'i';
	c[ci++] = 'n';
	c[ci] = 0;
	int got = read_host_file(a, dst, max);
	if (got < 0) {
		got = read_host_file(b, dst, max);
	}
	if (got < 0) {
		got = read_host_file(c, dst, max);
	}
	return got;
}

static int parse_nodes_into(const uint8_t *blob, int size, ScriptVMNode *out, int maxn) {
	if (!blob || size < 8 || blob[0] != 'N' || blob[1] != 'O' || blob[2] != 'D' || blob[3] != 'E') {
		return -1;
	}
	if (ru16(blob + 4) != PS1_COOK_ABI) {
		return -1;
	}
	const int nc = int(ru16(blob + 6));
	const uint8_t *p = blob + 8;
	const uint8_t *end = blob + size;
	int got = 0;
	for (int i = 0; i < nc && got < maxn && p + 4 <= end; i++) {
		ScriptVMNode n{};
		n.parent = int16_t(p[0] | (p[1] << 8));
		p += 2;
		const uint8_t nl = *p++;
		uint8_t cpy = nl < 31 ? nl : 31;
		for (uint8_t k = 0; k < cpy && p + k < end; k++) {
			n.name[k] = char(p[k]);
		}
		n.name[cpy] = 0;
		p += nl;
		if (p + 21 > end) {
			break;
		}
		n.type = *p++;
		n.flags = *p++;
		n.px = int16_t(p[0] | (p[1] << 8));
		n.py = int16_t(p[2] | (p[3] << 8));
		n.pz = int16_t(p[4] | (p[5] << 8));
		n.rx = int16_t(p[6] | (p[7] << 8));
		n.ry = int16_t(p[8] | (p[9] << 8));
		n.rz = int16_t(p[10] | (p[11] << 8));
		n.tri_lo = uint16_t(p[12] | (p[13] << 8));
		n.tri_hi = uint16_t(p[14] | (p[15] << 8));
		n.sprite = int16_t(p[16] | (p[17] << 8));
		n.script = int16_t(p[18] | (p[19] << 8));
		p += 20;
		g_parse_groups[got] = *p++;
		out[got++] = n;
	}
	if (p + 128 <= end) {
		for (int g = 0; g < 8; g++) {
			int k = 0;
			while (k < 15 && p[g * 16 + k]) {
				g_gname[g][k] = char(p[g * 16 + k]);
				k++;
			}
			g_gname[g][k] = 0;
		}
		p += 128;
	}
	if (p + 128 <= end) {
		for (int i = 0; i < PS1_MAX_NODES; i++) {
			g_parse_scroll[i] = i < 128 ? p[i] : 0;
		}
		p += 128;
	}
	if (p + 10 <= end) {
		g_fog_on = p[0] ? 1 : 0;
		g_fog_start = int(p[2] | (p[3] << 8));
		g_fog_end = int(p[4] | (p[5] << 8));
		g_fog_r = p[6];
		g_fog_g = p[7];
		g_fog_b = p[8];
		p += 10;
	}
	if (p + 128 <= end) {
		for (int i = 0; i < PS1_MAX_NODES; i++) {
			g_parse_ysort[i] = i < 128 ? p[i] : 0;
		}
		p += 128;
	}
	if (p + 128 <= end) {
		for (int i = 0; i < PS1_MAX_NODES; i++) {
			g_parse_kind[i] = i < 128 ? p[i] : 0;
		}
		p += 128;
	}
	if (p + 128 <= end) {
		for (int i = 0; i < PS1_MAX_NODES; i++) {
			g_parse_path[i] = i < 128 ? p[i] : 0;
		}
		p += 128;
	}
	if (p + 8 <= end) {
		g_hud_ox = int16_t(p[0] | (p[1] << 8));
		g_hud_oy = int16_t(p[2] | (p[3] << 8));
	}
	return got;
}

static int parse_hud_into(const uint8_t *blob, int size, ScriptVMHud *out, int maxn) {
	if (!blob || size < 8 || blob[0] != 'H' || blob[1] != 'U' || blob[2] != 'D' || blob[3] != '0') {
		return -1;
	}
	if (ru16(blob + 4) != PS1_COOK_ABI) {
		return -1;
	}
	const int nc = int(ru16(blob + 6));
	const uint8_t *p = blob + 8;
	const uint8_t *end = blob + size;
	int got = 0;
	for (int i = 0; i < nc && got < maxn && p + 17 <= end; i++) {
		ScriptVMHud h{};
		h.x = int16_t(p[0] | (p[1] << 8));
		h.y = int16_t(p[2] | (p[3] << 8));
		h.w = int16_t(p[4] | (p[5] << 8));
		h.h = int16_t(p[6] | (p[7] << 8));
		p += 8;
		h.kind = *p++;
		h.tex = *p++;
		h.rgb = *p++;
		h.node_id = *p++;
		h.flags = *p++;
		h.value = int16_t(p[0] | (p[1] << 8));
		h.vmin = int16_t(p[2] | (p[3] << 8));
		h.vmax = int16_t(p[4] | (p[5] << 8));
		p += 6;
		const uint8_t tl = *p++;
		uint8_t cpy = tl < 63 ? tl : 63;
		for (uint8_t k = 0; k < cpy && p + k < end; k++) {
			h.text[k] = char(p[k]);
		}
		h.text[cpy] = 0;
		p += tl;
		if (p >= end) {
			out[got++] = h;
			break;
		}
		h.nitems = *p++;
		if (h.nitems > 8) {
			h.nitems = 8;
		}
		for (int it = 0; it < h.nitems && p < end; it++) {
			const uint8_t il = *p++;
			uint8_t ic = il < 15 ? il : 15;
			for (uint8_t k = 0; k < ic && p + k < end; k++) {
				h.items[it][k] = char(p[k]);
			}
			h.items[it][ic] = 0;
			p += il;
		}
		out[got++] = h;
	}
	return got;
}

static int parse_tiles_into(const uint8_t *blob, int size, ScriptVMTile *out, int maxn) {
	if (!blob || size < 10 || blob[0] != 'T' || blob[1] != 'I' || blob[2] != 'L' || blob[3] != 'E') {
		return -1;
	}
	if (ru16(blob + 4) != PS1_COOK_ABI) {
		return -1;
	}
	const int nc = int(ru16(blob + 6));
	const uint8_t *p = blob + 10;
	const uint8_t *end = blob + size;
	int got = 0;
	for (int i = 0; i < nc && got < maxn && p + 9 <= end; i++) {
		ScriptVMTile t{};
		t.x = int16_t(p[0] | (p[1] << 8));
		t.y = int16_t(p[2] | (p[3] << 8));
		t.u = p[4];
		t.v = p[5];
		t.tex = p[6];
		t.node_id = p[7];
		t.flags = p[8];
		p += 9;
		out[got++] = t;
	}
	return got;
}

static int append_mesh_blob(const uint8_t *blob, int size) {
	if (!blob || size < 4) {
		return 0;
	}
	const int n = int(blob[0] | (blob[1] << 8));
	const int room = PS1_MAX_TRIS - g_ntri;
	const int take = n > room ? room : n;
	const size_t need = 4 + size_t(take) * 32;
	if (need > size_t(size)) {
		return -1;
	}
	for (int i = 0; i < take; i++) {
		const uint8_t *src = blob + 4 + i * 32;
		uint8_t *dst = g_mesh + 4 + (g_ntri + i) * 32;
		for (int b = 0; b < 32; b++) {
			dst[b] = src[b];
		}
	}
	g_ntri += take;
	g_mesh[0] = uint8_t(g_ntri & 0xff);
	g_mesh[1] = uint8_t(g_ntri >> 8);
	return take;
}

static int pack_copy_cost(int pack, int *nodes, int *hud, int *tiles, int *tris, int *tims, int *ram, int include_load) {
	if (pack < 0 || pack >= g_npack) {
		return 0;
	}
	const ScriptVMPack *p = &g_packs[pack];
	*nodes = int(p->node_count);
	*hud = int(p->hud_count);
	*tiles = int(p->tile_count);
	*tris = include_load && !p->resident ? int(p->tri_count) : 0;
	*tims = include_load && !p->resident ? int(p->tim_count) : 0;
	*ram = inst_row_ram(*nodes, *hud, *tiles);
	if (include_load && !p->resident) {
		*ram += int(p->ram_bytes);
	}
	return 1;
}

static int budgets_fit(int nodes, int hud, int tiles, int tris, int tims, int ram) {
	if (count_used_flags(g_node_used, PS1_MAX_NODES) + nodes > PS1_MAX_NODES) {
		return 0;
	}
	if (count_used_flags(g_hud_used, PS1_MAX_HUD) + hud > PS1_MAX_HUD) {
		return 0;
	}
	if (count_used_flags(g_tile_used, PS1_MAX_TILES) + tiles > PS1_MAX_TILES) {
		return 0;
	}
	if (g_ntri + tris > PS1_MAX_TRIS) {
		return 0;
	}
	if (g_ntim_used + tims > 16) {
		return 0;
	}
	if (g_ram_used + uint32_t(ram) > uint32_t(PS1_RAM_BUDGET)) {
		return 0;
	}
	return 1;
}

static int load_pack_resident(int pack);
static int unload_pack_resident(int pack);
static void reclaim_instance(int node);

static int find_free_run(const uint8_t *used, int maxn, int need) {
	if (need <= 0) {
		return 0;
	}
	int run = 0;
	int start = 0;
	for (int i = 0; i < maxn; i++) {
		if (!used[i]) {
			if (run == 0) {
				start = i;
			}
			run++;
			if (run == need) {
				return start;
			}
		} else {
			run = 0;
		}
	}
	return -1;
}

static void activate_pack(int pack, const ScriptVMHost *host) {
	if (pack < 0 || pack >= g_npack) {
		return;
	}
	if (!g_packs[pack].resident) {
		return;
	}
	const int lo = int(g_packs[pack].node_lo);
	const int hi = int(g_packs[pack].node_hi);
	for (int n = lo; n < hi && n < g_nnode; n++) {
		g_nodes[n].flags = uint8_t(g_nodes[n].flags | 1);
	}
	g_anim_playing = 0;
	g_anim_clip = -1;
	g_anim_ms = 0.0f;
	for (int t = 0; t < 8; t++) {
		g_timer_used[t] = 0;
		g_timer_end[t] = 0.0f;
	}
	for (int i = 0; i < g_ncam; i++) {
		if (g_cams[i].is_default && (g_cams[i].pack == uint8_t(pack) || g_ncam == 1)) {
			g_cam_cur = i;
			g_cam_def = i;
			break;
		}
	}
	if (g_cam_cur >= 0 && g_cam_cur < g_ncam && host) {
		if (host->pos_x) {
			*host->pos_x = g_cams[g_cam_cur].px;
		}
		if (host->pos_y) {
			*host->pos_y = g_cams[g_cam_cur].py;
		}
		if (host->pos_z) {
			*host->pos_z = g_cams[g_cam_cur].pz;
		}
		if (host->rot_x) {
			*host->rot_x = g_cams[g_cam_cur].rx;
		}
		if (host->rot_y) {
			*host->rot_y = g_cams[g_cam_cur].ry;
		}
		if (host->rot_z) {
			*host->rot_z = g_cams[g_cam_cur].rz;
		}
	} else if (g_packs[pack].has_cam && host) {
		if (host->pos_x) {
			*host->pos_x = g_packs[pack].cam_px;
		}
		if (host->pos_y) {
			*host->pos_y = g_packs[pack].cam_py;
		}
		if (host->pos_z) {
			*host->pos_z = g_packs[pack].cam_pz;
		}
		if (host->rot_x) {
			*host->rot_x = g_packs[pack].cam_rx;
		}
		if (host->rot_y) {
			*host->rot_y = g_packs[pack].cam_ry;
		}
		if (host->rot_z) {
			*host->rot_z = g_packs[pack].cam_rz;
		}
	}
	for (int h = 0; h < g_nhud; h++) {
		const int nid = int(g_hud[h].node_id);
		const int vis = nid >= 0 && nid < g_nnode && (g_nodes[nid].flags & 1);
		if (vis) {
			g_hud[h].flags = uint8_t(g_hud[h].flags | 1);
		} else {
			g_hud[h].flags = uint8_t(g_hud[h].flags & ~uint8_t(1));
		}
	}
	for (int n = 0; n < g_nnode; n++) {
		if (g_node_used[n] && (g_nodes[n].flags & 1)) {
			g_node_ready[n] = 0;
		}
	}
	g_did_ready = 0;
}

static int find_free_node() {
	for (int i = 0; i < PS1_MAX_NODES; i++) {
		if (!g_node_used[i]) {
			return i;
		}
	}
	return -1;
}

static int find_free_hud() {
	for (int i = 0; i < PS1_MAX_HUD; i++) {
		if (!g_hud_used[i]) {
			return i;
		}
	}
	return -1;
}

static int find_free_tile() {
	for (int i = 0; i < PS1_MAX_TILES; i++) {
		if (!g_tile_used[i]) {
			return i;
		}
	}
	return -1;
}

static int load_pack_resident(int pack) {
	if (pack < 0 || pack >= g_npack) {
		return 0;
	}
	if (g_packs[pack].resident) {
		return 1;
	}
	int nodes = 0, hud = 0, tiles = 0, tris = 0, tims = 0, ram = 0;
	if (!pack_copy_cost(pack, &nodes, &hud, &tiles, &tris, &tims, &ram, 1)) {
		return 0;
	}
	nodes = int(g_packs[pack].node_count);
	hud = int(g_packs[pack].hud_count);
	tiles = int(g_packs[pack].tile_count);
	tris = int(g_packs[pack].tri_count);
	tims = int(g_packs[pack].tim_count);
	ram = int(g_packs[pack].ram_bytes);
	if (!budgets_fit(nodes, hud, tiles, tris, tims, ram)) {
		return 0;
	}
	ScriptVMNode parsed[PS1_MAX_NODES];
	int ngot = 0;
	if (nodes > 0) {
		const int nb = try_read_pack_blob(pack, "NODE", g_load_buf, int(sizeof(g_load_buf)));
		ngot = parse_nodes_into(g_load_buf, nb, parsed, PS1_MAX_NODES);
		if (ngot <= 0) {
			return 0;
		}
	}
	int mesh_added = 0;
	if (tris > 0) {
		const int mb = try_read_pack_blob(pack, "MESH", g_load_buf, int(sizeof(g_load_buf)));
		mesh_added = append_mesh_blob(g_load_buf, mb);
		if (mesh_added < 0) {
			return 0;
		}
	}
	const int tri_base = g_ntri - (mesh_added > 0 ? mesh_added : 0);
	const int nbase = ngot > 0 ? find_free_run(g_node_used, PS1_MAX_NODES, ngot) : 0;
	if (ngot > 0 && nbase < 0) {
		if (mesh_added > 0) {
			g_ntri -= mesh_added;
			g_mesh[0] = uint8_t(g_ntri & 0xff);
			g_mesh[1] = uint8_t(g_ntri >> 8);
		}
		return 0;
	}
	int slots[PS1_MAX_NODES];
	for (int i = 0; i < ngot; i++) {
		slots[i] = nbase + i;
		g_node_used[slots[i]] = 1;
	}
	for (int i = 0; i < ngot; i++) {
		const int dst = slots[i];
		g_nodes[dst] = parsed[i];
		const int p = int(parsed[i].parent);
		if (i == 0 || p < 0 || p >= ngot) {
			g_nodes[dst].parent = -1;
		} else {
			g_nodes[dst].parent = int16_t(slots[p]);
		}
		if (g_nodes[dst].tri_hi > g_nodes[dst].tri_lo) {
			g_nodes[dst].tri_lo = uint16_t(int(g_nodes[dst].tri_lo) + tri_base);
			g_nodes[dst].tri_hi = uint16_t(int(g_nodes[dst].tri_hi) + tri_base);
		}
		g_nodes[dst].flags = uint8_t(g_nodes[dst].flags | 1);
		g_group[dst] = g_parse_groups[i];
		g_scroll[dst] = g_parse_scroll[i];
		g_node_ready[dst] = 0;
		if (dst + 1 > g_nnode) {
			g_nnode = dst + 1;
		}
	}
	ScriptVMHud hudn[PS1_MAX_HUD];
	const int hb = hud > 0 ? try_read_pack_blob(pack, "HUD", g_load_buf, int(sizeof(g_load_buf))) : 0;
	const int hgot = hud > 0 ? parse_hud_into(g_load_buf, hb, hudn, PS1_MAX_HUD) : 0;
	const int hbase = hgot > 0 ? find_free_run(g_hud_used, PS1_MAX_HUD, hgot) : 0;
	if (hgot > 0 && hbase < 0) {
		for (int i = 0; i < ngot; i++) {
			g_node_used[slots[i]] = 0;
		}
		if (mesh_added > 0) {
			g_ntri -= mesh_added;
			g_mesh[0] = uint8_t(g_ntri & 0xff);
			g_mesh[1] = uint8_t(g_ntri >> 8);
		}
		return 0;
	}
	for (int h = 0; h < hgot; h++) {
		const int dsth = hbase + h;
		g_hud[dsth] = hudn[h];
		const int nid = int(hudn[h].node_id);
		if (nid >= 0 && nid < ngot) {
			g_hud[dsth].node_id = uint8_t(slots[nid]);
		}
		g_hud_used[dsth] = 1;
		if (dsth + 1 > g_nhud) {
			g_nhud = dsth + 1;
		}
	}
	ScriptVMTile tilen[PS1_MAX_TILES];
	const int tb = tiles > 0 ? try_read_pack_blob(pack, "TILE", g_load_buf, int(sizeof(g_load_buf))) : 0;
	const int tgot = tiles > 0 ? parse_tiles_into(g_load_buf, tb, tilen, PS1_MAX_TILES) : 0;
	const int tbase = tgot > 0 ? find_free_run(g_tile_used, PS1_MAX_TILES, tgot) : 0;
	if (tgot > 0 && tbase < 0) {
		for (int i = 0; i < ngot; i++) {
			g_node_used[slots[i]] = 0;
		}
		for (int h = 0; h < hgot; h++) {
			g_hud_used[hbase + h] = 0;
		}
		if (mesh_added > 0) {
			g_ntri -= mesh_added;
			g_mesh[0] = uint8_t(g_ntri & 0xff);
			g_mesh[1] = uint8_t(g_ntri >> 8);
		}
		return 0;
	}
	for (int t = 0; t < tgot; t++) {
		const int dstt = tbase + t;
		g_tiles[dstt] = tilen[t];
		const int nid = int(tilen[t].node_id);
		if (nid >= 0 && nid < ngot) {
			g_tiles[dstt].node_id = uint8_t(slots[nid]);
		}
		g_tile_used[dstt] = 1;
		if (dstt + 1 > g_ntile) {
			g_ntile = dstt + 1;
		}
	}
	g_packs[pack].node_lo = uint16_t(ngot > 0 ? nbase : 0);
	g_packs[pack].node_hi = uint16_t(ngot > 0 ? nbase + ngot : 0);
	g_packs[pack].hud_lo = uint16_t(hgot > 0 ? hbase : 0);
	g_packs[pack].hud_hi = uint16_t(hgot > 0 ? hbase + hgot : 0);
	g_packs[pack].tile_lo = uint16_t(tgot > 0 ? tbase : 0);
	g_packs[pack].tile_hi = uint16_t(tgot > 0 ? tbase + tgot : 0);
	if (tims > 0 && g_host && g_host->upload_tpak) {
		const int tbk = try_read_pack_blob(pack, "TPAK", g_load_buf, int(sizeof(g_load_buf)));
		if (tbk > 0) {
			int lo = 0, hi = 0;
			if (!g_host->upload_tpak(g_load_buf, tbk, &lo, &hi)) {
				for (int i = 0; i < ngot; i++) {
					g_node_used[slots[i]] = 0;
				}
				for (int h = 0; h < hgot; h++) {
					g_hud_used[hbase + h] = 0;
				}
				for (int t = 0; t < tgot; t++) {
					g_tile_used[tbase + t] = 0;
				}
				if (mesh_added > 0) {
					g_ntri -= mesh_added;
					g_mesh[0] = uint8_t(g_ntri & 0xff);
					g_mesh[1] = uint8_t(g_ntri >> 8);
				}
				return 0;
			}
			g_packs[pack].tim_lo = uint8_t(lo);
			g_packs[pack].tim_hi = uint8_t(hi);
		}
	}
	g_packs[pack].resident = 1;
	g_ntim_used += tims;
	bump_ram(int(g_packs[pack].ram_bytes));
	g_did_ready = 0;
	return 1;
}

static int unload_pack_resident(int pack) {
	if (pack <= 0 || pack >= g_npack || !g_packs[pack].resident) {
		return 0;
	}
	if (g_fmv_pack == pack) {
		return 0;
	}
	const int lo = int(g_packs[pack].node_lo);
	const int hi = int(g_packs[pack].node_hi);
	if (g_cam_attach >= lo && g_cam_attach < hi) {
		return 0;
	}
	if (g_host && g_host->evict_tpak && g_packs[pack].tim_hi > g_packs[pack].tim_lo) {
		g_host->evict_tpak(int(g_packs[pack].tim_lo), int(g_packs[pack].tim_hi));
	}
	for (int n = 0; n < g_nnode; n++) {
		if (!g_node_used[n] || !(g_nodes[n].flags & 4)) {
			continue;
		}
		const int p = int(g_nodes[n].parent);
		if (p >= lo && p < hi) {
			reclaim_instance(n);
		}
	}
	for (int n = lo; n < hi && n < g_nnode; n++) {
		if (!g_node_used[n] || (g_nodes[n].flags & 4)) {
			continue;
		}
		for (int h = 0; h < g_nhud; h++) {
			if (g_hud_used[h] && int(g_hud[h].node_id) == n) {
				g_hud_used[h] = 0;
				g_hud[h].flags = 0;
			}
		}
		for (int t = 0; t < g_ntile; t++) {
			if (g_tile_used[t] && int(g_tiles[t].node_id) == n) {
				g_tile_used[t] = 0;
			}
		}
		g_nodes[n].flags = 0;
		g_node_used[n] = 0;
		g_node_ready[n] = 0;
		g_inst_group[n] = 0;
	}
	g_ntim_used -= int(g_packs[pack].tim_count);
	if (g_ntim_used < 0) {
		g_ntim_used = 0;
	}
	bump_ram(-int(g_packs[pack].ram_bytes));
	g_packs[pack].resident = 0;
	g_packs[pack].node_lo = 0;
	g_packs[pack].node_hi = 0;
	return 1;
}

static int instantiate_pack(int pack, int parent) {
	if (pack < 0 || pack >= g_npack) {
		return -1;
	}
	int cn = 0, ch = 0, ct = 0, ctr = 0, cti = 0, cr = 0;
	const int need_load = !g_packs[pack].resident;
	if (!pack_copy_cost(pack, &cn, &ch, &ct, &ctr, &cti, &cr, need_load)) {
		return -1;
	}
	int load_n = 0, load_h = 0, load_t = 0, load_tr = 0, load_ti = 0, load_r = 0;
	if (need_load) {
		load_n = int(g_packs[pack].node_count);
		load_h = int(g_packs[pack].hud_count);
		load_t = int(g_packs[pack].tile_count);
		load_tr = int(g_packs[pack].tri_count);
		load_ti = int(g_packs[pack].tim_count);
		load_r = int(g_packs[pack].ram_bytes);
	}
	const int copy_r = inst_row_ram(cn, ch, ct);
	if (!budgets_fit(load_n + cn, load_h + ch, load_t + ct, load_tr, load_ti, load_r + copy_r)) {
		return -1;
	}
	if (need_load && !load_pack_resident(pack)) {
		return -1;
	}
	const int lo = int(g_packs[pack].node_lo);
	const int hi = int(g_packs[pack].node_hi);
	const int need = int(g_packs[pack].node_count);
	if (need <= 0 || hi <= lo) {
		return -1;
	}
	const int hlo = int(g_packs[pack].hud_lo);
	const int hhi = int(g_packs[pack].hud_hi);
	const int tlo = int(g_packs[pack].tile_lo);
	const int thi = int(g_packs[pack].tile_hi);
	int hneed = ch;
	int tneed = ct;
	int nfree = 0;
	int hfree = 0;
	int tfree = 0;
	for (int i = 0; i < PS1_MAX_NODES; i++) {
		if (!g_node_used[i]) {
			nfree++;
		}
	}
	for (int i = 0; i < PS1_MAX_HUD; i++) {
		if (!g_hud_used[i]) {
			hfree++;
		}
	}
	for (int i = 0; i < PS1_MAX_TILES; i++) {
		if (!g_tile_used[i]) {
			tfree++;
		}
	}
	if (nfree < need || hfree < hneed || tfree < tneed) {
		if (need_load) {
			unload_pack_resident(pack);
		}
		return -1;
	}
	int slots[PS1_MAX_NODES];
	for (int i = 0; i < need; i++) {
		slots[i] = find_free_node();
		if (slots[i] < 0) {
			return -1;
		}
		g_node_used[slots[i]] = 1;
	}
	if (g_next_group == 0) {
		g_next_group = 1;
	}
	const uint8_t grp = g_next_group++;
	const int base = slots[0];
	for (int i = 0; i < need; i++) {
		const int dst = slots[i];
		g_nodes[dst] = g_nodes[lo + i];
		const int p = int(g_nodes[dst].parent);
		if (i == 0 || p < lo || p >= hi) {
			g_nodes[dst].parent = int16_t(parent);
		} else {
			g_nodes[dst].parent = int16_t(slots[p - lo]);
		}
		g_nodes[dst].flags = uint8_t(g_nodes[dst].flags | 1 | 4);
		g_inst_group[dst] = grp;
		g_group[dst] = g_group[lo + i];
		g_scroll[dst] = g_scroll[lo + i];
		g_nvel[dst][0] = g_nvel[dst][1] = g_nvel[dst][2] = 0;
		g_node_ready[dst] = 0;
		if (dst + 1 > g_nnode) {
			g_nnode = dst + 1;
		}
	}
	for (int h = hlo; h < hhi && h < g_nhud; h++) {
		const int nid = int(g_hud[h].node_id);
		if (nid < lo || nid >= hi) {
			continue;
		}
		const int dsth = find_free_hud();
		if (dsth < 0) {
			reclaim_instance(base);
			if (need_load) {
				unload_pack_resident(pack);
			}
			return -1;
		}
		g_hud[dsth] = g_hud[h];
		g_hud[dsth].node_id = uint8_t(slots[nid - lo]);
		g_hud[dsth].flags = uint8_t(g_hud[dsth].flags | 1);
		g_hud_used[dsth] = 1;
		if (dsth + 1 > g_nhud) {
			g_nhud = dsth + 1;
		}
	}
	for (int t = tlo; t < thi && t < g_ntile; t++) {
		const int nid = int(g_tiles[t].node_id);
		if (nid < lo || nid >= hi) {
			continue;
		}
		const int dstt = find_free_tile();
		if (dstt < 0) {
			reclaim_instance(base);
			if (need_load) {
				unload_pack_resident(pack);
			}
			return -1;
		}
		g_tiles[dstt] = g_tiles[t];
		g_tiles[dstt].node_id = uint8_t(slots[nid - lo]);
		g_tile_used[dstt] = 1;
		if (dstt + 1 > g_ntile) {
			g_ntile = dstt + 1;
		}
	}
	for (int h = 0; h < g_nhit; h++) {
		if (!g_hit_used[h]) {
			continue;
		}
		const int nid = int(g_hits[h].node_id);
		if (nid < lo || nid >= hi) {
			continue;
		}
		int dst = -1;
		for (int i = 0; i < PS1_MAX_HITS; i++) {
			if (!g_hit_used[i]) {
				dst = i;
				break;
			}
		}
		if (dst < 0) {
			break;
		}
		g_hits[dst] = g_hits[h];
		g_hits[dst].node_id = int16_t(slots[nid - lo]);
		g_hit_used[dst] = 1;
		if (dst + 1 > g_nhit) {
			g_nhit = dst + 1;
		}
	}
	bump_ram(copy_r);
	return base;
}

static void reclaim_instance(int node) {
	if (!node_ok(node) || !(g_nodes[node].flags & 4)) {
		if (node_ok(node)) {
			g_nodes[node].flags = uint8_t(g_nodes[node].flags & ~uint8_t(1));
		}
		return;
	}
	const uint8_t grp = g_inst_group[node];
	if (!grp) {
		g_nodes[node].flags = uint8_t(g_nodes[node].flags & ~uint8_t(1));
		return;
	}
	int fn = 0, fh = 0, ft = 0;
	for (int n = 0; n < g_nnode; n++) {
		if (!g_node_used[n] || g_inst_group[n] != grp) {
			continue;
		}
		for (int h = 0; h < g_nhud; h++) {
			if (g_hud_used[h] && int(g_hud[h].node_id) == n) {
				g_hud_used[h] = 0;
				g_hud[h].flags = 0;
				fh++;
			}
		}
		for (int t = 0; t < g_ntile; t++) {
			if (g_tile_used[t] && int(g_tiles[t].node_id) == n) {
				g_tile_used[t] = 0;
				ft++;
			}
		}
		for (int h = 0; h < PS1_MAX_HITS; h++) {
			if (g_hit_used[h] && int(g_hits[h].node_id) == n) {
				g_hit_used[h] = 0;
			}
		}
		for (int c = 0; c < PS1_MAX_CONNS; c++) {
			if (g_conn[c].used && (g_conn[c].src == n || g_conn[c].dest == n)) {
				g_conn[c].used = 0;
			}
		}
		g_nodes[n].flags = 0;
		g_node_used[n] = 0;
		g_node_ready[n] = 0;
		g_inst_group[n] = 0;
		g_group[n] = 0;
		g_scroll[n] = 0;
		fn++;
	}
	if (fn || fh || ft) {
		bump_ram(-inst_row_ram(fn, fh, ft));
	}
}

static void apply_cam_index(int idx, const ScriptVMHost *host) {
	if (idx < 0 || idx >= g_ncam || !host) {
		return;
	}
	g_cam_cur = idx;
	if (host->pos_x) {
		*host->pos_x = g_cams[idx].px;
	}
	if (host->pos_y) {
		*host->pos_y = g_cams[idx].py;
	}
	if (host->pos_z) {
		*host->pos_z = g_cams[idx].pz;
	}
	if (host->rot_x) {
		*host->rot_x = g_cams[idx].rx;
	}
	if (host->rot_y) {
		*host->rot_y = g_cams[idx].ry;
	}
	if (host->rot_z) {
		*host->rot_z = g_cams[idx].rz;
	}
}

static int aabb_overlap(int a, int b, int mask) {
	if (a < 0 || b < 0 || a >= PS1_MAX_HITS || b >= PS1_MAX_HITS) {
		return 0;
	}
	if (!g_hit_used[a] || !g_hit_used[b]) {
		return 0;
	}
	if (!(g_hits[a].flags & 1) || !(g_hits[b].flags & 1)) {
		return 0;
	}
	if (!(int(g_hits[a].layer) & mask) || !(int(g_hits[b].layer) & mask)) {
		return 0;
	}
	if (g_hits[a].dim != g_hits[b].dim) {
		return 0;
	}
	if (g_hits[a].max_x < g_hits[b].min_x || g_hits[b].max_x < g_hits[a].min_x) {
		return 0;
	}
	if (g_hits[a].max_y < g_hits[b].min_y || g_hits[b].max_y < g_hits[a].min_y) {
		return 0;
	}
	if (g_hits[a].dim == 3) {
		if (g_hits[a].max_z < g_hits[b].min_z || g_hits[b].max_z < g_hits[a].min_z) {
			return 0;
		}
	}
	if (kit_invuln(int(g_hits[a].node_id)) || kit_invuln(int(g_hits[b].node_id))) {
		return 0;
	}
	return 1;
}

static int hit_of_node(int node) {
	for (int i = 0; i < g_nhit; i++) {
		if (g_hit_used[i] && int(g_hits[i].node_id) == node) {
			return i;
		}
	}
	return -1;
}

static int group_bit_of(const char *name) {
	if (!name || !name[0]) {
		return -1;
	}
	if (name[0] >= '0' && name[0] <= '7' && name[1] == 0) {
		return name[0] - '0';
	}
	for (int i = 0; i < 8; i++) {
		if (g_gname[i][0] && name_is(name, g_gname[i])) {
			return i;
		}
	}
	for (int i = 0; i < 8; i++) {
		if (!g_gname[i][0]) {
			int k = 0;
			while (k < 15 && name[k]) {
				g_gname[i][k] = name[k];
				k++;
			}
			g_gname[i][k] = 0;
			return i;
		}
	}
	return -1;
}

static int group_bit_arg(const GVar *argv, int argc, int idx) {
	if (!argv || argc <= idx) {
		return -1;
	}
	if (argv[idx].type == V_STR) {
		return group_bit_of(argv[idx].s);
	}
	const int b = int(as_float(argv[idx]));
	return (b >= 0 && b < 8) ? b : -1;
}

static int load_pack_slice(int pack, const char *kind) {
	if (pack < 0 || pack >= g_npack) {
		return 0;
	}
	if (pack == 0 && name_is(kind, "ANIM") && g_nclip > 0) {
		g_packs[0].anim_resident = 1;
		return 1;
	}
	if (pack == 0 && name_is(kind, "CAM") && g_ncam > 0) {
		g_packs[0].cam_resident = 1;
		return 1;
	}
	if (pack == 0 && name_is(kind, "HIT") && g_nhit > 0) {
		g_packs[0].hit_resident = 1;
		return 1;
	}
	if (pack == 0 && name_is(kind, "NAV") && g_nav_loaded) {
		g_packs[0].nav_resident = 1;
		return 1;
	}
	if (pack == 0 && name_is(kind, "PATH") && g_path_loaded) {
		g_packs[0].path_resident = 1;
		return 1;
	}
	if (pack == 0 && name_is(kind, "WAY") && g_way_loaded) {
		g_packs[0].way_resident = 1;
		return 1;
	}
	const int n = try_read_pack_blob(pack, kind, g_load_buf, int(sizeof(g_load_buf)));
	if (n <= 0) {
		return 0;
	}
	if (name_is(kind, "ANIM")) {
		if (n < 8 || g_load_buf[0] != 'A' || g_load_buf[1] != 'N' || g_load_buf[2] != 'I' || g_load_buf[3] != 'M' || ru16(g_load_buf + 4) != PS1_COOK_ABI) {
			return 0;
		}
		const int nc = int(ru16(g_load_buf + 6));
		const uint8_t *p = g_load_buf + 8;
		const uint8_t *end = g_load_buf + n;
		int added = 0;
		for (int i = 0; i < nc && g_nclip < PS1_MAX_CLIPS && p < end; i++) {
			ScriptVMAnimClip clip{};
			const uint8_t sl = *p++;
			uint8_t cpy = sl < 31 ? sl : 31;
			for (uint8_t k = 0; k < cpy && p + k < end; k++) {
				clip.name[k] = char(p[k]);
			}
			clip.name[cpy] = 0;
			p += sl;
			if (p >= end) {
				break;
			}
			clip.nkeys = *p++;
			if (clip.nkeys > PS1_MAX_KEYS) {
				clip.nkeys = PS1_MAX_KEYS;
			}
			clip.loop = 0;
			for (uint8_t k = 0; k < clip.nkeys && p + 16 <= end; k++) {
				ScriptVMAnimKey key{};
				key.t_ms = uint16_t(p[0] | (p[1] << 8));
				key.node_id = p[2];
				key.flags = p[3];
				key.px = int16_t(p[4] | (p[5] << 8));
				key.py = int16_t(p[6] | (p[7] << 8));
				key.pz = int16_t(p[8] | (p[9] << 8));
				key.rx = int16_t(p[10] | (p[11] << 8));
				key.ry = int16_t(p[12] | (p[13] << 8));
				key.rz = int16_t(p[14] | (p[15] << 8));
				p += 16;
				clip.keys[k] = key;
				if (key.flags & 2) {
					clip.loop = 1;
				}
			}
			g_clips[g_nclip++] = clip;
			added++;
		}
		if (!added && nc > 0) {
			return 0;
		}
		g_packs[pack].anim_resident = 1;
		return 1;
	}
	if (name_is(kind, "MUSIC")) {
		if (g_host && g_host->load_music) {
			if (!g_host->load_music(g_load_buf, n)) {
				return 0;
			}
		}
		if (!g_packs[pack].music_resident) {
			bump_ram(32768);
		}
		g_packs[pack].music_resident = 1;
		return 1;
	}
	if (name_is(kind, "SPRITE")) {
		if (n >= 4) {
			const int nc = int(g_load_buf[0] | (g_load_buf[1] << 8));
			const uint8_t *p = g_load_buf + 4;
			g_nspr = 0;
			for (int i = 0; i < nc && g_nspr < PS1_MAX_SPRITES && p + 14 <= g_load_buf + n; i++) {
				ScriptVMSprite s{};
				s.x = int16_t(p[0] | (p[1] << 8));
				s.y = int16_t(p[2] | (p[3] << 8));
				s.w = uint16_t(p[4] | (p[5] << 8));
				s.h = uint16_t(p[6] | (p[7] << 8));
				s.u = p[8];
				s.v = p[9];
				s.tex = uint16_t(p[10] | (p[11] << 8));
				s.frame = p[12];
				s.nframes = p[13] ? p[13] : 1;
				s.fps = p[14];
				s.billboard = p[15];
				s.pack = uint8_t(pack);
				s.rgb = 255;
				p += 16;
				g_sprs[g_nspr++] = s;
			}
		}
		g_packs[pack].sprite_resident = 1;
		return 1;
	}
	if (name_is(kind, "TXT")) {
		if (n >= 8 && g_load_buf[0] == 'T' && g_load_buf[1] == 'X' && g_load_buf[2] == 'T' && g_load_buf[3] == '0' && ru16(g_load_buf + 4) == PS1_COOK_ABI) {
			const int nc = int(ru16(g_load_buf + 6));
			const uint8_t *p = g_load_buf + 8;
			g_ntxt = 0;
			for (int i = 0; i < nc && g_ntxt < PS1_MAX_TXT && p < g_load_buf + n; i++) {
				ScriptVMTextLine t{};
				const uint8_t nl = *p++;
				uint8_t cpy = nl < 31 ? nl : 31;
				for (uint8_t k = 0; k < cpy && p + k < g_load_buf + n; k++) {
					t.name[k] = char(p[k]);
				}
				t.name[cpy] = 0;
				p += nl;
				if (p >= g_load_buf + n) {
					break;
				}
				const uint8_t tl = *p++;
				uint8_t tc = tl < 63 ? tl : 63;
				for (uint8_t k = 0; k < tc && p + k < g_load_buf + n; k++) {
					t.text[k] = char(p[k]);
				}
				t.text[tc] = 0;
				p += tl;
				g_txt_pack[g_ntxt] = uint8_t(pack);
				g_txt[g_ntxt++] = t;
			}
			g_packs[pack].text_resident = 1;
			return 1;
		}
		return 0;
	}
	if (name_is(kind, "SFX")) {
		if (g_host && g_host->load_sfx_bank) {
			if (!g_host->load_sfx_bank(g_load_buf, n)) {
				return 0;
			}
		}
		g_packs[pack].audio_resident = 1;
		return 1;
	}
	if (name_is(kind, "HIT")) {
		if (g_load_buf[0] == 'H' && g_load_buf[1] == 'I' && g_load_buf[2] == 'T' && g_load_buf[3] == '0' && ru16(g_load_buf + 4) == PS1_COOK_ABI) {
			const int nc = int(ru16(g_load_buf + 6));
			const uint8_t *p = g_load_buf + 8;
			for (int i = 0; i < nc && g_nhit < PS1_MAX_HITS && p + 18 <= g_load_buf + n; i++) {
				ScriptVMHit h{};
				h.node_id = int16_t(p[0] | (p[1] << 8));
				h.dim = p[2];
				h.kind = p[3];
				h.flags = p[4];
				h.layer = p[5] ? p[5] : 1;
				h.min_x = int16_t(p[6] | (p[7] << 8));
				h.min_y = int16_t(p[8] | (p[9] << 8));
				h.min_z = int16_t(p[10] | (p[11] << 8));
				h.max_x = int16_t(p[12] | (p[13] << 8));
				h.max_y = int16_t(p[14] | (p[15] << 8));
				h.max_z = int16_t(p[16] | (p[17] << 8));
				h.pack = uint8_t(pack);
				p += 18;
				g_hits[g_nhit] = h;
				g_hit_used[g_nhit] = 1;
				g_nhit++;
			}
			g_packs[pack].hit_resident = 1;
			return 1;
		}
		return 0;
	}
	if (name_is(kind, "CAM")) {
		if (g_load_buf[0] == 'C' && g_load_buf[1] == 'A' && g_load_buf[2] == 'M' && g_load_buf[3] == '0' && ru16(g_load_buf + 4) == PS1_COOK_ABI) {
			const int nc = int(ru16(g_load_buf + 6));
			const uint8_t *p = g_load_buf + 8;
			const uint8_t *end = g_load_buf + n;
			for (int i = 0; i < nc && g_ncam < PS1_MAX_CAMS && p < end; i++) {
				ScriptVMCam c{};
				c.node_id = int16_t(p[0] | (p[1] << 8));
				p += 2;
				const uint8_t sl = *p++;
				uint8_t cpy = sl < 31 ? sl : 31;
				for (uint8_t k = 0; k < cpy && p + k < end; k++) {
					c.name[k] = char(p[k]);
				}
				c.name[cpy] = 0;
				p += sl;
				if (p + 24 > end) {
					break;
				}
				c.is_default = *p++;
				c.px = int16_t(p[0] | (p[1] << 8));
				c.py = int16_t(p[2] | (p[3] << 8));
				c.pz = int16_t(p[4] | (p[5] << 8));
				c.rx = int16_t(p[6] | (p[7] << 8));
				c.ry = int16_t(p[8] | (p[9] << 8));
				c.rz = int16_t(p[10] | (p[11] << 8));
				p += 12;
				c.dim = *p++;
				c.drag = *p++;
				c.dead = *p++;
				c.lim_l = int16_t(p[0] | (p[1] << 8));
				c.lim_t = int16_t(p[2] | (p[3] << 8));
				c.lim_r = int16_t(p[4] | (p[5] << 8));
				c.lim_b = int16_t(p[6] | (p[7] << 8));
				p += 8;
				c.pack = uint8_t(pack);
				g_cams[g_ncam++] = c;
			}
			g_packs[pack].cam_resident = 1;
			return 1;
		}
		return 0;
	}
	if (name_is(kind, "NAV")) {
		if (n < 8 || g_load_buf[0] != 'N' || g_load_buf[1] != 'A' || g_load_buf[2] != 'V' || g_load_buf[3] != '0' || ru16(g_load_buf + 4) != PS1_COOK_ABI) {
			return 0;
		}
		g_nvert = int(g_load_buf[6]);
		g_nedge = int(g_load_buf[7]);
		if (g_nvert > PS1_MAX_NAV_V) {
			g_nvert = PS1_MAX_NAV_V;
		}
		if (g_nedge > PS1_MAX_NAV_E) {
			g_nedge = PS1_MAX_NAV_E;
		}
		const uint8_t *p = g_load_buf + 8;
		for (int i = 0; i < g_nvert && p + 8 <= g_load_buf + n; i++) {
			g_nav_x[i] = int16_t(p[0] | (p[1] << 8));
			g_nav_y[i] = int16_t(p[2] | (p[3] << 8));
			g_nav_z[i] = int16_t(p[4] | (p[5] << 8));
			g_nav_node[i] = int16_t(p[6] | (p[7] << 8));
			p += 8;
		}
		for (int i = 0; i < g_nedge && p + 2 <= g_load_buf + n; i++) {
			g_nav_a[i] = p[0];
			g_nav_b[i] = p[1];
			p += 2;
		}
		g_nav_loaded = 1;
		g_packs[pack].nav_resident = 1;
		return 1;
	}
	if (name_is(kind, "PATH")) {
		if (n < 8 || g_load_buf[0] != 'P' || g_load_buf[1] != 'A' || g_load_buf[2] != 'T' || g_load_buf[3] != 'H' || ru16(g_load_buf + 4) != PS1_COOK_ABI) {
			return 0;
		}
		g_npath = int(g_load_buf[6]);
		if (g_npath > PS1_MAX_PATHS) {
			g_npath = PS1_MAX_PATHS;
		}
		const uint8_t *p = g_load_buf + 8;
		for (int i = 0; i < g_npath && p + 4 <= g_load_buf + n; i++) {
			g_pnpt[i] = p[1];
			g_pclosed[i] = p[2];
			if (g_pnpt[i] > PS1_MAX_PPTS) {
				g_pnpt[i] = PS1_MAX_PPTS;
			}
			p += 4;
			for (int k = 0; k < PS1_MAX_PPTS && p + 6 <= g_load_buf + n; k++) {
				g_ppx[i][k] = int16_t(p[0] | (p[1] << 8));
				g_ppy[i][k] = int16_t(p[2] | (p[3] << 8));
				g_ppz[i][k] = int16_t(p[4] | (p[5] << 8));
				p += 6;
			}
		}
		g_path_loaded = 1;
		g_packs[pack].path_resident = 1;
		return 1;
	}
	if (name_is(kind, "WAY")) {
		if (n < 8 || g_load_buf[0] != 'W' || g_load_buf[1] != 'A' || g_load_buf[2] != 'Y' || g_load_buf[3] != '0' || ru16(g_load_buf + 4) != PS1_COOK_ABI) {
			return 0;
		}
		g_nway = int(g_load_buf[6]);
		if (g_nway > PS1_MAX_WAYS) {
			g_nway = PS1_MAX_WAYS;
		}
		const uint8_t *p = g_load_buf + 8;
		for (int i = 0; i < g_nway && p + 4 <= g_load_buf + n; i++) {
			g_way_n[i] = p[0];
			g_way_p[i] = p[1];
			p += 4;
		}
		g_way_loaded = 1;
		g_packs[pack].way_resident = 1;
		return 1;
	}
	return 0;
}

static int unload_pack_slice(int pack, const char *kind) {
	if (pack < 0 || pack >= g_npack) {
		return 0;
	}
	if (pack == 0 && !name_is(kind, "TXT") && !name_is(kind, "SFX") && !name_is(kind, "MUSIC")) {
		return 0;
	}
	if (name_is(kind, "ANIM")) {
		g_packs[pack].anim_resident = 0;
		return 1;
	}
	if (name_is(kind, "SPRITE")) {
		g_packs[pack].sprite_resident = 0;
		return 1;
	}
	if (name_is(kind, "TXT")) {
		for (int i = 0; i < g_ntxt; i++) {
			if (g_txt_pack[i] == uint8_t(pack)) {
				g_txt[i].name[0] = 0;
			}
		}
		g_packs[pack].text_resident = 0;
		return 1;
	}
	if (name_is(kind, "SFX")) {
		if (g_host && g_host->unload_sfx_bank) {
			g_host->unload_sfx_bank();
		}
		g_packs[pack].audio_resident = 0;
		return 1;
	}
	if (name_is(kind, "HIT")) {
		for (int i = 0; i < g_nhit; i++) {
			if (g_hit_used[i] && g_hits[i].pack == uint8_t(pack)) {
				g_hit_used[i] = 0;
			}
		}
		g_packs[pack].hit_resident = 0;
		return 1;
	}
	if (name_is(kind, "CAM")) {
		if (g_cam_cur >= 0 && g_cam_cur < g_ncam && g_cams[g_cam_cur].pack == uint8_t(pack)) {
			return 0;
		}
		g_packs[pack].cam_resident = 0;
		return 1;
	}
	if (name_is(kind, "NAV")) {
		g_nav_loaded = 0;
		g_nvert = 0;
		g_nedge = 0;
		g_packs[pack].nav_resident = 0;
		return 1;
	}
	if (name_is(kind, "PATH")) {
		g_path_loaded = 0;
		g_npath = 0;
		g_packs[pack].path_resident = 0;
		return 1;
	}
	if (name_is(kind, "WAY")) {
		g_way_loaded = 0;
		g_nway = 0;
		g_packs[pack].way_resident = 0;
		return 1;
	}
	if (name_is(kind, "MUSIC")) {
		if (g_host && g_host->unload_music) {
			g_host->unload_music();
		}
		if (g_packs[pack].music_resident) {
			bump_ram(-32768);
		}
		g_packs[pack].music_resident = 0;
		return 1;
	}
	return 0;
}

static int mc_wrap_ok(const uint8_t *src, int n) {
	if (!src || n <= 0 || n > int(sizeof(g_mc_payload))) {
		return 0;
	}
	if (n >= 2 && src[0] == 'S' && src[1] == 'C') {
		const int pay = n - 512;
		if (pay <= 0 || pay > int(sizeof(g_mc_payload))) {
			return 0;
		}
		for (int i = 0; i < pay; i++) {
			g_mc_payload[i] = src[512 + i];
		}
		g_mc_len = pay;
		return 1;
	}
	for (int i = 0; i < n; i++) {
		g_mc_payload[i] = src[i];
	}
	g_mc_len = n;
	return 1;
}

static void mc_encode_var() {
	if (g_mc_var.type == V_STR) {
		copy_str((char *)g_mc_payload, 32, g_mc_var.s);
		g_mc_len = 0;
		while (g_mc_len < 31 && g_mc_payload[g_mc_len]) {
			g_mc_len++;
		}
		g_mc_len++;
		copy_str(g_mc_title, 32, g_mc_var.s);
		return;
	}
	if (g_mc_var.type == V_INT || g_mc_var.type == V_BOOL) {
		const int32_t v = g_mc_var.i;
		g_mc_payload[0] = uint8_t(v);
		g_mc_payload[1] = uint8_t(v >> 8);
		g_mc_payload[2] = uint8_t(v >> 16);
		g_mc_payload[3] = uint8_t(v >> 24);
		g_mc_len = 4;
		return;
	}
	if (g_mc_var.type == V_FLOAT) {
		union {
			float f;
			uint32_t u;
		} x;
		x.f = g_mc_var.f;
		g_mc_payload[0] = uint8_t(x.u);
		g_mc_payload[1] = uint8_t(x.u >> 8);
		g_mc_payload[2] = uint8_t(x.u >> 16);
		g_mc_payload[3] = uint8_t(x.u >> 24);
		g_mc_len = 4;
		return;
	}
	if (g_mc_var.type == V_V2 || g_mc_var.type == V_V3) {
		union {
			float f;
			uint32_t u;
		} x[3];
		x[0].f = g_mc_var.x;
		x[1].f = g_mc_var.y;
		x[2].f = g_mc_var.z;
		const int n = g_mc_var.type == V_V3 ? 3 : 2;
		for (int i = 0; i < n; i++) {
			g_mc_payload[i * 4] = uint8_t(x[i].u);
			g_mc_payload[i * 4 + 1] = uint8_t(x[i].u >> 8);
			g_mc_payload[i * 4 + 2] = uint8_t(x[i].u >> 16);
			g_mc_payload[i * 4 + 3] = uint8_t(x[i].u >> 24);
		}
		g_mc_len = n * 4;
	}
}

static void mc_persist() {
	uint8_t hdr[8];
	hdr[0] = 'M';
	hdr[1] = 'C';
	hdr[2] = '2';
	hdr[3] = '0';
	hdr[4] = g_mc_var.type;
	hdr[5] = uint8_t(g_mc_len & 0xff);
	hdr[6] = uint8_t((g_mc_len >> 8) & 0xff);
	hdr[7] = 0;
	write_host_file("SAVE.MCD", hdr, 8);
	if (g_mc_len > 0) {
		write_host_file("SAVE.BIN", g_mc_payload, g_mc_len);
	}
}

static int mc_off_ok(int off) {
	return off >= 0 && off <= 24572;
}

static void mc_poke32(int off, int32_t n) {
	if (!mc_off_ok(off)) {
		return;
	}
	g_mc_payload[off] = uint8_t(n);
	g_mc_payload[off + 1] = uint8_t(n >> 8);
	g_mc_payload[off + 2] = uint8_t(n >> 16);
	g_mc_payload[off + 3] = uint8_t(n >> 24);
	if (g_mc_len < off + 4) {
		g_mc_len = off + 4;
	}
	mc_persist();
}

static int32_t mc_peek32(int off) {
	if (!mc_off_ok(off)) {
		return 0;
	}
	return int32_t(uint32_t(g_mc_payload[off]) | (uint32_t(g_mc_payload[off + 1]) << 8) |
			(uint32_t(g_mc_payload[off + 2]) << 16) | (uint32_t(g_mc_payload[off + 3]) << 24));
}

static int part_free_slot() {
	for (int i = 0; i < PS1_MAX_PARTICLES; i++) {
		if (!g_parts[i].life) {
			return i;
		}
	}
	return -1;
}

static void part_birth(float x, float y, float z, float vx, float vy, float vz, int tex, int mode, int life) {
	const int i = part_free_slot();
	if (i < 0) {
		return;
	}
	g_parts[i].x = int16_t(x);
	g_parts[i].y = int16_t(-y);
	g_parts[i].z = int16_t(z);
	g_parts[i].px = g_parts[i].x;
	g_parts[i].py = g_parts[i].y;
	g_parts[i].pz = g_parts[i].z;
	g_parts[i].vx = int16_t(vx);
	g_parts[i].vy = int16_t(-vy);
	g_parts[i].vz = int16_t(vz);
	g_parts[i].life = uint8_t(life > 0 && life < 256 ? life : g_part_life);
	g_parts[i].tex = uint8_t(tex & 15);
	g_parts[i].r = g_part_r;
	g_parts[i].g = g_part_g;
	g_parts[i].b = g_part_b;
	g_parts[i].size = g_part_size ? g_part_size : 4;
	g_parts[i].mode = uint8_t(mode);
	g_parts[i].frame = 0;
	g_parts[i].nframes = g_part_nframes ? g_part_nframes : 1;
	g_parts[i].ftick = 0;
	g_parts[i].fps = g_part_fps ? g_part_fps : 8;
	if (g_npart < PS1_MAX_PARTICLES) {
		g_npart = PS1_MAX_PARTICLES;
	}
}

static int stream_of_node(int nid) {
	for (int i = 0; i < PS1_MAX_STREAMS; i++) {
		if (g_streams[i].used && g_streams[i].node == int16_t(nid)) {
			return i;
		}
	}
	return -1;
}

static void emit_mesh_samples(int nid, int count, int tex, int mode, int life) {
	if (!node_ok(nid) || count < 1) {
		return;
	}
	int lo = int(g_nodes[nid].tri_lo);
	int hi = int(g_nodes[nid].tri_hi);
	if (lo < 0) {
		lo = 0;
	}
	if (hi > g_ntri) {
		hi = g_ntri;
	}
	int born = 0;
	if (hi > lo && g_mesh[0] | g_mesh[1]) {
		for (int t = lo; t < hi && born < count; t++) {
			const uint8_t *v = g_mesh + 4 + size_t(t) * 32;
			const int16_t vx = int16_t(v[0] | (v[1] << 8));
			const int16_t vy = int16_t(v[2] | (v[3] << 8));
			const int16_t vz = int16_t(v[4] | (v[5] << 8));
			part_birth(float(g_nodes[nid].px + vx), float(-g_nodes[nid].py) + float(-vy), float(g_nodes[nid].pz + vz), 0, 0, 0, tex, mode, life);
			born++;
		}
	}
	while (born < count) {
		part_birth(float(g_nodes[nid].px), float(-g_nodes[nid].py), float(g_nodes[nid].pz), 0, 0, 0, tex, mode, life);
		born++;
	}
}

static void tick_anim(float delta) {
	if (!g_anim_playing || g_anim_clip < 0 || g_anim_clip >= g_nclip) {
		return;
	}
	g_anim_ms += delta * 1000.0f * (g_anim_speed != 0.0f ? g_anim_speed : 1.0f);
	const ScriptVMAnimClip *c = &g_clips[g_anim_clip];
	if (!c->nkeys) {
		g_anim_playing = 0;
		return;
	}
	uint16_t tmax = 0;
	for (int i = 0; i < c->nkeys; i++) {
		if (c->keys[i].t_ms > tmax) {
			tmax = c->keys[i].t_ms;
		}
	}
	if (g_anim_ms > float(tmax) && tmax > 0) {
		if (c->loop) {
			while (g_anim_ms > float(tmax)) {
				g_anim_ms -= float(tmax);
			}
		} else {
			g_anim_ms = float(tmax);
			if (g_anim_playing) {
				g_anim_just_finished = 1;
			}
			g_anim_playing = 0;
		}
	}
	const float t = g_anim_ms;
	for (int n = 0; n < g_nnode; n++) {
		if (!(g_nodes[n].flags & 1)) {
			continue;
		}
		int a = -1;
		int b = -1;
		for (int i = 0; i < c->nkeys; i++) {
			if (int(c->keys[i].node_id) != n) {
				continue;
			}
			if (float(c->keys[i].t_ms) <= t) {
				a = i;
			} else if (b < 0) {
				b = i;
			}
		}
		if (a < 0 && b < 0) {
			continue;
		}
		const ScriptVMAnimKey *ka = a >= 0 ? &c->keys[a] : &c->keys[b];
		const ScriptVMAnimKey *kb = b >= 0 ? &c->keys[b] : ka;
		float u = 0.0f;
		if (ka != kb && kb->t_ms > ka->t_ms) {
			u = (t - float(ka->t_ms)) / float(kb->t_ms - ka->t_ms);
			if (u < 0.0f) {
				u = 0.0f;
			}
			if (u > 1.0f) {
				u = 1.0f;
			}
		}
		g_nodes[n].px = int16_t(float(ka->px) + (float(kb->px) - float(ka->px)) * u);
		g_nodes[n].py = int16_t(float(ka->py) + (float(kb->py) - float(ka->py)) * u);
		g_nodes[n].pz = int16_t(float(ka->pz) + (float(kb->pz) - float(ka->pz)) * u);
		g_nodes[n].rx = int16_t(float(ka->rx) + (float(kb->rx) - float(ka->rx)) * u);
		g_nodes[n].ry = int16_t(float(ka->ry) + (float(kb->ry) - float(ka->ry)) * u);
		g_nodes[n].rz = int16_t(float(ka->rz) + (float(kb->rz) - float(ka->rz)) * u);
		if (u < 0.5f) {
			g_nodes[n].flags = uint8_t((g_nodes[n].flags & ~uint8_t(1)) | (ka->flags & 1));
		} else {
			g_nodes[n].flags = uint8_t((g_nodes[n].flags & ~uint8_t(1)) | (kb->flags & 1));
		}
	}
}

static int child_count_of(int parent) {
	int n = 0;
	for (int i = 0; i < g_nnode; i++) {
		if (g_nodes[i].parent == parent) {
			n++;
		}
	}
	return n;
}

static int child_at(int parent, int index) {
	int n = 0;
	for (int i = 0; i < g_nnode; i++) {
		if (g_nodes[i].parent != parent) {
			continue;
		}
		if (n == index) {
			return i;
		}
		n++;
	}
	return -1;
}

static int keyed_index(const GVar *key) {
	if (!key) {
		return -1;
	}
	if (key->type == V_STR) {
		if (name_is(key->s, "x")) {
			return 0;
		}
		if (name_is(key->s, "y")) {
			return 1;
		}
		if (name_is(key->s, "z")) {
			return 2;
		}
		return -1;
	}
	return int(as_float(*key));
}

static void get_keyed(const GVar *src, const GVar *key, GVar *dst) {
	if (!dst || !src) {
		return;
	}
	const int idx = keyed_index(key);
	if (src->type == V_V2 || src->type == V_V3) {
		if (idx == 0) {
			*dst = gv_float(src->x);
		} else if (idx == 1) {
			*dst = gv_float(src->y);
		} else if (idx == 2 && src->type == V_V3) {
			*dst = gv_float(src->z);
		} else {
			*dst = gv_nil();
		}
		return;
	}
	if (src->type == V_INT || src->type == V_FLOAT) {
		const int size = int(as_float(*src));
		if (idx >= 0 && idx < size) {
			*dst = gv_int(idx);
		} else {
			*dst = gv_nil();
		}
		return;
	}
	*dst = gv_nil();
}

static void set_keyed(GVar *dst, const GVar *key, const GVar *value) {
	if (!dst || !value) {
		return;
	}
	const int idx = keyed_index(key);
	const float f = as_float(*value);
	if (dst->type == V_V2 || dst->type == V_V3) {
		if (idx == 0) {
			dst->x = f;
		} else if (idx == 1) {
			dst->y = f;
		} else if (idx == 2 && dst->type == V_V3) {
			dst->z = f;
		}
	}
}

static int apply_call(const ScriptVMHost *host, int node, const char *name, float arg, const GVar *argv, int argc, GVar *ret) {
	if (name_is(name, "attach")) {
		return apply_call(host, node, "attach_camera", arg, argv, argc, ret);
	}
	if (name_is(name, "look")) {
		return apply_call(host, node, "look_camera", arg, argv, argc, ret);
	}
	if (name_is(name, "orbit")) {
		return apply_call(host, node, "orbit_camera", arg, argv, argc, ret);
	}
	if (name_is(name, "shake")) {
		return apply_call(host, node, "shake_camera", arg, argv, argc, ret);
	}
	if (name_is(name, "set_limits")) {
		return apply_call(host, node, "set_camera_limits", arg, argv, argc, ret);
	}
	if (name_is(name, "set_drag")) {
		return apply_call(host, node, "set_camera_drag", arg, argv, argc, ret);
	}
	if (name_is(name, "has_feature")) {
		const char *f = argv && argc > 0 && argv[0].type == V_STR ? argv[0].s : "";
		*ret = gv_bool(name_is(f, "ps1"));
		return 1;
	}
	if ((name_is(name, "get_position") || name_is(name, "get_global_position") || name_is(name, "get_global_rotation") || name_is(name, "get_rotation")) && node_ok(node)) {
		const float s = (2.0f * 3.14159265f) / 4096.0f;
		if (name_is(name, "get_global_position") || name_is(name, "get_global_rotation")) {
			float px, py, pz, rx, ry, rz;
			node_world(node, &px, &py, &pz, &rx, &ry, &rz);
			if (name_is(name, "get_global_rotation")) {
				*ret = gv_v3(rx * s, ry * s, rz * s);
			} else {
				*ret = gv_v3(px, -py, pz);
			}
			return 1;
		}
		if (name_is(name, "get_rotation")) {
			*ret = gv_v3(float(g_nodes[node].rx) * s, float(g_nodes[node].ry) * s, float(g_nodes[node].rz) * s);
			return 1;
		}
		*ret = gv_v3(float(g_nodes[node].px), float(-g_nodes[node].py), float(g_nodes[node].pz));
		return 1;
	}
	if (name_is(name, "abs")) {
		*ret = gv_float(util_abs(argc > 0 ? as_float(argv[0]) : arg));
		return 1;
	}
	if (name_is(name, "min") && argc >= 2) {
		const float a = as_float(argv[0]);
		const float b = as_float(argv[1]);
		*ret = gv_float(a < b ? a : b);
		return 1;
	}
	if (name_is(name, "max") && argc >= 2) {
		const float a = as_float(argv[0]);
		const float b = as_float(argv[1]);
		*ret = gv_float(a > b ? a : b);
		return 1;
	}
	if (name_is(name, "clamp") && argc >= 3) {
		float x = as_float(argv[0]);
		const float lo = as_float(argv[1]);
		const float hi = as_float(argv[2]);
		if (x < lo) {
			x = lo;
		}
		if (x > hi) {
			x = hi;
		}
		*ret = gv_float(x);
		return 1;
	}
	if (name_is(name, "lerp") && argc >= 3) {
		const float a = as_float(argv[0]);
		const float b = as_float(argv[1]);
		const float t = as_float(argv[2]);
		*ret = gv_float(a + (b - a) * t);
		return 1;
	}
	if (name_is(name, "sin")) {
		*ret = gv_float(util_sin(argc > 0 ? as_float(argv[0]) : arg));
		return 1;
	}
	if (name_is(name, "cos")) {
		*ret = gv_float(util_sin((argc > 0 ? as_float(argv[0]) : arg) + 1.5707963f));
		return 1;
	}
	if (name_is(name, "floor")) {
		*ret = gv_float(util_floor(argc > 0 ? as_float(argv[0]) : arg));
		return 1;
	}
	if (name_is(name, "ceil")) {
		*ret = gv_float(util_ceil(argc > 0 ? as_float(argv[0]) : arg));
		return 1;
	}
	if (name_is(name, "round")) {
		const float x = argc > 0 ? as_float(argv[0]) : arg;
		*ret = gv_float(util_floor(x + 0.5f));
		return 1;
	}
	if (name_is(name, "int")) {
		*ret = gv_int(int(argc > 0 ? as_float(argv[0]) : arg));
		return 1;
	}
	if (name_is(name, "float")) {
		*ret = gv_float(argc > 0 ? as_float(argv[0]) : arg);
		return 1;
	}
	if (name_is(name, "str")) {
		GVar v = gv_nil();
		v.type = V_STR;
		if (argc > 0 && argv[0].type == V_STR) {
			copy_str(v.s, 32, argv[0].s);
		} else {
			const float fv = argc > 0 ? as_float(argv[0]) : arg;
			const int is_int = (argc > 0 && argv[0].type == V_INT) || (fv == float(int(fv)));
			if (is_int) {
				const int n = int(fv);
				int x = n < 0 ? -n : n;
				char tmp[16];
				int ti = 0;
				if (x == 0) {
					tmp[ti++] = '0';
				}
				while (x > 0 && ti < 14) {
					tmp[ti++] = char('0' + (x % 10));
					x /= 10;
				}
				int o = 0;
				if (n < 0) {
					v.s[o++] = '-';
				}
				while (ti > 0 && o < 31) {
					v.s[o++] = tmp[--ti];
				}
				v.s[o] = 0;
			} else {
				int neg = fv < 0;
				float a = neg ? -fv : fv;
				int whole = int(a);
				int frac = int((a - float(whole)) * 100.0f + 0.5f);
				if (frac >= 100) {
					whole++;
					frac = 0;
				}
				char tmp[16];
				int ti = 0;
				int w = whole;
				if (w == 0) {
					tmp[ti++] = '0';
				}
				while (w > 0 && ti < 8) {
					tmp[ti++] = char('0' + (w % 10));
					w /= 10;
				}
				int o = 0;
				if (neg) {
					v.s[o++] = '-';
				}
				while (ti > 0 && o < 12) {
					v.s[o++] = tmp[--ti];
				}
				if (o < 12) {
					v.s[o++] = '.';
				}
				if (o < 12) {
					v.s[o++] = char('0' + (frac / 10));
				}
				if (o < 12) {
					v.s[o++] = char('0' + (frac % 10));
				}
				v.s[o] = 0;
			}
		}
		*ret = v;
		return 1;
	}
	if (name_is(name, "sign")) {
		*ret = gv_float(util_sign(argc > 0 ? as_float(argv[0]) : arg));
		return 1;
	}
	if (name_is(name, "sqrt")) {
		*ret = gv_float(util_sqrt(argc > 0 ? as_float(argv[0]) : arg));
		return 1;
	}
	if (name_is(name, "pow") && argc >= 2) {
		*ret = gv_float(util_pow(as_float(argv[0]), as_float(argv[1])));
		return 1;
	}
	if (name_is(name, "move_toward") && argc >= 3) {
		const float a = as_float(argv[0]);
		const float b = as_float(argv[1]);
		const float d = as_float(argv[2]);
		const float diff = b - a;
		if (util_abs(diff) <= d) {
			*ret = gv_float(b);
		} else {
			*ret = gv_float(a + util_sign(diff) * d);
		}
		return 1;
	}
	if (name_is(name, "deg_to_rad")) {
		*ret = gv_float((argc > 0 ? as_float(argv[0]) : arg) * 0.0174532925f);
		return 1;
	}
	if (name_is(name, "rad_to_deg")) {
		*ret = gv_float((argc > 0 ? as_float(argv[0]) : arg) * 57.2957795f);
		return 1;
	}
	if (name_is(name, "atan2") && argc >= 2) {
		*ret = gv_float(util_atan2(as_float(argv[0]), as_float(argv[1])));
		return 1;
	}
	if (name_is(name, "length") || name_is(name, "length_squared")) {
		float x = 0, y = 0, z = 0;
		if (argv && argc > 0 && (argv[0].type == V_V2 || argv[0].type == V_V3)) {
			x = argv[0].x;
			y = argv[0].y;
			z = argv[0].type == V_V3 ? argv[0].z : 0;
		} else if (argc >= 2) {
			x = as_float(argv[0]);
			y = as_float(argv[1]);
			z = argc > 2 ? as_float(argv[2]) : 0;
		}
		const float ls = x * x + y * y + z * z;
		*ret = gv_float(name_is(name, "length_squared") ? ls : util_sqrt(ls));
		return 1;
	}
	if (name_is(name, "normalized")) {
		float x = 0, y = 0, z = 0;
		int v3 = 0;
		if (argv && argc > 0 && (argv[0].type == V_V2 || argv[0].type == V_V3)) {
			x = argv[0].x;
			y = argv[0].y;
			z = argv[0].type == V_V3 ? argv[0].z : 0;
			v3 = argv[0].type == V_V3;
		}
		const float ls = util_sqrt(x * x + y * y + z * z);
		if (ls > 0.0001f) {
			x /= ls;
			y /= ls;
			z /= ls;
		}
		*ret = v3 ? gv_v3(x, y, z) : gv_v2(x, y);
		return 1;
	}
	if (name_is(name, "distance_to") && argc >= 2) {
		float ax = 0, ay = 0, az = 0, bx = 0, by = 0, bz = 0;
		if (argv[0].type == V_V3 || argv[0].type == V_V2) {
			ax = argv[0].x;
			ay = argv[0].y;
			az = argv[0].type == V_V3 ? argv[0].z : 0;
		}
		if (argv[1].type == V_V3 || argv[1].type == V_V2) {
			bx = argv[1].x;
			by = argv[1].y;
			bz = argv[1].type == V_V3 ? argv[1].z : 0;
		}
		const float dx = bx - ax, dy = by - ay, dz = bz - az;
		*ret = gv_float(util_sqrt(dx * dx + dy * dy + dz * dz));
		return 1;
	}
	if (name_is(name, "dot") && argc >= 2) {
		float ax = 0, ay = 0, az = 0, bx = 0, by = 0, bz = 0;
		if (argv[0].type == V_V3 || argv[0].type == V_V2) {
			ax = argv[0].x;
			ay = argv[0].y;
			az = argv[0].type == V_V3 ? argv[0].z : 0;
		}
		if (argv[1].type == V_V3 || argv[1].type == V_V2) {
			bx = argv[1].x;
			by = argv[1].y;
			bz = argv[1].type == V_V3 ? argv[1].z : 0;
		}
		*ret = gv_float(ax * bx + ay * by + az * bz);
		return 1;
	}
	if (name_is(name, "cross") && argc >= 2) {
		float ax = 0, ay = 0, az = 0, bx = 0, by = 0, bz = 0;
		if (argv[0].type == V_V3) {
			ax = argv[0].x;
			ay = argv[0].y;
			az = argv[0].z;
		}
		if (argv[1].type == V_V3) {
			bx = argv[1].x;
			by = argv[1].y;
			bz = argv[1].z;
		}
		*ret = gv_v3(ay * bz - az * by, az * bx - ax * bz, ax * by - ay * bx);
		return 1;
	}
	if (name_is(name, "wrapf") && argc >= 3) {
		float x = as_float(argv[0]);
		const float lo = as_float(argv[1]);
		const float hi = as_float(argv[2]);
		const float r = hi - lo;
		if (r != 0.0f) {
			x = x - r * util_floor((x - lo) / r);
		}
		*ret = gv_float(x);
		return 1;
	}
	if (name_is(name, "fmod") && argc >= 2) {
		const float a = as_float(argv[0]);
		const float b = as_float(argv[1]);
		*ret = gv_float(b != 0.0f ? a - b * util_floor(a / b) : 0);
		return 1;
	}
	if (name_is(name, "inverse_lerp") && argc >= 3) {
		const float a = as_float(argv[0]);
		const float b = as_float(argv[1]);
		const float v = as_float(argv[2]);
		*ret = gv_float((b != a) ? (v - a) / (b - a) : 0);
		return 1;
	}
	if (name_is(name, "randf")) {
		g_rng = g_rng * 1664525u + 1013904223u;
		*ret = gv_float(float(g_rng >> 8) * (1.0f / 16777216.0f));
		return 1;
	}
	if (name_is(name, "randi")) {
		g_rng = g_rng * 1664525u + 1013904223u;
		*ret = gv_int(int32_t(g_rng & 0x7fffffff));
		return 1;
	}
	if (name_is(name, "randf_range") && argc >= 2) {
		g_rng = g_rng * 1664525u + 1013904223u;
		const float u = float(g_rng >> 8) * (1.0f / 16777216.0f);
		const float a = as_float(argv[0]);
		const float b = as_float(argv[1]);
		*ret = gv_float(a + (b - a) * u);
		return 1;
	}
	if (name_is(name, "randi_range") && argc >= 2) {
		g_rng = g_rng * 1664525u + 1013904223u;
		int a = int(as_float(argv[0]));
		int b = int(as_float(argv[1]));
		if (b < a) {
			const int t = a;
			a = b;
			b = t;
		}
		const int span = b - a + 1;
		*ret = gv_int(a + (span > 0 ? int(g_rng % uint32_t(span)) : 0));
		return 1;
	}
	if (name_is(name, "get_vector")) {
		const char *nx = "ui_left";
		const char *px = "ui_right";
		const char *ny = "ui_up";
		const char *py = "ui_down";
		if (argv && argc >= 4) {
			if (argv[0].type == V_STR) {
				nx = argv[0].s;
			}
			if (argv[1].type == V_STR) {
				px = argv[1].s;
			}
			if (argv[2].type == V_STR) {
				ny = argv[2].s;
			}
			if (argv[3].type == V_STR) {
				py = argv[3].s;
			}
		}
		float x = 0.0f;
		float y = 0.0f;
		if (pad_pressed(host, px, 0)) {
			x += 1.0f;
		}
		if (pad_pressed(host, nx, 0)) {
			x -= 1.0f;
		}
		if (pad_pressed(host, py, 0)) {
			y += 1.0f;
		}
		if (pad_pressed(host, ny, 0)) {
			y -= 1.0f;
		}
		if (host) {
			const uint8_t *raws[2] = { host->pad34, host->pad34_1 };
			for (int d = 0; d < 2; d++) {
				if (!raws[d]) {
					continue;
				}
				const PADTYPE *pad = (const PADTYPE *)raws[d];
				if (pad->stat != 0 || (pad->type != PAD_ID_ANALOG && pad->type != PAD_ID_ANALOG_STICK)) {
					continue;
				}
				int sx = int(pad->ls_x) - 128;
				int sy = int(pad->ls_y) - 128;
				if (sx > -g_deadzone && sx < g_deadzone) {
					sx = 0;
				}
				if (sy > -g_deadzone && sy < g_deadzone) {
					sy = 0;
				}
				const float ax = float(sx) / 128.0f;
				const float ay = -float(sy) / 128.0f;
				if (ax < 0 && ax < x) {
					x = ax;
				}
				if (ax > 0 && ax > x) {
					x = ax;
				}
				if (ay < 0 && ay < y) {
					y = ay;
				}
				if (ay > 0 && ay > y) {
					y = ay;
				}
			}
		}
		*ret = gv_v2(x, y);
		return 1;
	}
	if (name_is(name, "get_joy_axis")) {
		int device = argv && argc > 0 ? int(as_float(argv[0])) : 0;
		int axis = argv && argc > 1 ? int(as_float(argv[1])) : 0;
		float v = 0.0f;
		if (host && (device == 0 || device == 1)) {
			const uint8_t *raw = device == 1 ? host->pad34_1 : host->pad34;
			if (raw) {
				const PADTYPE *pad = (const PADTYPE *)raw;
				if (pad->stat == 0 && (pad->type == PAD_ID_ANALOG || pad->type == PAD_ID_ANALOG_STICK)) {
					int a = 128;
					if (axis == 0) {
						a = int(pad->ls_x);
					} else if (axis == 1) {
						a = int(pad->ls_y);
					} else if (axis == 2) {
						a = int(pad->rs_x);
					} else if (axis == 3) {
						a = int(pad->rs_y);
					}
					int dlt = a - 128;
					if (dlt > -g_deadzone && dlt < g_deadzone) {
						dlt = 0;
					}
					v = float(dlt) / 128.0f;
					if (axis == 1 || axis == 3) {
						v = -v;
					}
				}
			}
		}
		*ret = gv_float(v);
		return 1;
	}
	if (name_is(name, "get_action_strength")) {
		const char *act = argv && argc > 0 && argv[0].type == V_STR ? argv[0].s : "";
		int axis = 0;
		for (int i = 0; i < g_naction; i++) {
			if (name_is(act, g_actions[i].name)) {
				axis = int(g_actions[i].axis);
				break;
			}
		}
		float v = 0.0f;
		if (axis > 0 && host && host->pad34) {
			const PADTYPE *pad = (const PADTYPE *)host->pad34;
			if (pad->stat == 0 && (pad->type == PAD_ID_ANALOG || pad->type == PAD_ID_ANALOG_STICK)) {
				int raw = 128;
				int sign = 1;
				if (axis == 1 || axis == 2) {
					raw = int(pad->ls_x);
					sign = axis == 1 ? 1 : -1;
				} else if (axis == 3 || axis == 4) {
					raw = int(pad->ls_y);
					sign = axis == 3 ? -1 : 1;
				} else if (axis == 5 || axis == 6) {
					raw = int(pad->rs_x);
					sign = axis == 5 ? 1 : -1;
				} else if (axis == 7 || axis == 8) {
					raw = int(pad->rs_y);
					sign = axis == 7 ? -1 : 1;
				} else if (axis == 9 || axis == 10) {
					v = pad_pressed(host, act, 0) ? 1.0f : 0.0f;
					raw = 128;
				}
				if (axis >= 1 && axis <= 8) {
					int dlt = raw - 128;
					if (dlt > -g_deadzone && dlt < g_deadzone) {
						dlt = 0;
					}
					v = float(dlt) / 128.0f * float(sign);
					if (v < 0.0f) {
						v = 0.0f;
					}
				}
			}
		}
		if (v == 0.0f && pad_pressed(host, act, 0)) {
			v = 1.0f;
		}
		*ret = gv_float(v);
		return 1;
	}
	if (name_is(name, "set_stick_deadzone")) {
		int z = argv && argc > 0 ? int(as_float(argv[0])) : int(arg);
		if (z < 0) {
			z = 0;
		}
		if (z > 127) {
			z = 127;
		}
		g_deadzone = z;
		*ret = gv_int(g_deadzone);
		return 1;
	}
	if (name_is(name, "get_velocity")) {
		int nid = node_ok(node) ? node : -1;
		if (argv && argc > 0) {
			nid = argv[0].type == V_OBJ ? argv[0].i : int(as_float(argv[0]));
		}
		if (node_ok(nid)) {
			*ret = gv_v3(g_nvel[nid][0], g_nvel[nid][1], g_nvel[nid][2]);
		} else {
			*ret = gv_v3(g_vel_x, g_vel_y, g_vel_z);
		}
		return 1;
	}
	if (name_is(name, "poke")) {
		const int off = argv && argc > 0 ? int(as_float(argv[0])) : int(arg);
		const int32_t n = argv && argc > 1 ? int32_t(as_float(argv[1])) : 0;
		if (mc_off_ok(off)) {
			mc_poke32(off, n);
			*ret = gv_bool(1);
		} else {
			*ret = gv_bool(0);
		}
		return 1;
	}
	if (name_is(name, "peek")) {
		const int off = argv && argc > 0 ? int(as_float(argv[0])) : int(arg);
		*ret = gv_int(mc_peek32(off));
		return 1;
	}
	if (name_is(name, "set_node_meta") || name_is(name, "set_meta")) {
		int nid = node;
		int16_t v = 0;
		if (argv && argc >= 2) {
			nid = argv[0].type == V_OBJ ? argv[0].i : int(as_float(argv[0]));
			v = int16_t(as_float(argv[1]));
		} else if (argv && argc == 1) {
			v = int16_t(as_float(argv[0]));
		}
		if (node_ok(nid)) {
			g_meta[nid] = v;
			*ret = gv_bool(1);
		} else {
			*ret = gv_bool(0);
		}
		return 1;
	}
	if (name_is(name, "get_node_meta") || name_is(name, "get_meta")) {
		int nid = node;
		if (argv && argc > 0) {
			nid = argv[0].type == V_OBJ ? argv[0].i : int(as_float(argv[0]));
		}
		*ret = gv_int(node_ok(nid) ? int(g_meta[nid]) : 0);
		return 1;
	}
	if (name_is(name, "set_angular_velocity")) {
		int nid = node;
		float ax = 0, ay = 0, az = 0;
		if (argv && argc >= 2 && argv[1].type == V_V3) {
			nid = argv[0].type == V_OBJ ? argv[0].i : int(as_float(argv[0]));
			ax = argv[1].x;
			ay = argv[1].y;
			az = argv[1].z;
		} else if (argv && argc >= 1 && argv[0].type == V_V3) {
			ax = argv[0].x;
			ay = argv[0].y;
			az = argv[0].z;
		}
		if (node_ok(nid)) {
			g_ang[nid][0] = ax;
			g_ang[nid][1] = ay;
			g_ang[nid][2] = az;
			*ret = gv_bool(1);
		} else {
			*ret = gv_bool(0);
		}
		return 1;
	}
	if (name_is(name, "get_angular_velocity")) {
		int nid = node;
		if (argv && argc > 0) {
			nid = argv[0].type == V_OBJ ? argv[0].i : int(as_float(argv[0]));
		}
		if (node_ok(nid)) {
			*ret = gv_v3(g_ang[nid][0], g_ang[nid][1], g_ang[nid][2]);
		} else {
			*ret = gv_v3(0, 0, 0);
		}
		return 1;
	}
	if (name_is(name, "set_particle_mode")) {
		g_part_mode = uint8_t(argv && argc > 0 ? int(as_float(argv[0])) : int(arg));
		if (g_part_mode > 4) {
			g_part_mode = 0;
		}
		*ret = gv_int(g_part_mode);
		return 1;
	}
	if (name_is(name, "set_particle_flipbook")) {
		if (argv && argc > 0) {
			g_part_nframes = uint8_t(int(as_float(argv[0])));
		}
		if (argv && argc > 1) {
			g_part_nframes = uint8_t(int(as_float(argv[1])));
		}
		if (argv && argc > 2) {
			g_part_fps = uint8_t(int(as_float(argv[2])));
		}
		if (!g_part_nframes) {
			g_part_nframes = 1;
		}
		if (!g_part_fps) {
			g_part_fps = 8;
		}
		g_part_mode = 1;
		*ret = gv_bool(1);
		return 1;
	}
	if (name_is(name, "set_particle_gravity")) {
		if (argv && argc > 0 && argv[0].type == V_V3) {
			g_grav_x = argv[0].x;
			g_grav_y = argv[0].y;
			g_grav_z = argv[0].z;
		}
		*ret = gv_bool(1);
		return 1;
	}
	if (name_is(name, "set_particle_color")) {
		if (argv && argc > 0 && argv[0].type == V_V3) {
			g_part_r = uint8_t(argv[0].x);
			g_part_g = uint8_t(argv[0].y);
			g_part_b = uint8_t(argv[0].z);
		}
		*ret = gv_bool(1);
		return 1;
	}
	if (name_is(name, "set_camera_limits")) {
		if (argv && argc >= 4) {
			g_cam_lim_live[0] = int16_t(as_float(argv[0]));
			g_cam_lim_live[1] = int16_t(as_float(argv[1]));
			g_cam_lim_live[2] = int16_t(as_float(argv[2]));
			g_cam_lim_live[3] = int16_t(as_float(argv[3]));
			g_cam_lim_set = 1;
		}
		*ret = gv_bool(1);
		return 1;
	}
	if (name_is(name, "set_camera_drag")) {
		int d = argv && argc > 0 ? int(as_float(argv[0])) : int(arg);
		if (d < 0) {
			d = 0;
		}
		if (d > 255) {
			d = 255;
		}
		g_cam_drag_live = d;
		*ret = gv_int(d);
		return 1;
	}
	if (name_is(name, "start_stream")) {
		int nid = node;
		int rate = 4;
		int tex = 0;
		if (argv && argc > 0) {
			nid = argv[0].type == V_OBJ ? argv[0].i : int(as_float(argv[0]));
		}
		if (argv && argc > 1) {
			rate = int(as_float(argv[1]));
		}
		if (argv && argc > 2) {
			tex = int(as_float(argv[2]));
		}
		if (!node_ok(nid)) {
			*ret = gv_bool(0);
			return 1;
		}
		int slot = stream_of_node(nid);
		if (slot < 0) {
			for (int i = 0; i < PS1_MAX_STREAMS; i++) {
				if (!g_streams[i].used) {
					slot = i;
					break;
				}
			}
		}
		if (slot < 0) {
			*ret = gv_bool(0);
			return 1;
		}
		if (rate < 1) {
			rate = 1;
		}
		if (rate > 16) {
			rate = 16;
		}
		g_streams[slot].used = 1;
		g_streams[slot].node = int16_t(nid);
		g_streams[slot].rate = uint8_t(rate);
		g_streams[slot].acc = 0;
		g_streams[slot].tex = uint8_t(tex & 15);
		g_streams[slot].mode = g_part_mode;
		g_streams[slot].life = g_part_life;
		g_streams[slot].nframes = g_part_nframes;
		g_streams[slot].fps = g_part_fps;
		g_streams[slot].vx = 0;
		g_streams[slot].vy = 0;
		g_streams[slot].vz = 0;
		g_streams[slot].spread = 0;
		g_streams[slot].ox = 0;
		g_streams[slot].oy = 0;
		g_streams[slot].oz = 0;
		*ret = gv_bool(1);
		return 1;
	}
	if (name_is(name, "stop_stream")) {
		int nid = node;
		if (argv && argc > 0) {
			nid = argv[0].type == V_OBJ ? argv[0].i : int(as_float(argv[0]));
		}
		const int slot = stream_of_node(nid);
		if (slot >= 0) {
			g_streams[slot].used = 0;
		}
		*ret = gv_bool(slot >= 0);
		return 1;
	}
	if (name_is(name, "set_stream_vel") || name_is(name, "set_stream_spread") || name_is(name, "set_stream_mode")) {
		int nid = node;
		int a1 = 1;
		if (argv && argc > 0 && (argv[0].type == V_OBJ || argv[0].type == V_INT || argv[0].type == V_FLOAT)) {
			if (argv[0].type != V_V3) {
				nid = argv[0].type == V_OBJ ? argv[0].i : int(as_float(argv[0]));
				a1 = 1;
			} else {
				a1 = 0;
			}
		} else {
			a1 = 0;
		}
		int slot = stream_of_node(nid);
		if (slot < 0) {
			*ret = gv_bool(0);
			return 1;
		}
		if (name_is(name, "set_stream_vel") && argv && argc > a1 && argv[a1].type == V_V3) {
			g_streams[slot].vx = int16_t(argv[a1].x);
			g_streams[slot].vy = int16_t(argv[a1].y);
			g_streams[slot].vz = int16_t(argv[a1].z);
		} else if (name_is(name, "set_stream_spread")) {
			int s = argv && argc > a1 ? int(as_float(argv[a1])) : 0;
			if (s < 0) {
				s = 0;
			}
			if (s > 255) {
				s = 255;
			}
			g_streams[slot].spread = uint8_t(s);
		} else if (name_is(name, "set_stream_mode")) {
			int m = argv && argc > a1 ? int(as_float(argv[a1])) : 0;
			if (m < 0 || m > 4) {
				m = 0;
			}
			g_streams[slot].mode = uint8_t(m);
		}
		*ret = gv_bool(1);
		return 1;
	}
	if (name_is(name, "emit_mesh")) {
		int nid = node;
		int count = 1;
		int tex = 0;
		if (argv && argc > 0) {
			nid = argv[0].type == V_OBJ ? argv[0].i : int(as_float(argv[0]));
		}
		if (argv && argc > 1) {
			count = int(as_float(argv[1]));
		}
		if (argv && argc > 2) {
			tex = int(as_float(argv[2]));
		}
		if (count < 1) {
			count = 1;
		}
		if (count > PS1_MAX_PARTICLES) {
			count = PS1_MAX_PARTICLES;
		}
		emit_mesh_samples(nid, count, tex, g_part_mode == 0 ? 4 : g_part_mode, g_part_life);
		*ret = gv_bool(1);
		return 1;
	}
	if (name_is(name, "get_speed")) {
		int nid = node;
		if (argv && argc > 0) {
			nid = argv[0].type == V_OBJ ? argv[0].i : int(as_float(argv[0]));
		}
		if (!node_ok(nid)) {
			*ret = gv_float(0);
			return 1;
		}
		const float vx = g_nvel[nid][0];
		const float vy = g_nvel[nid][1];
		const float vz = g_nvel[nid][2];
		float s2 = vx * vx + vy * vy + vz * vz;
		float s = 0;
		if (s2 > 0) {
			float x = s2;
			for (int i = 0; i < 8; i++) {
				x = 0.5f * (x + s2 / x);
			}
			s = x;
		}
		*ret = gv_float(s);
		return 1;
	}
	if (name_is(name, "set_drive_friction")) {
		int nid = node;
		float f = 0;
		if (argv && argc >= 2) {
			nid = argv[0].type == V_OBJ ? argv[0].i : int(as_float(argv[0]));
			f = as_float(argv[1]);
		} else if (argv && argc == 1) {
			f = as_float(argv[0]);
		}
		if (f < 0) {
			f = 0;
		}
		if (f > 1) {
			f = 1;
		}
		if (node_ok(nid)) {
			g_drive_fric[nid] = f;
			*ret = gv_bool(1);
		} else {
			*ret = gv_bool(0);
		}
		return 1;
	}
	if (name_is(name, "set_process") || name_is(name, "set_physics_process")) {
		if (node_ok(node)) {
			apply_method(host, node, name, arg, argv, argc);
		}
		*ret = gv_nil();
		return 1;
	}
	if (name_is(name, "world_to_screen")) {
		if (!host || !host->pos_x || !host->pos_y || !host->pos_z) {
			*ret = gv_nil();
			return 1;
		}
		float wx = 0, wy = 0, wz = 0;
		if (argv && argc > 0 && argv[0].type == V_V3) {
			wx = argv[0].x;
			wy = -argv[0].y;
			wz = argv[0].z;
		} else {
			*ret = gv_nil();
			return 1;
		}
		const float dx = wx - float(*host->pos_x);
		const float dy = wy - float(*host->pos_y);
		const float dz = wz - float(*host->pos_z);
		const float yaw = host->rot_y ? float(*host->rot_y) * (2.0f * 3.14159265f) / 4096.0f : 0.0f;
		const float pitch = host->rot_x ? float(*host->rot_x) * (2.0f * 3.14159265f) / 4096.0f : 0.0f;
		const float cy = util_sin(yaw + 1.5707963f);
		const float sy = util_sin(yaw);
		const float lx = dx * cy - dz * sy;
		const float lz = dx * sy + dz * cy;
		const float cp = util_sin(pitch + 1.5707963f);
		const float sp = util_sin(pitch);
		const float ly = dy * cp - lz * sp;
		const float lz2 = dy * sp + lz * cp;
		if (lz2 <= 1.0f) {
			*ret = gv_nil();
			return 1;
		}
		const float fov = (host->cam_scale && *host->cam_scale > 0) ? float(*host->cam_scale) : 160.0f;
		const float sh = host->region ? 256.0f : 240.0f;
		*ret = gv_v2(160.0f + lx * fov / lz2, sh * 0.5f - ly * fov / lz2);
		return 1;
	}
	if (name_is(name, "seek") || name_is(name, "seek_animation")) {
		g_anim_ms = (argv && argc > 0 ? as_float(argv[0]) : arg) * 1000.0f;
		if (g_anim_ms < 0.0f) {
			g_anim_ms = 0.0f;
		}
		*ret = gv_nil();
		return 1;
	}
	if (name_is(name, "set_animation_speed")) {
		g_anim_speed = argv && argc > 0 ? as_float(argv[0]) : arg;
		if (g_anim_speed == 0.0f) {
			g_anim_speed = 1.0f;
		}
		*ret = gv_nil();
		return 1;
	}
	if (name_is(name, "format_int")) {
		int n = argv && argc > 0 ? int(as_float(argv[0])) : int(arg);
		int width = argv && argc > 1 ? int(as_float(argv[1])) : 0;
		if (width > 16) {
			width = 16;
		}
		if (width < 0) {
			width = 0;
		}
		GVar v = gv_nil();
		v.type = V_STR;
		int neg = n < 0;
		int x = neg ? -n : n;
		char tmp[16];
		int ti = 0;
		if (x == 0) {
			tmp[ti++] = '0';
		}
		while (x > 0 && ti < 14) {
			tmp[ti++] = char('0' + (x % 10));
			x /= 10;
		}
		int digits = ti + (neg ? 1 : 0);
		int o = 0;
		while (digits < width && o < 15) {
			v.s[o++] = '0';
			digits++;
		}
		if (neg && o < 15) {
			v.s[o++] = '-';
		}
		while (ti > 0 && o < 15) {
			v.s[o++] = tmp[--ti];
		}
		v.s[o] = 0;
		*ret = v;
		return 1;
	}
	if (name_is(name, "set_hud_text")) {
		int hid = -1;
		const char *s = "";
		if (argv && argc > 0) {
			if (argv[0].type == V_OBJ) {
				hid = hud_index_for_node(argv[0].i);
			} else if (argv[0].type == V_STR) {
				s = argv[0].s;
				hid = node_ok(node) ? hud_index_for_node(node) : -1;
			} else {
				hid = hud_index_for_node(int(as_float(argv[0])));
			}
		} else if (node_ok(node)) {
			hid = hud_index_for_node(node);
		}
		if (argv && argc > 1 && argv[1].type == V_STR) {
			s = argv[1].s;
		}
		if (hid < 0 || hid >= g_nhud || !g_hud_used[hid]) {
			*ret = gv_bool(0);
			return 1;
		}
		copy_str(g_hud[hid].text, 64, s);
		*ret = gv_bool(1);
		return 1;
	}
	if (name_is(name, "load_music") || name_is(name, "unload_music") || name_is(name, "can_load_music") || name_is(name, "is_music_loaded")) {
		int pack = pack_id_of(node);
		if (argv && argc > 0) {
			if (argv[0].type == V_STR) {
				pack = find_pack_path(argv[0].s);
			} else if (argv[0].type == V_OBJ) {
				pack = pack_id_of(argv[0].i);
			} else {
				pack = int(as_float(argv[0]));
			}
		}
		if (name_is(name, "load_music")) {
			*ret = gv_bool(load_pack_slice(pack, "MUSIC"));
			return 1;
		}
		if (name_is(name, "unload_music")) {
			*ret = gv_bool(unload_pack_slice(pack, "MUSIC"));
			return 1;
		}
		if (pack < 0 || pack >= g_npack) {
			*ret = gv_bool(0);
			return 1;
		}
		if (name_is(name, "is_music_loaded")) {
			*ret = gv_bool(g_packs[pack].music_resident);
			return 1;
		}
		*ret = gv_bool(try_read_pack_blob(pack, "MUSIC", g_load_buf, 8) > 0 && budgets_fit(0, 0, 0, 0, 0, 32768));
		return 1;
	}
	if (name_is(name, "play_fmv")) {
		int pack = 0;
		if (argv && argc > 0) {
			if (argv[0].type == V_STR) {
				pack = find_pack_path(argv[0].s);
				if (pack < 0) {
					const char *p = argv[0].s;
					if (p[0] >= '0' && p[0] <= '9') {
						pack = int(as_float(argv[0]));
					}
				}
			} else {
				pack = int(as_float(argv[0]));
			}
		}
		if (pack < 0) {
			pack = 0;
		}
		{
			char iso[16];
			iso[0] = 'F';
			iso[1] = 'M';
			iso[2] = 'V';
			iso[3] = char('0' + (pack / 10) % 10);
			iso[4] = char('0' + pack % 10);
			iso[5] = '.';
			iso[6] = 'S';
			iso[7] = 'T';
			iso[8] = 'R';
			iso[9] = 0;
			if (host && host->play_fmv_cd && host->play_fmv_cd(iso)) {
				*ret = gv_bool(1);
				return 1;
			}
		}
		int n = try_read_pack_blob(pack, "STR", g_load_buf, int(sizeof(g_load_buf)));
		if (n <= 0) {
			char iso[16];
			iso[0] = 'F';
			iso[1] = 'M';
			iso[2] = 'V';
			iso[3] = char('0' + (pack / 10) % 10);
			iso[4] = char('0' + pack % 10);
			iso[5] = '.';
			iso[6] = 'S';
			iso[7] = 'T';
			iso[8] = 'R';
			iso[9] = 0;
			n = read_host_file(iso, g_load_buf, int(sizeof(g_load_buf)));
		}
		if (n <= 0) {
			*ret = gv_bool(0);
			return 1;
		}
		g_fmv_playing = 1;
		g_fmv_pack = pack;
		if (host && host->play_fmv_blob) {
			host->play_fmv_blob(g_load_buf, n);
		} else if (host && host->play_fmv) {
			host->play_fmv();
		}
		g_fmv_playing = 0;
		g_fmv_pack = -1;
		*ret = gv_bool(1);
		return 1;
	}
	if (name_is(name, "play_xa")) {
		int pack = 0;
		int file = 1;
		int chan = 0;
		char iso[16];
		iso[0] = 'X';
		iso[1] = 'A';
		iso[2] = '0';
		iso[3] = '0';
		iso[4] = '.';
		iso[5] = 'X';
		iso[6] = 'A';
		iso[7] = 0;
		if (argv && argc > 0) {
			if (argv[0].type == V_STR) {
				pack = find_pack_path(argv[0].s);
				if (pack < 0) {
					copy_str(iso, 16, argv[0].s);
					pack = 0;
				}
			} else {
				pack = int(as_float(argv[0]));
			}
		}
		if (argv && argc > 1) {
			file = int(as_float(argv[1]));
		}
		if (argv && argc > 2) {
			chan = int(as_float(argv[2]));
		}
		if (iso[0] == 'X' && iso[1] == 'A' && iso[4] == '.') {
			iso[2] = char('0' + (pack / 10) % 10);
			iso[3] = char('0' + pack % 10);
		}
		if (!host || !host->play_xa) {
			*ret = gv_bool(0);
			return 1;
		}
		*ret = gv_bool(host->play_xa(iso, file, chan));
		return 1;
	}
	if (name_is(name, "stop_xa")) {
		if (host && host->stop_xa) {
			host->stop_xa();
		}
		*ret = gv_bool(1);
		return 1;
	}
	if (name_is(name, "push_error")) {
		*ret = gv_nil();
		return 1;
	}
	if (name_is(name, "get_frames_per_second")) {
		*ret = gv_int(host && host->region ? 50 : 60);
		return 1;
	}
	if (name_is(name, "get_ticks_msec")) {
		*ret = gv_int(int(g_ticks_ms));
		return 1;
	}
	if (name_is(name, "create_timer")) {
		const float sec = argc > 0 ? as_float(argv[0]) : arg;
		int id = -1;
		for (int i = 0; i < 8; i++) {
			if (!g_timer_used[i]) {
				id = i;
				break;
			}
		}
		if (id < 0) {
			*ret = gv_nil();
			return 1;
		}
		g_timer_used[id] = 1;
		g_timer_end[id] = g_ticks_ms + sec * 1000.0f;
		*ret = gv_obj(kTimerBase + id);
		return 1;
	}
	if (name_is(name, "is_timer_done")) {
		int id = int(argc > 0 ? as_float(argv[0]) : arg);
		if (argc > 0 && argv[0].type == V_OBJ && argv[0].i >= kTimerBase) {
			id = argv[0].i - kTimerBase;
		}
		*ret = gv_bool(id >= 0 && id < 8 && g_timer_used[id] && g_ticks_ms >= g_timer_end[id]);
		return 1;
	}
	if (name_is(name, "get_tree")) {
		*ret = gv_obj(kTreeId);
		return 1;
	}
	if (node >= kTweenBase && node < kTweenBase + 8) {
		const int tid = node - kTweenBase;
		if (name_is(name, "tween_property")) {
			int nid = -1;
			const char *prop = "";
			float tx = 0, ty = 0, tz = 0;
			float sec = 1;
			int ai = 0;
			if (argv && argc > 0 && argv[0].type == V_OBJ) {
				nid = argv[0].i;
				ai = 1;
			} else if (node_ok(node)) {
				nid = node;
			}
			if (argv && argc > ai && argv[ai].type == V_STR) {
				prop = argv[ai].s;
				ai++;
			}
			if (argv && argc > ai && argv[ai].type == V_V3) {
				tx = argv[ai].x;
				ty = argv[ai].y;
				tz = argv[ai].z;
				ai++;
			} else if (argv && argc > ai) {
				tx = as_float(argv[ai]);
				ai++;
			}
			if (argv && argc > ai) {
				sec = as_float(argv[ai]);
			}
			if (sec < 0.016f) {
				sec = 0.016f;
			}
			g_tween[tid].used = 1;
			g_tween[tid].node = int16_t(nid);
			g_tween[tid].prop = name_is(prop, "rotation") ? 1 : (name_is(prop, "modulate") ? 2 : (name_is(prop, "velocity") ? 3 : 0));
			if (node_ok(nid)) {
				if (g_tween[tid].prop == 1) {
					g_tween[tid].from[0] = float(g_nodes[nid].rx);
					g_tween[tid].from[1] = float(g_nodes[nid].ry);
					g_tween[tid].from[2] = float(g_nodes[nid].rz);
				} else if (g_tween[tid].prop == 3) {
					g_tween[tid].from[0] = g_nvel[nid][0];
					g_tween[tid].from[1] = g_nvel[nid][1];
					g_tween[tid].from[2] = g_nvel[nid][2];
				} else {
					g_tween[tid].from[0] = float(g_nodes[nid].px);
					g_tween[tid].from[1] = float(-g_nodes[nid].py);
					g_tween[tid].from[2] = float(g_nodes[nid].pz);
				}
			}
			g_tween[tid].to[0] = tx;
			g_tween[tid].to[1] = ty;
			g_tween[tid].to[2] = tz;
			g_tween[tid].t = 0;
			g_tween[tid].dur = sec;
			*ret = gv_obj(kTweenBase + tid);
			return 1;
		}
		if (name_is(name, "stop") || name_is(name, "kill")) {
			g_tween[tid].used = 0;
			*ret = gv_bool(1);
			return 1;
		}
	}
	if (name_is(name, "create_tween")) {
		int id = -1;
		for (int i = 0; i < 8; i++) {
			if (!g_tween[i].used) {
				id = i;
				break;
			}
		}
		if (id < 0) {
			*ret = gv_nil();
			return 1;
		}
		g_tween[id].used = 1;
		g_tween[id].node = -1;
		g_tween[id].t = 0;
		g_tween[id].dur = 1;
		*ret = gv_obj(kTweenBase + id);
		return 1;
	}
	if (name_is(name, "tween_property")) {
		return apply_call(host, (argv && argc > 0 && argv[0].type == V_OBJ && argv[0].i >= kTweenBase) ? argv[0].i : kTweenBase, "tween_property", arg, argv, argc, ret);
	}
	if (name_is(name, "kill_tweens")) {
		int nid = node_ok(node) ? node : (argv && argc > 0 && argv[0].type == V_OBJ ? argv[0].i : -1);
		for (int i = 0; i < 8; i++) {
			if (g_tween[i].used && (nid < 0 || int(g_tween[i].node) == nid)) {
				g_tween[i].used = 0;
			}
		}
		*ret = gv_bool(1);
		return 1;
	}
	if (name_is(name, "tween_count")) {
		int n = 0;
		for (int i = 0; i < 8; i++) {
			if (g_tween[i].used) {
				n++;
			}
		}
		*ret = gv_int(n);
		return 1;
	}
	if (name_is(name, "get_forward") || name_is(name, "get_right") || name_is(name, "get_up")) {
		int nid = node_ok(node) ? node : (argv && argc > 0 && argv[0].type == V_OBJ ? argv[0].i : node);
		float fx, fy, fz, rx, ry, rz, ux, uy, uz;
		node_basis(nid, &fx, &fy, &fz, &rx, &ry, &rz, &ux, &uy, &uz);
		if (name_is(name, "get_right")) {
			*ret = gv_v3(rx, ry, rz);
		} else if (name_is(name, "get_up")) {
			*ret = gv_v3(ux, uy, uz);
		} else {
			*ret = gv_v3(fx, fy, fz);
		}
		return 1;
	}
	if (name_is(name, "translate_local")) {
		int nid = node;
		int ai = 0;
		if (argv && argc > 0 && argv[0].type == V_OBJ) {
			nid = argv[0].i;
			ai = 1;
		}
		float vx = 0, vy = 0, vz = 0;
		if (argv && argc > ai && argv[ai].type == V_V3) {
			vx = argv[ai].x;
			vy = argv[ai].y;
			vz = argv[ai].z;
		}
		if (!node_ok(nid)) {
			*ret = gv_bool(0);
			return 1;
		}
		float fx, fy, fz, rx, ry, rz, ux, uy, uz;
		node_basis(nid, &fx, &fy, &fz, &rx, &ry, &rz, &ux, &uy, &uz);
		g_nodes[nid].px = int16_t(g_nodes[nid].px + rx * vx + ux * vy + fx * vz);
		g_nodes[nid].py = int16_t(g_nodes[nid].py - (ry * vx + uy * vy + fy * vz));
		g_nodes[nid].pz = int16_t(g_nodes[nid].pz + rz * vx + uz * vy + fz * vz);
		*ret = gv_bool(1);
		return 1;
	}
	if (name_is(name, "move_and_fly")) {
		int nid = node;
		int ai = 0;
		if (argv && argc > 0 && argv[0].type == V_OBJ) {
			nid = argv[0].i;
			ai = 1;
		}
		const float throttle = argv && argc > ai ? as_float(argv[ai]) : 0;
		const float yaw = argv && argc > ai + 1 ? as_float(argv[ai + 1]) : 0;
		const float pitch = argv && argc > ai + 2 ? as_float(argv[ai + 2]) : 0;
		const float roll = argv && argc > ai + 3 ? as_float(argv[ai + 3]) : 0;
		if (!node_ok(nid)) {
			*ret = gv_v3(0, 0, 0);
			return 1;
		}
		g_nodes[nid].ry = int16_t(g_nodes[nid].ry + int16_t(rad_to_ps1(yaw)));
		g_nodes[nid].rx = int16_t(g_nodes[nid].rx + int16_t(rad_to_ps1(pitch)));
		g_nodes[nid].rz = int16_t(g_nodes[nid].rz + int16_t(rad_to_ps1(roll)));
		float fx, fy, fz, rx, ry, rz, ux, uy, uz;
		node_basis(nid, &fx, &fy, &fz, &rx, &ry, &rz, &ux, &uy, &uz);
		g_nvel[nid][0] = fx * throttle * 8.0f;
		g_nvel[nid][1] = fy * throttle * 8.0f;
		g_nvel[nid][2] = fz * throttle * 8.0f;
		return apply_call(host, nid, "move_and_slide", 0, nullptr, 0, ret);
	}
	if (name_is(name, "apply_gravity")) {
		int nid = node_ok(node) ? node : (argv && argc > 0 && argv[0].type == V_OBJ ? argv[0].i : node);
		if (node_ok(nid)) {
			g_nvel[nid][0] += g_body_gx;
			g_nvel[nid][1] += g_body_gy;
			g_nvel[nid][2] += g_body_gz;
		}
		*ret = gv_bool(node_ok(nid));
		return 1;
	}
	if (name_is(name, "set_gravity")) {
		if (argv && argc > 0 && argv[0].type == V_V3) {
			g_body_gx = argv[0].x;
			g_body_gy = argv[0].y;
			g_body_gz = argv[0].z;
		}
		*ret = gv_bool(1);
		return 1;
	}
	if (name_is(name, "add_velocity")) {
		int nid = node;
		int ai = 0;
		if (argv && argc > 0 && argv[0].type == V_OBJ) {
			nid = argv[0].i;
			ai = 1;
		}
		if (node_ok(nid) && argv && argc > ai && argv[ai].type == V_V3) {
			g_nvel[nid][0] += argv[ai].x;
			g_nvel[nid][1] += argv[ai].y;
			g_nvel[nid][2] += argv[ai].z;
		}
		*ret = gv_bool(node_ok(nid));
		return 1;
	}
	if (name_is(name, "follow_node")) {
		int self = node;
		int tgt = -1;
		float spd = 4;
		float tpx = 0, tpy = 0, tpz = 0;
		int have = 0;
		if (argv && argc >= 2 && argv[0].type == V_OBJ && argv[1].type == V_OBJ) {
			self = argv[0].i;
			tgt = argv[1].i;
			if (argc > 2) {
				spd = as_float(argv[2]);
			}
		} else if (argv && argc >= 2 && argv[0].type == V_OBJ && argv[1].type == V_V3) {
			self = argv[0].i;
			tpx = argv[1].x;
			tpy = argv[1].y;
			tpz = argv[1].z;
			have = 1;
			if (argc > 2) {
				spd = as_float(argv[2]);
			}
		} else if (argv && argc >= 1) {
			if (argv[0].type == V_V3) {
				tpx = argv[0].x;
				tpy = argv[0].y;
				tpz = argv[0].z;
				have = 1;
			} else {
				tgt = argv[0].type == V_OBJ ? argv[0].i : int(as_float(argv[0]));
			}
			if (argc > 1) {
				spd = as_float(argv[1]);
			}
		}
		if (!node_ok(self) || (!have && !node_ok(tgt))) {
			*ret = gv_bool(0);
			return 1;
		}
		if (!have) {
			tpx = float(g_nodes[tgt].px);
			tpy = float(-g_nodes[tgt].py);
			tpz = float(g_nodes[tgt].pz);
		}
		float dx = tpx - float(g_nodes[self].px);
		float dy = tpy - float(-g_nodes[self].py);
		float dz = tpz - float(g_nodes[self].pz);
		const float len = util_sqrt(dx * dx + dy * dy + dz * dz);
		if (len > 0.001f && len > spd) {
			dx = dx * spd / len;
			dy = dy * spd / len;
			dz = dz * spd / len;
		}
		g_nodes[self].px = int16_t(g_nodes[self].px + dx);
		g_nodes[self].py = int16_t(g_nodes[self].py - dy);
		g_nodes[self].pz = int16_t(g_nodes[self].pz + dz);
		*ret = gv_bool(1);
		return 1;
	}
	if (name_is(name, "raycast_group")) {
		float ox = 0, oy = 0, oz = 0, dx = 0, dy = 0, dz = 1, dist = 64;
		int bit = -1;
		if (argv && argc > 0 && argv[0].type == V_V3) {
			ox = argv[0].x;
			oy = argv[0].y;
			oz = argv[0].z;
		}
		if (argv && argc > 1 && argv[1].type == V_V3) {
			dx = argv[1].x;
			dy = argv[1].y;
			dz = argv[1].z;
		}
		if (argv && argc > 2) {
			dist = as_float(argv[2]);
		}
		if (argv && argc > 3) {
			bit = group_bit_arg(argv, argc, 3);
		}
		const int hitn = do_raycast(ox, oy, oz, dx, dy, dz, dist, 3, 0xFF, node_ok(node) ? node : -1, bit);
		*ret = hitn >= 0 ? gv_obj(hitn) : gv_nil();
		return 1;
	}
	if (name_is(name, "get_ray_point")) {
		*ret = g_ray_hit ? gv_v3(g_ray_hx, g_ray_hy, g_ray_hz) : gv_nil();
		return 1;
	}
	if (name_is(name, "get_ray_node")) {
		*ret = g_ray_node >= 0 ? gv_obj(g_ray_node) : gv_nil();
		return 1;
	}
	if (name_is(name, "spawn_shot")) {
		float ox = 0, oy = 0, oz = 0, vx = 0, vy = 0, vz = 0;
		float life = 30;
		int tex = 0;
		int owner = node_ok(node) ? node : -1;
		if (argv && argc > 0 && argv[0].type == V_V3) {
			ox = argv[0].x;
			oy = argv[0].y;
			oz = argv[0].z;
		}
		if (argv && argc > 1 && argv[1].type == V_V3) {
			vx = argv[1].x;
			vy = argv[1].y;
			vz = argv[1].z;
		}
		if (argv && argc > 2) {
			life = as_float(argv[2]);
		}
		if (argv && argc > 3) {
			tex = int(as_float(argv[3]));
		}
		int slot = -1;
		for (int i = 0; i < 32; i++) {
			if (!g_shot[i].used) {
				slot = i;
				break;
			}
		}
		if (slot < 0) {
			*ret = gv_int(-1);
			return 1;
		}
		g_shot[slot].used = 1;
		g_shot[slot].x = int16_t(ox);
		g_shot[slot].y = int16_t(-oy);
		g_shot[slot].z = int16_t(oz);
		g_shot[slot].vx = int16_t(vx);
		g_shot[slot].vy = int16_t(-vy);
		g_shot[slot].vz = int16_t(vz);
		g_shot[slot].life = uint8_t(life > 0 && life < 256 ? life : 30);
		g_shot[slot].tex = uint8_t(tex & 15);
		g_shot[slot].owner = int16_t(owner);
		g_shot[slot].hit = -1;
		*ret = gv_int(slot);
		return 1;
	}
	if (name_is(name, "clear_shots")) {
		for (int i = 0; i < 32; i++) {
			g_shot[i].used = 0;
			g_shot[i].hit = -1;
		}
		*ret = gv_bool(1);
		return 1;
	}
	if (name_is(name, "get_shot_count")) {
		int n = 0;
		for (int i = 0; i < 32; i++) {
			if (g_shot[i].used) {
				n++;
			}
		}
		*ret = gv_int(n);
		return 1;
	}
	if (name_is(name, "get_free_shots")) {
		int n = 0;
		for (int i = 0; i < 32; i++) {
			if (!g_shot[i].used) {
				n++;
			}
		}
		*ret = gv_int(n);
		return 1;
	}
	if (name_is(name, "shot_hit")) {
		const int i = argv && argc > 0 ? int(as_float(argv[0])) : -1;
		if (i < 0 || i >= 32 || g_shot[i].hit < 0 || kit_invuln(int(g_shot[i].hit))) {
			*ret = gv_nil();
		} else {
			*ret = gv_obj(int(g_shot[i].hit));
		}
		return 1;
	}
	if (name_is(name, "clip_orbit")) {
		g_orbit_clip = argv && argc > 0 ? as_truth(argv[0]) : 1;
		*ret = gv_bool(g_orbit_clip);
		return 1;
	}
	if (name_is(name, "set_y_sort")) {
		int nid = node;
		int on = 1;
		if (argv && argc >= 2) {
			nid = argv[0].type == V_OBJ ? argv[0].i : int(as_float(argv[0]));
			on = as_truth(argv[1]);
		} else if (argv && argc == 1) {
			on = as_truth(argv[0]);
		}
		if (node_ok(nid)) {
			g_ysort[nid] = uint8_t(on ? 1 : 0);
		}
		*ret = gv_bool(node_ok(nid));
		return 1;
	}
	if (name_is(name, "wrap_position")) {
		int nid = node;
		int ai = 0;
		if (argv && argc > 0 && argv[0].type == V_OBJ) {
			nid = argv[0].i;
			ai = 1;
		}
		if (!node_ok(nid) || argc < ai + 2) {
			*ret = gv_bool(0);
			return 1;
		}
		float mnx = 0, mny = 0, mnz = 0, mxx = 0, mxy = 0, mxz = 0;
		if (argv[ai].type == V_V3) {
			mnx = argv[ai].x;
			mny = argv[ai].y;
			mnz = argv[ai].z;
		} else if (argv[ai].type == V_V2) {
			mnx = argv[ai].x;
			mny = argv[ai].y;
		}
		if (argv[ai + 1].type == V_V3) {
			mxx = argv[ai + 1].x;
			mxy = argv[ai + 1].y;
			mxz = argv[ai + 1].z;
		} else if (argv[ai + 1].type == V_V2) {
			mxx = argv[ai + 1].x;
			mxy = argv[ai + 1].y;
		}
		float px = float(g_nodes[nid].px);
		float py = float(-g_nodes[nid].py);
		float pz = float(g_nodes[nid].pz);
		const float wx = mxx - mnx;
		const float wy = mxy - mny;
		const float wz = mxz - mnz;
		if (wx != 0) {
			while (px < mnx) {
				px += wx;
			}
			while (px > mxx) {
				px -= wx;
			}
		}
		if (wy != 0) {
			while (py < mny) {
				py += wy;
			}
			while (py > mxy) {
				py -= wy;
			}
		}
		if (wz != 0) {
			while (pz < mnz) {
				pz += wz;
			}
			while (pz > mxz) {
				pz -= wz;
			}
		}
		g_nodes[nid].px = int16_t(px);
		g_nodes[nid].py = int16_t(-py);
		g_nodes[nid].pz = int16_t(pz);
		*ret = gv_bool(1);
		return 1;
	}
	if (name_is(name, "set_cull_dist")) {
		g_cull_dist = argv && argc > 0 ? as_float(argv[0]) : 0;
		*ret = gv_bool(1);
		return 1;
	}
	if (name_is(name, "count_waypoints")) {
		int path = -1;
		if (argv && argc > 0) {
			path = int(as_float(argv[0]));
		}
		int n = 0;
		for (int i = 0; i < g_nway; i++) {
			if (path < 0 || int(g_way_p[i]) == path) {
				n++;
			}
		}
		*ret = gv_int(g_way_loaded ? n : 0);
		return 1;
	}
	if (name_is(name, "get_waypoint")) {
		int path = -1;
		int idx = 0;
		if (argv && argc >= 2) {
			path = int(as_float(argv[0]));
			idx = int(as_float(argv[1]));
		} else if (argv && argc == 1) {
			idx = int(as_float(argv[0]));
		}
		int n = 0;
		for (int i = 0; i < g_nway; i++) {
			if (path >= 0 && int(g_way_p[i]) != path) {
				continue;
			}
			if (n == idx) {
				*ret = gv_obj(int(g_way_n[i]));
				return 1;
			}
			n++;
		}
		*ret = gv_nil();
		return 1;
	}
	if (name_is(name, "get_waypoint_pos")) {
		const int idx = argv && argc > 0 ? int(as_float(argv[0])) : 0;
		if (idx < 0 || idx >= g_nway || !node_ok(int(g_way_n[idx]))) {
			*ret = gv_nil();
			return 1;
		}
		const int nid = int(g_way_n[idx]);
		*ret = gv_v3(float(g_nodes[nid].px), float(-g_nodes[nid].py), float(g_nodes[nid].pz));
		return 1;
	}
	if (name_is(name, "set_path_progress") || name_is(name, "get_path_progress")) {
		int nid = node;
		int ai = 0;
		if (argv && argc > 0 && argv[0].type == V_OBJ) {
			nid = argv[0].i;
			ai = 1;
		}
		if (name_is(name, "set_path_progress") && node_ok(nid) && argv && argc > ai) {
			g_path_prog[nid] = as_float(argv[ai]);
			if (g_path_prog[nid] < 0) {
				g_path_prog[nid] = 0;
			}
			if (g_path_prog[nid] > 1) {
				g_path_prog[nid] = 1;
			}
		}
		*ret = node_ok(nid) ? gv_float(g_path_prog[nid]) : gv_float(0);
		return 1;
	}
	if (name_is(name, "get_path_point")) {
		int path = argv && argc > 0 ? int(as_float(argv[0])) : 0;
		int i = argv && argc > 1 ? int(as_float(argv[1])) : 0;
		if (path < 0 || path >= g_npath || i < 0 || i >= int(g_pnpt[path]) || !g_path_loaded) {
			*ret = gv_nil();
			return 1;
		}
		*ret = gv_v3(float(g_ppx[path][i]), float(g_ppy[path][i]), float(g_ppz[path][i]));
		return 1;
	}
	if (name_is(name, "count_path_points")) {
		const int path = argv && argc > 0 ? int(as_float(argv[0])) : 0;
		*ret = gv_int(g_path_loaded && path >= 0 && path < g_npath ? int(g_pnpt[path]) : 0);
		return 1;
	}
	if (name_is(name, "nav_nearest") || name_is(name, "nav_next") || name_is(name, "nav_next_node")) {
		if (!g_nav_loaded || g_nvert <= 0) {
			*ret = gv_nil();
			return 1;
		}
		float fx = 0, fy = 0, fz = 0, tx = 0, ty = 0, tz = 0;
		if (argv && argc > 0 && argv[0].type == V_V3) {
			fx = argv[0].x;
			fy = argv[0].y;
			fz = argv[0].z;
		} else if (argv && argc > 0 && argv[0].type == V_OBJ && node_ok(argv[0].i)) {
			fx = float(g_nodes[argv[0].i].px);
			fy = float(-g_nodes[argv[0].i].py);
			fz = float(g_nodes[argv[0].i].pz);
		}
		if (argv && argc > 1 && argv[1].type == V_V3) {
			tx = argv[1].x;
			ty = argv[1].y;
			tz = argv[1].z;
		} else if (argv && argc > 1 && argv[1].type == V_OBJ && node_ok(argv[1].i)) {
			tx = float(g_nodes[argv[1].i].px);
			ty = float(-g_nodes[argv[1].i].py);
			tz = float(g_nodes[argv[1].i].pz);
		}
		int best = 0;
		float bd = 1e12f;
		for (int i = 0; i < g_nvert; i++) {
			const float dx = float(g_nav_x[i]) - fx;
			const float dy = float(g_nav_y[i]) - fy;
			const float dz = float(g_nav_z[i]) - fz;
			const float d = dx * dx + dy * dy + dz * dz;
			if (d < bd) {
				bd = d;
				best = i;
			}
		}
		if (name_is(name, "nav_nearest")) {
			*ret = gv_v3(float(g_nav_x[best]), float(g_nav_y[best]), float(g_nav_z[best]));
			return 1;
		}
		int goal = 0;
		float gd = 1e12f;
		for (int i = 0; i < g_nvert; i++) {
			const float dx = float(g_nav_x[i]) - tx;
			const float dy = float(g_nav_y[i]) - ty;
			const float dz = float(g_nav_z[i]) - tz;
			const float d = dx * dx + dy * dy + dz * dz;
			if (d < gd) {
				gd = d;
				goal = i;
			}
		}
		int nxt = goal;
		float nd = 1e12f;
		for (int e = 0; e < g_nedge; e++) {
			int oth = -1;
			if (int(g_nav_a[e]) == best) {
				oth = int(g_nav_b[e]);
			} else if (int(g_nav_b[e]) == best) {
				oth = int(g_nav_a[e]);
			}
			if (oth < 0 || oth >= g_nvert) {
				continue;
			}
			const float dx = float(g_nav_x[oth]) - tx;
			const float dy = float(g_nav_y[oth]) - ty;
			const float dz = float(g_nav_z[oth]) - tz;
			const float d = dx * dx + dy * dy + dz * dz;
			if (d < nd) {
				nd = d;
				nxt = oth;
			}
		}
		if (name_is(name, "nav_next_node")) {
			const int wn = int(g_nav_node[nxt]);
			*ret = (wn >= 0 && node_ok(wn)) ? gv_obj(wn) : gv_nil();
			return 1;
		}
		*ret = gv_v3(float(g_nav_x[nxt]), float(g_nav_y[nxt]), float(g_nav_z[nxt]));
		return 1;
	}
	if (name_is(name, "load_nav") || name_is(name, "unload_nav") || name_is(name, "can_load_nav") || name_is(name, "is_nav_loaded") ||
			name_is(name, "load_paths") || name_is(name, "unload_paths") || name_is(name, "can_load_paths") || name_is(name, "is_paths_loaded") ||
			name_is(name, "load_waypoints") || name_is(name, "unload_waypoints") || name_is(name, "can_load_waypoints") || name_is(name, "is_waypoints_loaded")) {
		int pack = 0;
		if (argv && argc > 0) {
			if (argv[0].type == V_STR) {
				pack = find_pack_path(argv[0].s);
			} else {
				pack = int(as_float(argv[0]));
			}
		}
		const char *kind = name_is(name, "load_nav") || name_is(name, "unload_nav") || name_is(name, "can_load_nav") || name_is(name, "is_nav_loaded") ? "NAV" : (name_is(name, "load_paths") || name_is(name, "unload_paths") || name_is(name, "can_load_paths") || name_is(name, "is_paths_loaded") ? "PATH" : "WAY");
		if (name_is(name, "load_nav") || name_is(name, "load_paths") || name_is(name, "load_waypoints")) {
			*ret = gv_bool(load_pack_slice(pack, kind));
			return 1;
		}
		if (name_is(name, "unload_nav") || name_is(name, "unload_paths") || name_is(name, "unload_waypoints")) {
			*ret = gv_bool(unload_pack_slice(pack, kind));
			return 1;
		}
		const int loaded = name_is(kind, "NAV") ? g_nav_loaded : (name_is(kind, "PATH") ? g_path_loaded : g_way_loaded);
		if (name_is(name, "is_nav_loaded") || name_is(name, "is_paths_loaded") || name_is(name, "is_waypoints_loaded")) {
			*ret = gv_bool(loaded);
			return 1;
		}
		*ret = gv_bool(try_read_pack_blob(pack, kind, g_load_buf, 8) > 0);
		return 1;
	}
	if (name_is(name, "unload_pack")) {
		return apply_call(host, node, "unload_scene", arg, argv, argc, ret);
	}
	if (name_is(name, "is_colliding") || name_is(name, "get_collider") || name_is(name, "get_collision_point") || name_is(name, "set_enabled")) {
		int nid = node_ok(node) ? node : (argv && argc > 0 && argv[0].type == V_OBJ ? argv[0].i : node);
		if (name_is(name, "set_enabled") && node_ok(nid)) {
			g_cast_on[nid] = argv && argc > 0 ? uint8_t(as_truth(argv[argc > 1 ? 1 : 0])) : 1;
			if (argv && argc == 1 && argv[0].type != V_OBJ) {
				g_cast_on[nid] = uint8_t(as_truth(argv[0]));
			}
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "is_colliding")) {
			*ret = gv_bool(node_ok(nid) && g_cast_hit[nid] >= 0);
			return 1;
		}
		if (name_is(name, "get_collider")) {
			*ret = (node_ok(nid) && g_cast_hit[nid] >= 0) ? gv_obj(int(g_cast_hit[nid])) : gv_nil();
			return 1;
		}
		*ret = g_ray_hit ? gv_v3(g_ray_hx, g_ray_hy, g_ray_hz) : gv_nil();
		return 1;
	}
	if (name_is(name, "is_on_screen")) {
		int nid = node_ok(node) ? node : (argv && argc > 0 && argv[0].type == V_OBJ ? argv[0].i : node);
		*ret = gv_bool(node_ok(nid) && g_on_screen[nid]);
		return 1;
	}
	if (name_is(name, "set_target_position")) {
		int nid = node;
		int ai = 0;
		if (argv && argc > 0 && argv[0].type == V_OBJ && argc > 1) {
			nid = argv[0].i;
			ai = 1;
		}
		if (node_ok(nid) && argv && argc > ai && argv[ai].type == V_V3) {
			g_agent_tx[nid] = argv[ai].x;
			g_agent_ty[nid] = argv[ai].y;
			g_agent_tz[nid] = argv[ai].z;
		}
		*ret = gv_bool(node_ok(nid));
		return 1;
	}
	if (name_is(name, "get_next_path_position")) {
		int nid = node_ok(node) ? node : (argv && argc > 0 && argv[0].type == V_OBJ ? argv[0].i : node);
		if (!node_ok(nid) || !g_nav_loaded) {
			*ret = gv_nil();
			return 1;
		}
		GVar a[2];
		a[0] = gv_v3(float(g_nodes[nid].px), float(-g_nodes[nid].py), float(g_nodes[nid].pz));
		a[1] = gv_v3(g_agent_tx[nid], g_agent_ty[nid], g_agent_tz[nid]);
		return apply_call(host, nid, "nav_next", 0, a, 2, ret);
	}
	if (name_is(name, "is_navigation_finished")) {
		int nid = node_ok(node) ? node : (argv && argc > 0 && argv[0].type == V_OBJ ? argv[0].i : node);
		if (!node_ok(nid)) {
			*ret = gv_bool(1);
			return 1;
		}
		const float dx = float(g_nodes[nid].px) - g_agent_tx[nid];
		const float dy = float(-g_nodes[nid].py) - g_agent_ty[nid];
		const float dz = float(g_nodes[nid].pz) - g_agent_tz[nid];
		*ret = gv_bool(dx * dx + dy * dy + dz * dz < 16);
		return 1;
	}
	if ((name_is(name, "start") || name_is(name, "stop") || name_is(name, "is_stopped")) && (node_ok(node) || (node >= kTimerBase && node < kTimerBase + 8))) {
		int tid = -1;
		if (node >= kTimerBase && node < kTimerBase + 8) {
			tid = node - kTimerBase;
		} else if (node_ok(node)) {
			for (int i = 0; i < 8; i++) {
				if (g_timer_node[i] == uint8_t(node)) {
					tid = i;
					break;
				}
			}
			if (tid < 0 && g_kind[node] == NK_TIMER) {
				for (int i = 0; i < 8; i++) {
					if (!g_timer_used[i] || g_timer_node[i] == 0xff) {
						tid = i;
						g_timer_node[i] = uint8_t(node);
						break;
					}
				}
			}
		}
		if (tid < 0) {
			*ret = gv_bool(name_is(name, "is_stopped") ? 1 : 0);
			return 1;
		}
		if (name_is(name, "start")) {
			const float sec = argv && argc > 0 ? as_float(argv[0]) : 1;
			g_timer_used[tid] = 1;
			g_timer_end[tid] = g_ticks_ms + sec * 1000.0f;
			*ret = gv_obj(kTimerBase + tid);
			return 1;
		}
		if (name_is(name, "stop")) {
			g_timer_used[tid] = 0;
			*ret = gv_bool(1);
			return 1;
		}
		*ret = gv_bool(!g_timer_used[tid] || g_ticks_ms >= g_timer_end[tid]);
		return 1;
	}
	if (name_is(name, "load_scene") || name_is(name, "unload_scene") || name_is(name, "is_scene_loaded") ||
			name_is(name, "can_load_scene") || name_is(name, "can_instantiate") ||
			name_is(name, "get_loaded_scene_count") || name_is(name, "get_loaded_scene") ||
			name_is(name, "get_static_memory_usage") || name_is(name, "get_static_memory_peak_usage") ||
			name_is(name, "get_ram_budget") || name_is(name, "get_free_ram") ||
			name_is(name, "get_node_count") || name_is(name, "get_node_budget") ||
			name_is(name, "get_triangle_count") || name_is(name, "get_triangle_budget") ||
			name_is(name, "get_free_nodes") || name_is(name, "get_free_hud") || name_is(name, "get_free_tiles") ||
			name_is(name, "set_camera") || name_is(name, "set_camera_node") || name_is(name, "set_default_camera") ||
			name_is(name, "get_camera") || name_is(name, "get_camera_count") || name_is(name, "get_camera_name") ||
			name_is(name, "is_camera_current") || name_is(name, "next_camera") || name_is(name, "prev_camera") ||
			name_is(name, "get_stick") || name_is(name, "set_rumble") || name_is(name, "stop_rumble") ||
			name_is(name, "set_camera_transform") || name_is(name, "get_camera_transform") || name_is(name, "get_camera_rotation") ||
			name_is(name, "attach_camera") || name_is(name, "look_camera") || name_is(name, "orbit_camera") ||
			name_is(name, "attach") || name_is(name, "look") || name_is(name, "orbit") || name_is(name, "shake") ||
			name_is(name, "set_limits") || name_is(name, "set_drag") || name_is(name, "make_current") ||
			name_is(name, "set_camera_scale") || name_is(name, "raycast") || name_is(name, "intersects_ray") || name_is(name, "raycast_point") ||
			name_is(name, "move_and_slide") || name_is(name, "move_and_drive") || name_is(name, "tile_solid_at") || name_is(name, "tile_at") ||
			name_is(name, "load_audio") || name_is(name, "unload_audio") || name_is(name, "can_load_audio") ||
			name_is(name, "is_audio_loaded") || name_is(name, "play_sfx") || name_is(name, "stop_sfx") ||
			name_is(name, "set_sfx_volume") || name_is(name, "set_music_volume") || name_is(name, "emit") || name_is(name, "set_fog") ||
			name_is(name, "set_fade") || name_is(name, "set_light") ||
			name_is(name, "set_coyote") || name_is(name, "set_jump_buffer") ||
			name_is(name, "can_jump") || name_is(name, "consume_jump") || name_is(name, "set_one_way_pass") ||
			name_is(name, "set_invuln") || name_is(name, "is_invuln") ||
			name_is(name, "set_air_jumps") || name_is(name, "set_wall_jump") ||
			name_is(name, "set_checkpoint") || name_is(name, "respawn") ||
			name_is(name, "set_frame") || name_is(name, "set_flip") ||
			name_is(name, "get_frame") || name_is(name, "get_flip_h") || name_is(name, "set_flip_h") ||
			name_is(name, "raycast_point") || name_is(name, "is_on_floor") || name_is(name, "is_on_wall") ||
			name_is(name, "is_on_ceiling") || name_is(name, "set_hitbox_layer") || name_is(name, "get_hitbox_layer") ||
			name_is(name, "shake_camera") || name_is(name, "set_paused") || name_is(name, "is_paused") ||
			name_is(name, "get_camera_rotation") || name_is(name, "load_text") || name_is(name, "unload_text") ||
			name_is(name, "can_load_text") || name_is(name, "is_text_loaded") || name_is(name, "set_line") ||
			name_is(name, "set_line_chars") ||
			name_is(name, "overlaps") || name_is(name, "overlaps_entered") || name_is(name, "has_overlapping_areas") ||
			name_is(name, "get_overlapping_area_count") || name_is(name, "get_overlapping_area") ||
			name_is(name, "hitbox_kind") || name_is(name, "set_hitbox_enabled") ||
			name_is(name, "load_animations") || name_is(name, "unload_animations") ||
			name_is(name, "can_load_animations") || name_is(name, "is_animations_loaded") ||
			name_is(name, "load_sprites") || name_is(name, "unload_sprites") ||
			name_is(name, "can_load_sprites") || name_is(name, "is_sprites_loaded") ||
			name_is(name, "load_hitboxes") || name_is(name, "unload_hitboxes") || name_is(name, "can_load_hitboxes") ||
			name_is(name, "is_hitboxes_loaded") ||
			name_is(name, "load_cameras") || name_is(name, "unload_cameras") ||
			name_is(name, "is_cameras_loaded") || name_is(name, "can_load_cameras") ||
			name_is(name, "is_action_pressed_on") || name_is(name, "is_action_just_pressed_on") ||
			name_is(name, "is_action_just_released_on") ||
			name_is(name, "memcard_present") || name_is(name, "memcard_ready") || name_is(name, "memcard_format") ||
			name_is(name, "memcard_exists") || name_is(name, "memcard_delete") ||
			name_is(name, "memcard_save") || name_is(name, "memcard_load") ||
			name_is(name, "memcard_count") || name_is(name, "memcard_name") ||
			name_is(name, "memcard_wrap") || name_is(name, "memcard_unwrap") ||
			name_is(name, "open") || name_is(name, "file_exists") || name_is(name, "store_buffer") ||
			name_is(name, "get_buffer") || name_is(name, "store_string") || name_is(name, "get_as_text") ||
			name_is(name, "store_var") || name_is(name, "get_var") || name_is(name, "get_length") ||
			name_is(name, "close") || name_is(name, "get_file_as_bytes")) {
		int pack = pack_id_of(node);
		if (argv && argc > 0) {
			if (argv[0].type == V_STR) {
				pack = find_pack_path(argv[0].s);
			} else if (argv[0].type == V_OBJ) {
				pack = pack_id_of(argv[0].i);
			} else if (argv[0].type == V_INT || argv[0].type == V_FLOAT) {
				pack = int(as_float(argv[0]));
			}
		}
		if (name_is(name, "load_scene")) {
			*ret = gv_bool(load_pack_resident(pack));
			return 1;
		}
		if (name_is(name, "unload_scene")) {
			*ret = gv_bool(unload_pack_resident(pack));
			return 1;
		}
		if (name_is(name, "is_scene_loaded")) {
			*ret = gv_bool(pack >= 0 && pack < g_npack && g_packs[pack].resident);
			return 1;
		}
		if (name_is(name, "can_load_scene")) {
			if (pack < 0 || pack >= g_npack) {
				*ret = gv_bool(0);
				return 1;
			}
			if (g_packs[pack].resident) {
				*ret = gv_bool(1);
				return 1;
			}
			int n = 0, h = 0, t = 0, tr = 0, ti = 0, r = 0;
			pack_copy_cost(pack, &n, &h, &t, &tr, &ti, &r, 1);
			n = int(g_packs[pack].node_count);
			h = int(g_packs[pack].hud_count);
			t = int(g_packs[pack].tile_count);
			tr = int(g_packs[pack].tri_count);
			ti = int(g_packs[pack].tim_count);
			r = int(g_packs[pack].ram_bytes);
			*ret = gv_bool(budgets_fit(n, h, t, tr, ti, r));
			return 1;
		}
		if (name_is(name, "can_instantiate")) {
			if (pack < 0 || pack >= g_npack) {
				*ret = gv_bool(0);
				return 1;
			}
			const int need_load = !g_packs[pack].resident;
			int n = int(g_packs[pack].node_count);
			int h = int(g_packs[pack].hud_count);
			int t = int(g_packs[pack].tile_count);
			int tr = need_load ? int(g_packs[pack].tri_count) : 0;
			int ti = need_load ? int(g_packs[pack].tim_count) : 0;
			int r = inst_row_ram(n, h, t) + (need_load ? int(g_packs[pack].ram_bytes) : 0);
			if (!g_packs[pack].text_resident) {
				r += int(sizeof(ScriptVMTextLine) * PS1_MAX_TXT);
			}
			if (!g_packs[pack].music_resident) {
				r += 32768;
			}
			if (need_load) {
				n *= 2;
				h *= 2;
				t *= 2;
			}
			*ret = gv_bool(budgets_fit(n, h, t, tr, ti, r));
			return 1;
		}
		if (name_is(name, "get_loaded_scene_count")) {
			int n = 0;
			for (int i = 0; i < g_npack; i++) {
				if (g_packs[i].resident) {
					n++;
				}
			}
			*ret = gv_int(n);
			return 1;
		}
		if (name_is(name, "get_loaded_scene")) {
			int want = argv && argc > 0 ? int(as_float(argv[0])) : 0;
			int n = 0;
			for (int i = 0; i < g_npack; i++) {
				if (!g_packs[i].resident) {
					continue;
				}
				if (n == want) {
					GVar s = gv_nil();
					s.type = V_STR;
					copy_str(s.s, 32, g_packs[i].path);
					*ret = s;
					return 1;
				}
				n++;
			}
			*ret = gv_nil();
			return 1;
		}
		if (name_is(name, "get_static_memory_usage")) {
			*ret = gv_int(int(g_ram_used));
			return 1;
		}
		if (name_is(name, "get_static_memory_peak_usage")) {
			*ret = gv_int(int(g_ram_peak));
			return 1;
		}
		if (name_is(name, "get_ram_budget")) {
			*ret = gv_int(PS1_RAM_BUDGET);
			return 1;
		}
		if (name_is(name, "get_free_ram")) {
			*ret = gv_int(int(uint32_t(PS1_RAM_BUDGET) > g_ram_used ? uint32_t(PS1_RAM_BUDGET) - g_ram_used : 0));
			return 1;
		}
		if (name_is(name, "get_node_count")) {
			*ret = gv_int(count_used_flags(g_node_used, PS1_MAX_NODES));
			return 1;
		}
		if (name_is(name, "get_node_budget")) {
			*ret = gv_int(PS1_MAX_NODES);
			return 1;
		}
		if (name_is(name, "get_triangle_count")) {
			*ret = gv_int(g_ntri);
			return 1;
		}
		if (name_is(name, "get_triangle_budget")) {
			*ret = gv_int(PS1_MAX_TRIS);
			return 1;
		}
		if (name_is(name, "get_free_nodes")) {
			*ret = gv_int(PS1_MAX_NODES - count_used_flags(g_node_used, PS1_MAX_NODES));
			return 1;
		}
		if (name_is(name, "get_free_hud")) {
			*ret = gv_int(PS1_MAX_HUD - count_used_flags(g_hud_used, PS1_MAX_HUD));
			return 1;
		}
		if (name_is(name, "get_free_tiles")) {
			*ret = gv_int(PS1_MAX_TILES - count_used_flags(g_tile_used, PS1_MAX_TILES));
			return 1;
		}
		if (name_is(name, "set_camera")) {
			const int idx = argv && argc > 0 ? int(as_float(argv[0])) : -1;
			if (idx < 0 || idx >= g_ncam) {
				*ret = gv_bool(0);
				return 1;
			}
			apply_cam_index(idx, host);
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "set_camera_node")) {
			const char *nm = argv && argc > 0 && argv[0].type == V_STR ? argv[0].s : "";
			for (int i = 0; i < g_ncam; i++) {
				if (name_is(g_cams[i].name, nm)) {
					apply_cam_index(i, host);
					*ret = gv_bool(1);
					return 1;
				}
			}
			*ret = gv_bool(0);
			return 1;
		}
		if (name_is(name, "set_default_camera")) {
			int idx = g_cam_cur;
			if (argv && argc > 0) {
				if (argv[0].type == V_STR) {
					idx = -1;
					for (int i = 0; i < g_ncam; i++) {
						if (name_is(g_cams[i].name, argv[0].s)) {
							idx = i;
							break;
						}
					}
				} else {
					idx = int(as_float(argv[0]));
				}
			}
			if (idx < 0 || idx >= g_ncam) {
				*ret = gv_bool(0);
				return 1;
			}
			for (int i = 0; i < g_ncam; i++) {
				g_cams[i].is_default = i == idx ? 1 : 0;
			}
			g_cam_def = idx;
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "get_camera")) {
			*ret = gv_int(g_cam_cur);
			return 1;
		}
		if (name_is(name, "get_camera_count")) {
			*ret = gv_int(g_ncam);
			return 1;
		}
		if (name_is(name, "get_camera_name")) {
			const int idx = argv && argc > 0 ? int(as_float(argv[0])) : -1;
			if (idx < 0 || idx >= g_ncam) {
				*ret = gv_nil();
				return 1;
			}
			GVar s = gv_nil();
			s.type = V_STR;
			copy_str(s.s, 32, g_cams[idx].name);
			*ret = s;
			return 1;
		}
		if (name_is(name, "is_camera_current")) {
			const int idx = argv && argc > 0 ? int(as_float(argv[0])) : -1;
			*ret = gv_bool(idx == g_cam_cur);
			return 1;
		}
		if (name_is(name, "next_camera")) {
			if (g_ncam <= 0) {
				*ret = gv_bool(0);
				return 1;
			}
			apply_cam_index((g_cam_cur + 1 + g_ncam) % g_ncam, host);
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "prev_camera")) {
			if (g_ncam <= 0) {
				*ret = gv_bool(0);
				return 1;
			}
			apply_cam_index((g_cam_cur - 1 + g_ncam) % g_ncam, host);
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "get_stick")) {
			int device = argv && argc > 0 ? int(as_float(argv[0])) : 0;
			int stick = argv && argc > 1 ? int(as_float(argv[1])) : 0;
			float x = 0, y = 0;
			if (host && (device == 0 || device == 1)) {
				const uint8_t *raw = device == 1 ? host->pad34_1 : host->pad34;
				if (raw) {
					const PADTYPE *pad = (const PADTYPE *)raw;
					if (pad->stat == 0 && (pad->type == PAD_ID_ANALOG || pad->type == PAD_ID_ANALOG_STICK)) {
						int sx = int(stick ? pad->rs_x : pad->ls_x) - 128;
						int sy = int(stick ? pad->rs_y : pad->ls_y) - 128;
						if (sx > -g_deadzone && sx < g_deadzone) {
							sx = 0;
						}
						if (sy > -g_deadzone && sy < g_deadzone) {
							sy = 0;
						}
						x = float(sx) / 128.0f;
						y = -float(sy) / 128.0f;
					}
				}
			}
			*ret = gv_v2(x, y);
			return 1;
		}
		if (name_is(name, "set_rumble")) {
			const int device = argv && argc > 0 ? int(as_float(argv[0])) : 0;
			const int sm = argv && argc > 1 ? int(as_float(argv[1])) : 0;
			const int lg = argv && argc > 2 ? int(as_float(argv[2])) : 0;
			if (device != 0 && device != 1) {
				*ret = gv_bool(0);
				return 1;
			}
			if (host && host->set_rumble) {
				host->set_rumble(device, sm, lg);
			}
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "stop_rumble")) {
			const int device = argv && argc > 0 ? int(as_float(argv[0])) : 0;
			if (device != 0 && device != 1) {
				*ret = gv_bool(0);
				return 1;
			}
			if (host && host->set_rumble) {
				host->set_rumble(device, 0, 0);
			}
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "set_camera_transform")) {
			if (host && argv && argc >= 2 && argv[0].type == V_V3 && argv[1].type == V_V3) {
				if (host->pos_x) {
					*host->pos_x = int32_t(argv[0].x);
				}
				if (host->pos_y) {
					*host->pos_y = int32_t(-argv[0].y);
				}
				if (host->pos_z) {
					*host->pos_z = int32_t(argv[0].z);
				}
				if (host->rot_x) {
					*host->rot_x = int16_t(rad_to_ps1(argv[1].x));
				}
				if (host->rot_y) {
					*host->rot_y = int16_t(rad_to_ps1(argv[1].y));
				}
				if (host->rot_z) {
					*host->rot_z = int16_t(rad_to_ps1(argv[1].z));
				}
				g_script_cam = 1;
				if (host->script_drives_cam) {
					*host->script_drives_cam = 1;
				}
				*ret = gv_bool(1);
				return 1;
			}
			*ret = gv_bool(0);
			return 1;
		}
		if (name_is(name, "get_camera_transform") || name_is(name, "get_camera_rotation")) {
			const float s = (2.0f * 3.14159265f) / 4096.0f;
			if (name_is(name, "get_camera_rotation")) {
				float rx = 0, ry = 0, rz = 0;
				if (host && host->rot_x) {
					rx = float(*host->rot_x) * s;
				}
				if (host && host->rot_y) {
					ry = float(*host->rot_y) * s;
				}
				if (host && host->rot_z) {
					rz = float(*host->rot_z) * s;
				}
				*ret = gv_v3(rx, ry, rz);
				return 1;
			}
			float px = 0, py = 0, pz = 0;
			if (host && host->pos_x) {
				px = float(*host->pos_x);
			}
			if (host && host->pos_y) {
				py = -float(*host->pos_y);
			}
			if (host && host->pos_z) {
				pz = float(*host->pos_z);
			}
			*ret = gv_v3(px, py, pz);
			return 1;
		}
		if (name_is(name, "shake_camera")) {
			g_shake_amp = argv && argc > 0 ? as_float(argv[0]) : 0;
			g_shake_ms = argv && argc > 1 ? as_float(argv[1]) : 0;
			if (g_shake_amp < 0) {
				g_shake_amp = 0;
			}
			g_script_cam = 1;
			if (host && host->script_drives_cam) {
				*host->script_drives_cam = 1;
			}
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "set_paused")) {
			g_paused = argv && argc > 0 && as_truth(argv[0]) ? 1 : 0;
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "is_paused")) {
			*ret = gv_bool(g_paused);
			return 1;
		}
		if (name_is(name, "attach_camera")) {
			int nid = -1;
			if (argv && argc > 0) {
				if (argv[0].type == V_OBJ) {
					nid = argv[0].i;
				} else if (argv[0].type == V_STR && argv[0].s[0] == 0) {
					nid = -1;
				} else {
					nid = int(as_float(argv[0]));
				}
			}
			g_cam_attach = node_ok(nid) ? nid : -1;
			g_cam_offx = 0;
			g_cam_offy = 0;
			g_cam_offz = 0;
			if (argv && argc > 1 && argv[1].type == V_V3) {
				g_cam_offx = int16_t(argv[1].x);
				g_cam_offy = int16_t(-argv[1].y);
				g_cam_offz = int16_t(argv[1].z);
			}
			g_script_cam = g_cam_attach >= 0 ? 1 : 0;
			if (host && host->script_drives_cam) {
				*host->script_drives_cam = g_script_cam;
			}
			*ret = gv_bool(g_cam_attach >= 0);
			return 1;
		}
		if (name_is(name, "look_camera")) {
			const float yaw = argv && argc > 0 ? as_float(argv[0]) : 0;
			float pitch = argv && argc > 1 ? as_float(argv[1]) : 0;
			if (pitch > 1.2f) {
				pitch = 1.2f;
			}
			if (pitch < -1.2f) {
				pitch = -1.2f;
			}
			if (host && host->rot_y) {
				*host->rot_y = int16_t(rad_to_ps1(yaw));
			}
			if (host && host->rot_x) {
				*host->rot_x = int16_t(rad_to_ps1(pitch));
			}
			if (argv && argc > 2 && host && host->rot_z) {
				*host->rot_z = int16_t(rad_to_ps1(as_float(argv[2])));
			}
			g_script_cam = 1;
			if (host && host->script_drives_cam) {
				*host->script_drives_cam = 1;
			}
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "orbit_camera")) {
			float tx = 0, ty = 0, tz = 0;
			if (argv && argc > 0) {
				if (argv[0].type == V_V3) {
					tx = argv[0].x;
					ty = argv[0].y;
					tz = argv[0].z;
				} else {
					const int nid = argv[0].type == V_OBJ ? argv[0].i : int(as_float(argv[0]));
					if (node_ok(nid)) {
						tx = float(g_nodes[nid].px);
						ty = float(-g_nodes[nid].py);
						tz = float(g_nodes[nid].pz);
					}
				}
			}
			const float yaw = argv && argc > 1 ? as_float(argv[1]) : 0;
			float pitch = argv && argc > 2 ? as_float(argv[2]) : 0.4f;
			float dist = argv && argc > 3 ? as_float(argv[3]) : 80;
			if (pitch > 1.2f) {
				pitch = 1.2f;
			}
			if (pitch < -0.1f) {
				pitch = -0.1f;
			}
			if (dist < 8) {
				dist = 8;
			}
			const float cy = util_sin(yaw + 1.5707963f);
			const float sy = util_sin(yaw);
			const float cp = util_sin(pitch + 1.5707963f);
			const float sp = util_sin(pitch);
			float cx = tx + sy * cp * dist;
			float cyw = ty + sp * dist;
			float cz = tz + cy * cp * dist;
			int clip = g_orbit_clip;
			if (argv && argc > 4) {
				clip = as_truth(argv[4]);
			}
			if (clip) {
				const float rdx = cx - tx;
				const float rdy = cyw - ty;
				const float rdz = cz - tz;
				const int hitn = do_raycast(tx, ty, tz, rdx, rdy, rdz, dist, 3, 0xFF, -1, -1);
				if (hitn >= 0 && g_ray_hit) {
					cx = g_ray_hx;
					cyw = g_ray_hy;
					cz = g_ray_hz;
				}
			}
			if (host && host->pos_x) {
				*host->pos_x = int32_t(cx);
			}
			if (host && host->pos_y) {
				*host->pos_y = int32_t(-cyw);
			}
			if (host && host->pos_z) {
				*host->pos_z = int32_t(cz);
			}
			if (host && host->rot_y) {
				*host->rot_y = int16_t(rad_to_ps1(yaw));
			}
			if (host && host->rot_x) {
				*host->rot_x = int16_t(rad_to_ps1(pitch));
			}
			g_script_cam = 1;
			if (host && host->script_drives_cam) {
				*host->script_drives_cam = 1;
			}
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "set_camera_scale")) {
			const int h = argv && argc > 0 ? int(as_float(argv[0])) : 0;
			g_cam_scale = h;
			if (host && host->cam_scale) {
				*host->cam_scale = h;
			}
			*ret = gv_bool(h > 0);
			return 1;
		}
		if (name_is(name, "raycast") || name_is(name, "intersects_ray") || name_is(name, "raycast_point")) {
			float ox = 0, oy = 0, oz = 0, dx = 0, dy = 0, dz = 1, dist = 64;
			int dim = 3;
			int mask = 0xFF;
			if (name_is(name, "intersects_ray") && node_ok(node)) {
				const int hi = hit_of_node(node);
				if (hi >= 0) {
					dim = int(g_hits[hi].dim);
				}
			}
			if (argv && argc > 0 && argv[0].type == V_V3) {
				ox = argv[0].x;
				oy = argv[0].y;
				oz = argv[0].z;
			}
			if (argv && argc > 1 && argv[1].type == V_V3) {
				dx = argv[1].x;
				dy = argv[1].y;
				dz = argv[1].z;
			}
			if (argv && argc > 2) {
				dist = as_float(argv[2]);
			}
			if (argv && argc > 3 && argv[3].type != V_OBJ) {
				mask = int(as_float(argv[3]));
				if (!mask) {
					mask = 0xFF;
				}
			}
			int skip = node_ok(node) ? node : -1;
			if (argv && argc > 0 && argv[argc - 1].type == V_OBJ) {
				skip = argv[argc - 1].i;
			}
			int hitn = -1;
			float best = dist;
			g_ray_hit = 0;
			for (int i = 0; i < g_nhit; i++) {
				if (!g_hit_used[i] || !(g_hits[i].flags & 1)) {
					continue;
				}
				if (skip >= 0 && int(g_hits[i].node_id) == skip) {
					continue;
				}
				if (!(int(g_hits[i].layer) & mask)) {
					continue;
				}
				if (int(g_hits[i].dim) != dim) {
					continue;
				}
				const float minx = float(g_hits[i].min_x);
				const float maxx = float(g_hits[i].max_x);
				const float miny = float(-g_hits[i].max_y);
				const float maxy = float(-g_hits[i].min_y);
				const float minz = dim == 2 ? -1 : float(g_hits[i].min_z);
				const float maxz = dim == 2 ? 1 : float(g_hits[i].max_z);
				float tmin = 0, tmax = dist;
				const float bmin[3] = { minx, miny, minz };
				const float bmax[3] = { maxx, maxy, maxz };
				const float o[3] = { ox, oy, oz };
				const float d[3] = { dx, dy, dz };
				int miss = 0;
				for (int a = 0; a < 3; a++) {
					if (d[a] == 0.0f) {
						if (o[a] < bmin[a] || o[a] > bmax[a]) {
							miss = 1;
						}
						continue;
					}
					float inv = 1.0f / d[a];
					float t0 = (bmin[a] - o[a]) * inv;
					float t1 = (bmax[a] - o[a]) * inv;
					if (t0 > t1) {
						const float tmp = t0;
						t0 = t1;
						t1 = tmp;
					}
					if (t0 > tmin) {
						tmin = t0;
					}
					if (t1 < tmax) {
						tmax = t1;
					}
					if (tmin > tmax) {
						miss = 1;
					}
				}
				if (!miss && tmin >= 0 && tmin < best) {
					best = tmin;
					hitn = int(g_hits[i].node_id);
					g_ray_hx = ox + dx * tmin;
					g_ray_hy = oy + dy * tmin;
					g_ray_hz = oz + dz * tmin;
					g_ray_hit = 1;
				}
			}
			if (name_is(name, "raycast_point")) {
				*ret = g_ray_hit ? gv_v3(g_ray_hx, g_ray_hy, g_ray_hz) : gv_nil();
			} else {
				*ret = hitn >= 0 ? gv_obj(hitn) : gv_nil();
			}
			return 1;
		}
		if (name_is(name, "move_and_slide")) {
			int nid = node;
			float vx = 0, vy = 0, vz = 0;
			int mask = 0xFF;
			if (argv && argc >= 2 && argv[1].type == V_V3) {
				if (argv[0].type == V_OBJ) {
					nid = argv[0].i;
				} else {
					nid = int(as_float(argv[0]));
				}
				vx = argv[1].x;
				vy = argv[1].y;
				vz = argv[1].z;
				if (argc > 2) {
					mask = int(as_float(argv[2]));
				}
			} else if (argv && argc >= 1 && argv[0].type == V_V3) {
				vx = argv[0].x;
				vy = argv[0].y;
				vz = argv[0].z;
				if (argc > 1) {
					mask = int(as_float(argv[1]));
				}
			} else if (node_ok(nid)) {
				vx = g_nvel[nid][0];
				vy = g_nvel[nid][1];
				vz = g_nvel[nid][2];
			}
			if (!mask) {
				mask = 0xFF;
			}
			if (!node_ok(nid)) {
				*ret = gv_v3(0, 0, 0);
				return 1;
			}
			const int hi = hit_of_node(nid);
			const int dim2 = (hi < 0) || (g_hits[hi].dim == 2);
			int floor_hit = -1;
			int floor_tile = -1;
			auto overlap_at = [&](int16_t px, int16_t py, int16_t pz, int axis) -> int {
				if (hi >= 0) {
					const int16_t dx = int16_t(px - g_nodes[nid].px);
					const int16_t dy = int16_t(py - g_nodes[nid].py);
					const int16_t dz = int16_t(pz - g_nodes[nid].pz);
					ScriptVMHit tmp = g_hits[hi];
					tmp.min_x = int16_t(tmp.min_x + dx);
					tmp.max_x = int16_t(tmp.max_x + dx);
					tmp.min_y = int16_t(tmp.min_y + dy);
					tmp.max_y = int16_t(tmp.max_y + dy);
					tmp.min_z = int16_t(tmp.min_z + dz);
					tmp.max_z = int16_t(tmp.max_z + dz);
					for (int i = 0; i < g_nhit; i++) {
						if (!g_hit_used[i] || i == hi || !(g_hits[i].flags & 1) || g_hits[i].dim != tmp.dim) {
							continue;
						}
						if (!(int(g_hits[i].layer) & mask)) {
							continue;
						}
						if (g_one_way_pass[nid] && axis == 1 && (dim2 ? (vy < 0.0f) : (vy > 0.0f))) {
							continue;
						}
						const int sep = tmp.max_x < g_hits[i].min_x || tmp.min_x > g_hits[i].max_x ||
								tmp.max_y < g_hits[i].min_y || tmp.min_y > g_hits[i].max_y ||
								(tmp.dim == 3 && (tmp.max_z < g_hits[i].min_z || tmp.min_z > g_hits[i].max_z));
						if (!sep) {
							if (axis == 1) {
								floor_hit = int(g_hits[i].node_id);
							}
							return 1;
						}
					}
				}
				if (hi < 0 || g_hits[hi].dim == 2) {
					for (int t = 0; t < g_ntile; t++) {
						if (!g_tile_used[t] || !(g_tiles[t].flags & 1)) {
							continue;
						}
						if ((g_tiles[t].flags & 2) && !(axis == 1 && vy > 0.0f)) {
							continue;
						}
						if (g_one_way_pass[nid] && (g_tiles[t].flags & 2) && axis == 1 && (dim2 ? (vy < 0.0f) : (vy > 0.0f))) {
							continue;
						}
						const int16_t x = g_tiles[t].x;
						const int16_t y = g_tiles[t].y;
						if (px >= x && px < int16_t(x + 16) && (-py) >= y && (-py) < int16_t(y + 16)) {
							if (axis == 1) {
								floor_tile = t;
							}
							return 1;
						}
					}
				}
				return 0;
			};
			g_on_floor = 0;
			g_on_wall = 0;
			g_on_ceiling = 0;
			g_floor_node = -1;
			g_floor_nx = 0;
			g_floor_ny = 1;
			g_floor_nz = 0;
			const float ovx = vx, ovy = vy, ovz = vz;
			int16_t nx = int16_t(g_nodes[nid].px + vx);
			int16_t ny = g_nodes[nid].py;
			int16_t nz = g_nodes[nid].pz;
			if (overlap_at(nx, ny, nz, 0)) {
				nx = g_nodes[nid].px;
				vx = 0;
				g_on_wall = 1;
			}
			ny = int16_t(g_nodes[nid].py - vy);
			if (overlap_at(nx, ny, nz, 1)) {
				ny = int16_t(g_nodes[nid].py);
				if (dim2 ? (ovy > 0.0f) : (ovy < 0.0f)) {
					g_on_floor = 1;
					g_floor_node = floor_hit;
					g_floor_nx = 0;
					g_floor_ny = 1;
					g_floor_nz = 0;
					if (floor_tile >= 0) {
						if (g_tiles[floor_tile].flags & 4) {
							g_floor_nx = -0.707f;
							g_floor_ny = 0.707f;
							nx = int16_t(nx - (ovy > 0 ? 1 : 0));
						} else if (g_tiles[floor_tile].flags & 8) {
							g_floor_nx = 0.707f;
							g_floor_ny = 0.707f;
							nx = int16_t(nx + (ovy > 0 ? 1 : 0));
						}
					}
				} else if (ovy != 0.0f) {
					g_on_ceiling = 1;
					g_floor_ny = -1;
				}
				vy = 0;
			}
			nz = int16_t(g_nodes[nid].pz + vz);
			if (overlap_at(nx, ny, nz, 2)) {
				nz = g_nodes[nid].pz;
				vz = 0;
				g_on_wall = 1;
			}
			(void)ovx;
			(void)ovz;
			if (g_on_floor && node_ok(g_floor_node) && g_floor_node != nid) {
				nx = int16_t(nx + g_nvel[g_floor_node][0]);
				ny = int16_t(ny + g_nvel[g_floor_node][1]);
				nz = int16_t(nz + g_nvel[g_floor_node][2]);
				vx += g_nvel[g_floor_node][0];
				vy += g_nvel[g_floor_node][1];
				vz += g_nvel[g_floor_node][2];
			}
			g_nodes[nid].px = nx;
			g_nodes[nid].py = ny;
			g_nodes[nid].pz = nz;
			g_vel_x = vx;
			g_vel_y = vy;
			g_vel_z = vz;
			g_nvel[nid][0] = vx;
			g_nvel[nid][1] = vy;
			g_nvel[nid][2] = vz;
			g_floor_n[nid] = uint8_t(g_on_floor);
			g_wall_n[nid] = uint8_t(g_on_wall);
			g_ceil_n[nid] = uint8_t(g_on_ceiling);
			if (g_on_floor) {
				if (!g_was_floor[nid] && g_jump_buf_until[nid] > g_ticks_ms) {
					g_auto_jump[nid] = 1;
				}
				g_coyote_until[nid] = g_ticks_ms + g_coyote_ms;
				g_air_left[nid] = g_air_jumps;
			} else if (g_was_floor[nid] && g_coyote_ms > 0) {
				g_coyote_until[nid] = g_ticks_ms + g_coyote_ms;
			}
			if (!g_on_wall && g_was_wall[nid] && g_wall_jump_ms > 0) {
				g_wall_until[nid] = g_ticks_ms + g_wall_jump_ms;
			}
			g_was_floor[nid] = uint8_t(g_on_floor);
			g_was_wall[nid] = uint8_t(g_on_wall);
			*ret = gv_v3(vx, vy, vz);
			return 1;
		}
		if (name_is(name, "move_and_drive")) {
			int nid = node;
			float throttle = 0;
			float steer = 0;
			if (argv && argc >= 3) {
				nid = argv[0].type == V_OBJ ? argv[0].i : int(as_float(argv[0]));
				throttle = as_float(argv[1]);
				steer = as_float(argv[2]);
			} else if (argv && argc >= 2) {
				throttle = as_float(argv[0]);
				steer = as_float(argv[1]);
			}
			if (!node_ok(nid)) {
				*ret = gv_v3(0, 0, 0);
				return 1;
			}
			if (throttle < -1.0f) {
				throttle = -1.0f;
			}
			if (throttle > 1.0f) {
				throttle = 1.0f;
			}
			if (steer < -1.0f) {
				steer = -1.0f;
			}
			if (steer > 1.0f) {
				steer = 1.0f;
			}
			const int dim2 = g_nodes[nid].type == 3 || g_nodes[nid].type == 5 || g_nodes[nid].type == 6 || g_nodes[nid].type == 11;
			g_ang[nid][1] = dim2 ? 0 : steer * 0.12f;
			g_ang[nid][2] = dim2 ? steer * 0.12f : 0;
			g_nodes[nid].ry = int16_t(g_nodes[nid].ry + int16_t(rad_to_ps1(g_ang[nid][1])));
			g_nodes[nid].rz = int16_t(g_nodes[nid].rz + int16_t(rad_to_ps1(g_ang[nid][2])));
			const float yaw = float(g_nodes[nid].ry) * (2.0f * 3.14159265f) / 4096.0f;
			const float roll = float(g_nodes[nid].rz) * (2.0f * 3.14159265f) / 4096.0f;
			float fx = 0, fz = 0, fy = 0;
			if (dim2) {
				fx = util_sin(roll + 1.5707963f) * throttle * 8.0f;
				fy = util_sin(roll) * throttle * 8.0f;
			} else {
				fx = util_sin(yaw + 1.5707963f) * throttle * 8.0f;
				fz = util_sin(yaw) * throttle * 8.0f;
			}
			if (throttle == 0.0f && g_drive_fric[nid] > 0) {
				fx = g_nvel[nid][0] * (1.0f - g_drive_fric[nid]);
				fy = g_nvel[nid][1] * (1.0f - g_drive_fric[nid]);
				fz = g_nvel[nid][2] * (1.0f - g_drive_fric[nid]);
			}
			GVar args[3];
			args[0] = gv_obj(nid);
			args[1] = gv_v3(fx, fy, fz);
			args[2] = gv_int(255);
			return apply_call(host, nid, "move_and_slide", 0, args, 3, ret);
		}
		if (name_is(name, "tile_solid_at") || name_is(name, "tile_at")) {
			const float x = argv && argc > 0 ? as_float(argv[0]) : 0;
			const float y = argv && argc > 1 ? as_float(argv[1]) : 0;
			int found = -1;
			int solid = 0;
			for (int t = 0; t < g_ntile; t++) {
				if (!g_tile_used[t]) {
					continue;
				}
				if (x >= float(g_tiles[t].x) && x < float(g_tiles[t].x + 16) && y >= float(g_tiles[t].y) && y < float(g_tiles[t].y + 16)) {
					found = t;
					solid = g_tiles[t].flags & 1;
					break;
				}
			}
			if (name_is(name, "tile_solid_at")) {
				*ret = gv_bool(solid);
			} else {
				*ret = found >= 0 ? gv_int(found) : gv_int(-1);
			}
			return 1;
		}
		if (name_is(name, "load_audio")) {
			*ret = gv_bool(load_pack_slice(pack, "SFX"));
			return 1;
		}
		if (name_is(name, "unload_audio")) {
			if (pack > 0 && pack < g_npack) {
				g_packs[pack].audio_resident = 0;
				if (host && host->unload_sfx_bank) {
					host->unload_sfx_bank();
				}
				*ret = gv_bool(1);
			} else {
				*ret = gv_bool(0);
			}
			return 1;
		}
		if (name_is(name, "is_audio_loaded")) {
			*ret = gv_bool(pack >= 0 && pack < g_npack && g_packs[pack].audio_resident);
			return 1;
		}
		if (name_is(name, "can_load_audio")) {
			if (pack < 0 || pack >= g_npack) {
				*ret = gv_bool(0);
				return 1;
			}
			if (g_packs[pack].audio_resident) {
				*ret = gv_bool(1);
				return 1;
			}
			*ret = gv_bool(try_read_pack_blob(pack, "SFX", g_load_buf, 8) > 0 && budgets_fit(0, 0, 0, 0, 0, 4096));
			return 1;
		}
		if (name_is(name, "play_sfx")) {
			const char *nm = argv && argc > 0 && argv[0].type == V_STR ? argv[0].s : "";
			*ret = gv_bool(host && host->play_sfx ? host->play_sfx(nm) : 0);
			return 1;
		}
		if (name_is(name, "stop_sfx")) {
			const char *nm = argv && argc > 0 && argv[0].type == V_STR ? argv[0].s : "";
			if (host && host->stop_sfx) {
				host->stop_sfx(nm);
			}
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "set_sfx_volume")) {
			const char *nm = argv && argc > 0 && argv[0].type == V_STR ? argv[0].s : "";
			const int vol = argv && argc > 1 ? int(as_float(argv[1]) * 0x3fff) : 0x3fff;
			if (host && host->set_sfx_volume) {
				host->set_sfx_volume(nm, vol);
			}
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "emit")) {
			float px = 0, py = 0, pz = 0;
			int count = 1;
			int tex = 0;
			float vx = 0, vy = 0, vz = 0;
			int life = int(g_part_life);
			int mode = int(g_part_mode);
			int argi = 0;
			if (node_ok(node) && (!argv || argc < 1 || argv[0].type != V_V3)) {
				px = float(g_nodes[node].px);
				py = float(-g_nodes[node].py);
				pz = float(g_nodes[node].pz);
			} else if (argv && argc > 0 && argv[0].type == V_V3) {
				px = argv[0].x;
				py = argv[0].y;
				pz = argv[0].z;
				argi = 1;
			}
			if (argv && argc > argi) {
				count = int(as_float(argv[argi]));
			}
			if (argv && argc > argi + 1) {
				tex = int(as_float(argv[argi + 1]));
			}
			if (argv && argc > argi + 2 && argv[argi + 2].type == V_V3) {
				vx = argv[argi + 2].x;
				vy = argv[argi + 2].y;
				vz = argv[argi + 2].z;
			}
			if (argv && argc > argi + 3) {
				life = int(as_float(argv[argi + 3]));
			}
			if (argv && argc > argi + 4) {
				mode = int(as_float(argv[argi + 4]));
			}
			if (count < 1) {
				count = 1;
			}
			if (count > PS1_MAX_PARTICLES) {
				count = PS1_MAX_PARTICLES;
			}
			for (int i = 0; i < count; i++) {
				part_birth(px, py, pz, vx, vy, vz, tex, mode, life);
			}
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "set_fog")) {
			g_fog_on = argv && argc > 0 ? as_truth(argv[0]) : 1;
			if (argv && argc > 1) {
				g_fog_start = int(as_float(argv[1]));
			}
			if (argv && argc > 2) {
				g_fog_end = int(as_float(argv[2]));
			}
			if (argv && argc > 3 && argv[3].type == V_V3) {
				g_fog_r = uint8_t(argv[3].x);
				g_fog_g = uint8_t(argv[3].y);
				g_fog_b = uint8_t(argv[3].z);
			}
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "set_fade")) {
			g_fade_out_left = 0;
			g_fade_out_dur = 0;
			g_fade_a = argv && argc > 0 ? int(as_float(argv[0]) * 255.0f) : 0;
			if (g_fade_a < 0) {
				g_fade_a = 0;
			}
			if (g_fade_a > 255) {
				g_fade_a = 255;
			}
			if (argv && argc > 1 && argv[1].type == V_V3) {
				g_fade_r = uint8_t(argv[1].x);
				g_fade_g = uint8_t(argv[1].y);
				g_fade_b = uint8_t(argv[1].z);
			}
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "set_light")) {
			int idx = 0;
			int ai = 0;
			if (argv && argc > 0 && argv[0].type != V_V3) {
				idx = int(as_float(argv[0]));
				ai = 1;
			}
			if (idx < 0) {
				idx = 0;
			}
			if (idx > 2) {
				idx = 2;
			}
			int dx = 0, dy = 4096, dz = 0, r = 255, g = 255, b = 255;
			if (argv && argc > ai && argv[ai].type == V_V3) {
				dx = int(argv[ai].x * 4096.0f);
				dy = int(-argv[ai].y * 4096.0f);
				dz = int(argv[ai].z * 4096.0f);
			}
			if (argv && argc > ai + 1 && argv[ai + 1].type == V_V3) {
				r = int(argv[ai + 1].x);
				g = int(argv[ai + 1].y);
				b = int(argv[ai + 1].z);
			}
			if (host && host->set_light) {
				host->set_light(idx, dx, dy, dz, r, g, b);
			}
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "set_coyote")) {
			g_coyote_ms = argv && argc > 0 ? as_float(argv[0]) : 0;
			if (g_coyote_ms < 0) {
				g_coyote_ms = 0;
			}
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "set_jump_buffer")) {
			g_jump_buf_ms = argv && argc > 0 ? as_float(argv[0]) : 0;
			if (g_jump_buf_ms < 0) {
				g_jump_buf_ms = 0;
			}
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "can_jump")) {
			const int nid = kit_nid(node, argv, argc);
			*ret = gv_bool(kit_can_jump(nid));
			return 1;
		}
		if (name_is(name, "consume_jump")) {
			const int nid = kit_nid(node, argv, argc);
			if (!node_ok(nid)) {
				*ret = gv_bool(0);
				return 1;
			}
			if (!kit_can_jump(nid)) {
				if (g_jump_buf_ms > 0) {
					g_jump_buf_until[nid] = g_ticks_ms + g_jump_buf_ms;
				}
				*ret = gv_bool(0);
				return 1;
			}
			const int grounded = g_floor_n[nid] || g_auto_jump[nid] || g_coyote_until[nid] > g_ticks_ms;
			const int walled = g_wall_n[nid] || g_wall_until[nid] > g_ticks_ms;
			if (!grounded && !walled && g_air_left[nid] > 0) {
				g_air_left[nid]--;
			}
			g_auto_jump[nid] = 0;
			g_coyote_until[nid] = 0;
			g_jump_buf_until[nid] = 0;
			g_wall_until[nid] = 0;
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "set_one_way_pass")) {
			int nid = node;
			int on = 1;
			if (argv && argc > 0) {
				if (argv[0].type == V_OBJ) {
					nid = argv[0].i;
					if (argc > 1) {
						on = int(as_float(argv[1]));
					}
				} else if (argc == 1) {
					on = int(as_float(argv[0]));
				} else {
					nid = int(as_float(argv[0]));
					on = int(as_float(argv[1]));
				}
			}
			if (!node_ok(nid)) {
				*ret = gv_bool(0);
				return 1;
			}
			g_one_way_pass[nid] = uint8_t(on ? 1 : 0);
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "set_invuln")) {
			int nid = node;
			float ms = 0;
			if (argv && argc > 0) {
				if (argv[0].type == V_OBJ) {
					nid = argv[0].i;
					if (argc > 1) {
						ms = as_float(argv[1]);
					}
				} else if (argc == 1) {
					ms = as_float(argv[0]);
				} else {
					nid = int(as_float(argv[0]));
					ms = as_float(argv[1]);
				}
			}
			if (!node_ok(nid)) {
				*ret = gv_bool(0);
				return 1;
			}
			if (ms < 0) {
				ms = 0;
			}
			g_invuln_until[nid] = g_ticks_ms + ms;
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "is_invuln")) {
			const int nid = kit_nid(node, argv, argc);
			*ret = gv_bool(kit_invuln(nid));
			return 1;
		}
		if (name_is(name, "set_air_jumps")) {
			int n = argv && argc > 0 ? int(as_float(argv[0])) : 0;
			if (n < 0) {
				n = 0;
			}
			if (n > 8) {
				n = 8;
			}
			g_air_jumps = uint8_t(n);
			for (int i = 0; i < PS1_MAX_NODES; i++) {
				if (g_floor_n[i]) {
					g_air_left[i] = g_air_jumps;
				}
			}
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "set_wall_jump")) {
			g_wall_jump_ms = argv && argc > 0 ? as_float(argv[0]) : 0;
			if (g_wall_jump_ms < 0) {
				g_wall_jump_ms = 0;
			}
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "set_checkpoint")) {
			const int nid = kit_nid(node, argv, argc);
			if (!node_ok(nid)) {
				*ret = gv_bool(0);
				return 1;
			}
			g_cp_x[nid] = g_nodes[nid].px;
			g_cp_y[nid] = g_nodes[nid].py;
			g_cp_z[nid] = g_nodes[nid].pz;
			g_cp_set[nid] = 1;
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "respawn")) {
			const int nid = kit_nid(node, argv, argc);
			if (!node_ok(nid) || !g_cp_set[nid]) {
				*ret = gv_bool(0);
				return 1;
			}
			g_nodes[nid].px = g_cp_x[nid];
			g_nodes[nid].py = g_cp_y[nid];
			g_nodes[nid].pz = g_cp_z[nid];
			g_nvel[nid][0] = g_nvel[nid][1] = g_nvel[nid][2] = 0;
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "set_frame") || name_is(name, "get_frame")) {
			int nid = node;
			int fr = 0;
			if (argv && argc > 0) {
				if (argv[0].type == V_OBJ) {
					nid = argv[0].i;
					if (argc > 1) {
						fr = int(as_float(argv[1]));
					}
				} else if (argc == 1 && !name_is(name, "get_frame")) {
					fr = int(as_float(argv[0]));
				} else if (argc >= 2) {
					nid = int(as_float(argv[0]));
					fr = int(as_float(argv[1]));
				} else {
					nid = int(as_float(argv[0]));
				}
			}
			if (!node_ok(nid)) {
				*ret = gv_int(0);
				return 1;
			}
			const int si = int(g_nodes[nid].sprite);
			if (si < 0 || si >= g_nspr) {
				*ret = gv_int(0);
				return 1;
			}
			if (name_is(name, "set_frame")) {
				if (fr < 0) {
					fr = 0;
				}
				if (g_sprs[si].nframes && fr >= int(g_sprs[si].nframes)) {
					fr = int(g_sprs[si].nframes) - 1;
				}
				g_sprs[si].frame = uint8_t(fr);
			}
			*ret = gv_int(g_sprs[si].frame);
			return 1;
		}
		if (name_is(name, "set_flip") || name_is(name, "set_flip_h") || name_is(name, "get_flip_h")) {
			int nid = node;
			int h = 0;
			int v = 0;
			if (argv && argc > 0) {
				if (argv[0].type == V_OBJ) {
					nid = argv[0].i;
					if (argc > 1) {
						h = as_truth(argv[1]);
					}
					if (argc > 2) {
						v = as_truth(argv[2]);
					}
				} else if (argc == 1 && !name_is(name, "get_flip_h")) {
					h = as_truth(argv[0]);
				} else if (argc >= 2) {
					nid = int(as_float(argv[0]));
					h = as_truth(argv[1]);
					if (argc > 2) {
						v = as_truth(argv[2]);
					}
				} else {
					nid = int(as_float(argv[0]));
				}
			}
			if (!node_ok(nid)) {
				*ret = gv_bool(0);
				return 1;
			}
			const int si = int(g_nodes[nid].sprite);
			if (si < 0 || si >= g_nspr) {
				*ret = gv_bool(0);
				return 1;
			}
			if (name_is(name, "set_flip") || name_is(name, "set_flip_h")) {
				g_sprs[si].flip_h = uint8_t(h ? 1 : 0);
				if (name_is(name, "set_flip")) {
					g_sprs[si].flip_v = uint8_t(v ? 1 : 0);
				}
			}
			*ret = gv_bool(g_sprs[si].flip_h);
			return 1;
		}
		if (name_is(name, "is_on_floor")) {
			*ret = gv_bool(node_ok(node) ? g_floor_n[node] : g_on_floor);
			return 1;
		}
		if (name_is(name, "is_on_wall")) {
			*ret = gv_bool(node_ok(node) ? g_wall_n[node] : g_on_wall);
			return 1;
		}
		if (name_is(name, "is_on_ceiling")) {
			*ret = gv_bool(node_ok(node) ? g_ceil_n[node] : g_on_ceiling);
			return 1;
		}
		if (name_is(name, "set_hitbox_layer") || name_is(name, "get_hitbox_layer")) {
			int nid = node;
			if (argv && argc > 0) {
				if (argv[0].type == V_OBJ) {
					nid = argv[0].i;
				} else {
					nid = int(as_float(argv[0]));
				}
			}
			const int hi = hit_of_node(nid);
			if (name_is(name, "get_hitbox_layer")) {
				*ret = gv_int(hi >= 0 ? int(g_hits[hi].layer) : 0);
				return 1;
			}
			if (hi >= 0 && argv && argc > 1) {
				int layer = int(as_float(argv[1]));
				if (layer <= 0) {
					layer = 1;
				}
				g_hits[hi].layer = uint8_t(layer & 0xFF);
				*ret = gv_bool(1);
			} else {
				*ret = gv_bool(0);
			}
			return 1;
		}
		if (name_is(name, "set_music_volume")) {
			int vol = argv && argc > 0 ? int(as_float(argv[0]) * 0x3fff) : 0x3fff;
			if (vol < 0) {
				vol = 0;
			}
			if (vol > 0x3fff) {
				vol = 0x3fff;
			}
			if (host && host->set_music_volume) {
				host->set_music_volume(vol);
			}
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "load_text")) {
			const int was = pack >= 0 && pack < g_npack && g_packs[pack].text_resident;
			const int ok = load_pack_slice(pack, "TXT");
			if (ok && !was) {
				bump_ram(int(sizeof(ScriptVMTextLine) * PS1_MAX_TXT));
			}
			*ret = gv_bool(ok);
			return 1;
		}
		if (name_is(name, "unload_text")) {
			const int was = pack >= 0 && pack < g_npack && g_packs[pack].text_resident;
			const int ok = unload_pack_slice(pack, "TXT");
			if (ok && was) {
				bump_ram(-int(sizeof(ScriptVMTextLine) * PS1_MAX_TXT));
			}
			*ret = gv_bool(ok);
			return 1;
		}
		if (name_is(name, "can_load_text") || name_is(name, "is_text_loaded")) {
			if (pack < 0 || pack >= g_npack) {
				*ret = gv_bool(0);
				return 1;
			}
			if (name_is(name, "is_text_loaded")) {
				*ret = gv_bool(g_packs[pack].text_resident);
				return 1;
			}
			*ret = gv_bool(try_read_pack_blob(pack, "TXT", g_load_buf, 8) > 0 && budgets_fit(0, 0, 0, 0, 0, int(sizeof(ScriptVMTextLine) * PS1_MAX_TXT)));
			return 1;
		}
		if (name_is(name, "set_line") || name_is(name, "set_line_chars")) {
			int hid = -1;
			const char *ln = "";
			int nch = 64;
			if (argv && argc > 0) {
				if (argv[0].type == V_OBJ) {
					hid = hud_index_for_node(argv[0].i);
				} else {
					hid = hud_index_for_node(int(as_float(argv[0])));
				}
			} else if (node_ok(node)) {
				hid = hud_index_for_node(node);
			}
			if (argv && argc > 1 && argv[1].type == V_STR) {
				ln = argv[1].s;
			}
			if (name_is(name, "set_line_chars") && argc > 2) {
				nch = int(as_float(argv[2]));
			}
			int found = -1;
			for (int i = 0; i < g_ntxt; i++) {
				if (g_txt[i].name[0] && name_is(g_txt[i].name, ln)) {
					found = i;
					break;
				}
			}
			if (found < 0 || hid < 0 || hid >= g_nhud) {
				*ret = gv_bool(0);
				return 1;
			}
			if (nch < 0) {
				nch = 0;
			}
			if (nch > 63) {
				nch = 63;
			}
			int i = 0;
			for (; i < nch && g_txt[found].text[i]; i++) {
				g_hud[hid].text[i] = g_txt[found].text[i];
			}
			g_hud[hid].text[i] = 0;
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "overlaps")) {
			int a = node_ok(node) ? hit_of_node(node) : -1;
			int b = -1;
			int mask = 0xFF;
			if (argv && argc >= 2) {
				a = hit_of_node(argv[0].type == V_OBJ ? argv[0].i : int(as_float(argv[0])));
				b = hit_of_node(argv[1].type == V_OBJ ? argv[1].i : int(as_float(argv[1])));
				if (argc > 2) {
					mask = int(as_float(argv[2]));
				}
			} else if (argv && argc == 1) {
				if (argv[0].type == V_OBJ) {
					b = hit_of_node(argv[0].i);
				} else {
					b = hit_of_node(int(as_float(argv[0])));
				}
			}
			if (!mask) {
				mask = 0xFF;
			}
			*ret = gv_bool(aabb_overlap(a, b, mask));
			return 1;
		}
		if (name_is(name, "overlaps_entered")) {
			int a = node_ok(node) ? hit_of_node(node) : -1;
			int b = -1;
			int mask = 0xFF;
			if (argv && argc >= 2) {
				a = hit_of_node(argv[0].type == V_OBJ ? argv[0].i : int(as_float(argv[0])));
				b = hit_of_node(argv[1].type == V_OBJ ? argv[1].i : int(as_float(argv[1])));
				if (argc > 2) {
					mask = int(as_float(argv[2]));
				}
			} else if (argv && argc == 1) {
				b = hit_of_node(argv[0].type == V_OBJ ? argv[0].i : int(as_float(argv[0])));
			}
			if (!mask) {
				mask = 0xFF;
			}
			const int now = aabb_overlap(a, b, mask);
			uint32_t bit = 0;
			if (b >= 0 && b < 32) {
				bit = uint32_t(1) << b;
			}
			int entered = 0;
			if (a >= 0 && a < PS1_MAX_HITS) {
				entered = now && !(g_hit_prev[a] & bit);
				if (now) {
					g_hit_prev[a] |= bit;
				} else {
					g_hit_prev[a] &= ~bit;
				}
			}
			*ret = gv_bool(entered);
			return 1;
		}
		if (name_is(name, "has_overlapping_areas") || name_is(name, "get_overlapping_area_count")) {
			const int self = hit_of_node(node);
			int mask = 0xFF;
			if (argv && argc > 0) {
				mask = int(as_float(argv[0]));
				if (!mask) {
					mask = 0xFF;
				}
			}
			int n = 0;
			for (int i = 0; i < g_nhit; i++) {
				if (i != self && aabb_overlap(self, i, mask)) {
					n++;
				}
			}
			*ret = name_is(name, "has_overlapping_areas") ? gv_bool(n > 0) : gv_int(n);
			return 1;
		}
		if (name_is(name, "get_overlapping_area")) {
			const int self = hit_of_node(node);
			int want = argv && argc > 0 ? int(as_float(argv[0])) : 0;
			int mask = 0xFF;
			if (argv && argc > 1) {
				mask = int(as_float(argv[1]));
				if (!mask) {
					mask = 0xFF;
				}
			}
			int n = 0;
			for (int i = 0; i < g_nhit; i++) {
				if (i != self && aabb_overlap(self, i, mask)) {
					if (n == want) {
						*ret = gv_obj(int(g_hits[i].node_id));
						return 1;
					}
					n++;
				}
			}
			*ret = gv_nil();
			return 1;
		}
		if (name_is(name, "hitbox_kind")) {
			int nid = node;
			if (argv && argc > 0) {
				nid = argv[0].type == V_OBJ ? argv[0].i : int(as_float(argv[0]));
			}
			const int h = hit_of_node(nid);
			*ret = gv_int(h >= 0 ? int(g_hits[h].kind) : 0);
			return 1;
		}
		if (name_is(name, "set_hitbox_enabled")) {
			int nid = node;
			int on = 1;
			if (argv && argc >= 2) {
				nid = argv[0].type == V_OBJ ? argv[0].i : int(as_float(argv[0]));
				on = int(as_float(argv[1]));
			} else if (argv && argc == 1) {
				on = int(as_float(argv[0]));
			}
			const int h = hit_of_node(nid);
			if (h >= 0) {
				if (on) {
					g_hits[h].flags = uint8_t(g_hits[h].flags | 1);
				} else {
					g_hits[h].flags = uint8_t(g_hits[h].flags & ~uint8_t(1));
				}
			}
			*ret = gv_nil();
			return 1;
		}
		if (name_is(name, "load_animations")) {
			*ret = gv_bool(load_pack_slice(pack, "ANIM"));
			return 1;
		}
		if (name_is(name, "unload_animations")) {
			*ret = gv_bool(unload_pack_slice(pack, "ANIM"));
			return 1;
		}
		if (name_is(name, "is_animations_loaded")) {
			*ret = gv_bool(pack >= 0 && pack < g_npack && (g_packs[pack].anim_resident || (pack == 0 && g_nclip > 0)));
			return 1;
		}
		if (name_is(name, "can_load_animations")) {
			if (pack < 0 || pack >= g_npack) {
				*ret = gv_bool(0);
				return 1;
			}
			if (g_packs[pack].anim_resident || (pack == 0 && g_nclip > 0)) {
				*ret = gv_bool(1);
				return 1;
			}
			*ret = gv_bool(try_read_pack_blob(pack, "ANIM", g_load_buf, 8) > 0);
			return 1;
		}
		if (name_is(name, "load_sprites")) {
			*ret = gv_bool(load_pack_slice(pack, "SPRITE"));
			return 1;
		}
		if (name_is(name, "unload_sprites")) {
			*ret = gv_bool(unload_pack_slice(pack, "SPRITE"));
			return 1;
		}
		if (name_is(name, "is_sprites_loaded")) {
			*ret = gv_bool(pack >= 0 && pack < g_npack && (g_packs[pack].sprite_resident || (pack == 0 && g_nspr > 0)));
			return 1;
		}
		if (name_is(name, "can_load_sprites")) {
			if (pack < 0 || pack >= g_npack) {
				*ret = gv_bool(0);
				return 1;
			}
			if (g_packs[pack].sprite_resident || (pack == 0 && g_nspr > 0)) {
				*ret = gv_bool(1);
				return 1;
			}
			*ret = gv_bool(try_read_pack_blob(pack, "SPRITE", g_load_buf, 8) > 0);
			return 1;
		}
		if (name_is(name, "load_hitboxes")) {
			*ret = gv_bool(load_pack_slice(pack, "HIT"));
			return 1;
		}
		if (name_is(name, "unload_hitboxes")) {
			*ret = gv_bool(unload_pack_slice(pack, "HIT"));
			return 1;
		}
		if (name_is(name, "is_hitboxes_loaded")) {
			*ret = gv_bool(pack >= 0 && pack < g_npack && (g_packs[pack].hit_resident || (pack == 0 && g_nhit > 0)));
			return 1;
		}
		if (name_is(name, "can_load_hitboxes")) {
			if (pack < 0 || pack >= g_npack) {
				*ret = gv_bool(0);
				return 1;
			}
			if (g_packs[pack].hit_resident || (pack == 0 && g_nhit > 0)) {
				*ret = gv_bool(1);
				return 1;
			}
			*ret = gv_bool(try_read_pack_blob(pack, "HIT", g_load_buf, 8) > 0);
			return 1;
		}
		if (name_is(name, "load_cameras")) {
			*ret = gv_bool(load_pack_slice(pack, "CAM"));
			return 1;
		}
		if (name_is(name, "unload_cameras")) {
			*ret = gv_bool(unload_pack_slice(pack, "CAM"));
			return 1;
		}
		if (name_is(name, "is_cameras_loaded")) {
			*ret = gv_bool(pack >= 0 && pack < g_npack && (g_packs[pack].cam_resident || (pack == 0 && g_ncam > 0)));
			return 1;
		}
		if (name_is(name, "can_load_cameras")) {
			if (pack < 0 || pack >= g_npack) {
				*ret = gv_bool(0);
				return 1;
			}
			if (g_packs[pack].cam_resident || (pack == 0 && g_ncam > 0)) {
				*ret = gv_bool(1);
				return 1;
			}
			*ret = gv_bool(try_read_pack_blob(pack, "CAM", g_load_buf, 8) > 0);
			return 1;
		}
		if (name_is(name, "is_action_pressed_on") || name_is(name, "is_action_just_pressed_on") ||
				name_is(name, "is_action_just_released_on")) {
			const int dev = argv && argc > 0 ? int(as_float(argv[0])) : 0;
			const char *act = argv && argc > 1 && argv[1].type == V_STR ? argv[1].s : "";
			int just = 0;
			if (name_is(name, "is_action_just_pressed_on")) {
				just = 1;
			} else if (name_is(name, "is_action_just_released_on")) {
				just = 2;
			}
			*ret = gv_bool(dev == 0 || dev == 1 ? pad_pressed_on(host, act, just, dev) : 0);
			return 1;
		}
		if (name_is(name, "memcard_present") || name_is(name, "memcard_ready")) {
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "memcard_format")) {
			g_mc_len = 0;
			g_mc_var.type = V_NIL;
			g_mc_var.s[0] = 0;
			mc_persist();
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "memcard_exists")) {
			*ret = gv_bool(g_mc_len > 0);
			return 1;
		}
		if (name_is(name, "memcard_delete")) {
			g_mc_len = 0;
			g_mc_var.type = V_NIL;
			g_mc_var.s[0] = 0;
			mc_persist();
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "memcard_save")) {
			if (argv && argc > 0 && argv[0].type == V_STR) {
				copy_str(g_mc_title, 32, argv[0].s);
			}
			if (argv && argc >= 3 && argv[2].type == V_STR) {
				g_mc_var.type = V_STR;
				copy_str(g_mc_var.s, 32, argv[2].s);
				*ret = gv_bool(mc_wrap_ok((const uint8_t *)argv[2].s, int(strlen(argv[2].s))));
			} else {
				*ret = gv_bool(g_mc_len >= 0);
			}
			mc_persist();
			return 1;
		}
		if (name_is(name, "memcard_load")) {
			GVar s = gv_nil();
			s.type = V_STR;
			copy_str(s.s, 32, g_mc_title);
			*ret = s;
			return 1;
		}
		if (name_is(name, "memcard_count")) {
			*ret = gv_int(g_mc_len > 0 ? 1 : 0);
			return 1;
		}
		if (name_is(name, "memcard_name")) {
			GVar s = gv_nil();
			s.type = V_STR;
			copy_str(s.s, 32, "BESCES-00000SAVE");
			*ret = s;
			return 1;
		}
		if (name_is(name, "memcard_wrap") || name_is(name, "memcard_unwrap") ||
				name_is(name, "open") || name_is(name, "close") || name_is(name, "file_exists") ||
				name_is(name, "store_buffer") || name_is(name, "get_buffer") ||
				name_is(name, "store_string") || name_is(name, "get_as_text") ||
				name_is(name, "store_var") || name_is(name, "get_var") ||
				name_is(name, "get_length") || name_is(name, "get_file_as_bytes")) {
			if (name_is(name, "file_exists") || name_is(name, "get_length")) {
				*ret = name_is(name, "get_length") ? gv_int(g_mc_len) : gv_bool(g_mc_len > 0);
				return 1;
			}
			if (name_is(name, "store_var") && argv && argc > 0) {
				g_mc_var = argv[0];
				mc_encode_var();
				mc_persist();
				*ret = gv_bool(1);
				return 1;
			}
			if (name_is(name, "store_buffer")) {
				int off = 0;
				const char *s = "";
				if (argv && argc >= 2 && argv[1].type == V_STR) {
					off = int(as_float(argv[0]));
					s = argv[1].s;
				} else if (argv && argc >= 1 && argv[0].type == V_STR) {
					s = argv[0].s;
				}
				if (!mc_off_ok(off)) {
					*ret = gv_bool(0);
					return 1;
				}
				int n = 0;
				while (s[n] && off + n < 24576) {
					g_mc_payload[off + n] = uint8_t(s[n]);
					n++;
				}
				if (g_mc_len < off + n) {
					g_mc_len = off + n;
				}
				mc_persist();
				*ret = gv_bool(1);
				return 1;
			}
			if (name_is(name, "get_buffer")) {
				const int off = argv && argc > 0 ? int(as_float(argv[0])) : 0;
				*ret = gv_int(mc_peek32(off));
				return 1;
			}
			if (name_is(name, "store_string") && argv && argc > 0 && argv[0].type == V_STR) {
				g_mc_var.type = V_STR;
				copy_str(g_mc_var.s, 32, argv[0].s);
				*ret = gv_bool(mc_wrap_ok((const uint8_t *)argv[0].s, int(strlen(argv[0].s))));
				mc_persist();
				return 1;
			}
			if (name_is(name, "memcard_wrap") && argv && argc > 0 && argv[0].type == V_STR) {
				*ret = gv_bool(mc_wrap_ok((const uint8_t *)argv[0].s, int(strlen(argv[0].s))));
				mc_persist();
				return 1;
			}
			if (name_is(name, "memcard_unwrap")) {
				if (g_mc_len > 0) {
					g_mc_var.type = V_STR;
					copy_str(g_mc_var.s, 32, (const char *)g_mc_payload);
				}
				*ret = gv_bool(g_mc_len > 0);
				return 1;
			}
			if (name_is(name, "get_var")) {
				*ret = g_mc_var;
				return 1;
			}
			if (name_is(name, "get_as_text")) {
				GVar s = gv_nil();
				s.type = V_STR;
				if (g_mc_var.type == V_STR) {
					copy_str(s.s, 32, g_mc_var.s);
				} else {
					copy_str(s.s, 32, (const char *)g_mc_payload);
				}
				*ret = s;
				return 1;
			}
			if (name_is(name, "open")) {
				const char *path = argv && argc > 0 && argv[0].type == V_STR ? argv[0].s : "";
				int ok = 0;
				int i = 0;
				const char *u = "user://";
				while (u[i] && path[i] == u[i]) {
					i++;
				}
				ok = u[i] == 0;
				g_mc_open = ok ? 1 : 0;
				*ret = gv_bool(ok);
				return 1;
			}
			if (name_is(name, "close")) {
				if (g_mc_open) {
					mc_persist();
				}
				g_mc_open = 0;
				*ret = gv_bool(1);
				return 1;
			}
			if (name_is(name, "get_file_as_bytes")) {
				*ret = gv_int(g_mc_open || g_mc_len > 0 ? mc_peek32(0) : 0);
				return 1;
			}
			*ret = gv_bool(0);
			return 1;
		}
	}
	if (name_is(name, "load")) {
		const char *path = "";
		if (argv && argc > 0 && argv[0].type == V_STR) {
			path = argv[0].s;
		}
		const int hit = find_pack_path(path);
		*ret = hit >= 0 ? gv_obj(kPackBase + hit) : gv_nil();
		return 1;
	}
	if (name_is(name, "instantiate")) {
		int pack = pack_id_of(node);
		if (pack < 0 && argv && argc > 0 && argv[0].type == V_OBJ) {
			pack = pack_id_of(argv[0].i);
		}
		const int hit = instantiate_pack(pack, node_ok(node) ? node : 0);
		*ret = hit >= 0 ? gv_obj(hit) : gv_nil();
		return 1;
	}
	if (name_is(name, "change_scene") || name_is(name, "change_scene_to_file") || name_is(name, "change_scene_fade")) {
		int pack = pack_id_of(node);
		if (argv && argc > 0) {
			if (argv[0].type == V_STR) {
				pack = find_pack_path(argv[0].s);
			} else if (argv[0].type == V_OBJ) {
				pack = pack_id_of(argv[0].i);
			}
		}
		if (name_is(name, "change_scene_fade")) {
			g_fade_a = 255;
			g_fade_out_dur = argv && argc > 1 ? as_float(argv[1]) : 0.5f;
			if (g_fade_out_dur < 0.01f) {
				g_fade_out_dur = 0.01f;
			}
			g_fade_out_left = g_fade_out_dur;
		}
		if (pack < 0 || !load_pack_resident(pack)) {
			*ret = gv_int(0);
			return 1;
		}
		activate_pack(pack, host);
		*ret = gv_int(1);
		return 1;
	}
	if (name_is(name, "queue_free")) {
		reclaim_instance(node);
		*ret = gv_nil();
		return 1;
	}
	if (name_is(name, "connect") || name_is(name, "disconnect")) {
		const char *sig = (argc > 0 && argv[0].type == V_STR) ? argv[0].s : "";
		uint8_t sid = 0;
		if (name_is(sig, "timeout")) {
			sid = SIG_TIMEOUT;
		} else if (name_is(sig, "pressed")) {
			sid = SIG_PRESSED;
		} else if (name_is(sig, "animation_finished")) {
			sid = SIG_ANIM;
		}
		int dest = node;
		const char *meth = "";
		if (argc >= 3 && argv[1].type == V_OBJ && argv[2].type == V_STR) {
			dest = argv[1].i;
			meth = argv[2].s;
		} else if (argc >= 2 && argv[1].type == V_STR) {
			meth = argv[1].s;
		} else if (argc >= 2 && argv[1].type == V_OBJ) {
			dest = argv[1].i;
		}
		int src = node;
		if (node >= kTimerBase && node < kTimerBase + 8) {
			src = node;
		}
		if (name_is(name, "disconnect")) {
			for (int i = 0; i < PS1_MAX_CONNS; i++) {
				if (g_conn[i].used && g_conn[i].src == src && g_conn[i].sig == sid && g_conn[i].dest == dest) {
					g_conn[i].used = 0;
				}
			}
			*ret = gv_nil();
			return 1;
		}
		if (!sid || !meth[0]) {
			*ret = gv_nil();
			return 1;
		}
		int slot = -1;
		for (int i = 0; i < PS1_MAX_CONNS; i++) {
			if (!g_conn[i].used) {
				slot = i;
				break;
			}
		}
		if (slot >= 0) {
			g_conn[slot].used = 1;
			g_conn[slot].src = src;
			g_conn[slot].sig = sid;
			g_conn[slot].dest = dest;
			copy_str(g_conn[slot].method, 24, meth);
		}
		*ret = gv_nil();
		return 1;
	}
	if (name_is(name, "play") || name_is(name, "stop") || name_is(name, "is_playing")) {
		const int typ = node_ok(node) ? int(g_nodes[node].type) : 0;
		if (typ == 8 || (!typ && host && host->play_vag && name_is(name, "play"))) {
			if (name_is(name, "play") && host && host->play_vag) {
				host->play_vag();
			} else if (name_is(name, "stop") && host && host->stop_vag) {
				host->stop_vag();
			}
			if (name_is(name, "is_playing")) {
				*ret = gv_bool(host && host->vag_playing ? host->vag_playing() : 0);
			} else {
				*ret = gv_nil();
			}
			return 1;
		}
		if (typ == 9) {
			if (name_is(name, "play") && host && host->play_fmv) {
				g_fmv_playing = 1;
				host->play_fmv();
				g_fmv_playing = 0;
			}
			*ret = name_is(name, "is_playing") ? gv_bool(g_fmv_playing) : gv_nil();
			return 1;
		}
		if (typ == 3) {
			const int si = int(g_nodes[node].sprite);
			if (si >= 0 && si < g_nspr) {
				if (name_is(name, "play")) {
					g_sprs[si].playing = 1;
				} else if (name_is(name, "stop")) {
					g_sprs[si].playing = 0;
				}
				*ret = name_is(name, "is_playing") ? gv_bool(g_sprs[si].playing) : gv_nil();
				return 1;
			}
		}
		if (typ == 7 || name_is(name, "play") || name_is(name, "stop") || name_is(name, "is_playing")) {
			if (name_is(name, "play")) {
				const char *clip = "";
				if (argv && argc > 0 && argv[0].type == V_STR) {
					clip = argv[0].s;
				} else if (argv && argc > 1 && argv[1].type == V_STR) {
					clip = argv[1].s;
				}
				int id = clip[0] ? find_clip_name(clip) : (g_nclip ? 0 : -1);
				if (id >= 0) {
					g_anim_clip = id;
					g_anim_ms = 0.0f;
					g_anim_playing = 1;
				}
			} else if (name_is(name, "stop")) {
				g_anim_playing = 0;
			}
			*ret = name_is(name, "is_playing") ? gv_bool(g_anim_playing) : gv_nil();
			return 1;
		}
	}
	if (name_is(name, "get_parent")) {
		if (node_ok(node) && g_nodes[node].parent >= 0) {
			*ret = gv_obj(g_nodes[node].parent);
		} else {
			*ret = gv_nil();
		}
		return 1;
	}
	if (name_is(name, "get_child_count")) {
		*ret = gv_int(child_count_of(node));
		return 1;
	}
	if (name_is(name, "get_child")) {
		int idx = int(arg);
		if (argv && argc > 0) {
			if (argv[0].type != V_OBJ) {
				idx = int(as_float(argv[0]));
			} else if (argc > 1) {
				idx = int(as_float(argv[1]));
			}
		}
		const int hit = child_at(node, idx);
		*ret = hit >= 0 ? gv_obj(hit) : gv_nil();
		return 1;
	}
	if (name_is(name, "is_visible")) {
		*ret = gv_bool(node_ok(node) && (g_nodes[node].flags & 1));
		return 1;
	}
	if (name_is(name, "has_node") || name_is(name, "get_node_or_null") || name_is(name, "get_node")) {
		const char *path = "";
		if (argv && argc > 0 && argv[0].type == V_STR) {
			path = argv[0].s;
		} else if (argv && argc > 1 && argv[1].type == V_STR) {
			path = argv[1].s;
		}
		const int hit = walk_path(node, path);
		if (name_is(name, "has_node")) {
			*ret = gv_bool(hit >= 0);
		} else {
			*ret = hit >= 0 ? gv_obj(hit) : gv_nil();
		}
		return 1;
	}
	if (name_is(name, "add_to_group") || name_is(name, "remove_from_group") || name_is(name, "is_in_group") ||
			name_is(name, "count_in_group") || name_is(name, "get_in_group") || name_is(name, "get_first_in_group") ||
			name_is(name, "find_nearest") || name_is(name, "set_billboard") || name_is(name, "get_billboard") ||
			name_is(name, "get_floor_node") || name_is(name, "get_floor_normal") || name_is(name, "make_current")) {
		int nid = node_ok(node) ? node : -1;
		int bit = -1;
		if (name_is(name, "add_to_group") || name_is(name, "remove_from_group") || name_is(name, "is_in_group")) {
			if (argv && argc >= 2) {
				if (argv[0].type == V_OBJ) {
					nid = argv[0].i;
				}
				bit = group_bit_arg(argv, argc, 1);
			} else {
				bit = group_bit_arg(argv, argc, 0);
			}
			if (!node_ok(nid) || bit < 0) {
				*ret = gv_bool(0);
				return 1;
			}
			const uint8_t mask = uint8_t(1u << bit);
			if (name_is(name, "add_to_group")) {
				g_group[nid] = uint8_t(g_group[nid] | mask);
				*ret = gv_bool(1);
			} else if (name_is(name, "remove_from_group")) {
				g_group[nid] = uint8_t(g_group[nid] & uint8_t(~mask));
				*ret = gv_bool(1);
			} else {
				*ret = gv_bool(g_group[nid] & mask);
			}
			return 1;
		}
		if (name_is(name, "count_in_group") || name_is(name, "get_first_in_group") || name_is(name, "get_in_group")) {
			bit = group_bit_arg(argv, argc, 0);
			if (bit < 0) {
				*ret = name_is(name, "count_in_group") ? gv_int(0) : gv_nil();
				return 1;
			}
			const uint8_t mask = uint8_t(1u << bit);
			int want = 0;
			if (name_is(name, "get_in_group") && argv && argc > 1) {
				want = int(as_float(argv[1]));
			}
			int n = 0;
			int first = -1;
			int pick = -1;
			for (int i = 0; i < g_nnode; i++) {
				if (!g_node_used[i] || !(g_group[i] & mask) || !(g_nodes[i].flags & 1)) {
					continue;
				}
				if (first < 0) {
					first = i;
				}
				if (n == want) {
					pick = i;
				}
				n++;
			}
			if (name_is(name, "count_in_group")) {
				*ret = gv_int(n);
			} else if (name_is(name, "get_in_group")) {
				*ret = pick >= 0 ? gv_obj(pick) : gv_nil();
			} else {
				*ret = first >= 0 ? gv_obj(first) : gv_nil();
			}
			return 1;
		}
		if (name_is(name, "find_nearest")) {
			int from = nid;
			if (argv && argc > 0 && argv[0].type == V_OBJ && node_ok(argv[0].i)) {
				from = argv[0].i;
				bit = group_bit_arg(argv, argc, 1);
			} else {
				bit = group_bit_arg(argv, argc, 0);
			}
			if (!node_ok(from) || bit < 0) {
				*ret = gv_nil();
				return 1;
			}
			const uint8_t mask = uint8_t(1u << bit);
			const int dim2 = g_nodes[from].type == 3 || g_nodes[from].type == 5 || g_nodes[from].type == 6;
			int best = -1;
			int best_d = 0x7fffffff;
			for (int i = 0; i < g_nnode; i++) {
				if (i == from || !g_node_used[i] || !(g_group[i] & mask) || !(g_nodes[i].flags & 1)) {
					continue;
				}
				const int dx = int(g_nodes[i].px) - int(g_nodes[from].px);
				const int dy = int(g_nodes[i].py) - int(g_nodes[from].py);
				const int dz = dim2 ? 0 : (int(g_nodes[i].pz) - int(g_nodes[from].pz));
				const int d = dx * dx + dy * dy + dz * dz;
				if (d < best_d) {
					best_d = d;
					best = i;
				}
			}
			*ret = best >= 0 ? gv_obj(best) : gv_nil();
			return 1;
		}
		if (name_is(name, "set_billboard") || name_is(name, "get_billboard")) {
			if (argv && argc > 0 && argv[0].type == V_OBJ) {
				nid = argv[0].i;
			}
			if (!node_ok(nid)) {
				*ret = gv_bool(0);
				return 1;
			}
			const int si = int(g_nodes[nid].sprite);
			if (si < 0 || si >= g_nspr) {
				*ret = gv_bool(0);
				return 1;
			}
			if (name_is(name, "set_billboard")) {
				int on = 1;
				if (argv && argc > 0 && argv[0].type != V_OBJ) {
					on = as_truth(argv[0]);
				} else if (argv && argc > 1) {
					on = as_truth(argv[1]);
				}
				g_sprs[si].billboard = on ? 1 : 0;
				*ret = gv_bool(1);
			} else {
				*ret = gv_bool(g_sprs[si].billboard);
			}
			return 1;
		}
		if (name_is(name, "get_floor_node")) {
			*ret = g_floor_node >= 0 ? gv_obj(g_floor_node) : gv_nil();
			return 1;
		}
		if (name_is(name, "get_floor_normal")) {
			*ret = gv_v3(g_floor_nx, g_floor_ny, g_floor_nz);
			return 1;
		}
		if (name_is(name, "make_current")) {
			int camn = node;
			if (argv && argc > 0 && argv[0].type == V_OBJ) {
				camn = argv[0].i;
			}
			if (node_ok(camn)) {
				for (int c = 0; c < g_ncam; c++) {
					if (int(g_cams[c].node_id) == camn) {
						apply_cam_index(c, host);
						*ret = gv_bool(1);
						return 1;
					}
				}
			}
			*ret = gv_bool(0);
			return 1;
		}
	}
	apply_method(host, node, name, arg, argv, argc);
	return 0;
}

static int pad_mask(const char *action) {
	if (name_is(action, "ui_left")) {
		return PAD_LEFT;
	}
	if (name_is(action, "ui_right")) {
		return PAD_RIGHT;
	}
	if (name_is(action, "ui_up")) {
		return PAD_UP;
	}
	if (name_is(action, "ui_down")) {
		return PAD_DOWN;
	}
	if (name_is(action, "ui_accept")) {
		return PAD_CROSS;
	}
	if (name_is(action, "ui_cancel")) {
		return PAD_CIRCLE;
	}
	if (name_is(action, "ui_select")) {
		return PAD_START;
	}
	if (name_is(action, "ui_focus_prev")) {
		return PAD_L1;
	}
	if (name_is(action, "ui_focus_next")) {
		return PAD_R1;
	}
	if (name_is(action, "ui_square")) {
		return PAD_SQUARE;
	}
	if (name_is(action, "ui_triangle")) {
		return PAD_TRIANGLE;
	}
	if (name_is(action, "ui_l2")) {
		return PAD_L2;
	}
	if (name_is(action, "ui_r2")) {
		return PAD_R2;
	}
	if (name_is(action, "ui_back")) {
		return PAD_SELECT;
	}
	for (int i = 0; i < g_naction; i++) {
		if (name_is(action, g_actions[i].name)) {
			return int(g_actions[i].mask);
		}
	}
	return 0;
}

static int pad_port_down(const uint8_t *raw, uint16_t mask) {
	if (!raw || !mask) {
		return 0;
	}
	const PADTYPE *pad = (const PADTYPE *)raw;
	if (pad->stat != 0) {
		return 0;
	}
	return !(pad->btn & mask);
}

static int pad_down(const ScriptVMHost *host, uint16_t mask) {
	if (!host || !mask) {
		return 0;
	}
	return pad_port_down(host->pad34, mask) || pad_port_down(host->pad34_1, mask);
}

static int pad_pressed_on(const ScriptVMHost *host, const char *action, int just, int device) {
	const uint16_t mask = uint16_t(pad_mask(action));
	if (!mask || !host) {
		return 0;
	}
	const uint8_t *raw = device == 1 ? host->pad34_1 : host->pad34;
	const int down = pad_port_down(raw, mask);
	const uint16_t prev = device == 1 ? g_prev_btn[1] : g_prev_btn[0];
	if (just == 2) {
		return !down && !(prev & mask);
	}
	if (!just) {
		return down;
	}
	return down && (prev & mask);
}

static int pad_pressed(const ScriptVMHost *host, const char *action, int just) {
	return pad_pressed_on(host, action, just, 0) || pad_pressed_on(host, action, just, 1);
}

static void pad_tick(const ScriptVMHost *host) {
	if (!host) {
		return;
	}
	if (host->pad34) {
		const PADTYPE *pad = (const PADTYPE *)host->pad34;
		if (pad->stat == 0) {
			g_prev_btn[0] = pad->btn;
		}
	}
	if (host->pad34_1) {
		const PADTYPE *pad = (const PADTYPE *)host->pad34_1;
		if (pad->stat == 0) {
			g_prev_btn[1] = pad->btn;
		}
	}
}

static GVar *slot(GVar *stack, int nstack, GVar *cvars, int nc, uint32_t addr) {
	const int type = int(addr >> 24);
	const int idx = int(addr & 0xffffffu);
	if (type == 0 && idx >= 0 && idx < nstack) {
		return &stack[idx];
	}
	if (type == 1 && idx >= 0 && idx < nc) {
		return &cvars[idx];
	}
	return nullptr;
}

static int is_call(int op) {
	return op == OP_CALL || op == OP_CALL_RETURN || op == OP_CALL_UTILITY || op == OP_CALL_UTILITY_VALIDATED ||
			op == OP_CALL_GDSCRIPT_UTILITY || op == OP_CALL_METHOD_BIND || op == OP_CALL_METHOD_BIND_RET ||
			op == OP_CALL_METHOD_BIND_VALIDATED_RETURN || op == OP_CALL_METHOD_BIND_VALIDATED_NO_RETURN ||
			op == OP_CALL_NATIVE_STATIC || op == OP_CALL_NATIVE_STATIC_VALIDATED_RETURN ||
			op == OP_CALL_NATIVE_STATIC_VALIDATED_NO_RETURN;
}

static int is_bind_call(int op) {
	return op == OP_CALL_METHOD_BIND || op == OP_CALL_METHOD_BIND_RET ||
			op == OP_CALL_METHOD_BIND_VALIDATED_RETURN || op == OP_CALL_METHOD_BIND_VALIDATED_NO_RETURN ||
			op == OP_CALL_NATIVE_STATIC || op == OP_CALL_NATIVE_STATIC_VALIDATED_RETURN ||
			op == OP_CALL_NATIVE_STATIC_VALIDATED_NO_RETURN;
}

static int is_input_name(const char *nm) {
	return name_is(nm, "is_action_pressed") || name_is(nm, "is_action_just_pressed") ||
			name_is(nm, "is_action_just_released");
}

static int input_just_mode(const char *nm) {
	if (name_is(nm, "is_action_just_released")) {
		return 2;
	}
	if (name_is(nm, "is_action_just_pressed")) {
		return 1;
	}
	return 0;
}

static int prop_named(const char *n, const char *w) {
	return name_is(n, w);
}

static ScriptVMHud *hud_for_node(int node) {
	for (int i = 0; i < g_nhud; i++) {
		if (int(g_hud[i].node_id) == node) {
			return &g_hud[i];
		}
	}
	return nullptr;
}

static int hud_index_for_node(int node) {
	for (int i = 0; i < g_nhud; i++) {
		if (int(g_hud[i].node_id) == node) {
			return i;
		}
	}
	return -1;
}

static void copy_str(char *dst, int cap, const char *src) {
	int k = 0;
	for (; k < cap - 1 && src[k]; k++) {
		dst[k] = src[k];
	}
	dst[k] = 0;
}

static void get_prop(int node, const char *n, GVar *d) {
	if (!d) {
		return;
	}
	if (prop_named(n, "PS1")) {
		*d = gv_obj(kPS1Id);
		return;
	}
	ScriptVMHud *h = hud_for_node(node);
	if (h) {
		if (prop_named(n, "text")) {
			d->type = V_STR;
			copy_str(d->s, 32, h->text);
			return;
		}
		if (prop_named(n, "disabled")) {
			*d = gv_bool(h->flags & 2);
			return;
		}
		if (prop_named(n, "pressed")) {
			*d = gv_bool(h->pressed);
			return;
		}
		if (prop_named(n, "button_pressed")) {
			*d = gv_bool(h->flags & 8);
			return;
		}
		if (prop_named(n, "value") || prop_named(n, "selected")) {
			*d = gv_int(h->value);
			return;
		}
		if (prop_named(n, "min_value")) {
			*d = gv_int(h->vmin);
			return;
		}
		if (prop_named(n, "max_value")) {
			*d = gv_int(h->vmax);
			return;
		}
		if (prop_named(n, "focus")) {
			*d = gv_bool(g_focus == hud_index_for_node(node));
			return;
		}
	}
	if (!node_ok(node)) {
		*d = gv_nil();
		return;
	}
	if (prop_named(n, "visible")) {
		*d = gv_bool(g_nodes[node].flags & 1);
		if (h) {
			*d = gv_bool(h->flags & 1);
		}
	} else if (prop_named(n, "name")) {
		d->type = V_STR;
		copy_str(d->s, 32, g_nodes[node].name);
	} else if (prop_named(n, "position")) {
		*d = gv_v3(float(g_nodes[node].px), float(-g_nodes[node].py), float(g_nodes[node].pz));
	} else if (prop_named(n, "rotation")) {
		const float s = (2.0f * 3.14159265f) / 4096.0f;
		*d = gv_v3(float(g_nodes[node].rx) * s, float(g_nodes[node].ry) * s, float(g_nodes[node].rz) * s);
	} else if (prop_named(n, "frame")) {
		const int si = int(g_nodes[node].sprite);
		if (si >= 0 && si < g_nspr) {
			*d = gv_int(g_sprs[si].frame);
		} else {
			*d = gv_int(0);
		}
	} else if (prop_named(n, "flip_h")) {
		const int si = int(g_nodes[node].sprite);
		*d = gv_bool(si >= 0 && si < g_nspr && g_sprs[si].flip_h);
	} else if (prop_named(n, "flip_v")) {
		const int si = int(g_nodes[node].sprite);
		*d = gv_bool(si >= 0 && si < g_nspr && g_sprs[si].flip_v);
	} else if (prop_named(n, "billboard")) {
		const int si = int(g_nodes[node].sprite);
		*d = gv_bool(si >= 0 && si < g_nspr && g_sprs[si].billboard);
	} else if (prop_named(n, "velocity")) {
		if (g_nodes[node].type == 3 || g_nodes[node].type == 5 || g_nodes[node].type == 6) {
			*d = gv_v2(g_nvel[node][0], g_nvel[node][1]);
		} else {
			*d = gv_v3(g_nvel[node][0], g_nvel[node][1], g_nvel[node][2]);
		}
	} else if (prop_named(n, "speed_scale")) {
		*d = gv_float(g_anim_speed);
	} else if (prop_named(n, "modulate")) {
		const int si = int(g_nodes[node].sprite);
		*d = gv_int(si >= 0 && si < g_nspr ? int(g_sprs[si].rgb) : 255);
	} else {
		*d = gv_nil();
	}
}

static void set_prop(int node, const char *n, const GVar *s) {
	if (!s) {
		return;
	}
	ScriptVMHud *h = hud_for_node(node);
	if (h) {
		if (prop_named(n, "text") && s->type == V_STR) {
			copy_str(h->text, 32, s->s);
			return;
		}
		if (prop_named(n, "disabled")) {
			if (as_truth(*s)) {
				h->flags = uint8_t(h->flags | 2);
			} else {
				h->flags = uint8_t(h->flags & ~uint8_t(2));
			}
			return;
		}
		if (prop_named(n, "pressed")) {
			h->pressed = as_truth(*s) ? 1 : 0;
			return;
		}
		if (prop_named(n, "button_pressed")) {
			if (as_truth(*s)) {
				h->flags = uint8_t(h->flags | 8);
			} else {
				h->flags = uint8_t(h->flags & ~uint8_t(8));
			}
			return;
		}
		if (prop_named(n, "value") || prop_named(n, "selected")) {
			h->value = int16_t(as_float(*s));
			if (h->kind == 7 && h->nitems) {
				int sel = int(h->value);
				if (sel < 0) {
					sel = 0;
				}
				if (sel >= int(h->nitems)) {
					sel = int(h->nitems) - 1;
				}
				h->value = int16_t(sel);
				copy_str(h->text, 32, h->items[sel]);
			}
			return;
		}
		if (prop_named(n, "min_value")) {
			h->vmin = int16_t(as_float(*s));
			return;
		}
		if (prop_named(n, "max_value")) {
			h->vmax = int16_t(as_float(*s));
			return;
		}
		if (prop_named(n, "focus")) {
			if (as_truth(*s)) {
				g_focus = hud_index_for_node(node);
			} else if (g_focus == hud_index_for_node(node)) {
				g_focus = -1;
			}
			return;
		}
		if (prop_named(n, "visible")) {
			if (as_truth(*s)) {
				h->flags = uint8_t(h->flags | 1);
			} else {
				h->flags = uint8_t(h->flags & ~uint8_t(1));
			}
		}
	}
	if (!node_ok(node)) {
		return;
	}
	if (prop_named(n, "visible")) {
		if (as_truth(*s)) {
			g_nodes[node].flags = uint8_t(g_nodes[node].flags | uint8_t(1));
		} else {
			g_nodes[node].flags = uint8_t(g_nodes[node].flags & ~uint8_t(1));
		}
	} else if (prop_named(n, "position")) {
		if (s->type == V_V3) {
			g_nodes[node].px = int16_t(s->x);
			g_nodes[node].py = int16_t(-s->y);
			g_nodes[node].pz = int16_t(s->z);
		}
	} else if (prop_named(n, "rotation")) {
		if (s->type == V_V3) {
			g_nodes[node].rx = int16_t(rad_to_ps1(s->x));
			g_nodes[node].ry = int16_t(rad_to_ps1(s->y));
			g_nodes[node].rz = int16_t(rad_to_ps1(s->z));
		}
	} else if (prop_named(n, "name") && s->type == V_STR) {
		copy_str(g_nodes[node].name, 32, s->s);
	} else if (prop_named(n, "frame")) {
		const int si = int(g_nodes[node].sprite);
		if (si >= 0 && si < g_nspr) {
			int f = int(as_float(*s));
			if (f < 0) {
				f = 0;
			}
			if (g_sprs[si].nframes && f >= int(g_sprs[si].nframes)) {
				f = int(g_sprs[si].nframes) - 1;
			}
			g_sprs[si].frame = uint8_t(f);
		}
	} else if (prop_named(n, "flip_h")) {
		const int si = int(g_nodes[node].sprite);
		if (si >= 0 && si < g_nspr) {
			g_sprs[si].flip_h = as_truth(*s) ? 1 : 0;
		}
	} else if (prop_named(n, "flip_v")) {
		const int si = int(g_nodes[node].sprite);
		if (si >= 0 && si < g_nspr) {
			g_sprs[si].flip_v = as_truth(*s) ? 1 : 0;
		}
	} else if (prop_named(n, "billboard")) {
		const int si = int(g_nodes[node].sprite);
		if (si >= 0 && si < g_nspr) {
			g_sprs[si].billboard = as_truth(*s) ? 1 : 0;
		}
	} else if (prop_named(n, "velocity")) {
		if (s->type == V_V3) {
			g_nvel[node][0] = s->x;
			g_nvel[node][1] = s->y;
			g_nvel[node][2] = s->z;
		} else if (s->type == V_V2) {
			g_nvel[node][0] = s->x;
			g_nvel[node][1] = s->y;
			g_nvel[node][2] = 0;
		} else {
			g_nvel[node][0] = as_float(*s);
		}
	} else if (prop_named(n, "speed_scale")) {
		g_anim_speed = as_float(*s);
		if (g_anim_speed == 0.0f) {
			g_anim_speed = 1.0f;
		}
	} else if (prop_named(n, "modulate")) {
		const int si = int(g_nodes[node].sprite);
		if (si >= 0 && si < g_nspr) {
			int v = int(as_float(*s));
			if (v < 0) {
				v = 0;
			}
			if (v > 255) {
				v = 255;
			}
			g_sprs[si].rgb = uint8_t(v);
		}
	}
}

static int run_iterate(int op, const uint8_t *codeb, int ip, GVar *stack, int nstack, GVar *cvars, int nc) {
	const uint32_t ca = uint32_t(ri32(codeb + (ip + 1) * 4));
	const uint32_t ba = uint32_t(ri32(codeb + (ip + 2) * 4));
	const uint32_t ia = uint32_t(ri32(codeb + (ip + 3) * 4));
	const int jumpto = ri32(codeb + (ip + 4) * 4);
	GVar *counter = slot(stack, nstack, cvars, nc, ca);
	GVar *container = slot(stack, nstack, cvars, nc, ba);
	GVar *iterator = slot(stack, nstack, cvars, nc, ia);
	if (!counter || !container || !iterator) {
		return ip + 5;
	}
	const int begin = (op == OP_ITERATE_BEGIN || op == OP_ITERATE_BEGIN_INT);
	const int size = int(as_float(*container));
	if (begin) {
		*counter = gv_int(0);
		if (size > 0) {
			*iterator = gv_int(0);
			return ip + 5;
		}
		return jumpto;
	}
	int c = int(as_float(*counter)) + 1;
	*counter = gv_int(c);
	if (c < size) {
		*iterator = gv_int(c);
		return ip + 5;
	}
	return jumpto;
}

static int hud_focusable(const ScriptVMHud *h) {
	if (!h || !(h->flags & 1) || (h->flags & 2)) {
		return 0;
	}
	return h->kind == 3 || h->kind == 4 || h->kind == 5 || h->kind == 6 || h->kind == 7 || h->kind == 8 || h->kind == 11 || h->kind == 12;
}

static int hud_next_focus(int dir) {
	int start = g_focus;
	if (start < 0) {
		start = dir > 0 ? -1 : g_nhud;
	}
	for (int step = 1; step <= g_nhud; step++) {
		int i = start + dir * step;
		if (i < 0) {
			i += g_nhud;
		}
		if (i >= g_nhud) {
			i -= g_nhud;
		}
		if (hud_focusable(&g_hud[i])) {
			return i;
		}
	}
	return g_focus;
}

void script_vm_hud_tick(const ScriptVMHost *host) {
	int any = 0;
	for (int i = 0; i < g_nhud; i++) {
		if (hud_focusable(&g_hud[i])) {
			any = 1;
			break;
		}
	}
	if (host) {
		((ScriptVMHost *)host)->hud_focus_blocks_cam = any;
	}
	if (!any) {
		g_focus = -1;
		return;
	}
	if (g_focus < 0 || !hud_focusable(&g_hud[g_focus])) {
		g_focus = hud_next_focus(1);
		if (g_focus < 0) {
			for (int i = 0; i < g_nhud; i++) {
				if (hud_focusable(&g_hud[i])) {
					g_focus = i;
					break;
				}
			}
		}
	}
	const int up = pad_pressed(host, "ui_up", 1);
	const int down = pad_pressed(host, "ui_down", 1);
	const int left = pad_pressed(host, "ui_left", 1);
	const int right = pad_pressed(host, "ui_right", 1);
	const int accept = pad_pressed(host, "ui_accept", 1);
	if (g_focus >= 0 && g_focus < g_nhud) {
		ScriptVMHud *h = &g_hud[g_focus];
		if (h->kind == 11) {
			const int step = 1;
			if (left) {
				h->value = int16_t(h->value - step);
			}
			if (right) {
				h->value = int16_t(h->value + step);
			}
			if (h->value < h->vmin) {
				h->value = h->vmin;
			}
			if (h->value > h->vmax) {
				h->value = h->vmax;
			}
		} else if (h->kind == 12) {
			const int step = 1;
			if (up) {
				h->value = int16_t(h->value + step);
			}
			if (down) {
				h->value = int16_t(h->value - step);
			}
			if (h->value < h->vmin) {
				h->value = h->vmin;
			}
			if (h->value > h->vmax) {
				h->value = h->vmax;
			}
		} else {
			if (down || right) {
				g_focus = hud_next_focus(1);
			} else if (up || left) {
				g_focus = hud_next_focus(-1);
			}
		}
	}
	if (accept && g_focus >= 0 && g_focus < g_nhud) {
		ScriptVMHud *h = &g_hud[g_focus];
		if (h->kind == 3 || h->kind == 4 || h->kind == 8) {
			if (h->flags & 4) {
				h->flags = uint8_t(h->flags ^ 8);
			} else {
				h->pressed = 1;
			}
		} else if (h->kind == 5 || h->kind == 6) {
			h->flags = uint8_t(h->flags ^ 8);
		} else if (h->kind == 7 && h->nitems) {
			int sel = int(h->value) + 1;
			if (sel >= int(h->nitems)) {
				sel = 0;
			}
			h->value = int16_t(sel);
			copy_str(h->text, 32, h->items[sel]);
		}
	}
}

static int run_official(const uint8_t *blob, size_t size, const char *want, float delta, const ScriptVMHost *host, int self_override, int script_filter) {
	if (size < 8 || blob[0] != 'G' || blob[1] != 'D' || blob[2] != 'B' || blob[3] != 'C') {
		return 0;
	}
	if (ru16(blob + 4) != PS1_COOK_ABI) {
		return 0;
	}
	const uint16_t nscripts = ru16(blob + 6);
	const uint8_t *p = blob + 8;
	const uint8_t *end = blob + size;
	int ran = 0;
	for (uint16_t s = 0; s < nscripts && p < end; s++) {
		if (p >= end) {
			break;
		}
		const uint8_t plen = *p++;
		if (p + plen > end) {
			break;
		}
		p += plen;
		if (p + 4 > end) {
			break;
		}
		const uint16_t owner = ru16(p);
		p += 2;
		char members[16][32];
		int nmem = 0;
		if (p + 2 > end) {
			break;
		}
		const uint16_t nmem_u = ru16(p);
		p += 2;
		for (uint16_t m = 0; m < nmem_u && p < end; m++) {
			const uint8_t sl = *p++;
			if (nmem < 16) {
				uint8_t n = sl < 31 ? sl : 31;
				for (uint8_t k = 0; k < n && p + k < end; k++) {
					members[nmem][k] = char(p[k]);
				}
				members[nmem][n] = 0;
				nmem++;
			}
			p += sl;
		}
		if (p + 2 > end) {
			break;
		}
		const uint16_t nfn = ru16(p);
		p += 2;
		if (script_filter >= 0 && int(s) != script_filter) {
			// still walk functions below to keep the cursor aligned — skip exec only
		}
		const int self_id = (self_override >= 0) ? self_override : ((owner == 0xffff) ? 0 : int(owner));
		for (uint16_t f = 0; f < nfn && p < end; f++) {
			const uint8_t nlen = *p++;
			if (p + nlen + 3 > end) {
				return ran;
			}
			char fname[40];
			uint8_t cpy = nlen < 39 ? nlen : 39;
			for (uint8_t k = 0; k < cpy; k++) {
				fname[k] = char(p[k]);
			}
			fname[cpy] = 0;
			p += nlen;
			p++; // argc
			const uint16_t stack_size = ru16(p);
			p += 2;
			const uint16_t ncode = ru16(p);
			p += 2;
			if (p + ncode * 4 > end) {
				return ran;
			}
			const uint8_t *codeb = p;
			p += ncode * 4;
			const uint16_t nconst = ru16(p);
			p += 2;
			GVar cvars[16];
			int nc = 0;
			for (uint16_t c = 0; c < nconst; c++) {
				if (p >= end) {
					break;
				}
				const uint8_t t = *p++;
				GVar v = gv_nil();
				if (t == 1) {
					v = gv_bool(*p++);
				} else if (t == 2) {
					v = gv_int(ri32(p));
					p += 4;
				} else if (t == 3) {
					v = gv_float(rf32(p));
					p += 4;
				} else if (t == 4 || t == 21) {
					const uint8_t sl = *p++;
					v.type = V_STR;
					uint8_t n = sl < 31 ? sl : 31;
					for (uint8_t k = 0; k < n; k++) {
						v.s[k] = char(p[k]);
					}
					v.s[n] = 0;
					p += sl;
				} else if (t == 5) {
					v.type = V_V2;
					v.x = rf32(p);
					v.y = rf32(p + 4);
					p += 8;
				} else if (t == 9) {
					v.type = V_V3;
					v.x = rf32(p);
					v.y = rf32(p + 4);
					v.z = rf32(p + 8);
					p += 12;
				}
				if (nc < 16) {
					cvars[nc++] = v;
				}
			}
			const uint16_t nnames = ru16(p);
			p += 2;
			char names[16][32];
			int nn = 0;
			for (uint16_t g = 0; g < nnames; g++) {
				if (p >= end) {
					break;
				}
				const uint8_t sl = *p++;
				if (nn < 16) {
					uint8_t n = sl < 31 ? sl : 31;
					for (uint8_t k = 0; k < n && p + k < end; k++) {
						names[nn][k] = char(p[k]);
					}
					names[nn][n] = 0;
					nn++;
				}
				p += sl;
			}
			const uint16_t nmeth = (p + 2 <= end) ? ru16(p) : 0;
			p += 2;
			char mnames[8][32];
			int nmethods = 0;
			for (uint16_t g = 0; g < nmeth; g++) {
				if (p >= end) {
					break;
				}
				const uint8_t sl = *p++;
				if (nmethods < 8) {
					uint8_t n = sl < 31 ? sl : 31;
					for (uint8_t k = 0; k < n && p + k < end; k++) {
						mnames[nmethods][k] = char(p[k]);
					}
					mnames[nmethods][n] = 0;
					nmethods++;
				}
				p += sl;
			}
			if (!name_is(fname, want)) {
				continue;
			}
			if (script_filter >= 0 && int(s) != script_filter) {
				continue;
			}
			if (self_override < 0 && !owner_node_visible(int(owner))) {
				continue;
			}
			const int nstack = stack_size > 4 && stack_size < 48 ? int(stack_size) : 16;
			GVar stack[48];
			for (int i = 0; i < 48; i++) {
				stack[i] = gv_nil();
			}
			stack[0] = gv_obj(self_id);
			stack[3] = gv_float(delta);
			int ip = 0;
			while (ip < int(ncode)) {
				const int32_t op = ri32(codeb + ip * 4);
				if (op == OP_END || op == OP_RETURN || op == OP_RETURN_TYPED_BUILTIN) {
					ran = 1;
					break;
				}
				if (op == OP_LINE || op == OP_BREAKPOINT) {
					ip += op == OP_LINE ? 2 : 1;
					continue;
				}
				if (op == OP_ASSIGN || op == OP_ASSIGN_TYPED_BUILTIN) {
					const uint32_t da = uint32_t(ri32(codeb + (ip + 1) * 4));
					const uint32_t sa = uint32_t(ri32(codeb + (ip + 2) * 4));
					GVar *d = slot(stack, nstack, cvars, nc, da);
					GVar *sv = slot(stack, nstack, cvars, nc, sa);
					if (d && sv) {
						*d = *sv;
					}
					ip += op == OP_ASSIGN ? 3 : 4;
					continue;
				}
				if (op == OP_CAST_TO_BUILTIN) {
					const uint32_t sa = uint32_t(ri32(codeb + (ip + 1) * 4));
					const uint32_t da = uint32_t(ri32(codeb + (ip + 2) * 4));
					const int typ = ri32(codeb + (ip + 3) * 4);
					GVar *sv = slot(stack, nstack, cvars, nc, sa);
					GVar *d = slot(stack, nstack, cvars, nc, da);
					if (sv && d) {
						if (typ == 2) {
							*d = gv_int(int(as_float(*sv)));
						} else if (typ == 3) {
							*d = gv_float(as_float(*sv));
						} else if (typ == 4) {
							GVar out = gv_nil();
							out.type = V_STR;
							if (sv->type == V_STR) {
								copy_str(out.s, 32, sv->s);
							} else {
								out.s[0] = 0;
							}
							*d = out;
						}
					}
					ip += 4;
					continue;
				}
				if (op == OP_ASSIGN_TRUE || op == OP_ASSIGN_FALSE || op == OP_ASSIGN_NULL) {
					const uint32_t da = uint32_t(ri32(codeb + (ip + 1) * 4));
					GVar *d = slot(stack, nstack, cvars, nc, da);
					if (d) {
						*d = op == OP_ASSIGN_NULL ? gv_nil() : gv_bool(op == OP_ASSIGN_TRUE);
					}
					ip += 2;
					continue;
				}
				if (op == OP_OPERATOR) {
					const uint32_t aa = uint32_t(ri32(codeb + (ip + 1) * 4));
					const uint32_t ba = uint32_t(ri32(codeb + (ip + 2) * 4));
					const uint32_t da = uint32_t(ri32(codeb + (ip + 3) * 4));
					const int vop = ri32(codeb + (ip + 4) * 4);
					GVar *a = slot(stack, nstack, cvars, nc, aa);
					GVar *b = slot(stack, nstack, cvars, nc, ba);
					GVar *d = slot(stack, nstack, cvars, nc, da);
					if (a && b && d) {
						const float fa = as_float(*a);
						const float fb = as_float(*b);
						if (vop == 6) {
							*d = gv_float(fa + fb);
						} else if (vop == 7) {
							*d = gv_float(fa - fb);
						} else if (vop == 8) {
							*d = gv_float(fa * fb);
						} else if (vop == 9 && fb != 0.0f) {
							*d = gv_float(fa / fb);
						} else if (vop <= 5) {
							int t = 0;
							if (vop == 0) {
								t = fa == fb;
							} else if (vop == 1) {
								t = fa != fb;
							} else if (vop == 2) {
								t = fa < fb;
							} else if (vop == 3) {
								t = fa <= fb;
							} else if (vop == 4) {
								t = fa > fb;
							} else {
								t = fa >= fb;
							}
							*d = gv_bool(t);
						}
					}
					ip += 5;
					continue;
				}
				if (op == OP_JUMP) {
					ip = ri32(codeb + (ip + 1) * 4);
					continue;
				}
				if (op == OP_JUMP_IF || op == OP_JUMP_IF_NOT || op == OP_JUMP_IF_SHARED) {
					const uint32_t a = uint32_t(ri32(codeb + (ip + 1) * 4));
					const int to = ri32(codeb + (ip + 2) * 4);
					GVar *sv = slot(stack, nstack, cvars, nc, a);
					const int t = sv ? as_truth(*sv) : 0;
					if ((op == OP_JUMP_IF && t) || (op != OP_JUMP_IF && !t)) {
						ip = to;
					} else {
						ip += 3;
					}
					continue;
				}
				if (op == OP_GET_NAMED || op == OP_SET_NAMED) {
					const uint32_t a0 = uint32_t(ri32(codeb + (ip + 1) * 4));
					const uint32_t a1 = uint32_t(ri32(codeb + (ip + 2) * 4));
					const int ni = ri32(codeb + (ip + 3) * 4);
					const char *pn = (ni >= 0 && ni < nn) ? names[ni] : "";
					GVar *o = slot(stack, nstack, cvars, nc, a0);
					if (o && (o->type == V_V2 || o->type == V_V3)) {
						GVar key = gv_nil();
						key.type = V_STR;
						copy_str(key.s, 32, pn);
						if (op == OP_GET_NAMED) {
							GVar *d = slot(stack, nstack, cvars, nc, a1);
							get_keyed(o, &key, d);
						} else {
							GVar *sv = slot(stack, nstack, cvars, nc, a1);
							set_keyed(o, &key, sv);
						}
					} else {
						int nid = self_id;
						if (o && o->type == V_OBJ) {
							nid = o->i;
						}
						if (op == OP_GET_NAMED) {
							GVar *d = slot(stack, nstack, cvars, nc, a1);
							get_prop(nid, pn, d);
						} else {
							GVar *sv = slot(stack, nstack, cvars, nc, a1);
							set_prop(nid, pn, sv);
						}
					}
					ip += 4;
					continue;
				}
				if (op == OP_GET_KEYED || op == OP_GET_KEYED_VALIDATED || op == OP_GET_INDEXED_VALIDATED) {
					const uint32_t a0 = uint32_t(ri32(codeb + (ip + 1) * 4));
					const uint32_t a1 = uint32_t(ri32(codeb + (ip + 2) * 4));
					const uint32_t a2 = uint32_t(ri32(codeb + (ip + 3) * 4));
					GVar *src = slot(stack, nstack, cvars, nc, a0);
					GVar *key = slot(stack, nstack, cvars, nc, a1);
					GVar *d = slot(stack, nstack, cvars, nc, a2);
					get_keyed(src, key, d);
					ip += (op == OP_GET_KEYED) ? 4 : 5;
					continue;
				}
				if (op == OP_SET_KEYED || op == OP_SET_KEYED_VALIDATED || op == OP_SET_INDEXED_VALIDATED) {
					const uint32_t a0 = uint32_t(ri32(codeb + (ip + 1) * 4));
					const uint32_t a1 = uint32_t(ri32(codeb + (ip + 2) * 4));
					const uint32_t a2 = uint32_t(ri32(codeb + (ip + 3) * 4));
					GVar *d = slot(stack, nstack, cvars, nc, a0);
					GVar *key = slot(stack, nstack, cvars, nc, a1);
					GVar *sv = slot(stack, nstack, cvars, nc, a2);
					set_keyed(d, key, sv);
					ip += (op == OP_SET_KEYED) ? 4 : 5;
					continue;
				}
				if (op == OP_GET_MEMBER || op == OP_SET_MEMBER) {
					const uint32_t a0 = uint32_t(ri32(codeb + (ip + 1) * 4));
					const int mi = ri32(codeb + (ip + 2) * 4);
					const char *pn = (mi >= 0 && mi < nmem) ? members[mi] : "";
					if (op == OP_GET_MEMBER) {
						GVar *d = slot(stack, nstack, cvars, nc, a0);
						get_prop(self_id, pn, d);
					} else {
						GVar *sv = slot(stack, nstack, cvars, nc, a0);
						set_prop(self_id, pn, sv);
					}
					ip += 3;
					continue;
				}
				if (op == OP_ITERATE_BEGIN || op == OP_ITERATE_BEGIN_INT || op == OP_ITERATE || op == OP_ITERATE_INT) {
					ip = run_iterate(op, codeb, ip, stack, nstack, cvars, nc);
					ran = 1;
					continue;
				}
				if (is_call(op)) {
					const int iac = ri32(codeb + (ip + 1) * 4);
					const int native = op == OP_CALL_NATIVE_STATIC || op == OP_CALL_NATIVE_STATIC_VALIDATED_RETURN ||
							op == OP_CALL_NATIVE_STATIC_VALIDATED_NO_RETURN;
					const int argc = native ? ri32(codeb + (ip + 3 + iac) * 4) : ri32(codeb + (ip + 2 + iac) * 4);
					const int namei = native ? ri32(codeb + (ip + 2 + iac) * 4) : ri32(codeb + (ip + 3 + iac) * 4);
					const char *meth = "rotate_y";
					if (is_bind_call(op) && namei >= 0 && namei < nmethods) {
						meth = mnames[namei];
					} else if (!is_bind_call(op) && namei >= 0 && namei < nn) {
						meth = names[namei];
					}
					int target = self_id;
					if (stack[0].type == V_OBJ) {
						target = stack[0].i;
					}
					GVar first = gv_nil();
					if (iac > 0) {
						const uint32_t aa = uint32_t(ri32(codeb + (ip + 2) * 4));
						GVar *sv = slot(stack, nstack, cvars, nc, aa);
						if (sv) {
							first = *sv;
							if (sv->type == V_OBJ) {
								target = sv->i;
							}
						}
					}
					float arg = 0.0f;
					GVar argv[4];
					int argvn = 0;
					for (int a = 0; a < argc && a < 4; a++) {
						const uint32_t aa = uint32_t(ri32(codeb + (ip + 2 + a) * 4));
						GVar *sv = slot(stack, nstack, cvars, nc, aa);
						if (sv) {
							argv[argvn++] = *sv;
							if (a == 0) {
								if (sv->type == V_STR) {
									arg = float(pad_pressed(host, sv->s, input_just_mode(meth)));
								} else {
									arg = as_float(*sv);
								}
							}
						}
					}
					const uint32_t ra = uint32_t(ri32(codeb + (ip + 3 + argc) * 4));
					GVar *dest = slot(stack, nstack, cvars, nc, ra);
					if (name_is(meth, "get_node")) {
						const char *path = (argvn && argv[0].type == V_STR) ? argv[0].s : "";
						const int hit = walk_path(self_id, path);
						if (hit < 0) {
							set_vm_err();
							return ran;
						}
						if (dest) {
							*dest = gv_obj(hit);
						}
					} else if (is_input_name(meth)) {
						if (dest) {
							*dest = gv_bool(int(arg));
						}
					} else {
						GVar retv = gv_nil();
						if (apply_call(host, target, meth, arg, argvn ? argv : nullptr, argvn, &retv) && dest) {
							*dest = retv;
						}
					}
					(void)first;
					ran = 1;
					ip += 4 + iac;
					continue;
				}
				if (op == OP_CONSTRUCT || op == OP_CONSTRUCT_VALIDATED) {
					const int iac = ri32(codeb + (ip + 1) * 4);
					const int argc = ri32(codeb + (ip + 2 + iac) * 4);
					const uint32_t da = uint32_t(ri32(codeb + (ip + 2 + argc) * 4));
					GVar *d = slot(stack, nstack, cvars, nc, da);
					if (d && argc == 1) {
						const uint32_t a0 = uint32_t(ri32(codeb + (ip + 2) * 4));
						GVar *x = slot(stack, nstack, cvars, nc, a0);
						const float fv = x ? as_float(*x) : 0.0f;
						const int typ = (op == OP_CONSTRUCT) ? ri32(codeb + (ip + 3 + iac) * 4) : 2;
						if (typ == 3) {
							*d = gv_float(fv);
						} else {
							*d = gv_int(int(fv));
						}
					} else if (d && argc >= 2) {
						const uint32_t a0 = uint32_t(ri32(codeb + (ip + 2) * 4));
						const uint32_t a1 = uint32_t(ri32(codeb + (ip + 3) * 4));
						GVar *x = slot(stack, nstack, cvars, nc, a0);
						GVar *y = slot(stack, nstack, cvars, nc, a1);
						if (argc >= 3) {
							const uint32_t a2 = uint32_t(ri32(codeb + (ip + 4) * 4));
							GVar *z = slot(stack, nstack, cvars, nc, a2);
							*d = gv_v3(x ? as_float(*x) : 0.0f, y ? as_float(*y) : 0.0f, z ? as_float(*z) : 0.0f);
						} else {
							*d = gv_v2(x ? as_float(*x) : 0.0f, y ? as_float(*y) : 0.0f);
						}
					}
					ip += 4 + iac;
					continue;
				}
				if (op == OP_TYPE_ADJUST_BOOL || op == OP_TYPE_ADJUST_INT || op == OP_TYPE_ADJUST_FLOAT ||
						op == OP_TYPE_ADJUST_STRING || op == OP_TYPE_ADJUST_VECTOR2 || op == OP_TYPE_ADJUST_VECTOR3 ||
						op == OP_TYPE_ADJUST_NODE_PATH || op == OP_TYPE_ADJUST_OBJECT ||
						op == OP_ASSERT || op == OP_JUMP_TO_DEF_ARGUMENT) {
					if (op == OP_ASSERT) {
						ip += 3;
					} else if (op == OP_JUMP_TO_DEF_ARGUMENT) {
						ip += 1;
					} else {
						ip += 2;
					}
					continue;
				}
				set_vm_err();
				break;
			}
		}
	}
	return ran;
}

static const char *tape_action(char names[][32], int n) {
	for (int i = 0; i < n; i++) {
		if (pad_mask(names[i])) {
			return names[i];
		}
	}
	return "";
}

static int run_tape(const uint8_t *blob, size_t size, const char *want, float delta, const ScriptVMHost *host, int self_override, int script_filter) {
	if (size < 8 || blob[0] != 'G' || blob[1] != 'D' || blob[2] != 'B' || blob[3] != 'C') {
		return 0;
	}
	if (ru16(blob + 4) != PS1_COOK_ABI) {
		return 0;
	}
	const uint16_t npaths = ru16(blob + 6);
	const uint8_t *p = blob + 8;
	const uint8_t *end = blob + size;
	uint16_t owners[16];
	int nown = 0;
	for (uint16_t i = 0; i < npaths; i++) {
		if (p >= end) {
			return 0;
		}
		const uint8_t nlen = *p++;
		p += nlen;
		if (p + 2 > end) {
			return 0;
		}
		const uint16_t owner = ru16(p);
		p += 2;
		if (nown < 16) {
			owners[nown++] = owner;
		}
	}
	if (p + 2 > end) {
		return 0;
	}
	const uint16_t nfunc = ru16(p);
	p += 2;
	int ran = 0;
	for (uint16_t f = 0; f < nfunc; f++) {
		if (p >= end) {
			break;
		}
		const uint8_t nlen = *p++;
		char fname[40];
		uint8_t cpy = nlen < 39 ? nlen : 39;
		for (uint8_t k = 0; k < cpy && p + k < end; k++) {
			fname[k] = char(p[k]);
		}
		fname[cpy] = 0;
		p += nlen;
		if (p + 5 > end) {
			break;
		}
		p++;
		const uint16_t nconst = ru16(p);
		p += 2;
		const uint16_t ncode = ru16(p);
		p += 2;
		float consts[8];
		for (uint16_t c = 0; c < nconst && c < 8; c++) {
			consts[c] = rf32(p);
			p += 4;
		}
		if (nconst > 8) {
			p += (nconst - 8) * 4;
		}
		if (p >= end) {
			break;
		}
		const uint8_t nnames = *p++;
		char names[8][32];
		uint8_t nstore = 0;
		for (uint8_t n = 0; n < nnames; n++) {
			const uint8_t sl = *p++;
			if (nstore < 8) {
				uint8_t cpy = sl < 31 ? sl : 31;
				for (uint8_t k = 0; k < cpy; k++) {
					names[nstore][k] = char(p[k]);
				}
				names[nstore][cpy] = 0;
				nstore++;
			}
			p += sl;
		}
		const uint8_t *code = p;
		p += ncode;
		if (!name_is(fname, want)) {
			continue;
		}
		if (script_filter >= 0 && int(f / 2) != script_filter) {
			continue;
		}
		const int owner = (f / 2 < nown) ? int(owners[f / 2]) : 0;
		if (self_override < 0 && !owner_node_visible(owner)) {
			continue;
		}
		const int self = (self_override >= 0) ? self_override : ((owner == 0xffff) ? 0 : owner);
		float stack[8];
		int sp = 0;
		size_t ip = 0;
		while (ip < ncode) {
			const uint8_t op = code[ip++];
			if (op == kTapeReturn) {
				break;
			}
			if (op == kTapeLoadConstF && ip < ncode) {
				const uint8_t ci = code[ip++];
				if (sp < 8 && ci < 8) {
					stack[sp++] = consts[ci];
				}
			} else if (op == kTapeLoadArg && ip < ncode) {
				ip++;
				if (sp < 8) {
					stack[sp++] = delta;
				}
			} else if (op == kTapeMul && sp >= 2) {
				const float b = stack[--sp];
				const float a = stack[--sp];
				stack[sp++] = a * b;
			} else if (op == kTapeAdd && sp >= 2) {
				const float b = stack[--sp];
				const float a = stack[--sp];
				stack[sp++] = a + b;
			} else if (op == kTapeCallSelf && ip < ncode) {
				const uint8_t ni = code[ip++];
				const float arg = sp > 0 ? stack[--sp] : 0.0f;
				const char *nm = (ni < nstore) ? names[ni] : "rotate_y";
				if (is_input_name(nm)) {
					if (sp < 8) {
						stack[sp++] = pad_pressed(host, tape_action(names, nstore), input_just_mode(nm)) ? 1.0f : 0.0f;
					}
				} else {
					apply_method(host, self, nm, arg, nullptr, 0);
				}
				ran = 1;
			} else if (op == kTapeCallNamed && ip < ncode) {
				const uint8_t ni = code[ip++];
				const char *nm = (ni < nstore) ? names[ni] : "";
				if (is_input_name(nm)) {
					if (sp < 8) {
						stack[sp++] = pad_pressed(host, tape_action(names, nstore), input_just_mode(nm)) ? 1.0f : 0.0f;
					}
				} else {
					apply_method(host, self, nm, 0.0f, nullptr, 0);
				}
				ran = 1;
			} else if (op == kTapeCallGetNode && ip < ncode) {
				const uint8_t ni = code[ip++];
				const char *path = (ni < nstore) ? names[ni] : "";
				if (walk_path(self, path) < 0) {
					set_vm_err();
				}
				ran = 1;
			}
		}
	}
	return ran;
}

static void emit_sig(int src, uint8_t sig, const ScriptVMHost *host, const uint8_t *tape, size_t tape_n) {
	for (int i = 0; i < PS1_MAX_CONNS; i++) {
		if (!g_conn[i].used || g_conn[i].sig != sig || g_conn[i].src != src) {
			continue;
		}
		const int dest = g_conn[i].dest;
		if (!node_ok(dest) || !(g_nodes[dest].flags & 1)) {
			continue;
		}
		const int si = int(g_nodes[dest].script);
		if (g_gdbc && g_gdbc_size) {
			run_official(g_gdbc, g_gdbc_size, g_conn[i].method, 0.0f, host, dest, si);
		}
		if (tape) {
			run_tape(tape, tape_n, g_conn[i].method, 0.0f, host, dest, si);
		}
	}
}

int script_vm_process(float delta, const ScriptVMHost *host) {
	if (!g_ready) {
		return 0;
	}
	g_host = host;
	if (!g_boot_slices && g_npack > 0) {
		g_boot_slices = 1;
		load_pack_slice(0, "NAV");
		load_pack_slice(0, "PATH");
		load_pack_slice(0, "WAY");
	}
	script_vm_hud_tick(host);
	int ran = 0;
	const uint8_t *tape = nullptr;
	size_t tape_n = 0;
	if (g_luau && g_luau_size > 8 && g_luau[0] == 'L' && g_luau[1] == 'U' && g_luau[2] == 'B' && g_luau[3] == 'C') {
		const uint16_t official = ru16(g_luau + 4);
		tape = g_luau + 6 + official;
		tape_n = g_luau_size - size_t(tape - g_luau);
	}
	int dispatched = 0;
	for (int n = 0; n < g_nnode; n++) {
		if (!g_node_used[n] || !(g_nodes[n].flags & 1) || g_nodes[n].script < 0) {
			continue;
		}
		dispatched = 1;
		const int si = int(g_nodes[n].script);
		if (!g_node_ready[n]) {
			if (g_gdbc && g_gdbc_size) {
				run_official(g_gdbc, g_gdbc_size, "_ready", 0.0f, host, n, si);
			}
			if (tape) {
				run_tape(tape, tape_n, "_ready", 0.0f, host, n, si);
			}
			g_node_ready[n] = 1;
		}
		if (!g_paused && !(g_nodes[n].flags & 2)) {
			if (g_gdbc && g_gdbc_size) {
				ran |= run_official(g_gdbc, g_gdbc_size, "_process", delta, host, n, si);
			}
			if (tape) {
				ran |= run_tape(tape, tape_n, "_process", delta, host, n, si);
			}
		}
	}
	if (!dispatched) {
		if (!g_did_ready) {
			if (g_gdbc && g_gdbc_size) {
				run_official(g_gdbc, g_gdbc_size, "_ready", 0.0f, host, -1, -1);
			}
			if (tape) {
				run_tape(tape, tape_n, "_ready", 0.0f, host, -1, -1);
			}
			g_did_ready = 1;
		}
		if (!g_paused) {
			if (g_gdbc && g_gdbc_size) {
				ran |= run_official(g_gdbc, g_gdbc_size, "_process", delta, host, -1, -1);
			}
			if (tape) {
				ran |= run_tape(tape, tape_n, "_process", delta, host, -1, -1);
			}
		}
	}
	g_ticks_ms += delta * 1000.0f;
	if (g_fade_out_left > 0) {
		g_fade_out_left -= delta;
		if (g_fade_out_left <= 0 || g_fade_out_dur <= 0) {
			g_fade_out_left = 0;
			g_fade_a = 0;
		} else {
			g_fade_a = int(255.0f * (g_fade_out_left / g_fade_out_dur));
		}
	}
	for (int i = 0; i < g_nnode; i++) {
		if (!g_node_used[i] || g_invuln_until[i] <= 0) {
			continue;
		}
		if (g_invuln_until[i] > g_ticks_ms) {
			if ((int(g_ticks_ms) / 80) & 1) {
				g_nodes[i].flags = uint8_t(g_nodes[i].flags & ~uint8_t(1));
			} else {
				g_nodes[i].flags = uint8_t(g_nodes[i].flags | uint8_t(1));
			}
		} else {
			g_nodes[i].flags = uint8_t(g_nodes[i].flags | uint8_t(1));
			g_invuln_until[i] = 0;
		}
	}
	for (int t = 0; t < 8; t++) {
		if (g_timer_used[t] && g_ticks_ms >= g_timer_end[t]) {
			emit_sig(kTimerBase + t, SIG_TIMEOUT, host, tape, tape_n);
			g_timer_used[t] = 0;
			for (int c = 0; c < PS1_MAX_CONNS; c++) {
				if (g_conn[c].used && g_conn[c].src == kTimerBase + t && g_conn[c].sig == SIG_TIMEOUT) {
					g_conn[c].used = 0;
				}
			}
		}
	}
	for (int h = 0; h < g_nhud; h++) {
		if (g_hud_used[h] && g_hud[h].pressed) {
			emit_sig(int(g_hud[h].node_id), SIG_PRESSED, host, tape, tape_n);
		}
	}
	tick_anim(delta);
	g_host = host;
	for (int s = 0; s < g_nspr; s++) {
		if (!g_sprs[s].playing || g_sprs[s].nframes <= 1 || g_sprs[s].fps == 0) {
			continue;
		}
		g_sprs[s].accum += delta * float(g_sprs[s].fps);
		while (g_sprs[s].accum >= 1.0f) {
			g_sprs[s].accum -= 1.0f;
			g_sprs[s].frame = uint8_t((int(g_sprs[s].frame) + 1) % int(g_sprs[s].nframes));
		}
	}
	if (!g_paused) {
		for (int n = 0; n < g_nnode; n++) {
			if (!g_node_used[n] || (g_nodes[n].flags & 8)) {
				continue;
			}
			if (g_ang[n][0] != 0 || g_ang[n][1] != 0 || g_ang[n][2] != 0) {
				g_nodes[n].rx = int16_t(g_nodes[n].rx + int16_t(rad_to_ps1(g_ang[n][0] * delta)));
				g_nodes[n].ry = int16_t(g_nodes[n].ry + int16_t(rad_to_ps1(g_ang[n][1] * delta)));
				g_nodes[n].rz = int16_t(g_nodes[n].rz + int16_t(rad_to_ps1(g_ang[n][2] * delta)));
			}
			if (g_scroll[n] && host && host->pos_x && host->pos_y) {
				const int f = int(g_scroll[n]);
				g_nodes[n].px = int16_t((*host->pos_x * f) / 256);
				g_nodes[n].py = int16_t((*host->pos_y * f) / 256);
			}
			if (g_kind[n] == NK_PATH && g_path_loaded) {
				const int pid = int(g_path_id[n]);
				if (pid >= 0 && pid < g_npath && g_pnpt[pid] >= 2) {
					g_path_prog[n] += g_path_spd[n] * delta;
					if (g_pclosed[pid]) {
						while (g_path_prog[n] > 1) {
							g_path_prog[n] -= 1;
						}
					} else if (g_path_prog[n] > 1) {
						g_path_prog[n] = 1;
					}
					const int last = int(g_pnpt[pid]) - 1;
					const float t = g_path_prog[n] * float(last);
					int i0 = int(t);
					if (i0 >= last) {
						i0 = last - 1;
					}
					if (i0 < 0) {
						i0 = 0;
					}
					const float u = t - float(i0);
					g_nodes[n].px = int16_t(float(g_ppx[pid][i0]) + (float(g_ppx[pid][i0 + 1]) - float(g_ppx[pid][i0])) * u);
					g_nodes[n].py = int16_t(-(float(g_ppy[pid][i0]) + (float(g_ppy[pid][i0 + 1]) - float(g_ppy[pid][i0])) * u));
					g_nodes[n].pz = int16_t(float(g_ppz[pid][i0]) + (float(g_ppz[pid][i0 + 1]) - float(g_ppz[pid][i0])) * u);
				}
			}
			if ((g_kind[n] == NK_RAY || g_kind[n] == NK_SHAPE) && g_cast_on[n]) {
				float fx, fy, fz, rx, ry, rz, ux, uy, uz;
				node_basis(n, &fx, &fy, &fz, &rx, &ry, &rz, &ux, &uy, &uz);
				g_cast_hit[n] = int16_t(do_raycast(float(g_nodes[n].px), float(-g_nodes[n].py), float(g_nodes[n].pz), fx, fy, fz, 64, 3, 0xFF, n, -1));
			}
			if (g_kind[n] == NK_REMOTE && node_ok(int(g_remote_tgt[n]))) {
				const int t = int(g_remote_tgt[n]);
				g_nodes[n].px = g_nodes[t].px;
				g_nodes[n].py = g_nodes[t].py;
				g_nodes[n].pz = g_nodes[t].pz;
				g_nodes[n].rx = g_nodes[t].rx;
				g_nodes[n].ry = g_nodes[t].ry;
				g_nodes[n].rz = g_nodes[t].rz;
			}
			if (g_kind[n] == NK_ARM) {
				const int par = int(g_nodes[n].parent);
				float ox = float(g_nodes[n].px);
				float oy = float(-g_nodes[n].py);
				float oz = float(g_nodes[n].pz);
				if (node_ok(par)) {
					ox = float(g_nodes[par].px);
					oy = float(-g_nodes[par].py);
					oz = float(g_nodes[par].pz);
				}
				float fx, fy, fz, rx, ry, rz, ux, uy, uz;
				node_basis(node_ok(par) ? par : n, &fx, &fy, &fz, &rx, &ry, &rz, &ux, &uy, &uz);
				const float len = float(g_path_id[n] ? g_path_id[n] * 4 : 64);
				const int hit = do_raycast(ox, oy, oz, -fx, -fy, -fz, len, 3, 0xFF, n, -1);
				if (hit >= 0 && g_ray_hit) {
					g_nodes[n].px = int16_t(g_ray_hx);
					g_nodes[n].py = int16_t(-g_ray_hy);
					g_nodes[n].pz = int16_t(g_ray_hz);
				} else {
					g_nodes[n].px = int16_t(ox - fx * len);
					g_nodes[n].py = int16_t(-(oy - fy * len));
					g_nodes[n].pz = int16_t(oz - fz * len);
				}
			}
			if (g_kind[n] == NK_LOOK && node_ok(int(g_look_tgt[n]))) {
				GVar a = gv_v3(float(g_nodes[g_look_tgt[n]].px), float(-g_nodes[g_look_tgt[n]].py), float(g_nodes[g_look_tgt[n]].pz));
				GVar dummy;
				apply_call(host, n, "look_at", 0, &a, 1, &dummy);
			}
			if (g_kind[n] == NK_AGENT && g_nav_loaded) {
				GVar a[2];
				a[0] = gv_v3(float(g_nodes[n].px), float(-g_nodes[n].py), float(g_nodes[n].pz));
				a[1] = gv_v3(g_agent_tx[n], g_agent_ty[n], g_agent_tz[n]);
				GVar hop;
				apply_call(host, n, "nav_next", 0, a, 2, &hop);
				if (hop.type == V_V3) {
					GVar args[3];
					args[0] = gv_obj(n);
					args[1] = hop;
					args[2] = gv_float(g_agent_spd[n]);
					GVar dummy;
					apply_call(host, n, "follow_node", 0, args, 3, &dummy);
				}
			}
			if (g_kind[n] == NK_NOTE || g_kind[n] == NK_ENAB) {
				int on = 1;
				if (host && host->pos_x && host->pos_y && host->pos_z) {
					const int dx = int(g_nodes[n].px) - int(*host->pos_x);
					const int dy = int(-g_nodes[n].py) - int(-*host->pos_y);
					const int dz = int(g_nodes[n].pz) - int(*host->pos_z);
					on = (dx * dx + dy * dy + dz * dz) < (220 * 220);
				}
				g_on_screen[n] = uint8_t(on);
				if (g_kind[n] == NK_ENAB) {
					if (on) {
						g_nodes[n].flags = uint8_t(g_nodes[n].flags & ~uint8_t(2));
					} else {
						g_nodes[n].flags = uint8_t(g_nodes[n].flags | 2);
					}
				}
			}
			if (g_cull_dist > 0 && host && host->pos_x) {
				const float dx = float(g_nodes[n].px) - float(*host->pos_x);
				const float dy = float(-g_nodes[n].py) - float(-*host->pos_y);
				const float dz = float(g_nodes[n].pz) - float(*host->pos_z);
				g_node_cull[n] = uint8_t((dx * dx + dy * dy + dz * dz) > (g_cull_dist * g_cull_dist));
			} else {
				g_node_cull[n] = 0;
			}
		}
		for (int i = 0; i < 8; i++) {
			if (!g_tween[i].used || g_tween[i].dur <= 0) {
				continue;
			}
			g_tween[i].t += delta;
			float u = g_tween[i].t / g_tween[i].dur;
			if (u > 1) {
				u = 1;
			}
			const int tn = int(g_tween[i].node);
			if (node_ok(tn)) {
				const float x = g_tween[i].from[0] + (g_tween[i].to[0] - g_tween[i].from[0]) * u;
				const float y = g_tween[i].from[1] + (g_tween[i].to[1] - g_tween[i].from[1]) * u;
				const float z = g_tween[i].from[2] + (g_tween[i].to[2] - g_tween[i].from[2]) * u;
				if (g_tween[i].prop == 1) {
					g_nodes[tn].rx = int16_t(x);
					g_nodes[tn].ry = int16_t(y);
					g_nodes[tn].rz = int16_t(z);
				} else if (g_tween[i].prop == 3) {
					g_nvel[tn][0] = x;
					g_nvel[tn][1] = y;
					g_nvel[tn][2] = z;
				} else {
					g_nodes[tn].px = int16_t(x);
					g_nodes[tn].py = int16_t(-y);
					g_nodes[tn].pz = int16_t(z);
				}
			}
			if (u >= 1) {
				if (g_tween[i].loop) {
					g_tween[i].t = 0;
				} else {
					g_tween[i].used = 0;
				}
			}
		}
		for (int s = 0; s < 32; s++) {
			if (!g_shot[s].used) {
				continue;
			}
			g_shot[s].x = int16_t(g_shot[s].x + g_shot[s].vx);
			g_shot[s].y = int16_t(g_shot[s].y + g_shot[s].vy);
			g_shot[s].z = int16_t(g_shot[s].z + g_shot[s].vz);
			if (g_shot[s].life) {
				g_shot[s].life--;
			}
			g_shot[s].hit = -1;
			for (int h = 0; h < g_nhit; h++) {
				if (!g_hit_used[h] || !(g_hits[h].flags & 1)) {
					continue;
				}
				if (g_shot[s].owner >= 0 && int(g_hits[h].node_id) == int(g_shot[s].owner)) {
					continue;
				}
				if (kit_invuln(int(g_hits[h].node_id))) {
					continue;
				}
				if (g_shot[s].x >= g_hits[h].min_x && g_shot[s].x <= g_hits[h].max_x &&
						g_shot[s].y >= g_hits[h].min_y && g_shot[s].y <= g_hits[h].max_y &&
						g_shot[s].z >= g_hits[h].min_z && g_shot[s].z <= g_hits[h].max_z) {
					g_shot[s].hit = g_hits[h].node_id;
					g_shot[s].used = 0;
					break;
				}
			}
			if (!g_shot[s].life) {
				g_shot[s].used = 0;
			}
		}
		for (int s = 0; s < PS1_MAX_STREAMS; s++) {
			if (!g_streams[s].used || !node_ok(g_streams[s].node)) {
				continue;
			}
			int step = 50 / (g_streams[s].rate ? int(g_streams[s].rate) : 1);
			if (step < 1) {
				step = 1;
			}
			g_streams[s].acc++;
			if (g_streams[s].acc >= uint8_t(step)) {
				g_streams[s].acc = 0;
				g_rng = g_rng * 1664525u + 1013904223u;
				const int spr = int(g_streams[s].spread);
				const float jx = spr ? float(int((g_rng >> 8) % (spr * 2 + 1)) - spr) : 0;
				g_rng = g_rng * 1664525u + 1013904223u;
				const float jy = spr ? float(int((g_rng >> 8) % (spr * 2 + 1)) - spr) : 0;
				const int nid = int(g_streams[s].node);
				part_birth(float(g_nodes[nid].px + g_streams[s].ox), float(-g_nodes[nid].py) + float(g_streams[s].oy),
						float(g_nodes[nid].pz + g_streams[s].oz), float(g_streams[s].vx) + jx, float(g_streams[s].vy) + jy,
						float(g_streams[s].vz), g_streams[s].tex, g_streams[s].mode, g_streams[s].life);
			}
		}
		for (int n = 0; n < g_nnode; n++) {
			if (g_node_used[n] && g_nodes[n].type == 12) {
				if (stream_of_node(n) < 0) {
					for (int i = 0; i < PS1_MAX_STREAMS; i++) {
						if (!g_streams[i].used) {
							g_streams[i].used = 1;
							g_streams[i].node = int16_t(n);
							g_streams[i].rate = 4;
							g_streams[i].acc = 0;
							g_streams[i].tex = 0;
							g_streams[i].mode = 0;
							g_streams[i].life = 20;
							g_streams[i].nframes = 1;
							g_streams[i].fps = 8;
							g_streams[i].vx = 0;
							g_streams[i].vy = 4;
							g_streams[i].vz = 0;
							g_streams[i].spread = 2;
							break;
						}
					}
				}
			}
		}
	}
	for (int p = 0; p < PS1_MAX_PARTICLES; p++) {
		if (!g_parts[p].life) {
			continue;
		}
		g_parts[p].px = g_parts[p].x;
		g_parts[p].py = g_parts[p].y;
		g_parts[p].pz = g_parts[p].z;
		g_parts[p].vx = int16_t(g_parts[p].vx + int16_t(g_grav_x));
		g_parts[p].vy = int16_t(g_parts[p].vy + int16_t(-g_grav_y));
		g_parts[p].vz = int16_t(g_parts[p].vz + int16_t(g_grav_z));
		g_parts[p].x = int16_t(g_parts[p].x + g_parts[p].vx);
		g_parts[p].y = int16_t(g_parts[p].y + g_parts[p].vy);
		g_parts[p].z = int16_t(g_parts[p].z + g_parts[p].vz);
		if (g_parts[p].nframes > 1 && g_parts[p].fps) {
			g_parts[p].ftick++;
			int step = 50 / int(g_parts[p].fps);
			if (step < 1) {
				step = 1;
			}
			if (g_parts[p].ftick >= uint8_t(step)) {
				g_parts[p].ftick = 0;
				g_parts[p].frame = uint8_t((int(g_parts[p].frame) + 1) % int(g_parts[p].nframes));
			}
		}
		g_parts[p].life--;
	}
	g_npart = PS1_MAX_PARTICLES;
	if (g_cam_attach >= 0 && node_ok(g_cam_attach) && host) {
		int32_t tx = int32_t(g_nodes[g_cam_attach].px + g_cam_offx);
		int32_t ty = int32_t(g_nodes[g_cam_attach].py + g_cam_offy);
		int32_t tz = int32_t(g_nodes[g_cam_attach].pz + g_cam_offz);
		int drag = 255;
		int dim2 = 0;
		int16_t lim[4] = { 0, 0, 0, 0 };
		int lim_on = 0;
		if (g_cam_cur >= 0 && g_cam_cur < g_ncam) {
			dim2 = g_cams[g_cam_cur].dim == 2;
			drag = g_cams[g_cam_cur].drag ? int(g_cams[g_cam_cur].drag) : 255;
			if (g_cams[g_cam_cur].lim_l || g_cams[g_cam_cur].lim_t || g_cams[g_cam_cur].lim_r || g_cams[g_cam_cur].lim_b) {
				lim[0] = g_cams[g_cam_cur].lim_l;
				lim[1] = g_cams[g_cam_cur].lim_t;
				lim[2] = g_cams[g_cam_cur].lim_r;
				lim[3] = g_cams[g_cam_cur].lim_b;
				lim_on = 1;
			}
		}
		if (g_cam_drag_live >= 0) {
			drag = g_cam_drag_live;
		}
		if (g_cam_lim_set) {
			lim[0] = g_cam_lim_live[0];
			lim[1] = g_cam_lim_live[1];
			lim[2] = g_cam_lim_live[2];
			lim[3] = g_cam_lim_live[3];
			lim_on = 1;
		}
		if (dim2 && host->rot_x) {
			*host->rot_x = 0;
		}
		if (host->pos_x) {
			if (drag < 255 && drag > 0) {
				*host->pos_x += (tx - *host->pos_x) * drag / 255;
			} else {
				*host->pos_x = tx;
			}
			if (lim_on) {
				if (*host->pos_x < lim[0]) {
					*host->pos_x = lim[0];
				}
				if (*host->pos_x > lim[2] && lim[2] != lim[0]) {
					*host->pos_x = lim[2];
				}
			}
		}
		if (host->pos_y) {
			if (drag < 255 && drag > 0) {
				*host->pos_y += (ty - *host->pos_y) * drag / 255;
			} else {
				*host->pos_y = ty;
			}
			if (lim_on) {
				if (*host->pos_y < lim[1]) {
					*host->pos_y = lim[1];
				}
				if (*host->pos_y > lim[3] && lim[3] != lim[1]) {
					*host->pos_y = lim[3];
				}
			}
		}
		if (host->pos_z && !dim2) {
			*host->pos_z = tz;
		}
		g_script_cam = 1;
		if (host->script_drives_cam) {
			*host->script_drives_cam = 1;
		}
	}
	if (g_shake_ms > 0.0f && host) {
		g_shake_ms -= delta * 1000.0f;
		if (g_shake_ms < 0.0f) {
			g_shake_ms = 0.0f;
			g_shake_amp = 0.0f;
		} else if (g_shake_amp > 0.0f) {
			g_rng = g_rng * 1664525u + 1013904223u;
			const int sx = int((g_rng >> 8) & 31) - 16;
			g_rng = g_rng * 1664525u + 1013904223u;
			const int sy = int((g_rng >> 8) & 31) - 16;
			if (host->pos_x) {
				*host->pos_x += int32_t(float(sx) * g_shake_amp / 16.0f);
			}
			if (host->pos_y) {
				*host->pos_y += int32_t(float(sy) * g_shake_amp / 16.0f);
			}
		}
	}
	if (g_anim_just_finished) {
		g_anim_just_finished = 0;
		for (int n = 0; n < g_nnode; n++) {
			if (g_node_used[n] && (g_nodes[n].flags & 1) && g_nodes[n].type == 7) {
				emit_sig(n, SIG_ANIM, host, tape, tape_n);
			}
		}
	}
	if (!g_paused) {
		if (dispatched) {
			for (int n = 0; n < g_nnode; n++) {
				if (!g_node_used[n] || !(g_nodes[n].flags & 1) || (g_nodes[n].flags & 8) || g_nodes[n].script < 0) {
					continue;
				}
				const int si = int(g_nodes[n].script);
				if (g_gdbc && g_gdbc_size) {
					ran |= run_official(g_gdbc, g_gdbc_size, "_physics_process", delta, host, n, si);
				}
				if (tape) {
					ran |= run_tape(tape, tape_n, "_physics_process", delta, host, n, si);
				}
			}
		} else {
			if (g_gdbc && g_gdbc_size) {
				ran |= run_official(g_gdbc, g_gdbc_size, "_physics_process", delta, host, -1, -1);
			}
			if (tape) {
				ran |= run_tape(tape, tape_n, "_physics_process", delta, host, -1, -1);
			}
		}
	}
	for (int i = 0; i < g_nhud; i++) {
		g_hud[i].pressed = 0;
	}
	pad_tick(host);
	return ran;
}

int script_vm_script_cam() {
	return g_script_cam;
}

void script_vm_get_fog(int *on, int *start, int *end, uint8_t *r, uint8_t *g, uint8_t *b) {
	if (on) {
		*on = g_fog_on;
	}
	if (start) {
		*start = g_fog_start;
	}
	if (end) {
		*end = g_fog_end;
	}
	if (r) {
		*r = g_fog_r;
	}
	if (g) {
		*g = g_fog_g;
	}
	if (b) {
		*b = g_fog_b;
	}
}

void script_vm_get_fade(int *a, uint8_t *r, uint8_t *g, uint8_t *b) {
	if (a) {
		*a = g_fade_a;
	}
	if (r) {
		*r = g_fade_r;
	}
	if (g) {
		*g = g_fade_g;
	}
	if (b) {
		*b = g_fade_b;
	}
}

int script_vm_particle_count() {
	return g_npart;
}

const ScriptVMParticle *script_vm_particles() {
	return g_parts;
}

const ScriptVMSprite *script_vm_sprites() {
	return g_sprs;
}

int script_vm_sprite_count() {
	return g_nspr;
}
