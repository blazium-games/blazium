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
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/box_container.h"
#include "scene/gui/label.h"

class PS1 : public Object {
	GDCLASS(PS1, Object);

	static void _fail_ps1() {
		ERR_PRINT("PS1.* only works on a PS1 export. Guard with OS.has_feature(\"ps1\").");
	}

protected:
	static void _bind_methods();

public:
	bool load_scene(const String &p_path) {
		_fail_ps1();
		return false;
	}
	bool unload_scene(const String &p_path) {
		_fail_ps1();
		return false;
	}
	bool is_scene_loaded(const String &p_path) const {
		_fail_ps1();
		return false;
	}
	int get_loaded_scene_count() const {
		_fail_ps1();
		return 0;
	}
	String get_loaded_scene(int p_index) const {
		_fail_ps1();
		return String();
	}
	int get_static_memory_usage() const {
		_fail_ps1();
		return 0;
	}
	int get_static_memory_peak_usage() const {
		_fail_ps1();
		return 0;
	}
	int get_ram_budget() const {
		_fail_ps1();
		return 0;
	}
	int get_free_ram() const {
		_fail_ps1();
		return 0;
	}
	bool can_load_scene(const String &p_path) const {
		_fail_ps1();
		return false;
	}
	bool can_instantiate(const Variant &p_path_or_pack) const {
		_fail_ps1();
		return false;
	}
	int get_node_count() const {
		_fail_ps1();
		return 0;
	}
	int get_node_budget() const {
		_fail_ps1();
		return 0;
	}
	int get_triangle_count() const {
		_fail_ps1();
		return 0;
	}
	int get_triangle_budget() const {
		_fail_ps1();
		return 0;
	}
	int get_free_nodes() const {
		_fail_ps1();
		return 0;
	}
	int get_free_hud() const {
		_fail_ps1();
		return 0;
	}
	int get_free_tiles() const {
		_fail_ps1();
		return 0;
	}
	bool set_camera(int p_index) {
		_fail_ps1();
		return false;
	}
	bool set_camera_node(const String &p_name) {
		_fail_ps1();
		return false;
	}
	bool set_default_camera(const Variant &p_index_or_name) {
		_fail_ps1();
		return false;
	}
	int get_camera() const {
		_fail_ps1();
		return -1;
	}
	int get_camera_count() const {
		_fail_ps1();
		return 0;
	}
	String get_camera_name(int p_index) const {
		_fail_ps1();
		return String();
	}
	bool is_camera_current(int p_index) const {
		_fail_ps1();
		return false;
	}
	bool next_camera() {
		_fail_ps1();
		return false;
	}
	bool prev_camera() {
		_fail_ps1();
		return false;
	}
	bool overlaps(const Variant &p_a, const Variant &p_b) const {
		_fail_ps1();
		return false;
	}
	bool overlaps_entered(const Variant &p_a, const Variant &p_b) const {
		_fail_ps1();
		return false;
	}
	bool has_overlapping_areas() const {
		_fail_ps1();
		return false;
	}
	int get_overlapping_area_count() const {
		_fail_ps1();
		return 0;
	}
	Variant get_overlapping_area(int p_index) const {
		_fail_ps1();
		return Variant();
	}
	Variant intersects_ray(const Vector3 &p_from, const Vector3 &p_dir) const {
		_fail_ps1();
		return Variant();
	}
	int hitbox_kind(const Variant &p_node) const {
		_fail_ps1();
		return 0;
	}
	void set_hitbox_enabled(const Variant &p_node, bool p_on) {
		_fail_ps1();
	}
	bool load_animations(const String &p_path) {
		_fail_ps1();
		return false;
	}
	bool unload_animations(const String &p_path) {
		_fail_ps1();
		return false;
	}
	bool can_load_animations(const String &p_path) const {
		_fail_ps1();
		return false;
	}
	bool is_animations_loaded(const String &p_path) const {
		_fail_ps1();
		return false;
	}
	bool load_sprites(const String &p_path) {
		_fail_ps1();
		return false;
	}
	bool unload_sprites(const String &p_path) {
		_fail_ps1();
		return false;
	}
	bool can_load_sprites(const String &p_path) const {
		_fail_ps1();
		return false;
	}
	bool is_sprites_loaded(const String &p_path) const {
		_fail_ps1();
		return false;
	}
	bool load_hitboxes(const String &p_path) {
		_fail_ps1();
		return false;
	}
	bool unload_hitboxes(const String &p_path) {
		_fail_ps1();
		return false;
	}
	bool can_load_hitboxes(const String &p_path) const {
		_fail_ps1();
		return false;
	}
	bool is_hitboxes_loaded(const String &p_path) const {
		_fail_ps1();
		return false;
	}
	bool load_cameras(const String &p_path) {
		_fail_ps1();
		return false;
	}
	bool unload_cameras(const String &p_path) {
		_fail_ps1();
		return false;
	}
	bool is_cameras_loaded(const String &p_path) const {
		_fail_ps1();
		return false;
	}
	bool can_load_cameras(const String &p_path) const {
		_fail_ps1();
		return false;
	}
	bool add_to_group(const Variant &p_node, const String &p_name) {
		_fail_ps1();
		return false;
	}
	bool remove_from_group(const Variant &p_node, const String &p_name) {
		_fail_ps1();
		return false;
	}
	bool is_in_group(const Variant &p_node, const String &p_name) const {
		_fail_ps1();
		return false;
	}
	int count_in_group(const String &p_name) const {
		_fail_ps1();
		return 0;
	}
	Variant get_in_group(const String &p_name, int p_index) const {
		_fail_ps1();
		return Variant();
	}
	Variant get_first_in_group(const String &p_name) const {
		_fail_ps1();
		return Variant();
	}
	Variant find_nearest(const Variant &p_from, const String &p_group) const {
		_fail_ps1();
		return Variant();
	}
	bool set_billboard(const Variant &p_node, bool p_on) {
		_fail_ps1();
		return false;
	}
	bool get_billboard(const Variant &p_node) const {
		_fail_ps1();
		return false;
	}
	Variant get_floor_node() const {
		_fail_ps1();
		return Variant();
	}
	Vector3 get_floor_normal() const {
		_fail_ps1();
		return Vector3();
	}
	bool make_current(const Variant &p_node) {
		_fail_ps1();
		return false;
	}
	bool is_action_pressed_on(int p_device, const String &p_action) const {
		_fail_ps1();
		return false;
	}
	bool is_action_just_pressed_on(int p_device, const String &p_action) const {
		_fail_ps1();
		return false;
	}
	bool is_action_just_released_on(int p_device, const String &p_action) const {
		_fail_ps1();
		return false;
	}
	bool memcard_present(int p_slot) const {
		_fail_ps1();
		return false;
	}
	bool memcard_ready(int p_slot) const {
		_fail_ps1();
		return false;
	}
	bool memcard_format(int p_slot) {
		_fail_ps1();
		return false;
	}
	bool memcard_exists(int p_slot, const String &p_name) const {
		_fail_ps1();
		return false;
	}
	bool memcard_delete(int p_slot, const String &p_name) {
		_fail_ps1();
		return false;
	}
	bool memcard_save(int p_slot, const String &p_name, const PackedByteArray &p_bytes) {
		_fail_ps1();
		return false;
	}
	PackedByteArray memcard_load(int p_slot, const String &p_name) const {
		_fail_ps1();
		return PackedByteArray();
	}
	int memcard_count(int p_slot) const {
		_fail_ps1();
		return 0;
	}
	String memcard_name(int p_slot, int p_index) const {
		_fail_ps1();
		return String();
	}
	PackedByteArray memcard_wrap(const PackedByteArray &p_payload, const String &p_title);
	PackedByteArray memcard_unwrap(const PackedByteArray &p_framed);
	Vector2 get_stick(int p_device, int p_stick) const {
		_fail_ps1();
		return Vector2();
	}
	bool set_rumble(int p_device, int p_small, int p_large) {
		_fail_ps1();
		return false;
	}
	bool stop_rumble(int p_device) {
		_fail_ps1();
		return false;
	}
	bool set_camera_transform(const Vector3 &p_pos, const Vector3 &p_rot) {
		_fail_ps1();
		return false;
	}
	Vector3 get_camera_transform() const {
		_fail_ps1();
		return Vector3();
	}
	Vector3 get_camera_rotation() const {
		_fail_ps1();
		return Vector3();
	}
	bool attach_camera(const Variant &p_node, const Vector3 &p_offset = Vector3()) {
		_fail_ps1();
		return false;
	}
	bool look_camera(float p_yaw, float p_pitch) {
		_fail_ps1();
		return false;
	}
	bool orbit_camera(const Variant &p_target, float p_yaw, float p_pitch, float p_dist) {
		_fail_ps1();
		return false;
	}
	bool set_camera_scale(int p_h) {
		_fail_ps1();
		return false;
	}
	Variant raycast(const Vector3 &p_from, const Vector3 &p_dir, float p_dist, int p_mask = 255) const {
		_fail_ps1();
		return Variant();
	}
	Variant raycast_point(const Vector3 &p_from, const Vector3 &p_dir, float p_dist, int p_mask = 255) const {
		_fail_ps1();
		return Variant();
	}
	Vector3 move_and_slide(const Variant &p_node, const Vector3 &p_velocity, int p_mask = 255) {
		_fail_ps1();
		return Vector3();
	}
	bool is_on_floor() const {
		_fail_ps1();
		return false;
	}
	bool is_on_wall() const {
		_fail_ps1();
		return false;
	}
	bool is_on_ceiling() const {
		_fail_ps1();
		return false;
	}
	bool set_hitbox_layer(const Variant &p_node, int p_layer) {
		_fail_ps1();
		return false;
	}
	int get_hitbox_layer(const Variant &p_node) const {
		_fail_ps1();
		return 0;
	}
	bool shake_camera(float p_amp, float p_ms) {
		_fail_ps1();
		return false;
	}
	bool set_paused(bool p_on) {
		_fail_ps1();
		return false;
	}
	bool is_paused() const {
		_fail_ps1();
		return false;
	}
	bool set_music_volume(float p_vol) {
		_fail_ps1();
		return false;
	}
	bool load_text(const String &p_path) {
		_fail_ps1();
		return false;
	}
	bool unload_text(const String &p_path) {
		_fail_ps1();
		return false;
	}
	bool can_load_text(const String &p_path) const {
		_fail_ps1();
		return false;
	}
	bool is_text_loaded(const String &p_path) const {
		_fail_ps1();
		return false;
	}
	bool set_line(const Variant &p_hud, const String &p_name) {
		_fail_ps1();
		return false;
	}
	bool set_line_chars(const Variant &p_hud, const String &p_name, int p_n) {
		_fail_ps1();
		return false;
	}
	float get_action_strength(const String &p_action) const {
		_fail_ps1();
		return 0.0f;
	}
	bool set_stick_deadzone(int p_n) {
		_fail_ps1();
		return false;
	}
	Vector3 get_velocity(const Variant &p_node = Variant()) const {
		(void)p_node;
		_fail_ps1();
		return Vector3();
	}
	bool poke(int p_off, int p_n) {
		_fail_ps1();
		return false;
	}
	int peek(int p_off) const {
		_fail_ps1();
		return 0;
	}
	bool set_node_meta(const Variant &p_node, int p_n) {
		_fail_ps1();
		return false;
	}
	int get_node_meta(const Variant &p_node) const {
		_fail_ps1();
		return 0;
	}
	bool set_angular_velocity(const Variant &p_node, const Vector3 &p_v) {
		_fail_ps1();
		return false;
	}
	Vector3 get_angular_velocity(const Variant &p_node) const {
		_fail_ps1();
		return Vector3();
	}
	bool set_camera_limits(int p_l, int p_t, int p_r, int p_b) {
		_fail_ps1();
		return false;
	}
	bool set_camera_drag(int p_n) {
		_fail_ps1();
		return false;
	}
	bool move_and_drive(const Variant &p_node, float p_throttle, float p_steer) {
		_fail_ps1();
		return false;
	}
	float get_speed(const Variant &p_node) const {
		_fail_ps1();
		return 0.0f;
	}
	bool set_drive_friction(const Variant &p_node, float p_f) {
		_fail_ps1();
		return false;
	}
	bool start_stream(const Variant &p_node, int p_rate, int p_tex) {
		_fail_ps1();
		return false;
	}
	bool stop_stream(const Variant &p_node) {
		_fail_ps1();
		return false;
	}
	bool set_stream_vel(const Variant &p_node, const Vector3 &p_v) {
		_fail_ps1();
		return false;
	}
	bool set_stream_spread(const Variant &p_node, int p_n) {
		_fail_ps1();
		return false;
	}
	bool set_stream_mode(const Variant &p_node, int p_m) {
		_fail_ps1();
		return false;
	}
	bool set_particle_mode(int p_m) {
		_fail_ps1();
		return false;
	}
	bool set_particle_flipbook(int p_tex, int p_frames, int p_fps) {
		_fail_ps1();
		return false;
	}
	bool set_particle_gravity(const Vector3 &p_g) {
		_fail_ps1();
		return false;
	}
	bool set_particle_color(const Vector3 &p_c) {
		_fail_ps1();
		return false;
	}
	bool emit_mesh(const Variant &p_node, int p_n, int p_tex) {
		_fail_ps1();
		return false;
	}
	Variant world_to_screen(const Vector3 &p_pos) const {
		_fail_ps1();
		return Variant();
	}
	bool seek_animation(float p_sec) {
		_fail_ps1();
		return false;
	}
	bool set_animation_speed(float p_s) {
		_fail_ps1();
		return false;
	}
	String format_int(int p_n, int p_width) const {
		_fail_ps1();
		return String();
	}
	bool set_hud_text(const Variant &p_hud, const String &p_text) {
		_fail_ps1();
		return false;
	}
	bool load_music(const String &p_path) {
		_fail_ps1();
		return false;
	}
	bool unload_music(const String &p_path) {
		_fail_ps1();
		return false;
	}
	bool can_load_music(const String &p_path) const {
		_fail_ps1();
		return false;
	}
	bool is_music_loaded(const String &p_path) const {
		_fail_ps1();
		return false;
	}
	bool play_fmv(const String &p_path) {
		_fail_ps1();
		return false;
	}
	bool play_xa(const String &p_path) {
		_fail_ps1();
		return false;
	}
	bool stop_xa() {
		_fail_ps1();
		return false;
	}
	bool tile_solid_at(float p_x, float p_y) const {
		_fail_ps1();
		return false;
	}
	int tile_at(float p_x, float p_y) const {
		_fail_ps1();
		return -1;
	}
	bool load_audio(const String &p_path) {
		_fail_ps1();
		return false;
	}
	bool unload_audio(const String &p_path) {
		_fail_ps1();
		return false;
	}
	bool can_load_audio(const String &p_path) const {
		_fail_ps1();
		return false;
	}
	bool is_audio_loaded(const String &p_path) const {
		_fail_ps1();
		return false;
	}
	bool play_sfx(const String &p_name) {
		_fail_ps1();
		return false;
	}
	bool stop_sfx(const String &p_name) {
		_fail_ps1();
		return false;
	}
	bool set_sfx_volume(const String &p_name, float p_vol) {
		_fail_ps1();
		return false;
	}
	bool emit(const Vector3 &p_pos, int p_count, int p_sprite_or_tim) {
		_fail_ps1();
		return false;
	}
	bool set_fog(bool p_on, float p_start, float p_end, const Color &p_color) {
		_fail_ps1();
		return false;
	}
	bool set_fade(float p_alpha, const Color &p_color) {
		_fail_ps1();
		return false;
	}
	bool set_light(int p_index, const Vector3 &p_dir, const Color &p_color) {
		_fail_ps1();
		return false;
	}
};

class PS1FlavorEditorPlugin : public EditorPlugin {
	GDCLASS(PS1FlavorEditorPlugin, EditorPlugin);

	VBoxContainer *dock = nullptr;
	Label *status = nullptr;
	Label *warn = nullptr;
	PS1 *ps1_stub = nullptr;
	ObjectID last_root;
	int last_nodes = -1;
	bool last_enabled = false;

	void _refresh();
	void _sync_singleton(bool p_enabled);

protected:
	static void _bind_methods() {}
	void _notification(int p_what);

public:
	virtual String get_plugin_name() const override { return "PS1"; }
	virtual bool has_main_screen() const override { return false; }

	PS1FlavorEditorPlugin();
	~PS1FlavorEditorPlugin();
};
