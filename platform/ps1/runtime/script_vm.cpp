/**************************************************************************/
/*  script_vm.cpp                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/

#include "script_vm.h"

#include <psxpad.h>

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
static uint16_t g_prev_btn = 0xffff;

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

void script_vm_init(const uint8_t *gdbc, size_t gdbc_size, const uint8_t *luau, size_t luau_size) {
	g_gdbc = gdbc;
	g_gdbc_size = gdbc_size;
	g_luau = luau;
	g_luau_size = luau_size;
	g_ready = 1;
	g_did_ready = 0;
	g_err[0] = 0;
	g_prev_btn = 0xffff;
}

void script_vm_set_nodes(const ScriptVMNode *nodes, int count) {
	g_nnode = 0;
	if (!nodes || count <= 0) {
		return;
	}
	g_nnode = count > PS1_MAX_NODES ? PS1_MAX_NODES : count;
	for (int i = 0; i < g_nnode; i++) {
		g_nodes[i] = nodes[i];
	}
}

void script_vm_set_hud(const ScriptVMHud *hud, int count) {
	g_nhud = 0;
	g_focus = -1;
	if (!hud || count <= 0) {
		return;
	}
	g_nhud = count > PS1_MAX_HUD ? PS1_MAX_HUD : count;
	for (int i = 0; i < g_nhud; i++) {
		g_hud[i] = hud[i];
	}
}

void script_vm_set_tiles(const ScriptVMTile *tiles, int count) {
	g_ntile = 0;
	if (!tiles || count <= 0) {
		return;
	}
	g_ntile = count > PS1_MAX_TILES ? PS1_MAX_TILES : count;
	for (int i = 0; i < g_ntile; i++) {
		g_tiles[i] = tiles[i];
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
	return id >= 0 && id < g_nnode;
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

static void apply_method(const ScriptVMHost *host, int node, const char *name, float arg, const GVar *argv, int argc) {
	if (name_is(name, "print") || name_is(name, "push_warning") || name_is(name, "get_node")) {
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
		float tz = 0.0f;
		if (argv && argc > 0 && argv[0].type == V_V3) {
			tx = argv[0].x;
			tz = argv[0].z;
		}
		const float dx = tx - float(g_nodes[node].px);
		const float dz = tz - float(g_nodes[node].pz);
		float ang = 0.0f;
		if (dx != 0.0f || dz != 0.0f) {
			ang = 0.0f;
			// yaw-only: atan2(dx, dz) approximated via ratio
			if (dz == 0.0f) {
				ang = dx > 0.0f ? 1.5707963f : -1.5707963f;
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
				ang = a;
			}
		}
		g_nodes[node].ry = int16_t(rad_to_ps1(ang));
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
	return 0;
}

static int pad_down(const ScriptVMHost *host, uint16_t mask) {
	if (!host || !host->pad34 || !mask) {
		return 0;
	}
	const PADTYPE *pad = (const PADTYPE *)host->pad34;
	if (pad->stat != 0) {
		return 0;
	}
	return !(pad->btn & mask);
}

static int pad_pressed(const ScriptVMHost *host, const char *action, int just) {
	const uint16_t mask = uint16_t(pad_mask(action));
	if (!mask) {
		return 0;
	}
	const int down = pad_down(host, mask);
	if (just == 2) {
		return !down && !(g_prev_btn & mask);
	}
	if (!just) {
		return down;
	}
	return down && (g_prev_btn & mask);
}

static void pad_tick(const ScriptVMHost *host) {
	if (!host || !host->pad34) {
		return;
	}
	const PADTYPE *pad = (const PADTYPE *)host->pad34;
	if (pad->stat == 0) {
		g_prev_btn = pad->btn;
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

static int run_official(const uint8_t *blob, size_t size, const char *want, float delta, const ScriptVMHost *host) {
	if (size < 8 || blob[0] != 'G' || blob[1] != 'D' || blob[2] != 'B' || blob[3] != 'C') {
		return 0;
	}
	if (ru16(blob + 4) != 8) {
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
		const int self_id = (owner == 0xffff) ? 0 : int(owner);
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

static int run_tape(const uint8_t *blob, size_t size, const char *want, float delta, const ScriptVMHost *host) {
	if (size < 8 || blob[0] != 'G' || blob[1] != 'D' || blob[2] != 'B' || blob[3] != 'C') {
		return 0;
	}
	if (ru16(blob + 4) != 8) {
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
		const int owner = (f / 2 < nown) ? int(owners[f / 2]) : 0;
		const int self = (owner == 0xffff) ? 0 : owner;
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
					(void)pad_pressed(host, tape_action(names, nstore), input_just_mode(nm));
				} else {
					apply_method(host, self, nm, arg, nullptr, 0);
				}
				ran = 1;
			} else if (op == kTapeCallNamed && ip < ncode) {
				const uint8_t ni = code[ip++];
				const char *nm = (ni < nstore) ? names[ni] : "";
				if (is_input_name(nm)) {
					(void)pad_pressed(host, tape_action(names, nstore), input_just_mode(nm));
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

int script_vm_process(float delta, const ScriptVMHost *host) {
	if (!g_ready) {
		return 0;
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
	if (!g_did_ready) {
		if (g_gdbc && g_gdbc_size) {
			run_official(g_gdbc, g_gdbc_size, "_ready", 0.0f, host);
		}
		if (tape) {
			run_tape(tape, tape_n, "_ready", 0.0f, host);
		}
		g_did_ready = 1;
	}
	if (g_gdbc && g_gdbc_size) {
		ran |= run_official(g_gdbc, g_gdbc_size, "_process", delta, host);
	}
	if (tape) {
		ran |= run_tape(tape, tape_n, "_process", delta, host);
	}
	for (int i = 0; i < g_nhud; i++) {
		g_hud[i].pressed = 0;
	}
	pad_tick(host);
	return ran;
}
