/**************************************************************************/
/*  script_cook.cpp                                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#include "script_cook.h"

#include "cooker.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"

#include <cstring>

namespace ps2_cook {

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
		"eval(", "ClassDB.", "HTTPClient", "Thread.", "await ",
		"OS.execute", "preload(", "JSON.parse"
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
		if (hit > 0) {
			const char32_t prev = src[hit - 1];
			if ((prev >= 'a' && prev <= 'z') || (prev >= 'A' && prev <= 'Z') || (prev >= '0' && prev <= '9') || prev == '_') {
				continue;
			}
		}
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
	if (src.contains("FileAccess") && !src.contains("user://")) {
		r_why = "FileAccess (user:// only)";
		return true;
	}
	return false;
}

void collect_literal_scenes(const String &p_source, Vector<String> &r_paths) {
	const String src = strip_line_comments(p_source);
	int p = 0;
	while (true) {
		const int q = src.find("res://", p);
		if (q < 0) {
			break;
		}
		int e = q + 6;
		while (e < src.length()) {
			const char32_t c = src[e];
			if (c == '"' || c == '\'' || c == ')' || c == ' ' || c == '\n') {
				break;
			}
			e++;
		}
		const String path = src.substr(q, e - q);
		if (path.ends_with(".tscn") && !r_paths.has(path)) {
			r_paths.push_back(path);
		}
		p = e;
	}
}

static bool skip_script_dir(const String &p_name) {
	return p_name == "tests" || p_name == "tools" || p_name == ".godot" || p_name == "export";
}

static bool reject_unsupported_apis(const String &p_src, String &r_why);

static void walk_scripts(const String &p_dir, ScriptCook &r_out) {
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
			if (!skip_script_dir(n)) {
				walk_scripts(path, r_out);
			}
			continue;
		}
		if (!(n.ends_with(".gd") || n.ends_with(".luau"))) {
			continue;
		}
		if (n == "run_tests.gd" || n.begins_with("illegal_")) {
			continue;
		}
		const String src = FileAccess::get_file_as_string(path);
		String why;
		if (source_is_rejected(src, why) || reject_unsupported_apis(strip_line_comments(src), why)) {
			r_out.rejected.push_back(path + " (" + why + ")");
			continue;
		}
		r_out.accepted.push_back(path);
		collect_literal_scenes(src, r_out.extra_scenes);
	}
}

static void append_u16(Vector<uint8_t> &o, uint16_t v) {
	o.push_back(uint8_t(v & 0xff));
	o.push_back(uint8_t(v >> 8));
}

static void append_u32(Vector<uint8_t> &o, uint32_t v) {
	append_u16(o, uint16_t(v & 0xffff));
	append_u16(o, uint16_t(v >> 16));
}

static void append_f32(Vector<uint8_t> &o, float v) {
	uint32_t bits = 0;
	memcpy(&bits, &v, sizeof(bits));
	append_u32(o, bits);
}

enum {
	OP_END = 0,
	OP_LOADK_F = 1,
	OP_LOAD_DELTA = 2,
	OP_MUL = 3,
	OP_CALL_ROTATE_Y = 4,
	OP_READY = 5,
	OP_PROCESS = 6,
	OP_HAS_FEATURE_PS2 = 7,
	OP_SWAP_PACK = 8,
	OP_PLAY_SFX = 9,
	OP_SET_RUMBLE = 10,
	OP_STOP_RUMBLE = 11,
	OP_INPUT_TRANSLATE = 12,
	OP_JUST_ACCEPT_SFX = 13,
	OP_USER_SAVE = 14,
	OP_USER_LOAD = 15,
	OP_MEMCARD_PRESENT = 16,
	OP_KIT_TICK = 17
};

static const char *kUnsupportedCalls[] = {
	"play_fmv", "play_xa", "stop_xa",
	"load_hud", "unload_hud", "load_tiles", "unload_tiles",
	"memcard_format", "memcard_wrap", "memcard_unwrap",
	"set_fog", "create_tween", "spawn_shot"
};

static bool is_ps2_alias(const String &p_name) {
	return p_name == "set_rumble" || p_name == "stop_rumble" || p_name == "get_pressure" ||
			p_name == "play_sfx" || p_name == "stop_sfx" || p_name == "swap_pack" ||
			p_name == "load_scene" || p_name == "unload_scene" || p_name == "get_abi" ||
			p_name == "is_ready" || p_name == "get_display_width" || p_name == "get_display_height" ||
			p_name == "memcard_present" || p_name == "memcard_ready" || p_name == "memcard_save" ||
			p_name == "memcard_load" || p_name == "get_stick" || p_name == "set_stick_deadzone" ||
			p_name == "set_checkpoint" || p_name == "respawn" || p_name == "portal" ||
			p_name == "hurt" || p_name == "say" || p_name == "can_load_scene" ||
			p_name == "is_scene_loaded" || p_name == "get_loaded_scene_count" ||
			p_name == "unload_pack";
}

static bool reject_unsupported_apis(const String &p_src, String &r_why) {
	for (const char *bad : kUnsupportedCalls) {
		if (p_src.contains(bad)) {
			r_why = String(bad) + " is not supported on PS2 ABI 1";
			return true;
		}
	}
	int p = 0;
	while (true) {
		const int hit = p_src.find("PS1.", p);
		if (hit < 0) {
			break;
		}
		int e = hit + 4;
		String name;
		while (e < p_src.length()) {
			const char32_t c = p_src[e];
			if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
				name += c;
				e++;
				continue;
			}
			break;
		}
		if (!name.is_empty() && !is_ps2_alias(name)) {
			r_why = "PS1." + name + " is not supported on PS2 ABI 1 (use PS2.* or drop the call)";
			return true;
		}
		p = e;
	}
	p = 0;
	while (true) {
		const int hit = p_src.find("PS2.", p);
		if (hit < 0) {
			break;
		}
		int e = hit + 4;
		String name;
		while (e < p_src.length()) {
			const char32_t c = p_src[e];
			if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
				name += c;
				e++;
				continue;
			}
			break;
		}
		if (!name.is_empty() && !is_ps2_alias(name)) {
			r_why = "PS2." + name + " is not implemented on PS2 ABI 1";
			return true;
		}
		p = e;
	}
	return false;
}

static bool parse_rotate_y_rate(const String &p_src, float &r_rate) {
	const int hit = p_src.find("rotate_y");
	if (hit < 0) {
		return false;
	}
	const int par = p_src.find("(", hit);
	if (par < 0) {
		return false;
	}
	const int star = p_src.find("*", par);
	if (star < 0) {
		return false;
	}
	int i = star + 1;
	while (i < p_src.length() && (p_src[i] == ' ' || p_src[i] == '\t')) {
		i++;
	}
	String num;
	while (i < p_src.length()) {
		const char32_t c = p_src[i];
		if ((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+') {
			num += c;
			i++;
			continue;
		}
		break;
	}
	if (num.is_empty() || !num.is_valid_float()) {
		return false;
	}
	r_rate = num.to_float();
	return true;
}

Error cook_scripts(const String &p_export_dir, ScriptCook &r_out) {
	walk_scripts("res://", r_out);
	if (!r_out.rejected.is_empty()) {
		return ERR_UNCONFIGURED;
	}

	bool has_gd_rotate = false;
	bool has_lu_rotate = false;
	bool has_feature = false;
	bool has_swap = false;
	bool has_sfx = false;
	bool has_rumble = false;
	bool has_input = false;
	bool has_translate = false;
	bool has_user = false;
	bool has_memcard = false;
	bool has_kit = false;
	float rotate_rate = 0.4f;
	float translate_rate = 1.0f;
	for (int i = 0; i < r_out.accepted.size(); i++) {
		const String path = r_out.accepted[i];
		const String src = strip_line_comments(FileAccess::get_file_as_string(path));
		float rate = 0.0f;
		if (parse_rotate_y_rate(src, rate)) {
			rotate_rate = rate;
			if (path.ends_with(".luau")) {
				has_lu_rotate = true;
			} else {
				has_gd_rotate = true;
			}
		}
		if (src.contains("OS.has_feature") && src.contains("\"ps2\"")) {
			has_feature = true;
		}
		if (src.contains("swap_pack") || src.contains("load_scene")) {
			has_swap = true;
		}
		if (src.contains("play_sfx")) {
			has_sfx = true;
		}
		if (src.contains("set_rumble") || src.contains("stop_rumble")) {
			has_rumble = true;
		}
		if (src.contains("Input.get_vector") || src.contains("Input.is_action") || src.contains("Input.get_joy_axis") || src.contains("get_stick")) {
			has_input = true;
		}
		if (src.contains("translate(") || src.contains("translate_object_local")) {
			has_translate = true;
		}
		if (src.contains("FileAccess") && src.contains("user://")) {
			has_user = true;
		}
		if (src.contains("memcard_present") || src.contains("memcard_save") || src.contains("memcard_load") || src.contains("memcard_ready")) {
			has_memcard = true;
		}
		if (src.contains("set_checkpoint") || src.contains("respawn") || src.contains("portal") ||
				src.contains("hurt") || src.contains("can_load_scene") || src.contains("is_scene_loaded") ||
				src.contains("unload_pack") || src.contains("Checkpoint") || src.contains("Hazard") ||
				src.contains("Pickup") || src.contains("LoadZone") || src.contains("kit_room")) {
			has_kit = true;
		}
	}
	if (!r_out.extra_scenes.is_empty()) {
		has_kit = true;
	}

	Vector<uint8_t> tape;
	if (has_feature) {
		tape.push_back(uint8_t(OP_READY));
		tape.push_back(uint8_t(OP_HAS_FEATURE_PS2));
	}
	if (has_swap) {
		tape.push_back(uint8_t(OP_READY));
		tape.push_back(uint8_t(OP_LOADK_F));
		append_f32(tape, 1.0f);
		tape.push_back(uint8_t(OP_SWAP_PACK));
	}
	if (has_sfx && !has_input) {
		tape.push_back(uint8_t(OP_READY));
		tape.push_back(uint8_t(OP_PLAY_SFX));
	}
	if (has_rumble) {
		tape.push_back(uint8_t(OP_READY));
		tape.push_back(uint8_t(OP_LOADK_F));
		append_f32(tape, 1.0f);
		tape.push_back(uint8_t(OP_LOADK_F));
		append_f32(tape, 0.0f);
		tape.push_back(uint8_t(OP_SET_RUMBLE));
		tape.push_back(uint8_t(OP_STOP_RUMBLE));
	}
	if (has_user || has_memcard) {
		tape.push_back(uint8_t(OP_READY));
		tape.push_back(uint8_t(OP_MEMCARD_PRESENT));
		tape.push_back(uint8_t(OP_USER_SAVE));
		tape.push_back(uint8_t(OP_USER_LOAD));
	}
	if (has_gd_rotate || has_lu_rotate || has_input || has_translate || has_kit) {
		tape.push_back(uint8_t(OP_PROCESS));
		if (has_gd_rotate || has_lu_rotate) {
			tape.push_back(uint8_t(OP_LOAD_DELTA));
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, rotate_rate);
			tape.push_back(uint8_t(OP_MUL));
			tape.push_back(uint8_t(OP_CALL_ROTATE_Y));
		}
		if (has_input || has_translate) {
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, translate_rate);
			tape.push_back(uint8_t(OP_INPUT_TRANSLATE));
		}
		if (has_input) {
			tape.push_back(uint8_t(OP_JUST_ACCEPT_SFX));
		}
		if (has_kit) {
			tape.push_back(uint8_t(OP_KIT_TICK));
		}
	}
	tape.push_back(uint8_t(OP_END));

	Vector<uint8_t> out;
	out.push_back('S');
	out.push_back('C');
	out.push_back('R');
	out.push_back('P');
	append_u16(out, uint16_t(kCookABI));
	append_u16(out, 0);
	append_u32(out, uint32_t(tape.size()));
	out.append_array(tape);
	r_out.scrp = out;

	r_out.ir_text = vformat("ABI %d\n", kCookABI);
	if (has_gd_rotate || has_lu_rotate) {
		String langs;
		if (has_gd_rotate && has_lu_rotate) {
			langs = "GDScript+Luau";
		} else if (has_lu_rotate) {
			langs = "Luau";
		} else {
			langs = "GDScript";
		}
		r_out.ir_text += vformat("IR rotate_y=%s (%s)\n", rtos(rotate_rate), langs);
	}
	if (has_feature) {
		r_out.ir_text += "IR OS.has_feature(\"ps2\")\n";
	}
	if (has_swap) {
		r_out.ir_text += "IR PS2.swap_pack/load_scene\n";
	}
	if (has_sfx) {
		r_out.ir_text += "IR PS2.play_sfx\n";
	}
	if (has_rumble) {
		r_out.ir_text += "IR PS2.set_rumble\n";
	}
	if (has_input) {
		r_out.ir_text += "IR Input.get_vector/is_action (ui_left..ui_accept)\n";
	}
	if (has_translate) {
		r_out.ir_text += "IR Node3D.translate\n";
	}
	if (has_user) {
		r_out.ir_text += "IR FileAccess user:// → host:SAVE.BIN / mc0:SAVE.BIN\n";
	}
	if (has_memcard) {
		r_out.ir_text += "IR PS2.memcard_present/save/load\n";
	}
	if (has_kit) {
		r_out.ir_text += "IR kit tick (portal/load/checkpoint/save/hazard)\n";
		r_out.ir_text += vformat("IR STREAM extra_scenes=%d\n", r_out.extra_scenes.size());
	}
	if (has_user) {
		r_out.warnings.push_back("FileAccess user:// maps to host:SAVE.BIN then mc0:SAVE.BIN. Missing HostFs/memcard is a guest error.");
	}
	r_out.ir_text += "kit: Spawn Checkpoint Portal Player Hazard Pickup Music Unload Load Talk Spawner Save\n";
	for (int i = 0; i < r_out.accepted.size(); i++) {
		r_out.ir_text += r_out.accepted[i] + "\n";
	}
	(void)p_export_dir;
	return OK;
}

} // namespace ps2_cook
