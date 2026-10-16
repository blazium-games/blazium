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
		"OS.execute", "preload(", "JSON.parse", "match ", "AnimationTree"
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
	return p_name == "tests" || p_name == "tools" || p_name == "projects" || p_name == ".godot" || p_name == "export";
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
			int line = 1;
			const int hit = src.find(why.get_slice(" ", 0));
			if (hit >= 0) {
				for (int i = 0; i < hit; i++) {
					if (src[i] == '\n') {
						line++;
					}
				}
			}
			r_out.rejected.push_back(vformat("%s:%d: %s", path, line, why));
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
	OP_KIT_TICK = 17,
	OP_SAY = 18,
	OP_NAV_FOLLOW = 19,
	OP_SAVE_SLOT = 20,
	OP_PLAY_FMV = 21,
	OP_SYS_TICK = 22,
	OP_PLAY_MUSIC = 23,
	OP_STOP_MUSIC = 24,
	OP_MUSIC_VOL = 25,
	OP_LOOK_STICK = 26,
	OP_LOOK_CAMERA = 27,
	OP_ORBIT_CAMERA = 28,
	OP_SHAKE_CAMERA = 29,
	OP_NEXT_CAMERA = 30,
	OP_ATTACH_CAMERA = 31,
	OP_TWEEN = 32,
	OP_TIMER = 33,
	OP_KILL_TWEENS = 34,
	OP_SLIDE = 35,
	OP_OVERLAP = 36,
	OP_RAYCAST = 37,
	OP_TILE_AT = 38,
	OP_SET_FADE = 39,
	OP_SCENE_FADE = 40,
	OP_CHANGE_SCENE = 41,
	OP_INSTANTIATE = 42,
	OP_UNLOAD_PACK = 43,
	OP_SAY_TEXT = 44,
	OP_SEEK_ANIM = 45,
	OP_JMP = 46,
	OP_JZ = 47,
	OP_CALL_NATIVE = 48,
	OP_LOADK_S = 49,
	OP_LOAD_SCENE = 50,
	NAT_EE_USED = 51,
	NAT_EE_FREE = 52,
	NAT_EE_LIMIT = 53,
	NAT_GS_USED = 54,
	NAT_GS_FREE = 55,
	NAT_GS_LIMIT = 56,
	NAT_PACK_COST_EE = 57,
	NAT_PACK_COST_GS = 58,
	NAT_CAN_LOAD = 59,
	NAT_CAN_INSTANTIATE = 60,
	NAT_IS_LOADED = 61,
	NAT_LOADED_COUNT = 62,
	NAT_SET_CAM = 63,
	NAT_SET_DEFAULT_CAM = 64,
	NAT_GET_CAM = 65,
	NAT_GET_CAM_COUNT = 66,
	NAT_MAKE_CURRENT = 67,
	NAT_PREV_CAM = 68,
	NAT_GET_DEFAULT_CAM = 69,
	NAT_LOAD_SPRITES = 70,
	NAT_UNLOAD_SPRITES = 71,
	NAT_CAN_LOAD_SPRITES = 72,
	NAT_IS_SPRITES_LOADED = 73,
	NAT_LOAD_ANIMS = 74,
	NAT_UNLOAD_ANIMS = 75,
	NAT_IS_ANIMS_LOADED = 76,
	NAT_LOAD_HITS = 77,
	NAT_UNLOAD_HITS = 78,
	NAT_IS_HITS_LOADED = 79,
	NAT_LOAD_TILES = 80,
	NAT_MOVE_PLANAR = 81,
	NAT_FOLLOW_NODE = 82,
	NAT_HURT = 83,
	NAT_SET_HITSTOP = 84,
	NAT_SET_INVULN = 85,
	NAT_IS_INVULN = 86,
	NAT_KNOCKBACK = 87,
	NAT_SET_HP = 88,
	NAT_GET_HP = 89,
	NAT_HITBOX_ON = 90,
	NAT_HITBOX_LAYER = 91,
	NAT_GET_PRESSURE = 92,
	NAT_SET_DEADZONE = 93,
	NAT_GET_STICK = 94,
	NAT_POKE = 95,
	NAT_PEEK = 96,
	NAT_SPAWN = 97,
	NAT_OVERLAPS_ENTERED = 98,
	NAT_SET_FLIP = 99,
	NAT_MOVE_6DOF = 100,
	NAT_LOAD_PARTICLES = 101,
	NAT_UNLOAD_PARTICLES = 102,
	NAT_CAN_LOAD_PARTICLES = 103,
	NAT_IS_PARTICLES_LOADED = 104,
	NAT_PATH_FOLLOW = 105,
	NAT_SET_PATH_OFFSET = 106,
	NAT_SET_THRUST = 107,
	NAT_SET_STEER = 108,
	NAT_SET_EYE_HEIGHT = 109,
	NAT_PREFETCH_SCENE = 110,
	NAT_NAVMESH = 111,
	NAT_TWEEN_PLAY = 112,
	NAT_TWEEN_KILL = 113,
	NAT_UNLOAD_TILES = 114,
	NAT_LOAD_HUD = 115,
	NAT_UNLOAD_HUD = 116,
	NAT_CAN_LOAD_HUD = 117,
	NAT_IS_HUD_LOADED = 118,
	NAT_LOAD_MUSIC = 119,
	NAT_UNLOAD_MUSIC = 120,
	NAT_CAN_LOAD_MUSIC = 121,
	NAT_IS_MUSIC_LOADED = 122,
	NAT_PLAY_ANIM = 123,
	NAT_STOP_ANIM = 124,
	NAT_SET_CHECKPOINT = 125,
	NAT_RESPAWN = 126
};

static void emit_loadk_s(Vector<uint8_t> &o, const char *s) {
	int n = 0;
	while (s[n]) {
		n++;
	}
	if (n > 127) {
		n = 127;
	}
	o.push_back(uint8_t(OP_LOADK_S));
	o.push_back(uint8_t(n));
	for (int i = 0; i < n; i++) {
		o.push_back(uint8_t(s[i]));
	}
}

static const char *kUnsupportedCalls[] = {
	"play_xa", "stop_xa",
	"memcard_wrap", "memcard_unwrap",
	"set_fog", "spawn_shot"
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
			p_name == "unload_pack" || p_name == "load_hud" || p_name == "unload_hud" ||
			p_name == "can_load_hud" || p_name == "is_hud_loaded" || p_name == "load_tiles" ||
			p_name == "unload_tiles" || p_name == "can_load_tiles" || p_name == "is_tiles_loaded" ||
			p_name == "nav_follow" || p_name == "nav_next_node" || p_name == "set_hp" ||
			p_name == "get_hp" || p_name == "set_frame" || p_name == "play_fmv" ||
			p_name == "save_slot" || p_name == "load_slot" || p_name == "memcard_format" ||
			p_name == "load_music" || p_name == "unload_music" || p_name == "can_load_music" ||
			p_name == "is_music_loaded" || p_name == "set_music_volume" || p_name == "set_music_fade" ||
			p_name == "stop_music" || p_name == "look_camera" || p_name == "orbit_camera" ||
			p_name == "attach_camera" || p_name == "shake_camera" || p_name == "next_camera" ||
			p_name == "prev_camera" || p_name == "set_camera" || p_name == "set_camera_node" ||
			p_name == "get_camera" || p_name == "get_camera_count" || p_name == "make_current" ||
			p_name == "create_tween" || p_name == "tween_property" || p_name == "kill_tweens" ||
			p_name == "tween_is_done" || p_name == "tween_count" || p_name == "create_timer" ||
			p_name == "is_timer_done" || p_name == "move_and_slide" || p_name == "is_on_floor" ||
			p_name == "is_on_wall" || p_name == "is_on_ceiling" || p_name == "overlaps" ||
			p_name == "overlaps_entered" || p_name == "has_overlapping_areas" ||
			p_name == "hitbox_kind" || p_name == "raycast" || p_name == "raycast_point" ||
			p_name == "intersects_ray" || p_name == "tile_solid_at" || p_name == "tile_at" ||
			p_name == "set_fade" || p_name == "change_scene_fade" || p_name == "change_scene" ||
			p_name == "instantiate" || p_name == "can_instantiate" || p_name == "say_done" ||
			p_name == "set_hud_text" || p_name == "seek_animation" || p_name == "set_animation_speed" ||
			p_name == "play_animation" || p_name == "stop_animation" ||
			p_name == "get_ee_ram_used" || p_name == "get_ee_ram_free" || p_name == "get_ee_ram_limit" ||
			p_name == "get_gs_vram_used" || p_name == "get_gs_vram_free" || p_name == "get_gs_vram_limit" ||
			p_name == "get_pack_cost" || p_name == "set_default_camera" || p_name == "get_default_camera" ||
			p_name == "load_sprites" || p_name == "unload_sprites" || p_name == "can_load_sprites" ||
			p_name == "is_sprites_loaded" || p_name == "load_animations" || p_name == "unload_animations" ||
			p_name == "can_load_animations" || p_name == "is_animations_loaded" || p_name == "load_hitboxes" ||
			p_name == "unload_hitboxes" || p_name == "can_load_hitboxes" || p_name == "is_hitboxes_loaded" ||
			p_name == "spawn" || p_name == "move_planar" || p_name == "follow_node" ||
			p_name == "set_hitbox_enabled" || p_name == "set_hitbox_layer" || p_name == "get_hitbox_layer" ||
			p_name == "set_hitstop" || p_name == "set_invuln" || p_name == "is_invuln" || p_name == "knockback" ||
			p_name == "set_cell" || p_name == "erase_cell" || p_name == "set_flip" ||
			p_name == "poke" || p_name == "peek" || p_name == "godot_save_bytes" || p_name == "godot_load_bytes" ||
			p_name == "move_6dof" || p_name == "set_steer" || p_name == "set_thrust" || p_name == "set_eye_height" ||
			p_name == "load_particles" || p_name == "unload_particles" || p_name == "can_load_particles" ||
			p_name == "is_particles_loaded" || p_name == "path_follow" || p_name == "set_path_offset" ||
			p_name == "prefetch_scene" || p_name == "navmesh" || p_name == "tween_play" || p_name == "tween_kill";
}

static bool reject_unsupported_apis(const String &p_src, String &r_why) {
	for (const char *bad : kUnsupportedCalls) {
		if (p_src.contains(bad)) {
			if (String(bad) == "play_xa" || String(bad) == "stop_xa") {
				r_why = String(bad) + " is not supported on PS2 ABI 1 (no CD-XA; use PS2.play_sfx / load_music IMA)";
			} else if (String(bad) == "set_fog") {
				r_why = "set_fog is not supported on PS2 ABI 1 (use PS2.set_fade for a GS overlay)";
			} else if (String(bad) == "spawn_shot") {
				r_why = "spawn_shot is not supported on PS2 ABI 1";
			} else {
				r_why = String(bad) + " is not supported on PS2 ABI 1";
			}
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
	bool has_say = false;
	bool has_nav = false;
	bool has_slot = false;
	bool has_fmv = false;
	bool has_sys = false;
	bool has_music = false;
	bool has_cam_rt = false;
	bool has_tween = false;
	bool has_slide = false;
	bool has_hitq = false;
	bool has_fade = false;
	bool has_change = false;
	bool has_load = false;
	bool has_inst = false;
	bool has_ram = false;
	bool has_seek = false;
	bool has_play = false;
	bool has_branch = false;
	bool has_gameplay = false;
	bool has_genre = false;
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
		if (src.contains("swap_pack")) {
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
		if (src.contains("say(") || src.contains("load_hud") || src.contains("set_hud") || src.contains("say_done") || src.contains("set_hud_text")) {
			has_say = true;
		}
		if (src.contains("nav_follow") || src.contains("nav_next")) {
			has_nav = true;
		}
		if (src.contains("save_slot") || src.contains("load_slot") || src.contains("memcard_format")) {
			has_slot = true;
		}
		if (src.contains("play_fmv")) {
			has_fmv = true;
		}
		if (src.contains("load_tiles") || src.contains("set_hp") || src.contains("set_frame") || src.contains("systems_kit")) {
			has_sys = true;
		}
		if (src.contains("load_music") || src.contains("stop_music") || src.contains("set_music_volume") ||
				src.contains("set_music_fade") || src.contains("unload_music") || src.contains("can_load_music") ||
				src.contains("is_music_loaded")) {
			has_music = true;
		}
		if (src.contains("look_camera") || src.contains("orbit_camera") || src.contains("attach_camera") ||
				src.contains("shake_camera") || src.contains("next_camera") || src.contains("prev_camera") ||
				src.contains("set_camera") || src.contains("set_default_camera") || src.contains("make_current")) {
			has_cam_rt = true;
		}
		if (src.contains("create_tween") || src.contains("tween_property") || src.contains("kill_tweens") ||
				src.contains("tween_is_done") || src.contains("create_timer") || src.contains("is_timer_done")) {
			has_tween = true;
		}
		if (src.contains("move_and_slide") || src.contains("is_on_floor") || src.contains("is_on_wall") ||
				src.contains("is_on_ceiling")) {
			has_slide = true;
		}
		if (src.contains("overlaps") || src.contains("raycast") || src.contains("intersects_ray") ||
				src.contains("hitbox_kind") || src.contains("tile_solid_at") || src.contains("tile_at")) {
			has_hitq = true;
		}
		if (src.contains("set_fade") || src.contains("change_scene_fade")) {
			has_fade = true;
		}
		if (src.contains("change_scene(")) {
			has_change = true;
		}
		if (src.contains("load_scene(")) {
			has_load = true;
		}
		if (src.contains("instantiate(") || src.contains("unload_pack(")) {
			has_inst = true;
		}
		if (src.contains("get_ee_ram") || src.contains("get_gs_vram") || src.contains("get_pack_cost") ||
				src.contains("can_load_scene") || src.contains("can_instantiate") || src.contains("is_scene_loaded") ||
				src.contains("get_loaded_scene_count")) {
			has_ram = true;
		}
		if (src.contains("seek_animation") || src.contains("set_animation_speed") || src.contains("set_frame(")) {
			has_seek = true;
		}
		if (src.contains("play_animation") || src.contains("stop_animation")) {
			has_play = true;
		}
		if (src.contains("if ") || src.contains("while ") || src.contains("elif ")) {
			has_branch = true;
		}
		if (src.contains("load_sprites") || src.contains("move_planar") || src.contains("poke(") ||
				src.contains("get_pressure") || src.contains("set_hitstop") || src.contains("set_stick_deadzone") ||
				src.contains("follow_node") || src.contains("can_load_sprites")) {
			has_gameplay = true;
		}
		if (src.contains("move_6dof") || src.contains("load_particles") || src.contains("path_follow") ||
				src.contains("set_thrust") || src.contains("navmesh") || src.contains("prefetch_scene") ||
				src.contains("set_steer") || src.contains("set_eye_height") || src.contains("tween_play")) {
			has_genre = true;
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
	if (has_user || has_memcard || has_slot) {
		tape.push_back(uint8_t(OP_READY));
		tape.push_back(uint8_t(OP_MEMCARD_PRESENT));
		tape.push_back(uint8_t(OP_USER_SAVE));
		tape.push_back(uint8_t(OP_USER_LOAD));
		if (has_slot) {
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 0.0f);
			tape.push_back(uint8_t(OP_SAVE_SLOT));
		}
	}
	if (has_fmv) {
		tape.push_back(uint8_t(OP_READY));
		tape.push_back(uint8_t(OP_PLAY_FMV));
	}
	if (has_music) {
		tape.push_back(uint8_t(OP_READY));
		tape.push_back(uint8_t(OP_LOADK_F));
		append_f32(tape, 0.6f);
		tape.push_back(uint8_t(OP_MUSIC_VOL));
		emit_loadk_s(tape, "res://audio/loop");
		tape.push_back(uint8_t(OP_CALL_NATIVE));
		tape.push_back(uint8_t(NAT_LOAD_MUSIC));
		tape.push_back(uint8_t(OP_PLAY_MUSIC));
		tape.push_back(uint8_t(OP_CALL_NATIVE));
		tape.push_back(uint8_t(NAT_UNLOAD_MUSIC));
	}
	if (has_cam_rt) {
		tape.push_back(uint8_t(OP_READY));
		tape.push_back(uint8_t(OP_LOADK_F));
		append_f32(tape, 0.0f);
		tape.push_back(uint8_t(OP_LOADK_F));
		append_f32(tape, 0.0f);
		tape.push_back(uint8_t(OP_LOADK_F));
		append_f32(tape, 0.0f);
		tape.push_back(uint8_t(OP_ATTACH_CAMERA));
		tape.push_back(uint8_t(OP_LOADK_F));
		append_f32(tape, 0.0f);
		tape.push_back(uint8_t(OP_LOADK_F));
		append_f32(tape, 0.0f);
		tape.push_back(uint8_t(OP_SHAKE_CAMERA));
	}
	if (has_tween) {
		tape.push_back(uint8_t(OP_READY));
		tape.push_back(uint8_t(OP_LOADK_F));
		append_f32(tape, 0.6f);
		tape.push_back(uint8_t(OP_LOADK_F));
		append_f32(tape, 0.6f);
		tape.push_back(uint8_t(OP_LOADK_F));
		append_f32(tape, 0.0f);
		tape.push_back(uint8_t(OP_LOADK_F));
		append_f32(tape, 1.0f);
		tape.push_back(uint8_t(OP_TWEEN));
		tape.push_back(uint8_t(OP_LOADK_F));
		append_f32(tape, 0.0f);
		tape.push_back(uint8_t(OP_TIMER));
	}
	if (has_fade) {
		tape.push_back(uint8_t(OP_READY));
		tape.push_back(uint8_t(OP_LOADK_F));
		append_f32(tape, 0.0f);
		tape.push_back(uint8_t(OP_LOADK_F));
		append_f32(tape, 0.0f);
		tape.push_back(uint8_t(OP_LOADK_F));
		append_f32(tape, 0.0f);
		tape.push_back(uint8_t(OP_LOADK_F));
		append_f32(tape, 0.0f);
		tape.push_back(uint8_t(OP_SET_FADE));
		tape.push_back(uint8_t(OP_LOADK_F));
		append_f32(tape, 0.0f);
		tape.push_back(uint8_t(OP_SCENE_FADE));
	}
	if (has_change) {
		tape.push_back(uint8_t(OP_READY));
		tape.push_back(uint8_t(OP_LOADK_S));
		const char *flow = "res://scenes/genres/adventure.tscn";
		const int n = 32;
		tape.push_back(uint8_t(n));
		for (int c = 0; c < n; c++) {
			tape.push_back(uint8_t(flow[c]));
		}
		tape.push_back(uint8_t(OP_CHANGE_SCENE));
	}
	if (has_load) {
		tape.push_back(uint8_t(OP_READY));
		tape.push_back(uint8_t(OP_LOADK_S));
		const char *kit = "res://scenes/spawn_kit.tscn";
		const int n = 26;
		tape.push_back(uint8_t(n));
		for (int c = 0; c < n; c++) {
			tape.push_back(uint8_t(kit[c]));
		}
		tape.push_back(uint8_t(OP_LOAD_SCENE));
	}
	if (has_inst) {
		tape.push_back(uint8_t(OP_READY));
		tape.push_back(uint8_t(OP_LOADK_S));
		const char *kit = "res://scenes/spawn_kit.tscn";
		const int n = 26;
		tape.push_back(uint8_t(n));
		for (int c = 0; c < n; c++) {
			tape.push_back(uint8_t(kit[c]));
		}
		tape.push_back(uint8_t(OP_INSTANTIATE));
		tape.push_back(uint8_t(OP_UNLOAD_PACK));
	}
	if (has_ram) {
		tape.push_back(uint8_t(OP_READY));
		tape.push_back(uint8_t(OP_CALL_NATIVE));
		tape.push_back(uint8_t(NAT_EE_USED));
		tape.push_back(uint8_t(OP_CALL_NATIVE));
		tape.push_back(uint8_t(NAT_GS_USED));
		tape.push_back(uint8_t(OP_LOADK_S));
		const char *kit = "res://scenes/spawn_kit.tscn";
		const int n = 26;
		tape.push_back(uint8_t(n));
		for (int c = 0; c < n; c++) {
			tape.push_back(uint8_t(kit[c]));
		}
		tape.push_back(uint8_t(OP_CALL_NATIVE));
		tape.push_back(uint8_t(NAT_PACK_COST_EE));
		tape.push_back(uint8_t(OP_CALL_NATIVE));
		tape.push_back(uint8_t(NAT_CAN_INSTANTIATE));
		tape.push_back(uint8_t(OP_CALL_NATIVE));
		tape.push_back(uint8_t(NAT_CAN_LOAD));
	}
	if (has_kit) {
		tape.push_back(uint8_t(OP_READY));
		tape.push_back(uint8_t(OP_CALL_NATIVE));
		tape.push_back(uint8_t(NAT_SET_CHECKPOINT));
		tape.push_back(uint8_t(OP_CALL_NATIVE));
		tape.push_back(uint8_t(NAT_RESPAWN));
	}
	if (has_gd_rotate || has_lu_rotate || has_input || has_translate || has_kit || has_say || has_nav || has_sys || has_cam_rt || has_tween || has_slide || has_hitq || has_fade || has_change || has_load || has_inst || has_ram || has_seek || has_play || has_branch || has_gameplay || has_genre) {
		tape.push_back(uint8_t(OP_PROCESS));
		if (has_gd_rotate || has_lu_rotate) {
			tape.push_back(uint8_t(OP_LOAD_DELTA));
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, rotate_rate);
			tape.push_back(uint8_t(OP_MUL));
			tape.push_back(uint8_t(OP_CALL_ROTATE_Y));
		}
		if (has_input || has_translate || has_slide) {
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, translate_rate);
			tape.push_back(uint8_t(OP_INPUT_TRANSLATE));
		}
		if (has_slide) {
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 0.0f);
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 0.0f);
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 0.0f);
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 255.0f);
			tape.push_back(uint8_t(OP_SLIDE));
		}
		if (has_hitq) {
			tape.push_back(uint8_t(OP_OVERLAP));
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 0.0f);
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 0.0f);
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 1.0f);
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 8.0f);
			tape.push_back(uint8_t(OP_RAYCAST));
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 0.0f);
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 1.0f);
			tape.push_back(uint8_t(OP_TILE_AT));
		}
		if (has_input) {
			tape.push_back(uint8_t(OP_JUST_ACCEPT_SFX));
		}
		if (has_kit) {
			tape.push_back(uint8_t(OP_KIT_TICK));
		}
		if (has_say) {
			tape.push_back(uint8_t(OP_SAY));
			tape.push_back(uint8_t(OP_SAY_TEXT));
			emit_loadk_s(tape, "res://scenes/systems_kit.tscn");
			tape.push_back(uint8_t(OP_CALL_NATIVE));
			tape.push_back(uint8_t(NAT_LOAD_HUD));
		}
		if (has_seek) {
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 0.0f);
			tape.push_back(uint8_t(OP_SEEK_ANIM));
		}
		if (has_play) {
			emit_loadk_s(tape, "idle");
			tape.push_back(uint8_t(OP_CALL_NATIVE));
			tape.push_back(uint8_t(NAT_PLAY_ANIM));
			tape.push_back(uint8_t(OP_CALL_NATIVE));
			tape.push_back(uint8_t(NAT_STOP_ANIM));
		}
		if (has_branch) {
			tape.push_back(uint8_t(OP_HAS_FEATURE_PS2));
			tape.push_back(uint8_t(OP_JZ));
			append_u16(tape, 2);
			tape.push_back(uint8_t(OP_CALL_NATIVE));
			tape.push_back(uint8_t(OP_HAS_FEATURE_PS2));
			tape.push_back(uint8_t(OP_JMP));
			append_u16(tape, 0);
		}
		if (has_nav) {
			tape.push_back(uint8_t(OP_NAV_FOLLOW));
		}
		if (has_say || has_nav || has_sys || has_cam_rt || has_tween || has_fade) {
			tape.push_back(uint8_t(OP_SYS_TICK));
		}
		if (has_cam_rt) {
			tape.push_back(uint8_t(OP_LOOK_STICK));
			tape.push_back(uint8_t(OP_NEXT_CAMERA));
			tape.push_back(uint8_t(OP_CALL_NATIVE));
			tape.push_back(uint8_t(NAT_SET_CAM));
			tape.push_back(uint8_t(OP_CALL_NATIVE));
			tape.push_back(uint8_t(NAT_GET_CAM));
			tape.push_back(uint8_t(OP_CALL_NATIVE));
			tape.push_back(uint8_t(NAT_GET_CAM_COUNT));
			tape.push_back(uint8_t(OP_CALL_NATIVE));
			tape.push_back(uint8_t(NAT_GET_DEFAULT_CAM));
		}
		if (has_gameplay) {
			tape.push_back(uint8_t(OP_LOADK_S));
			const char *kit = "res://scenes/gameplay_kit.tscn";
			const int n = 31;
			tape.push_back(uint8_t(n));
			for (int c = 0; c < n; c++) {
				tape.push_back(uint8_t(kit[c]));
			}
			tape.push_back(uint8_t(OP_CALL_NATIVE));
			tape.push_back(uint8_t(NAT_LOAD_SPRITES));
			tape.push_back(uint8_t(OP_CALL_NATIVE));
			tape.push_back(uint8_t(NAT_CAN_LOAD_SPRITES));
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 0.0f);
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 0.0f);
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 1.0f);
			tape.push_back(uint8_t(OP_CALL_NATIVE));
			tape.push_back(uint8_t(NAT_MOVE_PLANAR));
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 1.0f);
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 0.0f);
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 0.0f);
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 0.0f);
			tape.push_back(uint8_t(OP_CALL_NATIVE));
			tape.push_back(uint8_t(NAT_HURT));
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 0.0f);
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 1.0f);
			tape.push_back(uint8_t(OP_CALL_NATIVE));
			tape.push_back(uint8_t(NAT_POKE));
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 0.0f);
			tape.push_back(uint8_t(OP_CALL_NATIVE));
			tape.push_back(uint8_t(NAT_GET_PRESSURE));
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 0.2f);
			tape.push_back(uint8_t(OP_CALL_NATIVE));
			tape.push_back(uint8_t(NAT_SET_DEADZONE));
			tape.push_back(uint8_t(OP_CALL_NATIVE));
			tape.push_back(uint8_t(NAT_PEEK));
		}
		if (has_genre) {
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 0.0f);
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 0.0f);
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 0.0f);
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 0.0f);
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 0.0f);
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 0.0f);
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 1.0f);
			tape.push_back(uint8_t(OP_CALL_NATIVE));
			tape.push_back(uint8_t(NAT_MOVE_6DOF));
			tape.push_back(uint8_t(OP_CALL_NATIVE));
			tape.push_back(uint8_t(NAT_LOAD_PARTICLES));
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 1.0f);
			tape.push_back(uint8_t(OP_CALL_NATIVE));
			tape.push_back(uint8_t(NAT_PATH_FOLLOW));
			tape.push_back(uint8_t(OP_LOADK_F));
			append_f32(tape, 1.0f);
			tape.push_back(uint8_t(OP_CALL_NATIVE));
			tape.push_back(uint8_t(NAT_SET_THRUST));
			tape.push_back(uint8_t(OP_CALL_NATIVE));
			tape.push_back(uint8_t(NAT_NAVMESH));
			tape.push_back(uint8_t(OP_CALL_NATIVE));
			tape.push_back(uint8_t(NAT_PREFETCH_SCENE));
		}
	}
	tape.push_back(uint8_t(OP_END));
	if (tape.size() > 1536) {
		r_out.rejected.push_back("script too large for PS2 ABI 1 (tape exceeds 1.5 KiB)");
		return ERR_UNCONFIGURED;
	}

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
		r_out.ir_text += "IR PS2.swap_pack\n";
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
		r_out.ir_text += "IR kit tick on HIT enter (overlap kit, not Cross-only)\n";
		r_out.ir_text += "IR checkpoint xyz / respawn\n";
		r_out.ir_text += vformat("IR STREAM extra_scenes=%d\n", r_out.extra_scenes.size());
	}
	if (has_say) {
		r_out.ir_text += "IR PS2.say / load_hud\n";
	}
	if (has_nav) {
		r_out.ir_text += "IR PS2.nav_follow\n";
	}
	if (has_slot) {
		r_out.ir_text += "IR PS2.save_slot/load_slot\n";
	}
	if (has_fmv) {
		r_out.ir_text += "IR PS2.play_fmv (IPU not shipped)\n";
	}
	if (has_sys) {
		r_out.ir_text += "IR HUD/TILE/NAV/CAM/HIT/ANIM sys tick\n";
	}
	if (has_music) {
		r_out.ir_text += "IR PS2.load_music/unload_music/set_music_volume (voice 1 loop)\n";
	}
	if (has_cam_rt) {
		r_out.ir_text += "IR PS2.set_camera/set_default_camera/make_current\n";
		r_out.ir_text += "IR PS2.look_camera/orbit_camera/shake_camera (runtime CAM)\n";
	}
	if (has_gameplay) {
		r_out.ir_text += "IR PS2.load_sprites/can_load_sprites\n";
		r_out.ir_text += "IR PS2.move_planar/follow_node\n";
		r_out.ir_text += "IR PS2.hurt/set_hitstop/set_invuln/knockback\n";
		r_out.ir_text += "IR PS2.poke/peek (24KiB)\n";
		r_out.ir_text += "IR PS2.get_pressure/set_stick_deadzone\n";
	}
	if (has_genre) {
		r_out.ir_text += "IR PS2.move_6dof\n";
		r_out.ir_text += "IR PS2.load_particles\n";
		r_out.ir_text += "IR PS2.path_follow\n";
		r_out.ir_text += "IR PS2.set_thrust\n";
		r_out.ir_text += "IR PS2.navmesh\n";
	}
	if (has_tween) {
		r_out.ir_text += "IR PS2.create_tween/create_timer (16 linear)\n";
	}
	if (has_slide) {
		r_out.ir_text += "IR PS2.move_and_slide (HIT00 AABB enter-only)\n";
	}
	if (has_hitq) {
		r_out.ir_text += "IR PS2.overlaps/raycast/tile_solid_at (HIT00 AABB + TILE cells)\n";
	}
	if (has_fade) {
		r_out.ir_text += "IR PS2.set_fade/change_scene_fade (GS alpha overlay)\n";
	}
	if (has_change) {
		r_out.ir_text += "IR PS2.change_scene (STREAM swap)\n";
	}
	if (has_load) {
		r_out.ir_text += "IR PS2.load_scene (additive resident, not swap)\n";
	}
	if (has_inst) {
		r_out.ir_text += "IR PS2.instantiate/unload_pack (pack handle, 16 resident extras, RAM-gated)\n";
	}
	if (has_ram) {
		r_out.ir_text += "IR PS2.get_ee_ram/get_gs_vram/get_pack_cost/can_instantiate/can_load_scene\n";
	}
	if (has_seek) {
		r_out.ir_text += "IR PS2.seek_animation/set_frame (Node3D TRS keys)\n";
	}
	if (has_play) {
		r_out.ir_text += "IR PS2.play_animation/stop_animation\n";
	}
	if (has_branch || has_change || has_inst) {
		r_out.ir_text += "IR JMP/CALL compiled subset (not official GDScript/Luau VM)\n";
	}
	if (has_slide) {
		r_out.warnings.push_back("move_and_slide clips against HIT00 mesh AABBs (enter-only so oversized industrial boxes do not trap).");
	}
	if (has_hitq) {
		r_out.warnings.push_back("overlaps / raycast query HIT00 AABBs (including already-overlapping boxes). tile_solid_at uses TILE00 floor(x),floor(z) cells.");
	}
	if (has_fade) {
		r_out.warnings.push_back("set_fade is a GS blended fullscreen rect (not set_fog). change_scene_fade lerps that overlay to clear.");
	}
	if (has_tween) {
		r_out.warnings.push_back("create_tween / create_timer are 16 linear slots on PS2 ABI 1 (volume tweens drive MUSIC). Not SceneTreeTween.");
	}
	if (has_user) {
		r_out.warnings.push_back("FileAccess user:// maps to host:SAVE.BIN then mc0:SAVE.BIN. Missing HostFs/memcard is a guest error.");
	}
	r_out.ir_text += "kit: Spawn Checkpoint Portal Player Hazard Pickup Music Unload Load Talk Spawner Save\n";
	r_out.ir_text += "IR kit tick on HIT enter (overlap kit, not Cross-only)\n";
	r_out.ir_text += "IR checkpoint xyz / respawn\n";
	r_out.ir_text += "IR spawn_ofs gravity jump pause action|\n";
	for (int i = 0; i < r_out.accepted.size(); i++) {
		r_out.ir_text += r_out.accepted[i] + "\n";
	}
	(void)p_export_dir;
	return OK;
}

} // namespace ps2_cook
