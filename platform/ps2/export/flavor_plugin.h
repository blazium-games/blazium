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
#include "core/string/ustring.h"
#include "core/variant/variant.h"
#include "editor/plugins/editor_plugin.h"
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
};

class PS2FlavorEditorPlugin : public EditorPlugin {
	GDCLASS(PS2FlavorEditorPlugin, EditorPlugin);

	VBoxContainer *dock = nullptr;
	Label *status = nullptr;
	PS2 *ps2_stub = nullptr;
	bool last_enabled = false;

	void _refresh();
	void _sync_singleton();

protected:
	static void _bind_methods() {}
	void _notification(int p_what);

public:
	virtual String get_plugin_name() const override { return "PS2"; }
	virtual bool has_main_screen() const override { return false; }

	PS2FlavorEditorPlugin();
	~PS2FlavorEditorPlugin();
};
