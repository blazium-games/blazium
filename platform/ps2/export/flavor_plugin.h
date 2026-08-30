/**************************************************************************/
/*  flavor_plugin.h                                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#pragma once

#include "core/input/shortcut.h"
#include "core/math/color.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/object/object.h"
#include "core/string/node_path.h"
#include "core/string/ustring.h"
#include "core/object/object_id.h"
#include "core/io/marshalls.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/3d/node_3d.h"
#include "scene/gui/box_container.h"
#include "scene/gui/label.h"

class PS2 : public Object {
	GDCLASS(PS2, Object);

	static void _fail_ps2() {
		ERR_PRINT("PS2.* only works on a PS2 export. Guard with OS.has_feature(\"ps2\").");
	}

protected:
	static void _bind_methods();

public:
	int get_abi() const {
		_fail_ps2();
		return 0;
	}
	bool is_ready() const {
		_fail_ps2();
		return false;
	}
	bool set_rumble(int p_device, int p_small, int p_large) {
		_fail_ps2();
		return false;
	}
	bool stop_rumble(int p_device) {
		_fail_ps2();
		return false;
	}
	int get_pressure(int p_device, int p_button) const {
		_fail_ps2();
		return 0;
	}
	bool play_sfx(const String &p_name) {
		_fail_ps2();
		(void)p_name;
		return false;
	}
	bool stop_sfx() {
		_fail_ps2();
		return false;
	}
	bool swap_pack(const String &p_unload, const String &p_load) {
		_fail_ps2();
		(void)p_unload;
		(void)p_load;
		return false;
	}
	bool load_scene(const String &p_path) {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool unload_scene(const String &p_path) {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	int get_display_width() const {
		_fail_ps2();
		return 0;
	}
	int get_display_height() const {
		_fail_ps2();
		return 0;
	}
	bool memcard_present() const {
		_fail_ps2();
		return false;
	}
	bool memcard_ready() const {
		_fail_ps2();
		return false;
	}
	bool memcard_save(const String &p_name, const PackedByteArray &p_data) {
		_fail_ps2();
		(void)p_name;
		(void)p_data;
		return false;
	}
	PackedByteArray memcard_load(const String &p_name) {
		_fail_ps2();
		(void)p_name;
		return PackedByteArray();
	}
	Vector2 get_stick(int p_pad, int p_stick) const {
		_fail_ps2();
		(void)p_pad;
		(void)p_stick;
		return Vector2();
	}
	bool set_stick_deadzone(float p_zone) {
		_fail_ps2();
		(void)p_zone;
		return false;
	}
	bool set_checkpoint(const Variant &p_node) {
		_fail_ps2();
		(void)p_node;
		return false;
	}
	bool respawn(const Variant &p_node) {
		_fail_ps2();
		(void)p_node;
		return false;
	}
	bool portal(const String &p_unload, const String &p_load, float p_fade) {
		_fail_ps2();
		(void)p_unload;
		(void)p_load;
		(void)p_fade;
		return false;
	}
	int hurt(const Variant &p_node, int p_amount, float p_vx, float p_vy, float p_vz) {
		_fail_ps2();
		(void)p_node;
		(void)p_amount;
		(void)p_vx;
		(void)p_vy;
		(void)p_vz;
		return 0;
	}
	bool say(const Variant &p_hud, const String &p_line, float p_cps) {
		_fail_ps2();
		(void)p_hud;
		(void)p_line;
		(void)p_cps;
		return false;
	}
	bool can_load_scene(const String &p_path) const {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool is_scene_loaded(const String &p_path) const {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	int get_loaded_scene_count() const {
		_fail_ps2();
		return 0;
	}
	bool unload_pack(const String &p_path) {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool load_hud(const String &p_path) {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool unload_hud(const String &p_path) {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool can_load_hud(const String &p_path) const {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool is_hud_loaded(const String &p_path) const {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool load_tiles(const String &p_path) {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool unload_tiles(const String &p_path) {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool can_load_tiles(const String &p_path) const {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool is_tiles_loaded(const String &p_path) const {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool nav_follow(const Variant &p_node, const Variant &p_goal, float p_speed) {
		_fail_ps2();
		(void)p_node;
		(void)p_goal;
		(void)p_speed;
		return false;
	}
	int nav_next_node(int p_from, int p_to) const {
		_fail_ps2();
		(void)p_from;
		(void)p_to;
		return -1;
	}
	int set_hp(const Variant &p_node, int p_n) {
		_fail_ps2();
		(void)p_node;
		(void)p_n;
		return 0;
	}
	int get_hp(const Variant &p_node) const {
		_fail_ps2();
		(void)p_node;
		return 0;
	}
	bool set_frame(const Variant &p_node, int p_i) {
		_fail_ps2();
		(void)p_node;
		(void)p_i;
		return false;
	}
	bool play_fmv(const String &p_path) {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool save_slot(int p_slot) {
		_fail_ps2();
		(void)p_slot;
		return false;
	}
	PackedByteArray load_slot(int p_slot) {
		_fail_ps2();
		(void)p_slot;
		return PackedByteArray();
	}
	bool memcard_format() {
		_fail_ps2();
		return false;
	}
	bool load_music(const String &p_path) {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool unload_music(const String &p_path) {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool can_load_music(const String &p_path) const {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool is_music_loaded(const String &p_path) const {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool set_music_volume(float p_vol) {
		_fail_ps2();
		(void)p_vol;
		return false;
	}
	bool set_music_fade(float p_to_vol, float p_ms) {
		_fail_ps2();
		(void)p_to_vol;
		(void)p_ms;
		return false;
	}
	bool stop_music() {
		_fail_ps2();
		return false;
	}
	bool look_camera(float p_yaw, float p_pitch, float p_roll = 0.0f) {
		_fail_ps2();
		(void)p_yaw;
		(void)p_pitch;
		(void)p_roll;
		return false;
	}
	bool orbit_camera(const Variant &p_target, float p_yaw, float p_pitch, float p_dist, int p_clip = 0) {
		_fail_ps2();
		(void)p_target;
		(void)p_yaw;
		(void)p_pitch;
		(void)p_dist;
		(void)p_clip;
		return false;
	}
	bool attach_camera(const Variant &p_node, const Vector3 &p_offset = Vector3()) {
		_fail_ps2();
		(void)p_node;
		(void)p_offset;
		return false;
	}
	bool shake_camera(float p_amp, float p_ms) {
		_fail_ps2();
		(void)p_amp;
		(void)p_ms;
		return false;
	}
	bool next_camera() {
		_fail_ps2();
		return false;
	}
	bool prev_camera() {
		_fail_ps2();
		return false;
	}
	bool set_camera(int p_index) {
		_fail_ps2();
		(void)p_index;
		return false;
	}
	bool set_camera_node(const String &p_name) {
		_fail_ps2();
		(void)p_name;
		return false;
	}
	int get_camera() const {
		_fail_ps2();
		return 0;
	}
	int get_camera_count() const {
		_fail_ps2();
		return 0;
	}
	bool make_current(const Variant &p_node) {
		_fail_ps2();
		(void)p_node;
		return false;
	}
	Variant create_tween() {
		_fail_ps2();
		return Variant();
	}
	bool tween_property(const Variant &p_tween, const Variant &p_node, const String &p_prop, const Variant &p_to, float p_sec) {
		_fail_ps2();
		(void)p_tween;
		(void)p_node;
		(void)p_prop;
		(void)p_to;
		(void)p_sec;
		return false;
	}
	bool kill_tweens(const Variant &p_node) {
		_fail_ps2();
		(void)p_node;
		return false;
	}
	bool tween_is_done(const Variant &p_tw) const {
		_fail_ps2();
		(void)p_tw;
		return true;
	}
	int tween_count() const {
		_fail_ps2();
		return 0;
	}
	Variant create_timer(float p_sec) {
		_fail_ps2();
		(void)p_sec;
		return Variant();
	}
	bool is_timer_done(const Variant &p_timer) const {
		_fail_ps2();
		(void)p_timer;
		return true;
	}
	Vector3 move_and_slide(const Variant &p_node, const Vector3 &p_velocity, int p_mask = 255) {
		_fail_ps2();
		(void)p_node;
		(void)p_velocity;
		(void)p_mask;
		return Vector3();
	}
	bool is_on_floor() const {
		_fail_ps2();
		return false;
	}
	bool is_on_wall() const {
		_fail_ps2();
		return false;
	}
	bool is_on_ceiling() const {
		_fail_ps2();
		return false;
	}
	bool overlaps(const Variant &p_a, const Variant &p_b) const {
		_fail_ps2();
		(void)p_a;
		(void)p_b;
		return false;
	}
	bool overlaps_entered(const Variant &p_a, const Variant &p_b) const {
		_fail_ps2();
		(void)p_a;
		(void)p_b;
		return false;
	}
	bool has_overlapping_areas() const {
		_fail_ps2();
		return false;
	}
	int hitbox_kind(const Variant &p_node) const {
		_fail_ps2();
		(void)p_node;
		return 0;
	}
	Variant raycast(const Vector3 &p_from, const Vector3 &p_dir, float p_dist, int p_mask = 255) const {
		_fail_ps2();
		(void)p_from;
		(void)p_dir;
		(void)p_dist;
		(void)p_mask;
		return Variant();
	}
	Variant raycast_point(const Vector3 &p_from, const Vector3 &p_dir, float p_dist, int p_mask = 255) const {
		_fail_ps2();
		(void)p_from;
		(void)p_dir;
		(void)p_dist;
		(void)p_mask;
		return Variant();
	}
	bool intersects_ray(const Vector3 &p_from, const Vector3 &p_dir) const {
		_fail_ps2();
		(void)p_from;
		(void)p_dir;
		return false;
	}
	bool tile_solid_at(float p_x, float p_y) const {
		_fail_ps2();
		(void)p_x;
		(void)p_y;
		return false;
	}
	int tile_at(float p_x, float p_y) const {
		_fail_ps2();
		(void)p_x;
		(void)p_y;
		return 0;
	}
	bool set_fade(float p_alpha, const Color &p_color = Color()) {
		_fail_ps2();
		(void)p_alpha;
		(void)p_color;
		return false;
	}
	bool change_scene_fade(const String &p_path, float p_seconds) {
		_fail_ps2();
		(void)p_path;
		(void)p_seconds;
		return false;
	}
	bool change_scene(const String &p_path) {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	Variant instantiate(const Variant &p_path_or_pack) {
		_fail_ps2();
		(void)p_path_or_pack;
		return Variant();
	}
	bool can_instantiate(const Variant &p_path_or_pack) const {
		_fail_ps2();
		(void)p_path_or_pack;
		return false;
	}
	bool say_done() const {
		_fail_ps2();
		return true;
	}
	bool set_hud_text(const Variant &p_hud, const String &p_text) {
		_fail_ps2();
		(void)p_hud;
		(void)p_text;
		return false;
	}
	bool seek_animation(float p_sec) {
		_fail_ps2();
		(void)p_sec;
		return false;
	}
	bool set_animation_speed(float p_s) {
		_fail_ps2();
		(void)p_s;
		return false;
	}
	int get_ee_ram_used() const {
		_fail_ps2();
		return 0;
	}
	int get_ee_ram_free() const {
		_fail_ps2();
		return 0;
	}
	int get_ee_ram_limit() const {
		_fail_ps2();
		return 24 * 1024 * 1024;
	}
	int get_gs_vram_used() const {
		_fail_ps2();
		return 0;
	}
	int get_gs_vram_free() const {
		_fail_ps2();
		return 0;
	}
	int get_gs_vram_limit() const {
		_fail_ps2();
		return 4 * 1024 * 1024;
	}
	Dictionary get_pack_cost(const String &p_path) const {
		_fail_ps2();
		(void)p_path;
		Dictionary d;
		d["ee"] = 0;
		d["gs"] = 0;
		return d;
	}
	bool set_default_camera(const Variant &p_index_or_name) {
		_fail_ps2();
		(void)p_index_or_name;
		return false;
	}
	int get_default_camera() const {
		_fail_ps2();
		return 0;
	}
	bool load_sprites(const String &p_path) {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool unload_sprites(const String &p_path = String()) {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool can_load_sprites(const String &p_path) const {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool is_sprites_loaded(const String &p_path = String()) const {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool load_animations(const String &p_path) {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool unload_animations(const String &p_path = String()) {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool can_load_animations(const String &p_path) const {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool is_animations_loaded(const String &p_path = String()) const {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool load_hitboxes(const String &p_path) {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool unload_hitboxes(const String &p_path = String()) {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool can_load_hitboxes(const String &p_path) const {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool is_hitboxes_loaded(const String &p_path = String()) const {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	Variant spawn(const String &p_path, const Vector3 &p_pos = Vector3()) {
		_fail_ps2();
		(void)p_path;
		(void)p_pos;
		return Variant();
	}
	Vector3 move_planar(const Variant &p_node, float p_ax, float p_ay, float p_speed) {
		_fail_ps2();
		(void)p_node;
		(void)p_ax;
		(void)p_ay;
		(void)p_speed;
		return Vector3();
	}
	bool follow_node(const Variant &p_node, const Variant &p_target, float p_speed) {
		_fail_ps2();
		(void)p_node;
		(void)p_target;
		(void)p_speed;
		return false;
	}
	bool set_hitbox_enabled(const Variant &p_node, bool p_on) {
		_fail_ps2();
		(void)p_node;
		(void)p_on;
		return false;
	}
	bool set_hitbox_layer(const Variant &p_node, int p_layer) {
		_fail_ps2();
		(void)p_node;
		(void)p_layer;
		return false;
	}
	int get_hitbox_layer(const Variant &p_node) const {
		_fail_ps2();
		(void)p_node;
		return 0;
	}
	bool set_hitstop(float p_ms) {
		_fail_ps2();
		(void)p_ms;
		return false;
	}
	bool set_invuln(const Variant &p_node, float p_ms) {
		_fail_ps2();
		(void)p_node;
		(void)p_ms;
		return false;
	}
	bool is_invuln(const Variant &p_node) const {
		_fail_ps2();
		(void)p_node;
		return false;
	}
	bool knockback(const Variant &p_node, float p_vx, float p_vy, float p_vz) {
		_fail_ps2();
		(void)p_node;
		(void)p_vx;
		(void)p_vy;
		(void)p_vz;
		return false;
	}
	bool set_cell(int p_x, int p_y, int p_flags = 1) {
		_fail_ps2();
		(void)p_x;
		(void)p_y;
		(void)p_flags;
		return false;
	}
	bool erase_cell(int p_x, int p_y) {
		_fail_ps2();
		(void)p_x;
		(void)p_y;
		return false;
	}
	bool set_flip(const Variant &p_node, bool p_flip) {
		_fail_ps2();
		(void)p_node;
		(void)p_flip;
		return false;
	}
	bool poke(int p_off, int p_byte) {
		_fail_ps2();
		(void)p_off;
		(void)p_byte;
		return false;
	}
	int peek(int p_off) const {
		_fail_ps2();
		(void)p_off;
		return 0;
	}
	PackedByteArray godot_save_bytes(const Variant &p_v) const {
		int len = 0;
		encode_variant(p_v, nullptr, len, false);
		PackedByteArray out;
		out.resize(len);
		if (len > 0) {
			encode_variant(p_v, out.ptrw(), len, false);
		}
		return out;
	}
	Variant godot_load_bytes(const PackedByteArray &p_b) const {
		Variant v;
		if (!p_b.is_empty()) {
			decode_variant(v, p_b.ptr(), p_b.size(), nullptr, false);
		}
		return v;
	}
	Vector3 move_6dof(const Variant &p_node, float p_ax, float p_ay, float p_az, float p_pitch, float p_yaw, float p_roll, float p_speed) {
		_fail_ps2();
		(void)p_node;
		(void)p_ax;
		(void)p_ay;
		(void)p_az;
		(void)p_pitch;
		(void)p_yaw;
		(void)p_roll;
		(void)p_speed;
		return Vector3();
	}
	bool set_steer(const Variant &p_node, float p_steer) {
		_fail_ps2();
		(void)p_node;
		(void)p_steer;
		return false;
	}
	bool set_thrust(const Variant &p_node, float p_thrust) {
		_fail_ps2();
		(void)p_node;
		(void)p_thrust;
		return false;
	}
	bool set_eye_height(float p_h) {
		_fail_ps2();
		(void)p_h;
		return false;
	}
	bool load_particles(const String &p_path) {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool unload_particles(const String &p_path = String()) {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool can_load_particles(const String &p_path) const {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool is_particles_loaded(const String &p_path = String()) const {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool path_follow(const Variant &p_node, float p_speed) {
		_fail_ps2();
		(void)p_node;
		(void)p_speed;
		return false;
	}
	bool set_path_offset(const Variant &p_node, float p_t) {
		_fail_ps2();
		(void)p_node;
		(void)p_t;
		return false;
	}
	bool prefetch_scene(const String &p_path) const {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	int navmesh(const Variant &p_node, const Variant &p_goal) {
		_fail_ps2();
		(void)p_node;
		(void)p_goal;
		return -1;
	}
	bool tween_play(const Variant &p_tween) {
		_fail_ps2();
		(void)p_tween;
		return false;
	}
	bool tween_kill(const Variant &p_tween) {
		_fail_ps2();
		(void)p_tween;
		return false;
	}
};

class PS2Spawner : public Node3D {
	GDCLASS(PS2Spawner, Node3D);
	String scene_path;

protected:
	static void _bind_methods();

public:
	void set_scene(const String &p_path) { scene_path = p_path; }
	String get_scene() const { return scene_path; }
	Variant spawn() {
		ERR_PRINT("PS2Spawner.spawn only works on a PS2 export.");
		return Variant();
	}
};

class PS2CameraVolume : public Node3D {
	GDCLASS(PS2CameraVolume, Node3D);
	int camera_index = 0;

protected:
	static void _bind_methods();

public:
	void set_camera_index(int p_i) { camera_index = p_i; }
	int get_camera_index() const { return camera_index; }
	bool make_current() {
		ERR_PRINT("PS2CameraVolume.make_current only works on a PS2 export.");
		return false;
	}
};

class PS2Follow : public Node3D {
	GDCLASS(PS2Follow, Node3D);
	NodePath target;
	float speed = 4.0f;

protected:
	static void _bind_methods();

public:
	void set_target_path(const NodePath &p_p) { target = p_p; }
	NodePath get_target_path() const { return target; }
	void set_speed(float p_s) { speed = p_s; }
	float get_speed() const { return speed; }
	bool follow() {
		ERR_PRINT("PS2Follow.follow only works on a PS2 export.");
		return false;
	}
};

class PS2Waypoint : public Node3D {
	GDCLASS(PS2Waypoint, Node3D);

protected:
	static void _bind_methods();
};

class PS2Path : public Node3D {
	GDCLASS(PS2Path, Node3D);
	float offset = 0.0f;

protected:
	static void _bind_methods();

public:
	void set_path_offset(float p_t) { offset = p_t; }
	float get_path_offset() const { return offset; }
	bool path_follow(float p_speed) {
		ERR_PRINT("PS2Path.path_follow only works on a PS2 export.");
		(void)p_speed;
		return false;
	}
};

class PS2NavRegion : public Node3D {
	GDCLASS(PS2NavRegion, Node3D);

protected:
	static void _bind_methods();

public:
	bool nav_follow(const Variant &p_goal, float p_speed) {
		ERR_PRINT("PS2NavRegion.nav_follow only works on a PS2 export.");
		(void)p_goal;
		(void)p_speed;
		return false;
	}
	int nav_next() {
		ERR_PRINT("PS2NavRegion.nav_next only works on a PS2 export.");
		return -1;
	}
};

class PS2Vehicle : public Node3D {
	GDCLASS(PS2Vehicle, Node3D);
	int mode = 0;
	float mass = 1.0f;
	float thrust = 8.0f;
	float steer = 0.0f;

protected:
	static void _bind_methods();

public:
	void set_mode(int p_m) { mode = p_m; }
	int get_mode() const { return mode; }
	void set_mass(float p_m) { mass = p_m; }
	float get_mass() const { return mass; }
	void set_thrust(float p_t) { thrust = p_t; }
	float get_thrust() const { return thrust; }
	void set_steer(float p_s) { steer = p_s; }
	float get_steer() const { return steer; }
	Vector3 move_6dof(float p_ax, float p_ay, float p_az, float p_pitch, float p_yaw, float p_roll, float p_speed) {
		ERR_PRINT("PS2Vehicle.move_6dof only works on a PS2 export.");
		(void)p_ax;
		(void)p_ay;
		(void)p_az;
		(void)p_pitch;
		(void)p_yaw;
		(void)p_roll;
		(void)p_speed;
		return Vector3();
	}
};

class PS2Particles : public Node3D {
	GDCLASS(PS2Particles, Node3D);
	int mode = 0;
	int amount = 16;
	float lifetime = 1.0f;

protected:
	static void _bind_methods();

public:
	void set_mode(int p_m) { mode = p_m; }
	int get_mode() const { return mode; }
	void set_amount(int p_n) { amount = p_n; }
	int get_amount() const { return amount; }
	void set_lifetime(float p_t) { lifetime = p_t; }
	float get_lifetime() const { return lifetime; }
	bool load_particles() {
		ERR_PRINT("PS2Particles.load_particles only works on a PS2 export.");
		return false;
	}
	bool unload_particles() {
		ERR_PRINT("PS2Particles.unload_particles only works on a PS2 export.");
		return false;
	}
};

class PS2Tween : public Node3D {
	GDCLASS(PS2Tween, Node3D);
	float duration = 1.0f;

protected:
	static void _bind_methods();

public:
	void set_duration(float p_t) { duration = p_t; }
	float get_duration() const { return duration; }
	bool play() {
		ERR_PRINT("PS2Tween.play only works on a PS2 export.");
		return false;
	}
	bool kill() {
		ERR_PRINT("PS2Tween.kill only works on a PS2 export.");
		return false;
	}
};

class PS2FlavorEditorPlugin : public EditorPlugin {
	GDCLASS(PS2FlavorEditorPlugin, EditorPlugin);

	VBoxContainer *dock = nullptr;
	Label *status = nullptr;
	Label *warn = nullptr;
	PS2 *ps2_stub = nullptr;
	bool last_enabled = false;
	ObjectID last_root;
	int last_nodes = -1;

	void _refresh();
	void _sync_singleton(bool p_enabled);

protected:
	static void _bind_methods() {}
	void _notification(int p_what);

public:
	virtual String get_plugin_name() const override { return "PS2"; }
	virtual bool has_main_screen() const override { return false; }

	PS2FlavorEditorPlugin();
	~PS2FlavorEditorPlugin();
};
