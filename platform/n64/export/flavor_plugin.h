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
#include "core/io/marshalls.h"
#include "core/math/color.h"
#include "core/object/object.h"
#include "core/object/object_id.h"
#include "core/variant/variant.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/3d/node_3d.h"
#include "scene/gui/box_container.h"
#include "scene/gui/label.h"

class N64 : public Object {
	GDCLASS(N64, Object);

	static void _fail_n64() {
		ERR_PRINT("N64.* only works on an N64 export. Guard with OS.has_feature(\"n64\").");
	}

protected:
	static void _bind_methods();

public:
	int get_abi() const {
		_fail_n64();
		return 0;
	}
	bool is_ready() const {
		_fail_n64();
		return false;
	}
	bool play_sfx(const String &p_name) {
		_fail_n64();
		(void)p_name;
		return false;
	}
	bool stop_sfx() {
		_fail_n64();
		return false;
	}
	bool set_fade(float p_alpha, const Color &p_color = Color()) {
		_fail_n64();
		(void)p_alpha;
		(void)p_color;
		return false;
	}
	bool user_save(const PackedByteArray &p_data) {
		_fail_n64();
		(void)p_data;
		return false;
	}
	PackedByteArray user_load() {
		_fail_n64();
		return PackedByteArray();
	}
	int get_rdram_used() const {
		_fail_n64();
		return 0;
	}
	int get_rdram_free() const {
		_fail_n64();
		return 0;
	}
	int get_rdram_limit() const {
		_fail_n64();
		return 8 * 1024 * 1024;
	}
	int get_tmem_used() const {
		_fail_n64();
		return 0;
	}
	int get_tmem_free() const {
		_fail_n64();
		return 0;
	}
	int get_tmem_limit() const {
		_fail_n64();
		return 4 * 1024;
	}
	bool change_scene(const String &p_path) {
		_fail_n64();
		(void)p_path;
		return false;
	}
	Variant instantiate(const Variant &p_path_or_pack) {
		_fail_n64();
		(void)p_path_or_pack;
		return Variant();
	}
	bool set_checkpoint(const Variant &p_node) {
		_fail_n64();
		(void)p_node;
		return false;
	}
	bool respawn(const Variant &p_node) {
		_fail_n64();
		(void)p_node;
		return false;
	}
	bool look_camera(float p_yaw, float p_pitch, float p_roll = 0.0f) {
		_fail_n64();
		(void)p_yaw;
		(void)p_pitch;
		(void)p_roll;
		return false;
	}
	bool set_camera(int p_id) {
		_fail_n64();
		(void)p_id;
		return false;
	}
	bool set_default_camera(int p_id) {
		_fail_n64();
		(void)p_id;
		return false;
	}
	bool next_camera() {
		_fail_n64();
		return false;
	}
	bool play_anim(const String &p_name) {
		_fail_n64();
		(void)p_name;
		return false;
	}
	bool load_anims() {
		_fail_n64();
		return false;
	}
	bool unload_anims() {
		_fail_n64();
		return false;
	}
	bool load_sprites() {
		_fail_n64();
		return false;
	}
	int overlaps(float p_x, float p_y, float p_z) {
		_fail_n64();
		(void)p_x;
		(void)p_y;
		(void)p_z;
		return 0;
	}
	bool mempak_save(const PackedByteArray &p_data) {
		_fail_n64();
		(void)p_data;
		return false;
	}
	PackedByteArray mempak_load() {
		_fail_n64();
		return PackedByteArray();
	}
	bool path_follow(int p_id) {
		_fail_n64();
		(void)p_id;
		return false;
	}
	bool follow_node(int p_id) {
		_fail_n64();
		(void)p_id;
		return false;
	}
	bool load_particles() {
		_fail_n64();
		return false;
	}
	bool unload_particles() {
		_fail_n64();
		return false;
	}
	bool play_particles(int p_id) {
		_fail_n64();
		(void)p_id;
		return false;
	}
	int navmesh_next() {
		_fail_n64();
		return -1;
	}
	bool create_tween(int p_id, float p_dur) {
		_fail_n64();
		(void)p_id;
		(void)p_dur;
		return false;
	}
	bool kill_tweens() {
		_fail_n64();
		return false;
	}
	bool drive_vehicle(int p_id) {
		_fail_n64();
		(void)p_id;
		return false;
	}
	bool set_orbit(float p_az, float p_el, float p_rad) {
		_fail_n64();
		(void)p_az;
		(void)p_el;
		(void)p_rad;
		return false;
	}
	bool move_6dof(float p_dx, float p_dy, float p_dz) {
		_fail_n64();
		(void)p_dx;
		(void)p_dy;
		(void)p_dz;
		return false;
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
};

class N64Checkpoint : public Node3D {
	GDCLASS(N64Checkpoint, Node3D);

protected:
	static void _bind_methods();
};

class N64CameraVolume : public Node3D {
	GDCLASS(N64CameraVolume, Node3D);

protected:
	static void _bind_methods();
};

class N64Follow : public Node3D {
	GDCLASS(N64Follow, Node3D);

protected:
	static void _bind_methods();
};

class N64Path : public Node3D {
	GDCLASS(N64Path, Node3D);

protected:
	static void _bind_methods();
};

class N64Waypoint : public Node3D {
	GDCLASS(N64Waypoint, Node3D);

protected:
	static void _bind_methods();
};

class N64NavRegion : public Node3D {
	GDCLASS(N64NavRegion, Node3D);

protected:
	static void _bind_methods();
};

class N64Particles : public Node3D {
	GDCLASS(N64Particles, Node3D);
	int mode = 0;
	float rate = 8.0f;
	float life = 1.0f;
	float speed = 2.0f;

protected:
	static void _bind_methods();

public:
	void set_mode(int p_m) { mode = p_m; }
	int get_mode() const { return mode; }
	void set_rate(float p_r) { rate = p_r; }
	float get_rate() const { return rate; }
	void set_life(float p_l) { life = p_l; }
	float get_life() const { return life; }
	void set_speed(float p_s) { speed = p_s; }
	float get_speed() const { return speed; }
};

class N64Vehicle : public Node3D {
	GDCLASS(N64Vehicle, Node3D);
	int mode = 0;
	float speed = 8.0f;
	float turn = 2.0f;

protected:
	static void _bind_methods();

public:
	void set_mode(int p_m) { mode = p_m; }
	int get_mode() const { return mode; }
	void set_speed(float p_s) { speed = p_s; }
	float get_speed() const { return speed; }
	void set_turn(float p_t) { turn = p_t; }
	float get_turn() const { return turn; }
};

class N64Tween : public Node3D {
	GDCLASS(N64Tween, Node3D);

protected:
	static void _bind_methods();
};

class N64Spawner : public Node3D {
	GDCLASS(N64Spawner, Node3D);

protected:
	static void _bind_methods();
};

class N64LoadZone : public Node3D {
	GDCLASS(N64LoadZone, Node3D);

protected:
	static void _bind_methods();
};

class N64UnloadZone : public Node3D {
	GDCLASS(N64UnloadZone, Node3D);

protected:
	static void _bind_methods();
};

class N64FlavorEditorPlugin : public EditorPlugin {
	GDCLASS(N64FlavorEditorPlugin, EditorPlugin);

	VBoxContainer *dock = nullptr;
	Label *status = nullptr;
	bool last_enabled = false;

	void _refresh();

protected:
	static void _bind_methods() {}
	void _notification(int p_what);

public:
	virtual String get_plugin_name() const override { return "N64"; }
	virtual bool has_main_screen() const override { return false; }

	N64FlavorEditorPlugin();
	~N64FlavorEditorPlugin();
};
