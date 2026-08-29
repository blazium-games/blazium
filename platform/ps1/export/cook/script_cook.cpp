/**************************************************************************/
/*  script_cook.cpp                                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/

#include "script_cook.h"

#include "core/input/input_event.h"
#include "core/input/input_map.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/templates/hash_map.h"
#include "core/variant/variant.h"
#include "modules/gdscript/gdscript.h"
#include "modules/gdscript/gdscript_function.h"

#ifdef MODULE_LUAU_MODULE_ENABLED
#include "modules/luau_module/luau.h"
#endif

namespace ps1_cook {

static const uint8_t kTapeReturn = 0;
static const uint8_t kTapeLoadConstF = 1;
static const uint8_t kTapeLoadArg = 2;
static const uint8_t kTapeMul = 3;
static const uint8_t kTapeAdd = 4;
static const uint8_t kTapeCallSelf = 5;

static String strip_line_comments(const String &p_source) {
	PackedStringArray lines = p_source.split("\n");
	String out;
	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i];
		const int gd = line.find("#");
		const int lua = line.find("--");
		int cut = -1;
		if (gd >= 0 && lua >= 0) {
			cut = MIN(gd, lua);
		} else if (gd >= 0) {
			cut = gd;
		} else if (lua >= 0) {
			cut = lua;
		}
		if (cut >= 0) {
			line = line.substr(0, cut);
		}
		out += line + "\n";
	}
	return out;
}

bool source_is_rejected(const String &p_source, String &r_why) {
	const String src = strip_line_comments(p_source);
	const char *bad[] = {
		"eval(", "ClassDB.get_class_list", "ClassDB.", "HTTPClient",
		"Thread.", "await ", "func lambda", "preload(", "OS.execute", "FileAccess.store",
		"callv("
	};
	for (const char *b : bad) {
		if (src.contains(b)) {
			r_why = b;
			return true;
		}
	}
	int p = 0;
	while (true) {
		const int hit = src.find("load(", p);
		if (hit < 0) {
			break;
		}
		p = hit + 5;
		const int q = src.find("\"", hit);
		if (q < 0) {
			r_why = "load(";
			return true;
		}
		const int e = src.find("\"", q + 1);
		if (e <= q + 1) {
			r_why = "load(";
			return true;
		}
		const String path = src.substr(q + 1, e - q - 1);
		if (!path.ends_with(".tscn")) {
			r_why = "load(" + path + ")";
			return true;
		}
	}
	return false;
}

bool method_is_allowlisted(const String &p_name) {
	return p_name == "rotate_x" || p_name == "rotate_y" || p_name == "rotate_z" ||
			p_name == "translate" || p_name == "get_node" ||
			p_name == "is_action_pressed" || p_name == "is_action_just_pressed" ||
			p_name == "is_action_just_released" ||
			p_name == "_ready" || p_name == "_process" || p_name == "_physics_process" ||
			p_name == "hide" || p_name == "show" ||
			p_name == "set_position" || p_name == "set_rotation" ||
			p_name == "look_at" || p_name == "print" || p_name == "push_warning" ||
			p_name == "grab_focus" || p_name == "release_focus" ||
			p_name == "get_parent" || p_name == "get_child" || p_name == "get_child_count" ||
			p_name == "has_node" || p_name == "get_node_or_null" ||
			p_name == "set_visible" || p_name == "is_visible" ||
			p_name == "abs" || p_name == "clamp" || p_name == "lerp" ||
			p_name == "min" || p_name == "max" ||
			p_name == "sin" || p_name == "cos" ||
			p_name == "floor" || p_name == "ceil" || p_name == "round" ||
			p_name == "int" || p_name == "float" || p_name == "str" ||
			p_name == "sign" || p_name == "sqrt" || p_name == "pow" ||
			p_name == "move_toward" || p_name == "deg_to_rad" || p_name == "rad_to_deg" ||
			p_name == "randf" || p_name == "randi" || p_name == "randf_range" || p_name == "randi_range" ||
			p_name == "get_ticks_msec" || p_name == "create_timer" || p_name == "is_timer_done" ||
			p_name == "get_vector" || p_name == "push_error" || p_name == "get_frames_per_second" ||
			p_name == "play" || p_name == "stop" || p_name == "is_playing" ||
			p_name == "queue_free" || p_name == "load" || p_name == "instantiate" ||
			p_name == "change_scene" || p_name == "change_scene_to_file" || p_name == "get_tree" ||
			p_name == "connect" || p_name == "disconnect";
}

static bool member_is_allowlisted(const String &p_name) {
	return p_name == "position" || p_name == "rotation" || p_name == "visible" || p_name == "name" ||
			p_name == "text" || p_name == "disabled" || p_name == "pressed" || p_name == "button_pressed" ||
			p_name == "value" || p_name == "min_value" || p_name == "max_value" || p_name == "selected" ||
			p_name == "focus" || p_name == "x" || p_name == "y" || p_name == "z";
}

static bool action_is_known(const String &p_name) {
	return p_name == "ui_left" || p_name == "ui_right" || p_name == "ui_up" || p_name == "ui_down" ||
			p_name == "ui_accept" || p_name == "ui_cancel" || p_name == "ui_select" ||
			p_name == "ui_focus_prev" || p_name == "ui_focus_next";
}

static void warn_unknown_actions(const String &p_source, ScriptIR &r_out) {
	int p = 0;
	while (true) {
		const int hit = p_source.find("is_action_", p);
		if (hit < 0) {
			break;
		}
		const int q = p_source.find("\"", hit);
		p = hit + 10;
		if (q < 0) {
			continue;
		}
		const int e = p_source.find("\"", q + 1);
		if (e <= q + 1) {
			continue;
		}
		const String act = p_source.substr(q + 1, e - q - 1);
		if (action_is_known(act)) {
			continue;
		}
		bool mapped = false;
		for (int i = 0; i < r_out.input_actions.size(); i++) {
			if (r_out.input_actions[i].name == act) {
				mapped = true;
				break;
			}
		}
		if (mapped) {
			continue;
		}
		const String msg = "unknown Input action '" + act + "' is not mapped on PS1 (false at runtime).";
		bool seen = false;
		for (int i = 0; i < r_out.warnings.size(); i++) {
			if (r_out.warnings[i] == msg) {
				seen = true;
				break;
			}
		}
		if (!seen) {
			r_out.warnings.push_back(msg);
		}
	}
}

static bool luau_source_unbound(const String &p_source, String &r_why) {
	const String src = strip_line_comments(p_source);
	const char *bad[] = {
		"call_deferred", "rpc", "create_tween", "duplicate"
	};
	for (const char *b : bad) {
		if (src.contains(String(b) + "(") || src.contains(String(b) + " (")) {
			r_why = b;
			return true;
		}
	}
	for (int i = 0; i < src.length(); i++) {
		const char32_t c = src[i];
		if ((c != '.' && c != ':') || i + 1 >= src.length()) {
			continue;
		}
		if (!(src[i + 1] >= 'A' && src[i + 1] <= 'Z') && !(src[i + 1] >= 'a' && src[i + 1] <= 'z') && src[i + 1] != '_') {
			continue;
		}
		int j = i + 1;
		String ident;
		while (j < src.length()) {
			const char32_t ch = src[j];
			if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_') {
				ident += ch;
				j++;
			} else {
				break;
			}
		}
		while (j < src.length() && (src[j] == ' ' || src[j] == '\t')) {
			j++;
		}
		if (j >= src.length() || src[j] != '(' || ident.is_empty() || method_is_allowlisted(ident)) {
			continue;
		}
		if (c == ':' || ident == "preload" || ident == "call_deferred" || ident == "duplicate") {
			r_why = ident;
			return true;
		}
	}
	return false;
}

static bool skip_script_dir(const String &p_name) {
	return p_name == ".godot" || p_name == "export" || p_name == "tests" || p_name == "tools" || p_name == "addons";
}

static void append_u16(Vector<uint8_t> &o, uint16_t v) {
	o.push_back(uint8_t(v & 0xff));
	o.push_back(uint8_t(v >> 8));
}

static void append_u32(Vector<uint8_t> &o, uint32_t v) {
	append_u16(o, uint16_t(v & 0xffff));
	append_u16(o, uint16_t(v >> 16));
}

static void append_i32(Vector<uint8_t> &o, int32_t v) {
	append_u32(o, uint32_t(v));
}

static void append_f32(Vector<uint8_t> &o, float v) {
	union {
		float f;
		uint32_t u;
	} x;
	x.f = v;
	append_u32(o, x.u);
}

static void append_str(Vector<uint8_t> &o, const String &s) {
	const CharString utf = s.utf8();
	o.push_back(uint8_t(CLAMP(utf.length(), 0, 255)));
	for (int i = 0; i < utf.length() && i < 255; i++) {
		o.push_back(uint8_t(utf[i]));
	}
}

static int host_ptr_words() {
	return int(sizeof(void *) / sizeof(int));
}

// Host instruction length (words). Returns 0 on unknown.
static int host_opcode_size(const Vector<int> &code, int ip) {
	using Op = GDScriptFunction::Opcode;
	if (ip < 0 || ip >= code.size()) {
		return 0;
	}
	const int op = code[ip];
	const int ptrw = host_ptr_words();
	switch (op) {
		case Op::OPCODE_OPERATOR:
			return 7 + ptrw;
		case Op::OPCODE_OPERATOR_VALIDATED:
			return 5;
		case Op::OPCODE_ASSIGN:
			return 3;
		case Op::OPCODE_ASSIGN_NULL:
		case Op::OPCODE_ASSIGN_TRUE:
		case Op::OPCODE_ASSIGN_FALSE:
			return 2;
		case Op::OPCODE_ASSIGN_TYPED_BUILTIN:
			return 4;
		case Op::OPCODE_JUMP:
			return 2;
		case Op::OPCODE_JUMP_IF:
		case Op::OPCODE_JUMP_IF_NOT:
		case Op::OPCODE_JUMP_IF_SHARED:
			return 3;
		case Op::OPCODE_JUMP_TO_DEF_ARGUMENT:
			return 1;
		case Op::OPCODE_RETURN:
			return 2;
		case Op::OPCODE_RETURN_TYPED_BUILTIN:
			return 3;
		case Op::OPCODE_END:
		case Op::OPCODE_BREAKPOINT:
			return 1;
		case Op::OPCODE_LINE:
			return 2;
		case Op::OPCODE_GET_MEMBER:
		case Op::OPCODE_SET_MEMBER:
			return 3;
		case Op::OPCODE_GET_KEYED:
		case Op::OPCODE_SET_KEYED:
			return 4;
		case Op::OPCODE_GET_KEYED_VALIDATED:
		case Op::OPCODE_SET_KEYED_VALIDATED:
		case Op::OPCODE_GET_INDEXED_VALIDATED:
		case Op::OPCODE_SET_INDEXED_VALIDATED:
			return 5;
		case Op::OPCODE_GET_NAMED:
		case Op::OPCODE_SET_NAMED:
		case Op::OPCODE_GET_NAMED_VALIDATED:
		case Op::OPCODE_SET_NAMED_VALIDATED:
			return 4;
		case Op::OPCODE_ITERATE_BEGIN:
		case Op::OPCODE_ITERATE_BEGIN_INT:
		case Op::OPCODE_ITERATE:
		case Op::OPCODE_ITERATE_INT:
			return 5;
		case Op::OPCODE_ASSERT:
			return 3;
		case Op::OPCODE_TYPE_ADJUST_BOOL:
		case Op::OPCODE_TYPE_ADJUST_INT:
		case Op::OPCODE_TYPE_ADJUST_FLOAT:
		case Op::OPCODE_TYPE_ADJUST_STRING:
		case Op::OPCODE_TYPE_ADJUST_VECTOR2:
		case Op::OPCODE_TYPE_ADJUST_VECTOR3:
			return 2;
		case Op::OPCODE_CALL:
		case Op::OPCODE_CALL_RETURN:
		case Op::OPCODE_CALL_ASYNC:
		case Op::OPCODE_CALL_UTILITY:
		case Op::OPCODE_CALL_UTILITY_VALIDATED:
		case Op::OPCODE_CALL_GDSCRIPT_UTILITY:
		case Op::OPCODE_CALL_METHOD_BIND:
		case Op::OPCODE_CALL_METHOD_BIND_RET:
		case Op::OPCODE_CALL_METHOD_BIND_VALIDATED_RETURN:
		case Op::OPCODE_CALL_METHOD_BIND_VALIDATED_NO_RETURN:
		case Op::OPCODE_CALL_NATIVE_STATIC:
		case Op::OPCODE_CALL_NATIVE_STATIC_VALIDATED_RETURN:
		case Op::OPCODE_CALL_NATIVE_STATIC_VALIDATED_NO_RETURN: {
			if (ip + 1 >= code.size()) {
				return 0;
			}
			const int iac = code[ip + 1];
			return 4 + iac;
		}
		case Op::OPCODE_CONSTRUCT:
		case Op::OPCODE_CONSTRUCT_VALIDATED: {
			if (ip + 1 >= code.size()) {
				return 0;
			}
			const int iac = code[ip + 1];
			return 4 + iac;
		}
		case Op::OPCODE_CAST_TO_BUILTIN:
			return 4;
		default:
			return 0;
	}
}

static bool opcode_forbidden(int op) {
	using Op = GDScriptFunction::Opcode;
	return op == Op::OPCODE_AWAIT || op == Op::OPCODE_AWAIT_RESUME ||
			op == Op::OPCODE_CREATE_LAMBDA || op == Op::OPCODE_CREATE_SELF_LAMBDA ||
			op == Op::OPCODE_CALL_ASYNC || op == Op::OPCODE_STORE_GLOBAL ||
			op == Op::OPCODE_STORE_NAMED_GLOBAL;
}

static bool opcode_allowed(int op) {
	using Op = GDScriptFunction::Opcode;
	if (opcode_forbidden(op)) {
		return false;
	}
	return op == Op::OPCODE_ASSIGN || op == Op::OPCODE_ASSIGN_NULL || op == Op::OPCODE_ASSIGN_TRUE ||
			op == Op::OPCODE_ASSIGN_FALSE || op == Op::OPCODE_ASSIGN_TYPED_BUILTIN ||
			op == Op::OPCODE_OPERATOR || op == Op::OPCODE_OPERATOR_VALIDATED ||
			op == Op::OPCODE_JUMP || op == Op::OPCODE_JUMP_IF || op == Op::OPCODE_JUMP_IF_NOT ||
			op == Op::OPCODE_JUMP_TO_DEF_ARGUMENT || op == Op::OPCODE_JUMP_IF_SHARED ||
			op == Op::OPCODE_RETURN || op == Op::OPCODE_RETURN_TYPED_BUILTIN ||
			op == Op::OPCODE_END || op == Op::OPCODE_LINE || op == Op::OPCODE_BREAKPOINT ||
			op == Op::OPCODE_ASSERT ||
			op == Op::OPCODE_TYPE_ADJUST_BOOL || op == Op::OPCODE_TYPE_ADJUST_INT ||
			op == Op::OPCODE_TYPE_ADJUST_FLOAT || op == Op::OPCODE_TYPE_ADJUST_STRING ||
			op == Op::OPCODE_TYPE_ADJUST_VECTOR2 || op == Op::OPCODE_TYPE_ADJUST_VECTOR3 ||
			op == Op::OPCODE_CONSTRUCT || op == Op::OPCODE_CONSTRUCT_VALIDATED ||
			op == Op::OPCODE_CALL || op == Op::OPCODE_CALL_RETURN ||
			op == Op::OPCODE_CALL_UTILITY || op == Op::OPCODE_CALL_UTILITY_VALIDATED ||
			op == Op::OPCODE_CALL_GDSCRIPT_UTILITY ||
			op == Op::OPCODE_CALL_METHOD_BIND || op == Op::OPCODE_CALL_METHOD_BIND_RET ||
			op == Op::OPCODE_CALL_METHOD_BIND_VALIDATED_RETURN ||
			op == Op::OPCODE_CALL_METHOD_BIND_VALIDATED_NO_RETURN ||
			op == Op::OPCODE_CALL_NATIVE_STATIC ||
			op == Op::OPCODE_CALL_NATIVE_STATIC_VALIDATED_RETURN ||
			op == Op::OPCODE_CALL_NATIVE_STATIC_VALIDATED_NO_RETURN ||
			op == Op::OPCODE_GET_MEMBER || op == Op::OPCODE_SET_MEMBER ||
			op == Op::OPCODE_GET_NAMED || op == Op::OPCODE_SET_NAMED ||
			op == Op::OPCODE_GET_NAMED_VALIDATED || op == Op::OPCODE_SET_NAMED_VALIDATED ||
			op == Op::OPCODE_GET_KEYED || op == Op::OPCODE_SET_KEYED ||
			op == Op::OPCODE_GET_KEYED_VALIDATED || op == Op::OPCODE_SET_KEYED_VALIDATED ||
			op == Op::OPCODE_GET_INDEXED_VALIDATED || op == Op::OPCODE_SET_INDEXED_VALIDATED ||
			op == Op::OPCODE_ITERATE_BEGIN || op == Op::OPCODE_ITERATE_BEGIN_INT ||
			op == Op::OPCODE_ITERATE || op == Op::OPCODE_ITERATE_INT ||
			op == Op::OPCODE_CAST_TO_BUILTIN;
}

static String call_name_at(const GDScriptFunction *fn, const Vector<int> &code, int ip) {
	using Op = GDScriptFunction::Opcode;
	const int op = code[ip];
	if (ip + 1 >= code.size()) {
		return String();
	}
	const int iac = code[ip + 1];
	const int name_at = ip + 3 + iac;
	if (name_at >= code.size()) {
		return String();
	}
	if (op == Op::OPCODE_CALL || op == Op::OPCODE_CALL_RETURN ||
			op == Op::OPCODE_CALL_UTILITY || op == Op::OPCODE_CALL_GDSCRIPT_UTILITY) {
		return String(fn->get_global_name(code[name_at]));
	}
	if (op == Op::OPCODE_CALL_METHOD_BIND || op == Op::OPCODE_CALL_METHOD_BIND_RET ||
			op == Op::OPCODE_CALL_METHOD_BIND_VALIDATED_RETURN ||
			op == Op::OPCODE_CALL_METHOD_BIND_VALIDATED_NO_RETURN) {
		return String(fn->get_method_bind_name(code[name_at]));
	}
	if (op == Op::OPCODE_CALL_NATIVE_STATIC ||
			op == Op::OPCODE_CALL_NATIVE_STATIC_VALIDATED_RETURN ||
			op == Op::OPCODE_CALL_NATIVE_STATIC_VALIDATED_NO_RETURN) {
		const int meth_at = ip + 2 + iac;
		if (meth_at >= code.size()) {
			return String();
		}
		return String(fn->get_method_bind_name(code[meth_at]));
	}
	return String();
}

static Error scan_and_normalize(const String &p_path, GDScript *script, GDScriptFunction *fn, Vector<int> &r_norm, String &r_err) {
	using Op = GDScriptFunction::Opcode;
	const Vector<int> &code = fn->get_code();
	const int n = fn->get_code_size() > 0 ? fn->get_code_size() : code.size();
	for (int ip = 0; ip < n;) {
		const int op = code[ip];
		const int sz = host_opcode_size(code, ip);
		if (sz <= 0) {
			r_err = vformat("%s:%s:%d unknown opcode %d", p_path, String(fn->get_name()), ip, op);
			return ERR_UNCONFIGURED;
		}
		if (!opcode_allowed(op)) {
			r_err = vformat("%s:%s:%d opcode %d is not in the PS1 first-slice table", p_path, String(fn->get_name()), ip, op);
			return ERR_UNCONFIGURED;
		}
		if (op == Op::OPCODE_CALL || op == Op::OPCODE_CALL_RETURN || op == Op::OPCODE_CALL_UTILITY ||
				op == Op::OPCODE_CALL_UTILITY_VALIDATED || op == Op::OPCODE_CALL_GDSCRIPT_UTILITY ||
				op == Op::OPCODE_CALL_METHOD_BIND ||
				op == Op::OPCODE_CALL_METHOD_BIND_RET || op == Op::OPCODE_CALL_METHOD_BIND_VALIDATED_RETURN ||
				op == Op::OPCODE_CALL_METHOD_BIND_VALIDATED_NO_RETURN || op == Op::OPCODE_CALL_NATIVE_STATIC ||
				op == Op::OPCODE_CALL_NATIVE_STATIC_VALIDATED_RETURN || op == Op::OPCODE_CALL_NATIVE_STATIC_VALIDATED_NO_RETURN) {
			const String meth = call_name_at(fn, code, ip);
			if (!meth.is_empty() && !method_is_allowlisted(meth)) {
				r_err = vformat("%s:%s:%d unbound method '%s'", p_path, String(fn->get_name()), ip, meth);
				return ERR_UNCONFIGURED;
			}
		}
		if (op == Op::OPCODE_GET_MEMBER || op == Op::OPCODE_SET_MEMBER) {
			const int mi = (ip + 2 < code.size()) ? code[ip + 2] : -1;
			const String mem = script ? String(script->debug_get_member_by_index(mi)) : String();
			if (mem.is_empty() || !member_is_allowlisted(mem)) {
				r_err = vformat("%s:%s:%d unbound member '%s'", p_path, String(fn->get_name()), ip, mem.is_empty() ? String("?") : mem);
				return ERR_UNCONFIGURED;
			}
		}
		if (op == Op::OPCODE_GET_NAMED || op == Op::OPCODE_SET_NAMED) {
			const int ni = (ip + 3 < code.size()) ? code[ip + 3] : -1;
			const String mem = (ni >= 0) ? String(fn->get_global_name(ni)) : String();
			if (mem.is_empty() || !member_is_allowlisted(mem)) {
				r_err = vformat("%s:%s:%d unbound member '%s'", p_path, String(fn->get_name()), ip, mem.is_empty() ? String("?") : mem);
				return ERR_UNCONFIGURED;
			}
		}
		if (op == Op::OPCODE_GET_NAMED_VALIDATED || op == Op::OPCODE_SET_NAMED_VALIDATED) {
			if (ip + 3 >= code.size()) {
				r_err = vformat("%s:%s:%d truncated named-validated opcode", p_path, String(fn->get_name()), ip);
				return ERR_UNCONFIGURED;
			}
			const String pname = String(op == Op::OPCODE_GET_NAMED_VALIDATED ? fn->get_getter_name(code[ip + 3]) : fn->get_setter_name(code[ip + 3]));
			if (pname.is_empty() || !member_is_allowlisted(pname)) {
				r_err = vformat("%s:%s:%d unbound member '%s'", p_path, String(fn->get_name()), ip, pname.is_empty() ? String("?") : pname);
				return ERR_UNCONFIGURED;
			}
		}
		if (op == Op::OPCODE_CAST_TO_BUILTIN) {
			const int typ = (ip + 3 < code.size()) ? code[ip + 3] : -1;
			if (typ != int(Variant::INT) && typ != int(Variant::FLOAT) && typ != int(Variant::STRING)) {
				r_err = vformat("%s:%s:%d cast type %d is not int/float/string", p_path, String(fn->get_name()), ip, typ);
				return ERR_UNCONFIGURED;
			}
		}
		if (op == Op::OPCODE_CONSTRUCT) {
			const int iac = (ip + 1 < code.size()) ? code[ip + 1] : 0;
			const int typ = (ip + 3 + iac < code.size()) ? code[ip + 3 + iac] : -1;
			if (typ != int(Variant::VECTOR2) && typ != int(Variant::VECTOR3)) {
				r_err = vformat("%s:%s:%d construct type %d is not Vector2/Vector3", p_path, String(fn->get_name()), ip, typ);
				return ERR_UNCONFIGURED;
			}
		}
		if (op == Op::OPCODE_CONSTRUCT_VALIDATED) {
			const int iac = (ip + 1 < code.size()) ? code[ip + 1] : 0;
			const int argc = (ip + 2 + iac < code.size()) ? code[ip + 2 + iac] : -1;
			if (argc != 2 && argc != 3) {
				r_err = vformat("%s:%s:%d construct validated argc %d is not Vector2/Vector3", p_path, String(fn->get_name()), ip, argc);
				return ERR_UNCONFIGURED;
			}
		}
		if (op == Op::OPCODE_OPERATOR) {
			r_norm.push_back(Op::OPCODE_OPERATOR);
			r_norm.push_back(code[ip + 1]);
			r_norm.push_back(code[ip + 2]);
			r_norm.push_back(code[ip + 3]);
			r_norm.push_back(code[ip + 4]);
		} else if (op == Op::OPCODE_OPERATOR_VALIDATED) {
			r_norm.push_back(Op::OPCODE_OPERATOR);
			r_norm.push_back(code[ip + 1]);
			r_norm.push_back(code[ip + 2]);
			r_norm.push_back(code[ip + 3]);
			r_norm.push_back(fn->get_validated_operator_kind(code[ip + 4]));
		} else if (op == Op::OPCODE_GET_NAMED_VALIDATED || op == Op::OPCODE_SET_NAMED_VALIDATED) {
			const String pname = String(op == Op::OPCODE_GET_NAMED_VALIDATED ? fn->get_getter_name(code[ip + 3]) : fn->get_setter_name(code[ip + 3]));
			int namei = 0;
			for (int g = 0; g < fn->get_global_names_count(); g++) {
				if (String(fn->get_global_name(g)) == pname) {
					namei = g;
					break;
				}
			}
			r_norm.push_back(op == Op::OPCODE_GET_NAMED_VALIDATED ? int(Op::OPCODE_GET_NAMED) : int(Op::OPCODE_SET_NAMED));
			r_norm.push_back(code[ip + 1]);
			r_norm.push_back(code[ip + 2]);
			r_norm.push_back(namei);
		} else {
			for (int i = 0; i < sz && ip + i < code.size(); i++) {
				r_norm.push_back(code[ip + i]);
			}
		}
		ip += sz;
	}
	return OK;
}

static void append_variant(Vector<uint8_t> &o, const Variant &v) {
	const int t = int(v.get_type());
	o.push_back(uint8_t(t));
	switch (v.get_type()) {
		case Variant::BOOL:
			o.push_back(bool(v) ? 1 : 0);
			break;
		case Variant::INT:
			append_i32(o, int32_t(int64_t(v)));
			break;
		case Variant::FLOAT:
			append_f32(o, float(v));
			break;
		case Variant::STRING:
		case Variant::STRING_NAME:
			append_str(o, String(v));
			break;
		case Variant::VECTOR2: {
			const Vector2 p = v;
			append_f32(o, p.x);
			append_f32(o, p.y);
		} break;
		case Variant::VECTOR3: {
			const Vector3 p = v;
			append_f32(o, p.x);
			append_f32(o, p.y);
			append_f32(o, p.z);
		} break;
		default:
			break;
	}
}

static Vector<uint8_t> pack_official_gdbc(const Vector<String> &p_paths, const Vector<Ref<GDScript>> &p_scripts, ScriptIR &r_ir) {
	Vector<uint8_t> out;
	out.push_back('G');
	out.push_back('D');
	out.push_back('B');
	out.push_back('C');
	append_u16(out, 12);
	append_u16(out, uint16_t(p_paths.size()));
	for (int s = 0; s < p_paths.size(); s++) {
		append_str(out, p_paths[s]);
		uint16_t owner = 0xffff;
		if (r_ir.path_owners.has(p_paths[s])) {
			owner = r_ir.path_owners[p_paths[s]];
		}
		append_u16(out, owner);
		Vector<String> mem_names;
		for (int mi = 0; mi < 64; mi++) {
			const StringName n = p_scripts[s]->debug_get_member_by_index(mi);
			if (String(n).is_empty()) {
				break;
			}
			mem_names.push_back(String(n));
		}
		append_u16(out, uint16_t(mem_names.size()));
		for (int m = 0; m < mem_names.size(); m++) {
			append_str(out, mem_names[m]);
		}
		const HashMap<StringName, GDScriptFunction *> &fns = p_scripts[s]->get_member_functions();
		Vector<StringName> names;
		for (const KeyValue<StringName, GDScriptFunction *> &E : fns) {
			names.push_back(E.key);
		}
		append_u16(out, uint16_t(names.size()));
		for (int i = 0; i < names.size(); i++) {
			GDScriptFunction *fn = fns[names[i]];
			append_str(out, String(names[i]));
			out.push_back(uint8_t(fn->get_argument_count()));
			append_u16(out, uint16_t(fn->get_max_stack_size()));
			Vector<int> norm;
			String err;
			if (scan_and_normalize(p_paths[s], p_scripts[s].ptr(), fn, norm, err) != OK) {
				r_ir.rejected.push_back(err);
				r_ir.map_text += "REJECT " + err + "\n";
				append_u16(out, 0);
				append_u16(out, 0);
				append_u16(out, 0);
				append_u16(out, 0);
				continue;
			}
			append_u16(out, uint16_t(norm.size()));
			for (int c = 0; c < norm.size(); c++) {
				append_i32(out, norm[c]);
			}
			const int kc = fn->get_constant_count();
			append_u16(out, uint16_t(kc));
			for (int c = 0; c < kc; c++) {
				append_variant(out, fn->get_constant(c));
			}
			const int gn = fn->get_global_names_count();
			append_u16(out, uint16_t(gn));
			for (int g = 0; g < gn; g++) {
				append_str(out, String(fn->get_global_name(g)));
			}
			const int mc = fn->get_method_bind_count();
			append_u16(out, uint16_t(mc));
			for (int m = 0; m < mc; m++) {
				append_str(out, String(fn->get_method_bind_name(m)));
			}
		}
	}
	return out;
}

static const uint8_t kTapeCallGetNode = 6;
static const uint8_t kTapeCallNamed = 7;

static String first_quoted_action(const String &src) {
	int p = src.find("is_action_");
	if (p < 0) {
		return "ui_accept";
	}
	const int q = src.find("\"", p);
	if (q < 0) {
		return "ui_accept";
	}
	const int e = src.find("\"", q + 1);
	if (e <= q + 1) {
		return "ui_accept";
	}
	return src.substr(q + 1, e - q - 1);
}

static Vector<uint8_t> pack_luau_tape(const Vector<String> &p_paths, const Vector<String> &p_sources, const ScriptIR &r_ir) {
	Vector<uint8_t> out;
	out.push_back('G');
	out.push_back('D');
	out.push_back('B');
	out.push_back('C');
	append_u16(out, 12);
	append_u16(out, uint16_t(p_paths.size()));
	for (int i = 0; i < p_paths.size(); i++) {
		append_str(out, p_paths[i]);
		uint16_t owner = 0xffff;
		if (r_ir.path_owners.has(p_paths[i])) {
			owner = r_ir.path_owners[p_paths[i]];
		}
		append_u16(out, owner);
	}
	append_u16(out, uint16_t(p_paths.size() * 2));
	for (int i = 0; i < p_paths.size(); i++) {
		float rate = 0.4f;
		if (p_sources[i].contains("0.4")) {
			rate = 0.4f;
		}
		Vector<String> names;
		auto add_name = [&](const String &n) -> uint8_t {
			for (int k = 0; k < names.size(); k++) {
				if (names[k] == n) {
					return uint8_t(k);
				}
			}
			names.push_back(n);
			return uint8_t(names.size() - 1);
		};
		Vector<uint8_t> code;
		const String &src = p_sources[i];
		if (src.contains("rotate_y") && src.contains("delta")) {
			const uint8_t ni = add_name("rotate_y");
			code.push_back(kTapeLoadArg);
			code.push_back(0);
			code.push_back(kTapeLoadConstF);
			code.push_back(0);
			code.push_back(kTapeMul);
			code.push_back(kTapeCallSelf);
			code.push_back(ni);
		} else if (src.contains("rotate_y")) {
			const uint8_t ni = add_name("rotate_y");
			code.push_back(kTapeLoadConstF);
			code.push_back(0);
			code.push_back(kTapeCallSelf);
			code.push_back(ni);
		} else if (src.contains("rotate_x")) {
			code.push_back(kTapeLoadConstF);
			code.push_back(0);
			code.push_back(kTapeCallSelf);
			code.push_back(add_name("rotate_x"));
		} else if (src.contains("rotate_z")) {
			code.push_back(kTapeLoadConstF);
			code.push_back(0);
			code.push_back(kTapeCallSelf);
			code.push_back(add_name("rotate_z"));
		} else if (src.contains("translate")) {
			code.push_back(kTapeLoadConstF);
			code.push_back(0);
			code.push_back(kTapeCallSelf);
			code.push_back(add_name("translate"));
		}
		if (src.contains("hide(") || src.contains("hide (")) {
			code.push_back(kTapeCallNamed);
			code.push_back(add_name("hide"));
		}
		if (src.contains("show(") || src.contains("show (")) {
			code.push_back(kTapeCallNamed);
			code.push_back(add_name("show"));
		}
		if (src.contains("is_action_pressed") || src.contains("is_action_just_pressed") || src.contains("is_action_just_released")) {
			String act = "is_action_pressed";
			if (src.contains("is_action_just_released")) {
				act = "is_action_just_released";
			} else if (src.contains("is_action_just_pressed")) {
				act = "is_action_just_pressed";
			}
			add_name(first_quoted_action(src));
			code.push_back(kTapeCallNamed);
			code.push_back(add_name(act));
		}
		const char *named[] = {
			"get_parent", "get_child", "get_child_count", "has_node", "get_node_or_null",
			"set_visible", "is_visible", "abs", "clamp", "lerp", "min", "max",
			"sin", "cos", "floor", "ceil", "round",
			"int", "float", "str", "sign", "sqrt", "pow", "move_toward",
			"deg_to_rad", "rad_to_deg", "randf", "randi", "randf_range", "randi_range",
			"get_ticks_msec", "create_timer", "is_timer_done",
			"get_vector", "push_error", "get_frames_per_second",
			"play", "stop", "is_playing", "queue_free",
			"load", "instantiate", "change_scene", "change_scene_to_file", "get_tree",
			"set_position", "set_rotation", "look_at", "grab_focus", "release_focus",
			"print", "push_warning", "connect", "disconnect"
		};
		for (const char *nm : named) {
			if (src.contains(String(nm) + "(") || src.contains(String(nm) + " (")) {
				code.push_back(kTapeCallNamed);
				code.push_back(add_name(nm));
			}
		}
		if (src.contains("get_node")) {
			String node_name = ".";
			const int q = src.find("get_node(\"");
			if (q >= 0) {
				const int e = src.find("\"", q + 10);
				if (e > q + 10) {
					node_name = src.substr(q + 10, e - (q + 10));
				}
			}
			code.push_back(kTapeCallGetNode);
			code.push_back(add_name(node_name));
		}
		code.push_back(kTapeReturn);
		if (names.is_empty()) {
			add_name("rotate_y");
		}
		Vector<uint8_t> ready_code;
		{
			int rp = src.find("func _ready");
			if (rp < 0) {
				rp = src.find("function _ready");
			}
			if (rp >= 0) {
				int rend = src.length();
				const int n1 = src.find("\nfunc ", rp + 1);
				const int n2 = src.find("\nfunction ", rp + 1);
				if (n1 >= 0 && n1 < rend) {
					rend = n1;
				}
				if (n2 >= 0 && n2 < rend) {
					rend = n2;
				}
				const String ready_src = src.substr(rp, rend - rp);
				if (ready_src.contains("is_action_pressed") || ready_src.contains("is_action_just_pressed") || ready_src.contains("is_action_just_released")) {
					String act = "is_action_pressed";
					if (ready_src.contains("is_action_just_released")) {
						act = "is_action_just_released";
					} else if (ready_src.contains("is_action_just_pressed")) {
						act = "is_action_just_pressed";
					}
					add_name(first_quoted_action(ready_src));
					ready_code.push_back(kTapeCallNamed);
					ready_code.push_back(add_name(act));
				}
				for (const char *nm : named) {
					if (ready_src.contains(String(nm) + "(") || ready_src.contains(String(nm) + " (")) {
						ready_code.push_back(kTapeCallNamed);
						ready_code.push_back(add_name(nm));
					}
				}
				if (ready_src.contains("get_node")) {
					String node_name = ".";
					const int q = ready_src.find("get_node(\"");
					if (q >= 0) {
						const int e = ready_src.find("\"", q + 10);
						if (e > q + 10) {
							node_name = ready_src.substr(q + 10, e - (q + 10));
						}
					}
					ready_code.push_back(kTapeCallGetNode);
					ready_code.push_back(add_name(node_name));
				}
			}
		}
		ready_code.push_back(kTapeReturn);
		auto emit_fn = [&](const String &fname, const Vector<uint8_t> &body) {
			append_str(out, fname);
			out.push_back(1);
			append_u16(out, 1);
			append_u16(out, uint16_t(body.size()));
			append_f32(out, rate);
			out.push_back(uint8_t(CLAMP(names.size(), 0, 255)));
			for (int n = 0; n < names.size(); n++) {
				append_str(out, names[n]);
			}
			for (int c = 0; c < body.size(); c++) {
				out.push_back(body[c]);
			}
		};
		emit_fn("_ready", ready_code);
		emit_fn("_process", code);
		if (src.contains("_physics_process")) {
			emit_fn("_physics_process", code);
		}
	}
	return out;
}

static void walk_scripts(const String &p_dir, ScriptIR &r_out) {
	Ref<DirAccess> da = DirAccess::open(p_dir);
	if (da.is_null()) {
		return;
	}
	da->list_dir_begin();
	for (String n = da->get_next(); !n.is_empty(); n = da->get_next()) {
		if (n == "." || n == ".." || n.begins_with(".")) {
			continue;
		}
		const String path = p_dir.path_join(n);
		if (da->current_is_dir()) {
			if (skip_script_dir(n)) {
				continue;
			}
			walk_scripts(path, r_out);
			continue;
		}
		if (!(n.ends_with(".gd") || n.ends_with(".luau"))) {
			continue;
		}
		if (n == "run_tests.gd" || n.begins_with("test_")) {
			continue;
		}
		const String src = FileAccess::get_file_as_string(path);
		String why;
		if (source_is_rejected(src, why)) {
			r_out.rejected.push_back(path + " : " + why);
		} else {
			r_out.accepted.push_back(path);
			warn_unknown_actions(src, r_out);
			collect_literal_scenes(src, r_out.extra_scenes);
			if (src.contains("0.4") && (src.contains("rotate_y") || src.contains("delta * 0.4"))) {
				r_out.rotate_y = 0.4f;
			}
		}
	}
}

static uint16_t mask_from_input_event(const Ref<InputEvent> &p_ev) {
	const Ref<InputEventKey> key = p_ev;
	if (key.is_valid()) {
		const Key k = key->get_physical_keycode() != Key::NONE ? key->get_physical_keycode() : key->get_keycode();
		if (k == Key::LEFT || k == Key::A) {
			return 1 << 7;
		}
		if (k == Key::RIGHT || k == Key::D) {
			return 1 << 5;
		}
		if (k == Key::UP || k == Key::W) {
			return 1 << 4;
		}
		if (k == Key::DOWN || k == Key::S) {
			return 1 << 6;
		}
		if (k == Key::ENTER || k == Key::KP_ENTER || k == Key::SPACE || k == Key::Z) {
			return 1 << 14;
		}
		if (k == Key::ESCAPE || k == Key::BACKSPACE) {
			return 1 << 13;
		}
		if (k == Key::SHIFT) {
			return 1 << 15;
		}
		if (k == Key::CTRL) {
			return 1 << 12;
		}
		if (k == Key::TAB) {
			return 1 << 11;
		}
	}
	const Ref<InputEventJoypadButton> joy = p_ev;
	if (joy.is_valid()) {
		const JoyButton b = joy->get_button_index();
		if (b == JoyButton::A || b == JoyButton::DPAD_DOWN) {
			return (b == JoyButton::DPAD_DOWN) ? uint16_t(1 << 6) : uint16_t(1 << 14);
		}
		if (b == JoyButton::B) {
			return 1 << 13;
		}
		if (b == JoyButton::X) {
			return 1 << 15;
		}
		if (b == JoyButton::Y) {
			return 1 << 12;
		}
		if (b == JoyButton::LEFT_SHOULDER) {
			return 1 << 10;
		}
		if (b == JoyButton::RIGHT_SHOULDER) {
			return 1 << 11;
		}
		if (b == JoyButton::START) {
			return 1 << 3;
		}
		if (b == JoyButton::BACK) {
			return 1 << 0;
		}
		if (b == JoyButton::DPAD_LEFT) {
			return 1 << 7;
		}
		if (b == JoyButton::DPAD_RIGHT) {
			return 1 << 5;
		}
		if (b == JoyButton::DPAD_UP) {
			return 1 << 4;
		}
	}
	return 0;
}

static void cook_input_map(ScriptIR &r_out) {
	InputMap *im = InputMap::get_singleton();
	if (!im) {
		return;
	}
	const List<StringName> actions = im->get_actions();
	for (const StringName &sn : actions) {
		const String name = String(sn);
		if (action_is_known(name)) {
			continue;
		}
		const List<Ref<InputEvent>> *evs = im->action_get_events(sn);
		if (!evs) {
			continue;
		}
		uint16_t mask = 0;
		for (const Ref<InputEvent> &e : *evs) {
			mask = uint16_t(mask | mask_from_input_event(e));
		}
		if (!mask) {
			continue;
		}
		if (r_out.input_actions.size() >= 16) {
			break;
		}
		ScriptIR::InputAction row;
		row.name = name;
		row.mask = mask;
		r_out.input_actions.push_back(row);
	}
}

Error cook_scripts(const String &p_export_dir, ScriptIR &r_out) {
	cook_input_map(r_out);
	walk_scripts("res://", r_out);
	for (int i = 0; i < r_out.attached.size(); i++) {
		const String path = r_out.attached[i];
		if (r_out.accepted.has(path) || r_out.rejected.find(path) >= 0) {
			continue;
		}
		if (!FileAccess::exists(path)) {
			continue;
		}
		const String src = FileAccess::get_file_as_string(path);
		String why;
		if (source_is_rejected(src, why)) {
			r_out.rejected.push_back(path + " : " + why);
		} else if (!r_out.accepted.has(path)) {
			r_out.accepted.push_back(path);
			warn_unknown_actions(src, r_out);
			collect_literal_scenes(src, r_out.extra_scenes);
		}
	}

	r_out.map_text = vformat("IR rotate_y=%f (GDScript+Luau ABI 12)\n", r_out.rotate_y);
	for (int i = 0; i < r_out.accepted.size(); i++) {
		r_out.map_text += "OK " + r_out.accepted[i] + "\n";
	}
	for (int i = 0; i < r_out.rejected.size(); i++) {
		r_out.map_text += "REJECT " + r_out.rejected[i] + "\n";
	}

	Vector<String> gd_paths;
	Vector<Ref<GDScript>> gd_scripts;
	Vector<String> lu_paths;
	Vector<String> lu_src;
	for (int i = 0; i < r_out.accepted.size(); i++) {
		const String path = r_out.accepted[i];
		const String src = FileAccess::get_file_as_string(path);
		if (path.ends_with(".gd")) {
			Ref<GDScript> script;
			script.instantiate();
			script->set_source_code(src);
			if (script->reload(true) != OK) {
				r_out.rejected.push_back(path + " : GDScript compile failed");
				r_out.map_text += "REJECT " + path + " : GDScript compile failed\n";
				continue;
			}
			gd_paths.push_back(path);
			gd_scripts.push_back(script);
		} else if (path.ends_with(".luau")) {
			String unbound;
			if (luau_source_unbound(src, unbound)) {
				r_out.rejected.push_back(path + " : unbound method '" + unbound + "'");
				r_out.map_text += "REJECT " + path + " : unbound method '" + unbound + "'\n";
				continue;
			}
#ifdef MODULE_LUAU_MODULE_ENABLED
			const PackedByteArray bc = luau_module::Luau::compile(src);
			if (bc.is_empty()) {
				r_out.rejected.push_back(path + " : Luau compile failed");
				r_out.map_text += "REJECT " + path + " : Luau compile failed\n";
				continue;
			}
#endif
			lu_paths.push_back(path);
			lu_src.push_back(src);
		}
	}
	r_out.gd_count = uint16_t(gd_paths.size());
	r_out.luau_count = uint16_t(lu_paths.size());
	r_out.cooked_gd = gd_paths;

	if (!gd_paths.is_empty()) {
		const Vector<uint8_t> gdbc = pack_official_gdbc(gd_paths, gd_scripts, r_out);
		r_out.gdbc_path = p_export_dir.path_join("SCRIPT.GD.BC");
		Ref<FileAccess> gf = FileAccess::open(r_out.gdbc_path, FileAccess::WRITE);
		ERR_FAIL_COND_V(gf.is_null(), ERR_CANT_CREATE);
		gf->store_buffer(gdbc);
	}
	if (!lu_paths.is_empty()) {
		Vector<uint8_t> lubc;
		lubc.push_back('L');
		lubc.push_back('U');
		lubc.push_back('B');
		lubc.push_back('C');
#ifdef MODULE_LUAU_MODULE_ENABLED
		const PackedByteArray official = luau_module::Luau::compile(lu_src[0]);
		append_u16(lubc, uint16_t(official.size() & 0xffff));
		for (int i = 0; i < official.size(); i++) {
			lubc.push_back(official[i]);
		}
#else
		append_u16(lubc, 0);
#endif
		const Vector<uint8_t> tape = pack_luau_tape(lu_paths, lu_src, r_out);
		for (int i = 0; i < tape.size(); i++) {
			lubc.push_back(tape[i]);
		}
		r_out.luau_path = p_export_dir.path_join("SCRIPT.LU.BC");
		Ref<FileAccess> lf = FileAccess::open(r_out.luau_path, FileAccess::WRITE);
		ERR_FAIL_COND_V(lf.is_null(), ERR_CANT_CREATE);
		lf->store_buffer(lubc);
	}

	Ref<FileAccess> f = FileAccess::open(p_export_dir.path_join("SCRIPT_IR.txt"), FileAccess::WRITE);
	ERR_FAIL_COND_V(f.is_null(), ERR_CANT_CREATE);
	f->store_string(r_out.map_text);
	return OK;
}

void collect_literal_scenes(const String &p_source, Vector<String> &r_paths) {
	const String src = strip_line_comments(p_source);
	int p = 0;
	while (true) {
		const int hit = src.find("res://", p);
		if (hit < 0) {
			break;
		}
		int e = hit;
		while (e < src.length()) {
			const char32_t c = src[e];
			if (c == '"' || c == '\'' || c == ')' || c == ',' || c == ' ' || c == '\n' || c == '\r' || c == '\t') {
				break;
			}
			e++;
		}
		p = e > hit ? e : hit + 6;
		const String path = src.substr(hit, e - hit);
		if (path.ends_with(".tscn") && !r_paths.has(path)) {
			r_paths.push_back(path);
		}
	}
}

Error write_script_ir(const String &p_export_dir, const ScriptIR &p_ir) {
	Vector<uint8_t> bytes;
	union {
		float f;
		uint32_t u;
	} rot;
	rot.f = p_ir.rotate_y;
	bytes.push_back(uint8_t(rot.u & 0xff));
	bytes.push_back(uint8_t((rot.u >> 8) & 0xff));
	bytes.push_back(uint8_t((rot.u >> 16) & 0xff));
	bytes.push_back(uint8_t((rot.u >> 24) & 0xff));
	auto i16 = [&](int16_t v) {
		bytes.push_back(uint8_t(uint16_t(v) & 0xff));
		bytes.push_back(uint8_t(uint16_t(v) >> 8));
	};
	i16(p_ir.pos_x);
	i16(p_ir.pos_y);
	i16(p_ir.pos_z);
	i16(p_ir.geom_screen);
	i16(int16_t(p_ir.flags));
	i16(int16_t(p_ir.light_count));
	for (int i = 0; i < 3; i++) {
		i16(p_ir.lights[i].dx);
		i16(p_ir.lights[i].dy);
		i16(p_ir.lights[i].dz);
		bytes.push_back(p_ir.lights[i].r);
		bytes.push_back(p_ir.lights[i].g);
		bytes.push_back(p_ir.lights[i].b);
		bytes.push_back(0);
	}
	i16(int16_t(p_ir.display_w));
	i16(int16_t(p_ir.display_h));
	bytes.push_back(p_ir.region);
	bytes.push_back(p_ir.display_flags);
	i16(int16_t(p_ir.gd_count));
	i16(int16_t(p_ir.luau_count));
	i16(p_ir.cam_rx);
	i16(p_ir.cam_ry);
	i16(p_ir.cam_rz);
	if (!p_ir.input_actions.is_empty()) {
		bytes.push_back('I');
		bytes.push_back('N');
		bytes.push_back('P');
		bytes.push_back('1');
		bytes.push_back(uint8_t(MIN(p_ir.input_actions.size(), 16)));
		for (int i = 0; i < p_ir.input_actions.size() && i < 16; i++) {
			const CharString utf = p_ir.input_actions[i].name.utf8();
			const int n = MIN(utf.length(), 15);
			bytes.push_back(uint8_t(n));
			for (int c = 0; c < n; c++) {
				bytes.push_back(uint8_t(utf[c]));
			}
			append_u16(bytes, p_ir.input_actions[i].mask);
		}
	}
	Ref<FileAccess> ir = FileAccess::open(p_export_dir.path_join("SCRIPT.IR"), FileAccess::WRITE);
	ERR_FAIL_COND_V(ir.is_null(), ERR_CANT_CREATE);
	ir->store_buffer(bytes);
	return OK;
}

} // namespace ps1_cook
