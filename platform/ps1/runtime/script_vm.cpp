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

static int pad_pressed_on(const ScriptVMHost *host, const char *action, int just, int device);

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
		g_hit_used[i] = 1;
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
	if (name_is(name, "print") || name_is(name, "push_warning") || name_is(name, "push_error") || name_is(name, "get_node")) {
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
	if (name_is(name, "look_at") && node_ok(node)) {
		float tx = arg;
		float ty = float(-g_nodes[node].py);
		float tz = 0.0f;
		if (argv && argc > 0 && argv[0].type == V_V3) {
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
		g_nodes[node].ry = int16_t(rad_to_ps1(yaw));
		g_nodes[node].rx = int16_t(rad_to_ps1(pitch));
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
	if (ru16(blob + 4) != 15) {
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
		if (p + 20 > end) {
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
		out[got++] = n;
	}
	return got;
}

static int parse_hud_into(const uint8_t *blob, int size, ScriptVMHud *out, int maxn) {
	if (!blob || size < 8 || blob[0] != 'H' || blob[1] != 'U' || blob[2] != 'D' || blob[3] != '0') {
		return -1;
	}
	if (ru16(blob + 4) != 15) {
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
	if (ru16(blob + 4) != 15) {
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

static int aabb_overlap(int a, int b) {
	if (a < 0 || b < 0 || a >= PS1_MAX_HITS || b >= PS1_MAX_HITS) {
		return 0;
	}
	if (!g_hit_used[a] || !g_hit_used[b]) {
		return 0;
	}
	if (!(g_hits[a].flags & 1) || !(g_hits[b].flags & 1)) {
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
	const int n = try_read_pack_blob(pack, kind, g_load_buf, int(sizeof(g_load_buf)));
	if (n <= 0) {
		return 0;
	}
	if (name_is(kind, "ANIM")) {
		if (g_nclip + 1 > PS1_MAX_CLIPS) {
			return 0;
		}
		g_packs[pack].anim_resident = 1;
		return 1;
	}
	if (name_is(kind, "SPRITE")) {
		g_packs[pack].sprite_resident = 1;
		return 1;
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
		if (g_load_buf[0] == 'H' && g_load_buf[1] == 'I' && g_load_buf[2] == 'T' && g_load_buf[3] == '0' && ru16(g_load_buf + 4) == 15) {
			const int nc = int(ru16(g_load_buf + 6));
			const uint8_t *p = g_load_buf + 8;
			for (int i = 0; i < nc && g_nhit < PS1_MAX_HITS && p + 18 <= g_load_buf + n; i++) {
				ScriptVMHit h{};
				h.node_id = int16_t(p[0] | (p[1] << 8));
				h.dim = p[2];
				h.kind = p[3];
				h.flags = p[4];
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
		if (g_load_buf[0] == 'C' && g_load_buf[1] == 'A' && g_load_buf[2] == 'M' && g_load_buf[3] == '0' && ru16(g_load_buf + 4) == 15) {
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
				if (p + 13 > end) {
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
				c.pack = uint8_t(pack);
				g_cams[g_ncam++] = c;
			}
			g_packs[pack].cam_resident = 1;
			return 1;
		}
		return 0;
	}
	return 0;
}

static int unload_pack_slice(int pack, const char *kind) {
	if (pack <= 0 || pack >= g_npack) {
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
	return 0;
}

static uint8_t g_mc_payload[24576];
static int g_mc_len = 0;
static char g_mc_title[32] = "BLAZIUM";

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

static void tick_anim(float delta) {
	if (!g_anim_playing || g_anim_clip < 0 || g_anim_clip >= g_nclip) {
		return;
	}
	g_anim_ms += delta * 1000.0f;
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
		g_anim_ms = float(tmax);
		if (g_anim_playing) {
			g_anim_just_finished = 1;
		}
		g_anim_playing = 0;
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
	if (name_is(name, "has_feature")) {
		const char *f = argv && argc > 0 && argv[0].type == V_STR ? argv[0].s : "";
		*ret = gv_bool(name_is(f, "ps1"));
		return 1;
	}
	if ((name_is(name, "get_position") || name_is(name, "get_global_position")) && node_ok(node)) {
		*ret = gv_v3(float(g_nodes[node].px), float(-g_nodes[node].py), float(g_nodes[node].pz));
		return 1;
	}
	if (name_is(name, "get_rotation") && node_ok(node)) {
		const float s = (2.0f * 3.14159265f) / 4096.0f;
		*ret = gv_v3(float(g_nodes[node].rx) * s, float(g_nodes[node].ry) * s, float(g_nodes[node].rz) * s);
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
			const int n = int(argc > 0 ? as_float(argv[0]) : arg);
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
				if (sx > -16 && sx < 16) {
					sx = 0;
				}
				if (sy > -16 && sy < 16) {
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
					if (dlt > -16 && dlt < 16) {
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
			name_is(name, "set_camera_transform") || name_is(name, "get_camera_transform") ||
			name_is(name, "attach_camera") || name_is(name, "look_camera") || name_is(name, "orbit_camera") ||
			name_is(name, "set_camera_scale") || name_is(name, "raycast") || name_is(name, "intersects_ray") ||
			name_is(name, "move_and_slide") || name_is(name, "tile_solid_at") || name_is(name, "tile_at") ||
			name_is(name, "load_audio") || name_is(name, "unload_audio") || name_is(name, "can_load_audio") ||
			name_is(name, "is_audio_loaded") || name_is(name, "play_sfx") || name_is(name, "stop_sfx") ||
			name_is(name, "set_sfx_volume") || name_is(name, "emit") || name_is(name, "set_fog") ||
			name_is(name, "set_fade") || name_is(name, "set_light") ||
			name_is(name, "overlaps") || name_is(name, "has_overlapping_areas") ||
			name_is(name, "get_overlapping_area_count") || name_is(name, "get_overlapping_area") ||
			name_is(name, "hitbox_kind") || name_is(name, "set_hitbox_enabled") ||
			name_is(name, "load_animations") || name_is(name, "unload_animations") ||
			name_is(name, "can_load_animations") || name_is(name, "is_animations_loaded") ||
			name_is(name, "load_sprites") || name_is(name, "unload_sprites") ||
			name_is(name, "can_load_sprites") || name_is(name, "is_sprites_loaded") ||
			name_is(name, "load_hitboxes") || name_is(name, "unload_hitboxes") || name_is(name, "can_load_hitboxes") ||
			name_is(name, "load_cameras") || name_is(name, "unload_cameras") ||
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
						if (sx > -16 && sx < 16) {
							sx = 0;
						}
						if (sy > -16 && sy < 16) {
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
		if (name_is(name, "get_camera_transform")) {
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
			if (host && host->pos_x) {
				*host->pos_x = int32_t(tx + sy * cp * dist);
			}
			if (host && host->pos_y) {
				*host->pos_y = int32_t(-(ty + sp * dist));
			}
			if (host && host->pos_z) {
				*host->pos_z = int32_t(tz + cy * cp * dist);
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
		if (name_is(name, "raycast") || name_is(name, "intersects_ray")) {
			float ox = 0, oy = 0, oz = 0, dx = 0, dy = 0, dz = 1, dist = 64;
			int dim = 3;
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
			int hitn = -1;
			float best = dist;
			for (int i = 0; i < g_nhit; i++) {
				if (!g_hit_used[i] || !(g_hits[i].flags & 1)) {
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
				}
			}
			*ret = hitn >= 0 ? gv_obj(hitn) : gv_nil();
			return 1;
		}
		if (name_is(name, "move_and_slide")) {
			int nid = node;
			float vx = 0, vy = 0, vz = 0;
			if (argv && argc >= 2 && argv[1].type == V_V3) {
				if (argv[0].type == V_OBJ) {
					nid = argv[0].i;
				} else {
					nid = int(as_float(argv[0]));
				}
				vx = argv[1].x;
				vy = argv[1].y;
				vz = argv[1].z;
			} else if (argv && argc >= 1 && argv[0].type == V_V3) {
				vx = argv[0].x;
				vy = argv[0].y;
				vz = argv[0].z;
			}
			if (!node_ok(nid)) {
				*ret = gv_v3(0, 0, 0);
				return 1;
			}
			const int hi = hit_of_node(nid);
			auto overlap_at = [&](int16_t px, int16_t py, int16_t pz) -> int {
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
						const int sep = tmp.max_x < g_hits[i].min_x || tmp.min_x > g_hits[i].max_x ||
								tmp.max_y < g_hits[i].min_y || tmp.min_y > g_hits[i].max_y ||
								(tmp.dim == 3 && (tmp.max_z < g_hits[i].min_z || tmp.min_z > g_hits[i].max_z));
						if (!sep) {
							return 1;
						}
					}
				}
				if (hi < 0 || g_hits[hi].dim == 2) {
					for (int t = 0; t < g_ntile; t++) {
						if (!g_tile_used[t] || !(g_tiles[t].flags & 1)) {
							continue;
						}
						const int16_t x = g_tiles[t].x;
						const int16_t y = g_tiles[t].y;
						if (px >= x && px < int16_t(x + 16) && (-py) >= y && (-py) < int16_t(y + 16)) {
							return 1;
						}
					}
				}
				return 0;
			};
			int16_t nx = int16_t(g_nodes[nid].px + vx);
			int16_t ny = g_nodes[nid].py;
			int16_t nz = g_nodes[nid].pz;
			if (overlap_at(nx, ny, nz)) {
				nx = g_nodes[nid].px;
				vx = 0;
			}
			ny = int16_t(g_nodes[nid].py - vy);
			if (overlap_at(nx, ny, nz)) {
				ny = int16_t(g_nodes[nid].py);
				vy = 0;
			}
			nz = int16_t(g_nodes[nid].pz + vz);
			if (overlap_at(nx, ny, nz)) {
				nz = g_nodes[nid].pz;
				vz = 0;
			}
			g_nodes[nid].px = nx;
			g_nodes[nid].py = ny;
			g_nodes[nid].pz = nz;
			*ret = gv_v3(vx, vy, vz);
			return 1;
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
		if (name_is(name, "can_load_audio") || name_is(name, "is_audio_loaded")) {
			*ret = gv_bool(pack >= 0 && pack < g_npack && (name_is(name, "is_audio_loaded") ? g_packs[pack].audio_resident : 1));
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
			if (argv && argc > 0 && argv[0].type == V_V3) {
				px = argv[0].x;
				py = argv[0].y;
				pz = argv[0].z;
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
			for (int i = 0; i < count; i++) {
				g_parts[i].x = int16_t(px);
				g_parts[i].y = int16_t(-py);
				g_parts[i].z = int16_t(pz);
				g_parts[i].life = 20;
				g_parts[i].tex = uint8_t(tex & 15);
			}
			g_npart = count;
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
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "overlaps")) {
			int a = node_ok(node) ? hit_of_node(node) : -1;
			int b = -1;
			if (argv && argc >= 2) {
				a = hit_of_node(int(as_float(argv[0])));
				b = hit_of_node(int(as_float(argv[1])));
			} else if (argv && argc == 1) {
				if (argv[0].type == V_OBJ) {
					b = hit_of_node(argv[0].i);
				} else {
					b = hit_of_node(int(as_float(argv[0])));
				}
			}
			*ret = gv_bool(aabb_overlap(a, b));
			return 1;
		}
		if (name_is(name, "has_overlapping_areas") || name_is(name, "get_overlapping_area_count")) {
			const int self = hit_of_node(node);
			int n = 0;
			for (int i = 0; i < g_nhit; i++) {
				if (i != self && aabb_overlap(self, i)) {
					n++;
				}
			}
			*ret = name_is(name, "has_overlapping_areas") ? gv_bool(n > 0) : gv_int(n);
			return 1;
		}
		if (name_is(name, "get_overlapping_area")) {
			const int self = hit_of_node(node);
			int want = argv && argc > 0 ? int(as_float(argv[0])) : 0;
			int n = 0;
			for (int i = 0; i < g_nhit; i++) {
				if (i != self && aabb_overlap(self, i)) {
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
		if (name_is(name, "can_load_animations") || name_is(name, "is_animations_loaded")) {
			*ret = gv_bool(pack >= 0 && pack < g_npack && (g_packs[pack].anim_resident || (pack == 0 && g_nclip > 0)));
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
		if (name_is(name, "can_load_sprites") || name_is(name, "is_sprites_loaded")) {
			*ret = gv_bool(pack >= 0 && pack < g_npack && (g_packs[pack].sprite_resident || pack == 0));
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
		if (name_is(name, "can_load_hitboxes")) {
			*ret = gv_bool(pack >= 0 && pack < g_npack);
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
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "memcard_exists")) {
			*ret = gv_bool(g_mc_len > 0);
			return 1;
		}
		if (name_is(name, "memcard_delete")) {
			g_mc_len = 0;
			*ret = gv_bool(1);
			return 1;
		}
		if (name_is(name, "memcard_save")) {
			if (argv && argc >= 3 && argv[2].type == V_STR) {
				*ret = gv_bool(mc_wrap_ok((const uint8_t *)argv[2].s, int(strlen(argv[2].s))));
			} else {
				*ret = gv_bool(g_mc_len >= 0);
			}
			return 1;
		}
		if (name_is(name, "memcard_load")) {
			GVar s = gv_nil();
			s.type = V_STR;
			copy_str(s.s, 32, g_mc_len > 0 ? "ok" : "");
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
			if (name_is(name, "store_buffer") || name_is(name, "store_string") || name_is(name, "store_var")) {
				if (argv && argc > 0 && argv[0].type == V_STR) {
					*ret = gv_bool(mc_wrap_ok((const uint8_t *)argv[0].s, int(strlen(argv[0].s))));
				} else {
					*ret = gv_bool(1);
				}
				return 1;
			}
			*ret = gv_bool(1);
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
	if (name_is(name, "change_scene") || name_is(name, "change_scene_to_file")) {
		int pack = pack_id_of(node);
		if (argv && argc > 0) {
			if (argv[0].type == V_STR) {
				pack = find_pack_path(argv[0].s);
			} else if (argv[0].type == V_OBJ) {
				pack = pack_id_of(argv[0].i);
			}
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
				host->play_fmv();
			}
			*ret = name_is(name, "is_playing") ? gv_bool(0) : gv_nil();
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
	if (name_is(name, "has_node") || name_is(name, "get_node_or_null")) {
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
	if (ru16(blob + 4) != 15) {
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
					if (d && argc >= 2) {
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
	if (ru16(blob + 4) != 15) {
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
		if (g_gdbc && g_gdbc_size) {
			ran |= run_official(g_gdbc, g_gdbc_size, "_process", delta, host, n, si);
		}
		if (tape) {
			ran |= run_tape(tape, tape_n, "_process", delta, host, n, si);
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
		if (g_gdbc && g_gdbc_size) {
			ran |= run_official(g_gdbc, g_gdbc_size, "_process", delta, host, -1, -1);
		}
		if (tape) {
			ran |= run_tape(tape, tape_n, "_process", delta, host, -1, -1);
		}
	}
	g_ticks_ms += delta * 1000.0f;
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
	for (int p = 0; p < g_npart; p++) {
		if (g_parts[p].life) {
			g_parts[p].life--;
		}
	}
	if (g_cam_attach >= 0 && node_ok(g_cam_attach) && host) {
		if (host->pos_x) {
			*host->pos_x = int32_t(g_nodes[g_cam_attach].px + g_cam_offx);
		}
		if (host->pos_y) {
			*host->pos_y = int32_t(g_nodes[g_cam_attach].py + g_cam_offy);
		}
		if (host->pos_z) {
			*host->pos_z = int32_t(g_nodes[g_cam_attach].pz + g_cam_offz);
		}
		g_script_cam = 1;
		if (host->script_drives_cam) {
			*host->script_drives_cam = 1;
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
	if (dispatched) {
		for (int n = 0; n < g_nnode; n++) {
			if (!g_node_used[n] || !(g_nodes[n].flags & 1) || g_nodes[n].script < 0) {
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
