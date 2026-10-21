/**************************************************************************/
/*  gdir_compile.cpp                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "gdir_compile.h"

#include "core/config/project_settings.h"

#include "gdir_ops.h"

#include "modules/modules_enabled.gen.h" // For gdscript.

#ifdef MODULE_GDSCRIPT_ENABLED
#include "modules/gdscript/gdscript_analyzer.h"
#include "modules/gdscript/gdscript_parser.h"
#endif

namespace ps2_cook {

static void put_u16(Vector<uint8_t> &o, uint32_t v) {
	o.push_back(uint8_t(v & 0xff));
	o.push_back(uint8_t((v >> 8) & 0xff));
}

static void put_u32(Vector<uint8_t> &o, uint32_t v) {
	put_u16(o, v & 0xffff);
	put_u16(o, v >> 16);
}

static void put_f32(Vector<uint8_t> &o, float f) {
	uint32_t u;
	memcpy(&u, &f, 4);
	put_u32(o, u);
}

#ifdef MODULE_GDSCRIPT_ENABLED

typedef GDScriptParser P;

// ------------------------------------------------------------------ constants
struct ConstPool {
	Vector<Variant> values;
	HashMap<String, int> index;
	HashMap<int, Vector<int>> arrays; // const index -> element const indices

	// Read-only array constant: elements first, then the array record.
	int add_array(const Vector<int> &p_items) {
		String key = "ARR:";
		for (int i = 0; i < p_items.size(); i++) {
			key += itos(p_items[i]) + ",";
		}
		if (const int *hit = index.getptr(key)) {
			return *hit;
		}
		values.push_back(Array());
		const int at = values.size() - 1;
		index[key] = at;
		arrays[at] = p_items;
		return at;
	}

	int add(const Variant &p_v) {
		const String key = Variant::get_type_name(p_v.get_type()) + ":" + p_v.stringify();
		if (const int *hit = index.getptr(key)) {
			return *hit;
		}
		values.push_back(p_v);
		index[key] = values.size() - 1;
		return values.size() - 1;
	}

	void write(Vector<uint8_t> &o) const {
		put_u32(o, values.size());
		for (int vi = 0; vi < values.size(); vi++) {
			const Variant &v = values[vi];
			if (const Vector<int> *items = arrays.getptr(vi)) {
				o.push_back(GDIR_K_ARRAY);
				put_u16(o, items->size());
				for (int i = 0; i < items->size(); i++) {
					put_u16(o, (*items)[i]);
				}
				continue;
			}
			switch (v.get_type()) {
				case Variant::NIL:
					o.push_back(GDIR_K_NIL);
					break;
				case Variant::BOOL:
					o.push_back(GDIR_K_BOOL);
					o.push_back(bool(v) ? 1 : 0);
					break;
				case Variant::INT:
					o.push_back(GDIR_K_INT);
					put_u32(o, uint32_t(int32_t(int64_t(v))));
					break;
				case Variant::FLOAT:
					o.push_back(GDIR_K_FLOAT);
					put_f32(o, float(double(v)));
					break;
				case Variant::VECTOR2: {
					const Vector2 x = v;
					o.push_back(GDIR_K_VEC2);
					put_f32(o, x.x);
					put_f32(o, x.y);
				} break;
				case Variant::VECTOR3: {
					const Vector3 x = v;
					o.push_back(GDIR_K_VEC3);
					put_f32(o, x.x);
					put_f32(o, x.y);
					put_f32(o, x.z);
				} break;
				default: {
					const CharString s = String(v).utf8();
					o.push_back(GDIR_K_STR);
					const int n = MIN(s.length(), 63);
					put_u16(o, n);
					for (int i = 0; i < n; i++) {
						o.push_back(uint8_t(s[i]));
					}
					o.push_back(0);
				} break;
			}
		}
	}
};

// Representable as a GDIR constant (arrays are emitted element by element).
static bool const_ok(const Variant &p_v) {
	switch (p_v.get_type()) {
		case Variant::NIL:
		case Variant::BOOL:
		case Variant::INT:
		case Variant::FLOAT:
		case Variant::STRING:
		case Variant::STRING_NAME:
		case Variant::VECTOR2:
		case Variant::VECTOR3:
			return true;
		default:
			return false;
	}
}

// ------------------------------------------------------------------ tables
struct NameId {
	const char *name;
	int id;
};

static const NameId kProps[] = {
	{ "position", GP_POSITION },
	{ "rotation", GP_ROTATION },
	{ "rotation_degrees", GP_ROTATION_DEGREES },
	{ "scale", GP_SCALE },
	{ "visible", GP_VISIBLE },
	{ "text", GP_TEXT },
	{ "x", GP_X },
	{ "y", GP_Y },
	{ "z", GP_Z },
	{ "global_position", GP_GLOBAL_POSITION },
	{ "name", GP_NAME },
	{ "color", GP_COLOR },
	{ "r", GP_R },
	{ "g", GP_G },
	{ "b", GP_B },
	{ "size", GP_SIZE },
};

static int prop_id(const String &p_name) {
	for (const NameId &e : kProps) {
		if (p_name == e.name) {
			return e.id;
		}
	}
	return -1;
}

// Global functions and constructors: name -> native, argc range.
struct NativeSig {
	const char *name;
	int id;
	int min_args;
	int max_args;
};

static const NativeSig kGlobals[] = {
	{ "print", GN_PRINT, 0, 15 },
	{ "print_debug", GN_PRINT, 0, 15 },
	{ "push_warning", GN_PRINT, 0, 15 },
	{ "push_error", GN_PRINT, 0, 15 },
	{ "prints", GN_PRINT, 0, 15 },
	{ "str", GN_STR, 0, 15 },
	{ "int", GN_INT, 1, 1 },
	{ "float", GN_FLOAT, 1, 1 },
	{ "bool", GN_BOOL, 1, 1 },
	{ "abs", GN_ABS, 1, 1 },
	{ "absf", GN_ABS, 1, 1 },
	{ "absi", GN_ABS, 1, 1 },
	{ "sign", GN_SIGN, 1, 1 },
	{ "signf", GN_SIGN, 1, 1 },
	{ "signi", GN_SIGN, 1, 1 },
	{ "min", GN_MIN, 2, 15 },
	{ "mini", GN_MIN, 2, 2 },
	{ "minf", GN_MIN, 2, 2 },
	{ "max", GN_MAX, 2, 15 },
	{ "maxi", GN_MAX, 2, 2 },
	{ "maxf", GN_MAX, 2, 2 },
	{ "clamp", GN_CLAMP, 3, 3 },
	{ "clampi", GN_CLAMP, 3, 3 },
	{ "clampf", GN_CLAMP, 3, 3 },
	{ "lerp", GN_LERP, 3, 3 },
	{ "lerpf", GN_LERP, 3, 3 },
	{ "move_toward", GN_MOVE_TOWARD, 3, 3 },
	{ "sin", GN_SIN, 1, 1 },
	{ "cos", GN_COS, 1, 1 },
	{ "tan", GN_TAN, 1, 1 },
	{ "asin", GN_ASIN, 1, 1 },
	{ "acos", GN_ACOS, 1, 1 },
	{ "atan2", GN_ATAN2, 2, 2 },
	{ "exp", GN_EXP, 1, 1 },
	{ "sqrt", GN_SQRT, 1, 1 },
	{ "pow", GN_POW, 2, 2 },
	{ "floor", GN_FLOOR, 1, 1 },
	{ "floorf", GN_FLOOR, 1, 1 },
	{ "ceil", GN_CEIL, 1, 1 },
	{ "ceilf", GN_CEIL, 1, 1 },
	{ "round", GN_ROUND, 1, 1 },
	{ "roundf", GN_ROUND, 1, 1 },
	{ "randf", GN_RANDF, 0, 0 },
	{ "randi", GN_RANDI, 0, 0 },
	{ "randf_range", GN_RANDF_RANGE, 2, 2 },
	{ "randi_range", GN_RANDI_RANGE, 2, 2 },
	{ "fmod", GN_FMOD, 2, 2 },
	{ "fposmod", GN_POSMOD, 2, 2 },
	{ "posmod", GN_POSMOD, 2, 2 },
	{ "wrapi", GN_WRAPI, 3, 3 },
	{ "wrapf", GN_WRAPF, 3, 3 },
	{ "deg_to_rad", GN_DEG_TO_RAD, 1, 1 },
	{ "rad_to_deg", GN_RAD_TO_DEG, 1, 1 },
	{ "is_equal_approx", GN_IS_EQUAL_APPROX, 2, 2 },
	{ "len", GN_LEN, 1, 1 },
	{ "Vector2", GN_VEC2, 0, 2 },
	{ "Vector2i", GN_VEC2, 0, 2 },
	{ "Vector3", GN_VEC3, 0, 3 },
	{ "Vector3i", GN_VEC3, 0, 3 },
	{ "Color", GN_COLOR, 3, 4 },
	{ "String", GN_STR, 0, 1 },
};

// Methods called on a value: name -> native (receiver is the first argument).
static const NativeSig kMethods[] = {
	{ "rotate_x", GN_NODE_ROTATE_X, 1, 1 },
	{ "rotate_y", GN_NODE_ROTATE_Y, 1, 1 },
	{ "rotate_z", GN_NODE_ROTATE_Z, 1, 1 },
	{ "rotate_object_local", -1, 0, 0 },
	{ "translate", GN_NODE_TRANSLATE, 1, 1 },
	{ "hide", GN_NODE_HIDE, 0, 0 },
	{ "show", GN_NODE_SHOW, 0, 0 },
	{ "queue_free", GN_NODE_HIDE, 0, 0 },
	{ "set_visible", GN_NODE_SET_VISIBLE, 1, 1 },
	{ "is_visible", GN_NODE_IS_VISIBLE, 0, 0 },
	{ "is_visible_in_tree", GN_NODE_IS_VISIBLE, 0, 0 },
	{ "grab_focus", GN_NODE_GRAB_FOCUS, 0, 0 },
	{ "has_focus", GN_NODE_HAS_FOCUS, 0, 0 },
	{ "look_at", GN_NODE_LOOK_AT, 1, 2 },
	{ "get_child_count", GN_NODE_CHILD_COUNT, 0, 0 },
	{ "get_child", GN_NODE_GET_CHILD, 1, 1 },
	{ "size", GN_ARR_SIZE, 0, 0 },
	{ "length", GN_STR_LENGTH, 0, 0 },
	{ "append", GN_ARR_APPEND, 1, 1 },
	{ "push_back", GN_ARR_APPEND, 1, 1 },
	{ "pop_back", GN_ARR_POP_BACK, 0, 0 },
	{ "clear", GN_ARR_CLEAR, 0, 0 },
	{ "has", GN_ARR_HAS, 1, 1 },
	{ "find", GN_ARR_FIND, 1, 1 },
	{ "is_empty", GN_ARR_IS_EMPTY, 0, 0 },
	{ "front", GN_ARR_FRONT, 0, 0 },
	{ "back", GN_ARR_BACK, 0, 0 },
	{ "erase", GN_ARR_ERASE, 1, 1 },
	{ "insert", GN_ARR_INSERT, 2, 2 },
	{ "to_upper", GN_STR_UPPER, 0, 0 },
	{ "to_lower", GN_STR_LOWER, 0, 0 },
	{ "begins_with", GN_STR_BEGINS, 1, 1 },
	{ "ends_with", GN_STR_ENDS, 1, 1 },
	{ "substr", GN_STR_SUBSTR, 1, 2 },
	{ "pad_zeros", GN_STR_PAD_ZEROS, 1, 1 },
	{ "normalized", GN_VEC_NORMALIZED, 0, 0 },
	{ "dot", GN_VEC_DOT, 1, 1 },
	{ "lerp", GN_VEC_LERP, 2, 2 },
	{ "distance_to", GN_VEC_DISTANCE, 1, 1 },
};

static const NativeSig kInput[] = {
	{ "is_action_pressed", GN_INPUT_PRESSED, 1, 2 },
	{ "is_action_just_pressed", GN_INPUT_JUST_PRESSED, 1, 2 },
	{ "is_action_just_released", GN_INPUT_JUST_RELEASED, 1, 2 },
	{ "get_axis", GN_INPUT_AXIS, 2, 2 },
	{ "get_vector", GN_INPUT_VECTOR, 4, 5 },
	{ "get_action_strength", GN_INPUT_STRENGTH, 1, 2 },
};

static const NativeSig kPS2[] = {
	{ "play_sfx", GN_PS2_PLAY_SFX, 0, 2 },
	{ "set_rumble", GN_PS2_SET_RUMBLE, 0, 2 },
	{ "stop_rumble", GN_PS2_STOP_RUMBLE, 0, 0 },
	{ "set_hud_text", GN_PS2_SET_HUD_TEXT, 2, 2 },
	{ "shake_camera", GN_PS2_SHAKE_CAMERA, 0, 2 },
	{ "get_display_width", GN_PS2_GET_DISPLAY_W, 0, 0 },
	{ "get_display_height", GN_PS2_GET_DISPLAY_H, 0, 0 },
	{ "get_abi", GN_PS2_GET_ABI, 0, 0 },
	{ "get_ee_ram_used", GN_PS2_EE_USED, 0, 0 },
	{ "get_ee_ram_free", GN_PS2_EE_FREE, 0, 0 },
	{ "get_ee_ram_limit", GN_PS2_EE_LIMIT, 0, 0 },
	{ "get_gs_vram_used", GN_PS2_GS_USED, 0, 0 },
	{ "get_gs_vram_free", GN_PS2_GS_FREE, 0, 0 },
	{ "get_gs_vram_limit", GN_PS2_GS_LIMIT, 0, 0 },
	{ "change_scene", GN_CHANGE_SCENE, 1, 1 },
	{ "change_scene_fade", GN_CHANGE_SCENE_FADE, 1, 2 },
	{ "set_fade", GN_SET_FADE, 1, 2 },
	{ "load_music", GN_PS2_LOAD_MUSIC, 0, 1 },
	{ "stop_music", GN_PS2_STOP_MUSIC, 0, 0 },
	{ "set_music_volume", GN_PS2_SET_MUSIC_VOLUME, 1, 1 },
	{ "say", GN_PS2_SAY, 1, 2 },
	{ "poke", GN_PS2_POKE, 2, 2 },
	{ "peek", GN_PS2_PEEK, 1, 1 },
	{ "set_fog", GN_PS2_SET_FOG, 0, 3 },
	{ "save_slot", GN_PS2_SAVE_SLOT, 1, 1 },
	{ "load_slot", GN_PS2_LOAD_SLOT, 1, 1 },
	{ "memcard_present", GN_PS2_MEMCARD_PRESENT, 0, 0 },
	{ "set_views", GN_PS2_SET_VIEWS, 1, 5 },
	{ "pad_count", GN_PS2_PAD_COUNT, 0, 0 },
	{ "pad_pressed", GN_PS2_PAD_PRESSED, 2, 2 },
	{ "pad_just_pressed", GN_PS2_PAD_JUST_PRESSED, 2, 2 },
	{ "pad_axis", GN_PS2_PAD_AXIS, 1, 1 },
};

static const NativeSig *find_sig(const NativeSig *p_table, int p_n, const String &p_name) {
	for (int i = 0; i < p_n; i++) {
		if (p_name == p_table[i].name) {
			return &p_table[i];
		}
	}
	return nullptr;
}

#define SIG(t, n) find_sig(t, int(sizeof(t) / sizeof(t[0])), n)

// ------------------------------------------------------------------ compiler
struct ClassOut {
	String path;
	Vector<String> members; // member variable names, in order
	Vector<String> noderefs; // relative node paths used by the class
	struct Fn {
		String name;
		int argc = 0;
		int locals = 0;
		uint32_t code = 0;
	};
	Vector<Fn> funcs;
	int fn_init = -1;
	int fn_ready = -1;
	int fn_process = -1;
	int fn_physics = -1;
	struct CodeConn {
		int noderef = -1; // -1: self
		String signal;
		String method;
	};
	Vector<CodeConn> code_conns;
};

class Compiler {
public:
	ConstPool *k = nullptr;
	Vector<uint8_t> *code = nullptr;
	Vector<String> *errors = nullptr;
	ClassOut *out = nullptr;
	String path;
	P::ClassNode *cls = nullptr;
	HashMap<StringName, int> func_index;
	HashMap<StringName, P::FunctionNode *> func_nodes;

	// function state
	Vector<HashMap<StringName, int>> scopes;
	int locals = 0;
	int max_locals = 0;
	struct Loop {
		Vector<uint32_t> breaks;
		Vector<uint32_t> continues;
		uint32_t cont_target = 0;
	};
	Vector<Loop> loops;
	bool ok = true;

	void err(const P::Node *p_node, const String &p_msg) {
		errors->push_back(vformat("%s:%d: %s", path, p_node ? p_node->start_line : 0, p_msg));
		ok = false;
	}

	// emit helpers
	uint32_t pc() const { return uint32_t(code->size()); }
	void op(int p_op) { code->push_back(uint8_t(p_op)); }
	void op8(int p_op, int p_a) {
		op(p_op);
		code->push_back(uint8_t(p_a));
	}
	void op16(int p_op, int p_a) {
		op(p_op);
		put_u16(*code, uint32_t(p_a));
	}
	uint32_t jump(int p_op, uint32_t p_to = 0) {
		op(p_op);
		const uint32_t at = pc();
		put_u32(*code, p_to);
		return at;
	}
	void patch(uint32_t p_at, uint32_t p_to) {
		uint8_t *w = code->ptrw() + p_at;
		w[0] = uint8_t(p_to);
		w[1] = uint8_t(p_to >> 8);
		w[2] = uint8_t(p_to >> 16);
		w[3] = uint8_t(p_to >> 24);
	}
	void native(int p_id, int p_argc) {
		op(GOP_NATIVE);
		code->push_back(uint8_t(p_id));
		code->push_back(uint8_t(p_argc));
	}

	bool emit_const(const Variant &p_v, const P::Node *p_at) {
		Variant v = p_v;
		if (v.get_type() == Variant::COLOR) {
			const Color c = v;
			v = Vector3(c.r, c.g, c.b);
		} else if (v.get_type() == Variant::VECTOR2I) {
			v = Vector2(Vector2i(v));
		} else if (v.get_type() == Variant::VECTOR3I) {
			v = Vector3(Vector3i(v));
		}
		if (v.get_type() == Variant::NIL) {
			op(GOP_NIL);
			return true;
		}
		if (v.get_type() == Variant::BOOL) {
			op(bool(v) ? GOP_TRUE : GOP_FALSE);
			return true;
		}
		if (v.get_type() == Variant::ARRAY) {
			const Array a = v;
			if (a.is_read_only() || a.size() > 64) {
				// const arrays live in the constant pool: no per-use copy, no
				// 64-element cap, read-only like GDScript's own const arrays.
				const int at = const_array(a, p_at);
				if (at < 0) {
					return false;
				}
				op16(GOP_CONST, at);
				return true;
			}
			for (int i = 0; i < a.size(); i++) {
				if (!emit_const(a[i], p_at)) {
					return false;
				}
			}
			op8(GOP_ARRAY, a.size());
			return true;
		}
		if (v.get_type() == Variant::STRING || v.get_type() == Variant::STRING_NAME) {
			if (String(v).utf8().length() > 63) {
				err(p_at, "string constants are limited to 63 bytes on PS2");
				return false;
			}
		}
		if (!const_ok(v)) {
			err(p_at, vformat("a %s constant is not supported on PS2", Variant::get_type_name(v.get_type())));
			return false;
		}
		op16(GOP_CONST, k->add(v));
		return true;
	}

	static Variant normalize(const Variant &p_v) {
		if (p_v.get_type() == Variant::COLOR) {
			const Color c = p_v;
			return Vector3(c.r, c.g, c.b);
		}
		if (p_v.get_type() == Variant::VECTOR2I) {
			return Vector2(Vector2i(p_v));
		}
		if (p_v.get_type() == Variant::VECTOR3I) {
			return Vector3(Vector3i(p_v));
		}
		return p_v;
	}

	int const_array(const Array &p_a, const P::Node *p_at) {
		if (p_a.size() > 4000) {
			err(p_at, "const arrays are limited to 4000 elements on PS2");
			return -1;
		}
		Vector<int> items;
		for (int i = 0; i < p_a.size(); i++) {
			const Variant e = normalize(p_a[i]);
			if (e.get_type() == Variant::ARRAY) {
				const int sub = const_array(e, p_at);
				if (sub < 0) {
					return -1;
				}
				items.push_back(sub);
				continue;
			}
			if (e.get_type() == Variant::STRING && String(e).utf8().length() > 63) {
				err(p_at, "string constants are limited to 63 bytes on PS2");
				return -1;
			}
			if (!const_ok(e)) {
				err(p_at, vformat("a %s constant is not supported on PS2", Variant::get_type_name(e.get_type())));
				return -1;
			}
			items.push_back(k->add(e));
		}
		return k->add_array(items);
	}

	int declare(const StringName &p_name) {
		const int slot = locals++;
		if (locals > max_locals) {
			max_locals = locals;
		}
		scopes.write[scopes.size() - 1][p_name] = slot;
		return slot;
	}

	int lookup_local(const StringName &p_name) const {
		for (int i = scopes.size() - 1; i >= 0; i--) {
			if (const int *s = scopes[i].getptr(p_name)) {
				return *s;
			}
		}
		return -1;
	}

	int member(const StringName &p_name) const {
		return out->members.find(String(p_name));
	}

	int noderef(const String &p_rel) {
		int i = out->noderefs.find(p_rel);
		if (i < 0) {
			out->noderefs.push_back(p_rel);
			i = out->noderefs.size() - 1;
		}
		if (i > 255) {
			return -1;
		}
		return i;
	}

	// ------------------------------------------------------------- expressions
	void expr(P::ExpressionNode *e) {
		if (!e || !ok) {
			return;
		}
		if (e->is_constant && e->reduced && e->type != P::Node::GET_NODE) {
			emit_const(e->reduced_value, e);
			return;
		}
		switch (e->type) {
			case P::Node::LITERAL:
				emit_const(static_cast<P::LiteralNode *>(e)->value, e);
				return;
			case P::Node::SELF:
				op(GOP_SELF);
				return;
			case P::Node::GET_NODE: {
				const int r = noderef(static_cast<P::GetNodeNode *>(e)->full_path);
				if (r < 0) {
					err(e, "too many node paths in one script (256)");
					return;
				}
				op8(GOP_NODEREF, r);
				return;
			}
			case P::Node::IDENTIFIER:
				identifier(static_cast<P::IdentifierNode *>(e));
				return;
			case P::Node::SUBSCRIPT:
				subscript(static_cast<P::SubscriptNode *>(e));
				return;
			case P::Node::CALL:
				call(static_cast<P::CallNode *>(e));
				return;
			case P::Node::BINARY_OPERATOR:
				binary(static_cast<P::BinaryOpNode *>(e));
				return;
			case P::Node::UNARY_OPERATOR: {
				P::UnaryOpNode *u = static_cast<P::UnaryOpNode *>(e);
				expr(u->operand);
				if (u->operation == P::UnaryOpNode::OP_NEGATIVE) {
					op(GOP_NEG);
				} else if (u->operation == P::UnaryOpNode::OP_LOGIC_NOT) {
					op(GOP_NOT);
				} else if (u->operation == P::UnaryOpNode::OP_COMPLEMENT) {
					err(e, "bitwise ~ is not supported on PS2");
				}
				return;
			}
			case P::Node::TERNARY_OPERATOR: {
				P::TernaryOpNode *t = static_cast<P::TernaryOpNode *>(e);
				expr(t->condition);
				const uint32_t jf = jump(GOP_JZ);
				expr(t->true_expr);
				const uint32_t je = jump(GOP_JMP);
				patch(jf, pc());
				expr(t->false_expr);
				patch(je, pc());
				return;
			}
			case P::Node::ARRAY: {
				P::ArrayNode *a = static_cast<P::ArrayNode *>(e);
				if (a->elements.size() > 64) {
					err(e, "arrays are limited to 64 elements on PS2");
					return;
				}
				for (P::ExpressionNode *el : a->elements) {
					expr(el);
				}
				op8(GOP_ARRAY, a->elements.size());
				return;
			}
			case P::Node::CAST:
				expr(static_cast<P::CastNode *>(e)->operand);
				return;
			case P::Node::ASSIGNMENT:
				err(e, "assignments are statements here, not values");
				return;
			case P::Node::DICTIONARY:
				err(e, "Dictionary is not supported on PS2 (use arrays or member variables)");
				return;
			case P::Node::LAMBDA:
				err(e, "lambdas are not supported on PS2");
				return;
			case P::Node::AWAIT:
				err(e, "await is not supported on PS2");
				return;
			case P::Node::PRELOAD:
				err(e, "preload is not supported on PS2");
				return;
			case P::Node::TYPE_TEST:
				err(e, "'is' type tests are not supported on PS2");
				return;
			default:
				err(e, "unsupported expression");
				return;
		}
	}

	void identifier(P::IdentifierNode *id) {
		const StringName &n = id->name;
		switch (id->source) {
			case P::IdentifierNode::FUNCTION_PARAMETER:
			case P::IdentifierNode::LOCAL_VARIABLE:
			case P::IdentifierNode::LOCAL_ITERATOR:
			case P::IdentifierNode::LOCAL_BIND: {
				const int slot = lookup_local(n);
				if (slot < 0) {
					err(id, vformat("unknown local '%s'", n));
					return;
				}
				op8(GOP_LLOAD, slot);
				return;
			}
			case P::IdentifierNode::LOCAL_CONSTANT:
			case P::IdentifierNode::MEMBER_CONSTANT:
				if (id->is_constant) {
					emit_const(id->reduced_value, id);
				} else {
					err(id, vformat("constant '%s' could not be reduced", n));
				}
				return;
			case P::IdentifierNode::MEMBER_VARIABLE: {
				const int m = member(n);
				if (m < 0) {
					err(id, vformat("unknown member '%s'", n));
					return;
				}
				op8(GOP_MLOAD, m);
				return;
			}
			case P::IdentifierNode::INHERITED_VARIABLE: {
				const int p = prop_id(n);
				if (p < 0) {
					err(id, vformat("node property '%s' is not supported on PS2", n));
					return;
				}
				op(GOP_SELF);
				op8(GOP_GETP, p);
				return;
			}
			case P::IdentifierNode::STATIC_VARIABLE:
				err(id, "static variables are not supported on PS2");
				return;
			default:
				break;
		}
		if (n == StringName("PI")) {
			emit_const(Math::PI, id);
			return;
		}
		if (n == StringName("TAU")) {
			emit_const(Math::TAU, id);
			return;
		}
		const int p = prop_id(n);
		if (p >= 0) {
			op(GOP_SELF);
			op8(GOP_GETP, p);
			return;
		}
		err(id, vformat("'%s' is not supported on PS2", n));
	}

	void subscript(P::SubscriptNode *s) {
		if (!s->is_attribute) {
			expr(s->base);
			expr(s->index);
			op(GOP_INDEX);
			return;
		}
		const String attr = s->attribute->name;
		const int p = prop_id(attr);
		if (p < 0) {
			err(s, vformat("property '%s' is not supported on PS2", attr));
			return;
		}
		expr(s->base);
		op8(GOP_GETP, p);
	}

	void binary(P::BinaryOpNode *b) {
		if (b->operation == P::BinaryOpNode::OP_LOGIC_AND || b->operation == P::BinaryOpNode::OP_LOGIC_OR) {
			expr(b->left_operand);
			const uint32_t j = jump(b->operation == P::BinaryOpNode::OP_LOGIC_AND ? GOP_JZK : GOP_JNZK);
			expr(b->right_operand);
			patch(j, pc());
			// normalize to bool like GDScript
			op(GOP_NOT);
			op(GOP_NOT);
			return;
		}
		expr(b->left_operand);
		expr(b->right_operand);
		switch (b->operation) {
			case P::BinaryOpNode::OP_ADDITION:
				op(GOP_ADD);
				break;
			case P::BinaryOpNode::OP_SUBTRACTION:
				op(GOP_SUB);
				break;
			case P::BinaryOpNode::OP_MULTIPLICATION:
				op(GOP_MUL);
				break;
			case P::BinaryOpNode::OP_DIVISION:
				op(GOP_DIV);
				break;
			case P::BinaryOpNode::OP_MODULO:
				op(GOP_MOD);
				break;
			case P::BinaryOpNode::OP_BIT_AND:
				op(GOP_BAND);
				break;
			case P::BinaryOpNode::OP_BIT_OR:
				op(GOP_BOR);
				break;
			case P::BinaryOpNode::OP_COMP_EQUAL:
				op(GOP_EQ);
				break;
			case P::BinaryOpNode::OP_COMP_NOT_EQUAL:
				op(GOP_NE);
				break;
			case P::BinaryOpNode::OP_COMP_LESS:
				op(GOP_LT);
				break;
			case P::BinaryOpNode::OP_COMP_LESS_EQUAL:
				op(GOP_LE);
				break;
			case P::BinaryOpNode::OP_COMP_GREATER:
				op(GOP_GT);
				break;
			case P::BinaryOpNode::OP_COMP_GREATER_EQUAL:
				op(GOP_GE);
				break;
			case P::BinaryOpNode::OP_CONTENT_TEST:
				op(GOP_IN);
				break;
			default:
				err(b, "this operator is not supported on PS2");
				break;
		}
	}

	// Node path relative to self written as a string literal (get_node("X")).
	bool literal_path(P::ExpressionNode *a, String &r_path) {
		if (a && a->is_constant && (a->reduced_value.get_type() == Variant::STRING || a->reduced_value.get_type() == Variant::NODE_PATH || a->reduced_value.get_type() == Variant::STRING_NAME)) {
			r_path = String(a->reduced_value);
			return true;
		}
		return false;
	}

	void args(P::CallNode *c, int p_skip = 0) {
		for (int i = p_skip; i < c->arguments.size(); i++) {
			expr(c->arguments[i]);
		}
	}

	bool check_argc(P::CallNode *c, const NativeSig *s) {
		const int n = c->arguments.size();
		if (n < s->min_args || n > s->max_args) {
			err(c, vformat("%s() takes %d..%d arguments", s->name, s->min_args, s->max_args));
			return false;
		}
		return true;
	}

	void call(P::CallNode *c) {
		if (c->is_super) {
			err(c, "super calls are not supported on PS2");
			return;
		}
		const String fname = c->function_name;
		P::ExpressionNode *callee = c->callee;
		// Own (or self) method: callee is an identifier.
		if (!callee || callee->type == P::Node::IDENTIFIER) {
			if (const int *fi = func_index.getptr(c->function_name)) {
				P::FunctionNode *fn = func_nodes[c->function_name];
				const int want = fn->parameters.size();
				const int have = c->arguments.size();
				if (have > want) {
					err(c, vformat("too many arguments to %s()", fname));
					return;
				}
				args(c);
				for (int i = have; i < want; i++) {
					P::ParameterNode *pn = fn->parameters[i];
					if (!pn->initializer) {
						err(c, vformat("missing argument %d to %s()", i + 1, fname));
						return;
					}
					expr(pn->initializer);
				}
				op(GOP_CALL);
				put_u16(*code, uint32_t(*fi));
				code->push_back(uint8_t(want));
				return;
			}
			if (fname == "get_node" || fname == "get_node_or_null") {
				String rel;
				if (c->arguments.size() != 1 || !literal_path(c->arguments[0], rel)) {
					err(c, "get_node() needs a constant path on PS2");
					return;
				}
				const int r = noderef(rel);
				op8(GOP_NODEREF, r);
				return;
			}
			if (fname == "get_parent") {
				op8(GOP_NODEREF, noderef(".."));
				return;
			}
			if (fname == "get_tree") {
				err(c, "get_tree() is only supported as get_tree().change_scene_to_file(path) or get_tree().quit()");
				return;
			}
			if (const NativeSig *g = SIG(kGlobals, fname)) {
				if (!check_argc(c, g)) {
					return;
				}
				args(c);
				native(g->id, c->arguments.size());
				return;
			}
			if (const NativeSig *m = SIG(kMethods, fname)) {
				if (m->id < 0) {
					err(c, vformat("%s() is not supported on PS2", fname));
					return;
				}
				if (!check_argc(c, m)) {
					return;
				}
				op(GOP_SELF);
				args(c);
				native(m->id, c->arguments.size() + 1);
				return;
			}
			if (fname == "connect") {
				conn_from_code(c, -1);
				return;
			}
			err(c, vformat("%s() is not supported on PS2", fname));
			return;
		}
		if (callee->type != P::Node::SUBSCRIPT) {
			err(c, "unsupported call");
			return;
		}
		P::SubscriptNode *sub = static_cast<P::SubscriptNode *>(callee);
		P::ExpressionNode *base = sub->base;
		// Singletons: Input / OS / PS2 / PS1 / Time
		if (base && base->type == P::Node::IDENTIFIER) {
			const StringName bn = static_cast<P::IdentifierNode *>(base)->name;
			if (bn == StringName("Input")) {
				const NativeSig *s = SIG(kInput, fname);
				if (!s) {
					err(c, vformat("Input.%s() is not supported on PS2", fname));
					return;
				}
				if (!check_argc(c, s)) {
					return;
				}
				for (int i = 0; i < c->arguments.size() && i < (s->id == GN_INPUT_VECTOR ? 4 : (s->id == GN_INPUT_AXIS ? 2 : 1)); i++) {
					expr(c->arguments[i]);
				}
				native(s->id, s->id == GN_INPUT_VECTOR ? 4 : (s->id == GN_INPUT_AXIS ? 2 : 1));
				return;
			}
			if (bn == StringName("OS")) {
				if (fname == "has_feature" && c->arguments.size() == 1) {
					args(c);
					native(GN_HAS_FEATURE, 1);
					return;
				}
				err(c, vformat("OS.%s() is not supported on PS2", fname));
				return;
			}
			if (bn == StringName("Time")) {
				if (fname == "get_ticks_msec") {
					native(GN_TICKS_MSEC, 0);
					return;
				}
				err(c, vformat("Time.%s() is not supported on PS2", fname));
				return;
			}
			if (bn == StringName("PS2") || bn == StringName("PS1")) {
				const NativeSig *s = SIG(kPS2, fname);
				if (!s) {
					err(c, vformat("%s.%s() is in the catalog but not in the compiled PS2 IR yet", bn, fname));
					return;
				}
				if (!check_argc(c, s)) {
					return;
				}
				if (s->id == GN_PS2_PLAY_SFX && c->arguments.size() >= 1) {
					// The bank is cooked in ps2/audio/sfx_bank order, so a sound's
					// name resolves to its index here: a full res:// path or the
					// file name without ".wav" ("bolt").
					const P::ExpressionNode *a0 = c->arguments[0];
					if (!a0->is_constant || (a0->reduced_value.get_type() != Variant::STRING && a0->reduced_value.get_type() != Variant::STRING_NAME)) {
						err(c, "PS2.play_sfx() needs a constant sound name on PS2");
						return;
					}
					const String want = a0->reduced_value;
					const PackedStringArray bank = GLOBAL_GET("ps2/audio/sfx_bank");
					int found = -1;
					for (int i = 0; i < bank.size() && found < 0; i++) {
						if (bank[i] == want || bank[i].get_file().get_basename() == want) {
							found = i;
						}
					}
					if (found < 0) {
						err(c, vformat("PS2.play_sfx(\"%s\"): not in Project Settings ps2/audio/sfx_bank", want));
						return;
					}
					op16(GOP_CONST, k->add(found));
					native(GN_PS2_PLAY_SFX, 1);
					return;
				}
				args(c);
				native(s->id, c->arguments.size());
				return;
			}
		}
		// get_tree().change_scene_to_file(path) / get_tree().quit()
		if (base && base->type == P::Node::CALL && static_cast<P::CallNode *>(base)->function_name == StringName("get_tree")) {
			if ((fname == "change_scene_to_file" || fname == "change_scene") && c->arguments.size() == 1) {
				args(c);
				native(GN_CHANGE_SCENE, 1);
				return;
			}
			if (fname == "quit") {
				native(GN_QUIT, 0);
				return;
			}
			err(c, vformat("get_tree().%s() is not supported on PS2", fname));
			return;
		}
		// $Button.pressed.connect(_on_x)
		if (fname == "connect" && sub->base && sub->base->type == P::Node::SUBSCRIPT &&
				static_cast<P::SubscriptNode *>(sub->base)->is_attribute) {
			P::SubscriptNode *sig = static_cast<P::SubscriptNode *>(sub->base);
			int ref = -1;
			if (!node_ref_of(sig->base, ref)) {
				err(c, "signals can only be connected on $Node paths or self on PS2");
				return;
			}
			if (c->arguments.size() < 1 || c->arguments[0]->type != P::Node::IDENTIFIER) {
				err(c, "connect() needs a method of this script on PS2");
				return;
			}
			ClassOut::CodeConn cc;
			cc.noderef = ref;
			cc.signal = sig->attribute->name;
			cc.method = static_cast<P::IdentifierNode *>(c->arguments[0])->name;
			add_code_conn(c, cc);
			op(GOP_NIL);
			return;
		}
		if (fname == "get_node" || fname == "get_node_or_null") {
			String rel;
			int ref = -1;
			if (c->arguments.size() == 1 && literal_path(c->arguments[0], rel) && node_ref_of(base, ref) && ref >= 0) {
				op8(GOP_NODEREF, noderef(out->noderefs[ref] + "/" + rel));
				return;
			}
			err(c, "get_node() needs a constant path on PS2");
			return;
		}
		if (fname == "play" || fname == "stop") {
			err(c, "AnimationPlayer.play/stop are not in the compiled PS2 IR yet (use PS2.play_animation later)");
			return;
		}
		const NativeSig *m = SIG(kMethods, fname);
		if (!m || m->id < 0) {
			err(c, vformat("method %s() is not supported on PS2", fname));
			return;
		}
		if (!check_argc(c, m)) {
			return;
		}
		expr(base);
		args(c);
		native(m->id, c->arguments.size() + 1);
	}

	// self or $Path / get_node("..") as a node-ref slot (-1 = self).
	bool node_ref_of(P::ExpressionNode *e, int &r_ref) {
		if (!e || e->type == P::Node::SELF) {
			r_ref = -1;
			return true;
		}
		if (e->type == P::Node::GET_NODE) {
			r_ref = noderef(static_cast<P::GetNodeNode *>(e)->full_path);
			return r_ref >= 0;
		}
		if (e->type == P::Node::CALL) {
			P::CallNode *c = static_cast<P::CallNode *>(e);
			String rel;
			if ((c->function_name == StringName("get_node") || c->function_name == StringName("get_node_or_null")) &&
					(!c->callee || c->callee->type == P::Node::IDENTIFIER) && c->arguments.size() == 1 && literal_path(c->arguments[0], rel)) {
				r_ref = noderef(rel);
				return r_ref >= 0;
			}
		}
		return false;
	}

	void add_code_conn(P::Node *at, const ClassOut::CodeConn &cc) {
		if (!func_index.has(cc.method)) {
			err(at, vformat("connect(): %s is not a method of this script", cc.method));
			return;
		}
		if (cc.signal != "pressed") {
			err(at, vformat("only the 'pressed' signal is supported on PS2 (got '%s')", cc.signal));
			return;
		}
		out->code_conns.push_back(cc);
	}

	void conn_from_code(P::CallNode *c, int p_ref) {
		// connect("pressed", _on_x) on self
		if (c->arguments.size() < 2 || c->arguments[1]->type != P::Node::IDENTIFIER || !c->arguments[0]->is_constant) {
			err(c, "connect() needs (\"pressed\", method) on PS2");
			return;
		}
		ClassOut::CodeConn cc;
		cc.noderef = p_ref;
		cc.signal = String(c->arguments[0]->reduced_value);
		cc.method = static_cast<P::IdentifierNode *>(c->arguments[1])->name;
		add_code_conn(c, cc);
		op(GOP_NIL);
	}

	// ------------------------------------------------------------- stores
	// Stores the value on top of the stack into p_target (pops it).
	void store(P::ExpressionNode *t) {
		if (!t || !ok) {
			return;
		}
		if (t->type == P::Node::IDENTIFIER) {
			P::IdentifierNode *id = static_cast<P::IdentifierNode *>(t);
			switch (id->source) {
				case P::IdentifierNode::FUNCTION_PARAMETER:
				case P::IdentifierNode::LOCAL_VARIABLE:
				case P::IdentifierNode::LOCAL_ITERATOR: {
					const int slot = lookup_local(id->name);
					if (slot < 0) {
						err(t, vformat("unknown local '%s'", id->name));
						return;
					}
					op8(GOP_LSTORE, slot);
					return;
				}
				case P::IdentifierNode::MEMBER_VARIABLE: {
					const int m = member(id->name);
					if (m < 0) {
						err(t, vformat("unknown member '%s'", id->name));
						return;
					}
					op8(GOP_MSTORE, m);
					return;
				}
				default: {
					const int p = prop_id(id->name);
					if (p < 0) {
						err(t, vformat("cannot assign '%s' on PS2", id->name));
						return;
					}
					op(GOP_SELF);
					op8(GOP_SETP, p);
					op(GOP_POP);
					return;
				}
			}
		}
		if (t->type == P::Node::SUBSCRIPT) {
			P::SubscriptNode *s = static_cast<P::SubscriptNode *>(t);
			if (!s->is_attribute) {
				// [value] -> [value base index] -> SETINDEX
				expr(s->base);
				expr(s->index);
				op(GOP_SETINDEX);
				return;
			}
			const int p = prop_id(s->attribute->name);
			if (p < 0) {
				err(t, vformat("property '%s' is not supported on PS2", s->attribute->name));
				return;
			}
			expr(s->base);
			op8(GOP_SETP, p);
			// write the edited copy back when the base is a stored value (vectors)
			if (storable(s->base)) {
				store(s->base);
			} else {
				op(GOP_POP);
			}
			return;
		}
		err(t, "cannot assign to this expression on PS2");
	}

	bool storable(P::ExpressionNode *e) {
		if (!e) {
			return false;
		}
		if (e->type == P::Node::IDENTIFIER) {
			return true;
		}
		if (e->type == P::Node::SUBSCRIPT) {
			P::SubscriptNode *s = static_cast<P::SubscriptNode *>(e);
			if (!s->is_attribute) {
				return true;
			}
			// node.position is storable (the node takes the new vector); node itself is not
			return prop_id(s->attribute->name) >= 0;
		}
		return false;
	}

	void assignment(P::AssignmentNode *a) {
		if (a->operation == P::AssignmentNode::OP_NONE) {
			expr(a->assigned_value);
		} else {
			expr(a->assignee);
			expr(a->assigned_value);
			switch (a->operation) {
				case P::AssignmentNode::OP_ADDITION:
					op(GOP_ADD);
					break;
				case P::AssignmentNode::OP_SUBTRACTION:
					op(GOP_SUB);
					break;
				case P::AssignmentNode::OP_MULTIPLICATION:
					op(GOP_MUL);
					break;
				case P::AssignmentNode::OP_DIVISION:
					op(GOP_DIV);
					break;
				case P::AssignmentNode::OP_MODULO:
					op(GOP_MOD);
					break;
				case P::AssignmentNode::OP_BIT_AND:
					op(GOP_BAND);
					break;
				case P::AssignmentNode::OP_BIT_OR:
					op(GOP_BOR);
					break;
				default:
					err(a, "this compound assignment is not supported on PS2");
					return;
			}
		}
		store(a->assignee);
	}

	// ------------------------------------------------------------- statements
	void suite(P::SuiteNode *s) {
		if (!s) {
			return;
		}
		scopes.push_back(HashMap<StringName, int>());
		const int saved = locals;
		for (P::Node *st : s->statements) {
			statement(st);
			if (!ok) {
				break;
			}
		}
		locals = saved;
		scopes.resize(scopes.size() - 1);
	}

	void statement(P::Node *n) {
		switch (n->type) {
			case P::Node::VARIABLE: {
				P::VariableNode *v = static_cast<P::VariableNode *>(n);
				if (v->initializer) {
					expr(v->initializer);
				} else {
					default_value(v->get_datatype(), v);
				}
				const int slot = declare(v->identifier->name);
				op8(GOP_LSTORE, slot);
				return;
			}
			case P::Node::CONSTANT:
				return; // reduced at use sites
			case P::Node::PASS:
			case P::Node::BREAKPOINT:
			case P::Node::ASSERT:
				return;
			case P::Node::RETURN: {
				P::ReturnNode *r = static_cast<P::ReturnNode *>(n);
				if (r->return_value) {
					expr(r->return_value);
					op(GOP_RET);
				} else {
					op(GOP_RETNIL);
				}
				return;
			}
			case P::Node::IF: {
				P::IfNode *i = static_cast<P::IfNode *>(n);
				expr(i->condition);
				const uint32_t jf = jump(GOP_JZ);
				suite(i->true_block);
				if (i->false_block) {
					const uint32_t je = jump(GOP_JMP);
					patch(jf, pc());
					suite(i->false_block);
					patch(je, pc());
				} else {
					patch(jf, pc());
				}
				return;
			}
			case P::Node::WHILE: {
				P::WhileNode *w = static_cast<P::WhileNode *>(n);
				const uint32_t top = pc();
				expr(w->condition);
				const uint32_t jx = jump(GOP_JZ);
				loops.push_back(Loop());
				loops.write[loops.size() - 1].cont_target = top;
				suite(w->loop);
				jump(GOP_JMP, top);
				const uint32_t end = pc();
				patch(jx, end);
				finish_loop(top, end);
				return;
			}
			case P::Node::FOR:
				for_loop(static_cast<P::ForNode *>(n));
				return;
			case P::Node::BREAK:
				if (loops.is_empty()) {
					err(n, "break outside a loop");
					return;
				}
				loops.write[loops.size() - 1].breaks.push_back(jump(GOP_JMP));
				return;
			case P::Node::CONTINUE:
				if (loops.is_empty()) {
					err(n, "continue outside a loop");
					return;
				}
				loops.write[loops.size() - 1].continues.push_back(jump(GOP_JMP));
				return;
			case P::Node::MATCH:
				err(n, "match is not supported on PS2 (use if/elif)");
				return;
			case P::Node::ASSIGNMENT:
				assignment(static_cast<P::AssignmentNode *>(n));
				return;
			default:
				if (n->is_expression()) {
					P::ExpressionNode *e = static_cast<P::ExpressionNode *>(n);
					if (e->type == P::Node::AWAIT) {
						err(n, "await is not supported on PS2");
						return;
					}
					expr(e);
					op(GOP_POP);
					return;
				}
				err(n, "unsupported statement");
				return;
		}
	}

	void finish_loop(uint32_t p_cont, uint32_t p_end) {
		Loop l = loops[loops.size() - 1];
		loops.resize(loops.size() - 1);
		for (uint32_t b : l.breaks) {
			patch(b, p_end);
		}
		for (uint32_t c : l.continues) {
			patch(c, p_cont);
		}
	}

	void for_loop(P::ForNode *f) {
		scopes.push_back(HashMap<StringName, int>());
		const int saved = locals;
		P::ExpressionNode *list = f->list;
		P::CallNode *range = nullptr;
		if (list && list->type == P::Node::CALL && static_cast<P::CallNode *>(list)->function_name == StringName("range") &&
				(!static_cast<P::CallNode *>(list)->callee || static_cast<P::CallNode *>(list)->callee->type == P::Node::IDENTIFIER)) {
			range = static_cast<P::CallNode *>(list);
		}
		const bool int_count = !range && list && list->get_datatype().kind == P::DataType::BUILTIN &&
				list->get_datatype().builtin_type == Variant::INT;
		if (range || int_count) {
			// i = from; while (step > 0 ? i < to : i > to): body; i += step
			const int var = declare(f->variable->name);
			const int to = declare("@to");
			const int step = declare("@step");
			const int n = range ? range->arguments.size() : 1;
			if (range && (n < 1 || n > 3)) {
				err(f, "range() takes 1..3 arguments");
				return;
			}
			if (n == 1) {
				emit_const(int64_t(0), f);
				op8(GOP_LSTORE, var);
				if (range) {
					expr(range->arguments[0]);
				} else {
					expr(list);
				}
				op8(GOP_LSTORE, to);
				emit_const(int64_t(1), f);
				op8(GOP_LSTORE, step);
			} else {
				expr(range->arguments[0]);
				op8(GOP_LSTORE, var);
				expr(range->arguments[1]);
				op8(GOP_LSTORE, to);
				if (n == 3) {
					expr(range->arguments[2]);
				} else {
					emit_const(int64_t(1), f);
				}
				op8(GOP_LSTORE, step);
			}
			const uint32_t top = pc();
			// (step > 0 and i < to) or (step < 0 and i > to)
			op8(GOP_LLOAD, step);
			emit_const(int64_t(0), f);
			op(GOP_GT);
			const uint32_t jneg = jump(GOP_JZ);
			op8(GOP_LLOAD, var);
			op8(GOP_LLOAD, to);
			op(GOP_LT);
			const uint32_t jcond = jump(GOP_JMP);
			patch(jneg, pc());
			op8(GOP_LLOAD, var);
			op8(GOP_LLOAD, to);
			op(GOP_GT);
			patch(jcond, pc());
			const uint32_t jx = jump(GOP_JZ);
			loops.push_back(Loop());
			suite(f->loop);
			const uint32_t cont = pc();
			op8(GOP_LLOAD, var);
			op8(GOP_LLOAD, step);
			op(GOP_ADD);
			op8(GOP_LSTORE, var);
			jump(GOP_JMP, top);
			const uint32_t end = pc();
			patch(jx, end);
			finish_loop(cont, end);
		} else {
			// for x in array: idx over a hidden copy of the array value
			const int arr = declare("@arr");
			const int idx = declare("@idx");
			const int var = declare(f->variable->name);
			expr(list);
			op8(GOP_LSTORE, arr);
			emit_const(int64_t(0), f);
			op8(GOP_LSTORE, idx);
			const uint32_t top = pc();
			op8(GOP_LLOAD, idx);
			op8(GOP_LLOAD, arr);
			native(GN_LEN, 1);
			op(GOP_LT);
			const uint32_t jx = jump(GOP_JZ);
			op8(GOP_LLOAD, arr);
			op8(GOP_LLOAD, idx);
			op(GOP_INDEX);
			op8(GOP_LSTORE, var);
			loops.push_back(Loop());
			suite(f->loop);
			const uint32_t cont = pc();
			op8(GOP_LLOAD, idx);
			emit_const(int64_t(1), f);
			op(GOP_ADD);
			op8(GOP_LSTORE, idx);
			jump(GOP_JMP, top);
			const uint32_t end = pc();
			patch(jx, end);
			finish_loop(cont, end);
		}
		locals = saved;
		scopes.resize(scopes.size() - 1);
	}

	void default_value(const P::DataType &t, P::Node *at) {
		if (t.kind == P::DataType::BUILTIN) {
			switch (t.builtin_type) {
				case Variant::INT:
					emit_const(int64_t(0), at);
					return;
				case Variant::FLOAT:
					emit_const(0.0, at);
					return;
				case Variant::BOOL:
					op(GOP_FALSE);
					return;
				case Variant::STRING:
				case Variant::STRING_NAME:
					emit_const(String(), at);
					return;
				case Variant::VECTOR2:
				case Variant::VECTOR2I:
					emit_const(Vector2(), at);
					return;
				case Variant::VECTOR3:
				case Variant::VECTOR3I:
					emit_const(Vector3(), at);
					return;
				case Variant::COLOR:
					emit_const(Vector3(), at);
					return;
				case Variant::ARRAY:
					op8(GOP_ARRAY, 0);
					return;
				default:
					break;
			}
		}
		op(GOP_NIL);
	}

	// ------------------------------------------------------------- functions
	int begin_function(const String &p_name, int p_argc) {
		ClassOut::Fn fn;
		fn.name = p_name;
		fn.argc = p_argc;
		fn.code = pc();
		out->funcs.push_back(fn);
		scopes.clear();
		scopes.push_back(HashMap<StringName, int>());
		locals = 0;
		max_locals = 0;
		loops.clear();
		return out->funcs.size() - 1;
	}

	void end_function(int p_idx) {
		op(GOP_RETNIL);
		out->funcs.write[p_idx].locals = max_locals;
		if (max_locals > 255) {
			err(cls, vformat("%s() has more than 255 locals", out->funcs[p_idx].name));
		}
	}

	void function(P::FunctionNode *f, int p_idx) {
		ClassOut::Fn &slot = out->funcs.write[p_idx];
		slot.code = pc();
		scopes.clear();
		scopes.push_back(HashMap<StringName, int>());
		locals = 0;
		max_locals = 0;
		loops.clear();
		for (P::ParameterNode *p : f->parameters) {
			declare(p->identifier->name);
		}
		suite(f->body);
		end_function(p_idx);
	}

	bool compile_class() {
		// members first (indices are fixed before any code is emitted)
		for (const P::ClassNode::Member &m : cls->members) {
			switch (m.type) {
				case P::ClassNode::Member::VARIABLE:
					if (m.variable->is_static) {
						err(m.variable, "static variables are not supported on PS2");
					} else if (m.variable->property != P::VariableNode::PROP_NONE) {
						err(m.variable, "properties with set/get are not supported on PS2");
					} else {
						out->members.push_back(m.variable->identifier->name);
					}
					break;
				case P::ClassNode::Member::FUNCTION: {
					if (m.function->is_static) {
						err(m.function, "static functions are not supported on PS2");
						break;
					}
					if (m.function->parameters.size() > 15) {
						err(m.function, "functions take at most 15 arguments on PS2");
						break;
					}
					func_index[m.function->identifier->name] = out->funcs.size();
					func_nodes[m.function->identifier->name] = m.function;
					ClassOut::Fn fn;
					fn.name = m.function->identifier->name;
					fn.argc = m.function->parameters.size();
					out->funcs.push_back(fn);
				} break;
				case P::ClassNode::Member::CLASS:
					err(m.m_class, "inner classes are not supported on PS2");
					break;
				default:
					break; // constants, enums, signals, groups
			}
		}
		if (out->members.size() > 255) {
			err(cls, "more than 255 member variables");
		}
		if (!ok) {
			return false;
		}
		for (const P::ClassNode::Member &m : cls->members) {
			if (m.type == P::ClassNode::Member::FUNCTION) {
				function(m.function, func_index[m.function->identifier->name]);
				if (!ok) {
					return false;
				}
			}
		}
		// synthetic member initialiser (+ _init)
		const int init = begin_function("@init", 0);
		for (const P::ClassNode::Member &m : cls->members) {
			if (m.type != P::ClassNode::Member::VARIABLE) {
				continue;
			}
			P::VariableNode *v = m.variable;
			if (v->initializer) {
				expr(v->initializer);
			} else {
				default_value(v->get_datatype(), v);
			}
			op8(GOP_MSTORE, member(v->identifier->name));
		}
		if (const int *fi = func_index.getptr("_init")) {
			op(GOP_CALL);
			put_u16(*code, uint32_t(*fi));
			code->push_back(0);
			op(GOP_POP);
		}
		end_function(init);
		out->fn_init = init;
		if (const int *fi = func_index.getptr("_ready")) {
			out->fn_ready = *fi;
		}
		if (const int *fi = func_index.getptr("_process")) {
			out->fn_process = *fi;
		}
		if (const int *fi = func_index.getptr("_physics_process")) {
			out->fn_physics = *fi;
		}
		return ok;
	}
};

// ------------------------------------------------------------------ paths
static String join_path(const String &p_base, const String &p_rel) {
	if (p_rel.begins_with("/")) {
		return String(); // absolute paths are not supported
	}
	Vector<String> parts;
	if (p_base != "." && !p_base.is_empty()) {
		parts = p_base.split("/");
	}
	for (const String &seg : p_rel.split("/", false)) {
		if (seg == ".") {
			continue;
		}
		if (seg == "..") {
			if (parts.is_empty()) {
				return String();
			}
			parts.resize(parts.size() - 1);
			continue;
		}
		parts.push_back(seg);
	}
	return parts.is_empty() ? String(".") : String("/").join(parts);
}

static int resolve_node(const GdirPack &p_pack, int p_from, const String &p_rel) {
	if (p_rel.begins_with("%")) {
		const String uname = p_rel.substr(1);
		for (int i = 0; i < p_pack.nodes.size(); i++) {
			if (p_pack.nodes[i].name == uname) {
				return i;
			}
		}
		return -1;
	}
	const String want = join_path(p_pack.nodes[p_from].path, p_rel);
	if (want.is_empty()) {
		return -1;
	}
	for (int i = 0; i < p_pack.nodes.size(); i++) {
		if (p_pack.nodes[i].path == want) {
			return i;
		}
	}
	return -1;
}

#endif // MODULE_GDSCRIPT_ENABLED

void gdir_compile(const Vector<GdirPack> &p_packs, GdirResult &r_out) {
	bool any = false;
	for (const GdirPack &pk : p_packs) {
		for (const GdirNode &n : pk.nodes) {
			if (!n.script_path.is_empty()) {
				any = true;
			}
		}
	}
	if (!any) {
		return;
	}
#ifndef MODULE_GDSCRIPT_ENABLED
	r_out.errors.push_back("compiling scripts for PS2 needs the gdscript module in the editor");
#else
	ConstPool consts;
	Vector<uint8_t> code;
	Vector<ClassOut> classes;
	HashMap<String, int> class_of;

	// 1. compile each distinct script once
	for (const GdirPack &pk : p_packs) {
		for (const GdirNode &n : pk.nodes) {
			if (n.script_path.is_empty() || class_of.has(n.script_path)) {
				continue;
			}
			class_of[n.script_path] = -1;
			GDScriptParser parser;
			const Error perr = parser.parse(n.script_source, n.script_path, false);
			if (perr == OK) {
				GDScriptAnalyzer analyzer(&parser);
				analyzer.analyze();
			}
			if (!parser.get_errors().is_empty()) {
				for (const GDScriptParser::ParserError &e : parser.get_errors()) {
					r_out.errors.push_back(vformat("%s:%d: %s", n.script_path, e.line, e.message));
				}
				continue;
			}
			ClassOut co;
			co.path = n.script_path;
			Compiler c;
			c.k = &consts;
			c.code = &code;
			c.errors = &r_out.errors;
			c.out = &co;
			c.path = n.script_path;
			c.cls = parser.get_tree();
			if (c.compile_class()) {
				class_of[n.script_path] = classes.size();
				classes.push_back(co);
			}
		}
	}
	if (!r_out.errors.is_empty()) {
		return;
	}
	if (classes.size() > 64) {
		r_out.errors.push_back("more than 64 scripts in one PS2 export");
		return;
	}

	// 2. serialize: class table and packs first (they add constants), then
	// header + final constant pool + classes + code + packs.
	Vector<uint8_t> class_bytes;
	int nfuncs = 0;
	for (const ClassOut &c : classes) {
		put_u16(class_bytes, c.members.size());
		put_u16(class_bytes, c.noderefs.size());
		put_u16(class_bytes, c.funcs.size());
		put_u16(class_bytes, uint32_t(int16_t(c.fn_init)));
		put_u16(class_bytes, uint32_t(int16_t(c.fn_ready)));
		put_u16(class_bytes, uint32_t(int16_t(c.fn_process)));
		put_u16(class_bytes, uint32_t(int16_t(c.fn_physics)));
		for (const ClassOut::Fn &f : c.funcs) {
			class_bytes.push_back(uint8_t(f.argc));
			class_bytes.push_back(uint8_t(f.locals));
			put_u32(class_bytes, f.code);
			put_u16(class_bytes, consts.add(f.name));
		}
		nfuncs += c.funcs.size();
	}
	Vector<uint8_t> pack_bytes;
	Vector<uint8_t> &o = pack_bytes;
	put_u16(o, p_packs.size());
	String listing;
	for (const GdirPack &pk : p_packs) {
		o.push_back(uint8_t(pk.pack));
		o.push_back(uint8_t(CLAMP(int(pk.clear.r * 255.0f), 0, 255)));
		o.push_back(uint8_t(CLAMP(int(pk.clear.g * 255.0f), 0, 255)));
		o.push_back(uint8_t(CLAMP(int(pk.clear.b * 255.0f), 0, 255)));
		o.push_back(pk.has_clear ? 1 : 0);
		put_u16(o, uint32_t(int16_t(pk.camera)));
		put_f32(o, pk.fov);
		put_u16(o, pk.nodes.size());
		put_u16(o, consts.add(pk.scene)); // change_scene target, pack 0 included
		for (const GdirNode &n : pk.nodes) {
			put_u16(o, uint32_t(int16_t(n.parent)));
			o.push_back(n.flags);
			o.push_back(uint8_t(int8_t(n.hud)));
			put_f32(o, n.lpos.x);
			put_f32(o, n.lpos.y);
			put_f32(o, n.lpos.z);
			put_f32(o, n.lrot.x);
			put_f32(o, n.lrot.y);
			put_f32(o, n.lrot.z);
			put_f32(o, n.lscale.x);
			put_f32(o, n.lscale.y);
			put_f32(o, n.lscale.z);
			const Basis &b = n.w0.basis;
			for (int r = 0; r < 3; r++) {
				put_f32(o, b.rows[r].x);
				put_f32(o, b.rows[r].y);
				put_f32(o, b.rows[r].z);
				put_f32(o, n.w0.origin[r]);
			}
		}
		// instances
		Vector<int> inst_nodes;
		for (int i = 0; i < pk.nodes.size(); i++) {
			const int *ci = class_of.getptr(pk.nodes[i].script_path);
			if (!pk.nodes[i].script_path.is_empty() && ci && *ci >= 0) {
				inst_nodes.push_back(i);
			}
		}
		put_u16(o, inst_nodes.size());
		Vector<GdirConn> conns = pk.conns;
		for (int ii = 0; ii < inst_nodes.size(); ii++) {
			const int nid = inst_nodes[ii];
			const GdirNode &n = pk.nodes[nid];
			const int ci = class_of[n.script_path];
			const ClassOut &c = classes[ci];
			put_u16(o, nid);
			put_u16(o, ci);
			for (const String &rel : c.noderefs) {
				const int target = resolve_node(pk, nid, rel);
				if (target < 0) {
					r_out.warnings.push_back(vformat("%s: node path '%s' from %s does not exist in %s (null on PS2)", c.path, rel, n.path, pk.scene));
				}
				put_u16(o, target < 0 ? 0xffff : uint32_t(target));
			}
			// @export values set in the scene
			Vector<Pair<int, int>> ov;
			for (const KeyValue<String, Variant> &kv : n.exported) {
				const int m = c.members.find(kv.key);
				Variant v = kv.value;
				if (v.get_type() == Variant::COLOR) {
					const Color col = v;
					v = Vector3(col.r, col.g, col.b);
				}
				if (m >= 0 && const_ok(v)) {
					ov.push_back(Pair<int, int>(m, consts.add(v)));
				}
			}
			o.push_back(uint8_t(MIN(ov.size(), 255)));
			for (int k = 0; k < ov.size() && k < 255; k++) {
				o.push_back(uint8_t(ov[k].first));
				put_u16(o, ov[k].second);
			}
			// connections made in code ($Button.pressed.connect(_on_x))
			for (const ClassOut::CodeConn &cc : c.code_conns) {
				GdirConn g;
				g.src = cc.noderef < 0 ? nid : resolve_node(pk, nid, c.noderefs[cc.noderef]);
				g.signal = cc.signal;
				g.target = nid;
				g.method = cc.method;
				conns.push_back(g);
			}
			listing += vformat("pack %d node %d %s: %s\n", pk.pack, nid, n.path, c.path);
		}
		// connections (scene file + code), resolved to (instance, function)
		Vector<uint8_t> conn_bytes;
		int nconn = 0;
		for (const GdirConn &g : conns) {
			if (g.signal != "pressed" || g.src < 0) {
				continue;
			}
			const int ii = inst_nodes.find(g.target);
			if (ii < 0) {
				r_out.warnings.push_back(vformat("%s: %s.%s targets a node without a cooked script", pk.scene, g.signal, g.method));
				continue;
			}
			const ClassOut &c = classes[class_of[pk.nodes[g.target].script_path]];
			int fi = -1;
			for (int f = 0; f < c.funcs.size(); f++) {
				if (c.funcs[f].name == g.method) {
					fi = f;
				}
			}
			if (fi < 0) {
				r_out.errors.push_back(vformat("%s: %s is connected to %s() which %s does not define", pk.scene, g.signal, g.method, c.path));
				continue;
			}
			put_u16(conn_bytes, g.src);
			conn_bytes.push_back(0); // pressed
			put_u16(conn_bytes, ii);
			put_u16(conn_bytes, fi);
			nconn++;
		}
		put_u16(o, nconn);
		o.append_array(conn_bytes);
		r_out.instances += inst_nodes.size();
	}
	if (consts.values.size() > 16384) {
		r_out.errors.push_back("more than 16384 script constants in one PS2 export");
		return;
	}
	Vector<uint8_t> &blob = r_out.blob;
	blob.push_back('G');
	blob.push_back('D');
	blob.push_back('I');
	blob.push_back('R');
	put_u16(blob, GDIR_VERSION);
	put_u16(blob, 0);
	put_u32(blob, 0); // total size, patched below
	consts.write(blob);
	put_u16(blob, classes.size());
	blob.append_array(class_bytes);
	put_u32(blob, code.size());
	blob.append_array(code);
	blob.append_array(pack_bytes);
	{
		const uint32_t total = blob.size();
		uint8_t *w = blob.ptrw() + 8;
		w[0] = uint8_t(total);
		w[1] = uint8_t(total >> 8);
		w[2] = uint8_t(total >> 16);
		w[3] = uint8_t(total >> 24);
	}
	r_out.classes = classes.size();
	r_out.functions = nfuncs;
	r_out.listing = listing;
#endif
}

} // namespace ps2_cook
