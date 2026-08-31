/**************************************************************************/
/*  script_writer.cpp                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#include "script_writer.h"

#include "cooker.h"

#include <cstring>

namespace n64_cook {

enum {
	OP_END = 0,
	OP_LOADK_F = 1,
	OP_LOAD_DELTA = 2,
	OP_CALL_ROTATE_Y = 4,
	OP_SET_RUMBLE = 10,
	OP_STOP_RUMBLE = 11,
	OP_USER_SAVE = 14,
	OP_USER_LOAD = 15,
	OP_LOOK_CAMERA = 27,
	OP_NEXT_CAMERA = 30,
	OP_SLIDE = 35,
	OP_OVERLAP = 36,
	OP_RAYCAST = 37,
	OP_CHANGE_SCENE = 41,
	OP_INSTANTIATE = 42,
	OP_SEEK_ANIM = 45,
	OP_CALL_NATIVE = 48,
	OP_LOAD_SCENE = 50,
};

enum {
	NAT_SET_CAM = 63,
	NAT_LOAD_SPRITES = 64,
	NAT_SET_DEFAULT_CAM = 72,
	NAT_PLAY_ANIM = 73,
	NAT_STOP_ANIM = 74,
	NAT_LOAD_ANIMS = 75,
	NAT_UNLOAD_ANIMS = 76,
	NAT_LOAD_HITS = 77,
	NAT_UNLOAD_HITS = 78,
	NAT_UNLOAD_SPRITES = 79,
};

static void append_u16(Vector<uint8_t> &o, uint16_t v) {
	o.push_back(uint8_t(v & 0xff));
	o.push_back(uint8_t(v >> 8));
}

static void append_f32(Vector<uint8_t> &o, float v) {
	uint32_t bits = 0;
	memcpy(&bits, &v, sizeof(bits));
	o.push_back(uint8_t(bits & 0xff));
	o.push_back(uint8_t((bits >> 8) & 0xff));
	o.push_back(uint8_t((bits >> 16) & 0xff));
	o.push_back(uint8_t((bits >> 24) & 0xff));
}

static String extract_quoted(const String &p_src, int p_from) {
	const int a = p_src.find("\"", p_from);
	const int b = p_src.find("'", p_from);
	int q = -1;
	char32_t quote = '"';
	if (a >= 0 && (b < 0 || a < b)) {
		q = a;
		quote = '"';
	} else if (b >= 0) {
		q = b;
		quote = '\'';
	}
	if (q < 0) {
		return String();
	}
	const int e = p_src.find_char(quote, q + 1);
	if (e < 0) {
		return String();
	}
	return p_src.substr(q + 1, e - q - 1);
}

static void collect_after_token(const String &p_src, const String &p_token, Vector<String> &r_paths) {
	int from = 0;
	while (true) {
		const int hit = p_src.find(p_token, from);
		if (hit < 0) {
			break;
		}
		const String path = extract_quoted(p_src, hit + p_token.length());
		if (path.begins_with("res://") && path.ends_with(".tscn") && !r_paths.has(path)) {
			r_paths.push_back(path);
		}
		from = hit + p_token.length();
	}
}

void collect_scene_refs(const String &p_src, Vector<String> &r_paths) {
	collect_after_token(p_src, "change_scene(", r_paths);
	collect_after_token(p_src, "instantiate(", r_paths);
	collect_after_token(p_src, "load(", r_paths);
}

static bool load_call_is_tscn_only(const String &p_src) {
	int from = 0;
	while (true) {
		const int hit = p_src.find("load(", from);
		if (hit < 0) {
			return true;
		}
		const String path = extract_quoted(p_src, hit + 5);
		if (!path.begins_with("res://") || !path.ends_with(".tscn")) {
			return false;
		}
		from = hit + 5;
	}
}

bool source_is_forbidden(const String &p_src) {
	if (p_src.contains("eval(") || p_src.contains("HTTPClient") || p_src.contains("add_child") || p_src.contains("ClassDB")) {
		return true;
	}
	if (p_src.contains("load(") && !load_call_is_tscn_only(p_src)) {
		return true;
	}
	return false;
}

static Error emit_pack_op(Vector<uint8_t> &code, const Vector<String> &p_sources, const String &p_token, int p_op, const HashMap<String, int> &p_packs, String &r_err) {
	Vector<String> paths;
	for (int i = 0; i < p_sources.size(); i++) {
		collect_after_token(p_sources[i], p_token, paths);
	}
	for (int i = 0; i < paths.size(); i++) {
		const int *id = p_packs.getptr(paths[i]);
		if (!id) {
			r_err = "pack path not cooked: " + paths[i];
			return FAILED;
		}
		code.push_back(uint8_t(OP_LOADK_F));
		append_f32(code, float(*id));
		code.push_back(uint8_t(p_op));
	}
	return OK;
}

Error write_script_blob(const Vector<String> &p_sources, const HashMap<String, int> &p_packs, Vector<uint8_t> &r_out, String &r_ir_extra) {
	r_ir_extra = String();
	for (int i = 0; i < p_sources.size(); i++) {
		if (source_is_forbidden(p_sources[i])) {
			return FAILED;
		}
	}
	Vector<uint8_t> code;
	bool rotate_y = false;
	bool rumble = false;
	bool change = false;
	bool inst = false;
	bool load_sc = false;
	for (int i = 0; i < p_sources.size(); i++) {
		if (p_sources[i].contains("rotate_y")) {
			rotate_y = true;
		}
		if (p_sources[i].contains("start_joy_vibration")) {
			rumble = true;
		}
		if (p_sources[i].contains("change_scene(")) {
			change = true;
		}
		if (p_sources[i].contains("instantiate(")) {
			inst = true;
		}
		if (p_sources[i].contains("load(")) {
			load_sc = true;
		}
	}
	if (rotate_y) {
		code.push_back(uint8_t(OP_LOAD_DELTA));
		code.push_back(uint8_t(OP_CALL_ROTATE_Y));
	}
	if (rumble) {
		code.push_back(uint8_t(OP_SET_RUMBLE));
		code.push_back(uint8_t(OP_STOP_RUMBLE));
	}
	String err;
	if (change) {
		if (emit_pack_op(code, p_sources, "change_scene(", OP_CHANGE_SCENE, p_packs, err) != OK) {
			r_ir_extra = err;
			return FAILED;
		}
		r_ir_extra += "IR N64.change_scene (pack swap)\n";
	}
	if (inst) {
		if (emit_pack_op(code, p_sources, "instantiate(", OP_INSTANTIATE, p_packs, err) != OK) {
			r_ir_extra = err;
			return FAILED;
		}
		r_ir_extra += "IR N64.instantiate (RAM-gated layer)\n";
	}
	if (load_sc) {
		if (emit_pack_op(code, p_sources, "load(", OP_LOAD_SCENE, p_packs, err) != OK) {
			r_ir_extra = err;
			return FAILED;
		}
		r_ir_extra += "IR N64.load (res://*.tscn pack)\n";
	}
	bool any_src = false;
	for (int i = 0; i < p_sources.size(); i++) {
		const String &src = p_sources[i];
		any_src = true;
		if (src.contains("set_camera") || src.contains("set_cam")) {
			code.push_back(uint8_t(OP_LOADK_F));
			append_f32(code, 0.0f);
			code.push_back(uint8_t(OP_CALL_NATIVE));
			code.push_back(uint8_t(NAT_SET_CAM));
			r_ir_extra += "IR N64.set_camera\n";
		}
		if (src.contains("set_default_camera")) {
			code.push_back(uint8_t(OP_LOADK_F));
			append_f32(code, 0.0f);
			code.push_back(uint8_t(OP_CALL_NATIVE));
			code.push_back(uint8_t(NAT_SET_DEFAULT_CAM));
		}
		if (src.contains("next_camera")) {
			code.push_back(uint8_t(OP_NEXT_CAMERA));
		}
		if (src.contains("look_camera")) {
			code.push_back(uint8_t(OP_LOADK_F));
			append_f32(code, 0.0f);
			code.push_back(uint8_t(OP_LOADK_F));
			append_f32(code, 0.0f);
			code.push_back(uint8_t(OP_LOOK_CAMERA));
		}
		if (src.contains("play_anim") || src.contains(".play(")) {
			code.push_back(uint8_t(OP_LOADK_F));
			append_f32(code, 0.0f);
			code.push_back(uint8_t(OP_CALL_NATIVE));
			code.push_back(uint8_t(NAT_PLAY_ANIM));
			r_ir_extra += "IR ANIM TRS\n";
		}
		if (src.contains("seek_anim") || src.contains(".seek(")) {
			code.push_back(uint8_t(OP_LOADK_F));
			append_f32(code, 0.0f);
			code.push_back(uint8_t(OP_SEEK_ANIM));
		}
		if (src.contains("load_anims")) {
			code.push_back(uint8_t(OP_LOADK_F));
			append_f32(code, 0.0f);
			code.push_back(uint8_t(OP_CALL_NATIVE));
			code.push_back(uint8_t(NAT_LOAD_ANIMS));
		}
		if (src.contains("unload_anims")) {
			code.push_back(uint8_t(OP_CALL_NATIVE));
			code.push_back(uint8_t(NAT_UNLOAD_ANIMS));
		}
		if (src.contains("load_sprites")) {
			code.push_back(uint8_t(OP_LOADK_F));
			append_f32(code, 0.0f);
			code.push_back(uint8_t(OP_CALL_NATIVE));
			code.push_back(uint8_t(NAT_LOAD_SPRITES));
		}
		if (src.contains("unload_sprites")) {
			code.push_back(uint8_t(OP_CALL_NATIVE));
			code.push_back(uint8_t(NAT_UNLOAD_SPRITES));
		}
		if (src.contains("unload_hits")) {
			code.push_back(uint8_t(OP_CALL_NATIVE));
			code.push_back(uint8_t(NAT_UNLOAD_HITS));
		}
		if (src.contains("overlaps") || src.contains("overlap")) {
			code.push_back(uint8_t(OP_OVERLAP));
			r_ir_extra += "IR HIT AABB\n";
		}
		if (src.contains("raycast")) {
			code.push_back(uint8_t(OP_RAYCAST));
		}
		if (src.contains("move_and_slide")) {
			code.push_back(uint8_t(OP_LOADK_F));
			append_f32(code, 0.0f);
			code.push_back(uint8_t(OP_LOADK_F));
			append_f32(code, 0.0f);
			code.push_back(uint8_t(OP_LOADK_F));
			append_f32(code, 0.0f);
			code.push_back(uint8_t(OP_SLIDE));
		}
		if (src.contains("user_save") || src.contains("godot_save_bytes") || src.contains("user://")) {
			code.push_back(uint8_t(OP_USER_SAVE));
			code.push_back(uint8_t(OP_USER_LOAD));
		}
	}
	(void)any_src;
	code.push_back(uint8_t(OP_END));
	r_out.clear();
	r_out.push_back('S');
	r_out.push_back('C');
	r_out.push_back('R');
	r_out.push_back('P');
	append_u16(r_out, uint16_t(kCookABI));
	append_u16(r_out, uint16_t(code.size()));
	for (int i = 0; i < code.size(); i++) {
		r_out.push_back(code[i]);
	}
	if (r_out.size() >= 2 && r_out[0] == 0x4D && r_out[1] == 0x5A) {
		return FAILED;
	}
	return OK;
}

} // namespace n64_cook
