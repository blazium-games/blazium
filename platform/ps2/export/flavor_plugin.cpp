/**************************************************************************/
/*  flavor_plugin.cpp                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#include "flavor_plugin.h"

#include "cook/cooker.h"
#include "cook/script_cook.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "core/templates/hash_set.h"
#include "editor/editor_interface.h"
#include "scene/2d/sprite_2d.h"
#include "scene/2d/tile_map.h"
#include "scene/2d/tile_map_layer.h"
#include "scene/2d/camera_2d.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/physics/collision_object_3d.h"
#include "scene/gui/control.h"
#include "scene/resources/mesh.h"
#include "servers/display_server.h"

void PS2::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_abi"), &PS2::get_abi);
	ClassDB::bind_method(D_METHOD("is_ready"), &PS2::is_ready);
	ClassDB::bind_method(D_METHOD("set_rumble", "device", "small", "large"), &PS2::set_rumble);
	ClassDB::bind_method(D_METHOD("stop_rumble", "device"), &PS2::stop_rumble);
	ClassDB::bind_method(D_METHOD("get_pressure", "device", "button"), &PS2::get_pressure);
	ClassDB::bind_method(D_METHOD("play_sfx", "name"), &PS2::play_sfx);
	ClassDB::bind_method(D_METHOD("stop_sfx"), &PS2::stop_sfx);
	ClassDB::bind_method(D_METHOD("swap_pack", "unload_path", "load_path"), &PS2::swap_pack);
	ClassDB::bind_method(D_METHOD("load_scene", "path"), &PS2::load_scene);
	ClassDB::bind_method(D_METHOD("unload_scene", "path"), &PS2::unload_scene);
	ClassDB::bind_method(D_METHOD("get_display_width"), &PS2::get_display_width);
	ClassDB::bind_method(D_METHOD("get_display_height"), &PS2::get_display_height);
	ClassDB::bind_method(D_METHOD("memcard_present"), &PS2::memcard_present);
	ClassDB::bind_method(D_METHOD("memcard_ready"), &PS2::memcard_ready);
	ClassDB::bind_method(D_METHOD("memcard_save", "name", "data"), &PS2::memcard_save);
	ClassDB::bind_method(D_METHOD("memcard_load", "name"), &PS2::memcard_load);
	ClassDB::bind_method(D_METHOD("get_stick", "pad", "stick"), &PS2::get_stick);
	ClassDB::bind_method(D_METHOD("set_stick_deadzone", "zone"), &PS2::set_stick_deadzone);
	ClassDB::bind_method(D_METHOD("set_checkpoint", "node"), &PS2::set_checkpoint);
	ClassDB::bind_method(D_METHOD("respawn", "node"), &PS2::respawn);
	ClassDB::bind_method(D_METHOD("portal", "unload_path", "load_path", "fade"), &PS2::portal);
	ClassDB::bind_method(D_METHOD("hurt", "node", "amount", "vx", "vy", "vz"), &PS2::hurt);
	ClassDB::bind_method(D_METHOD("say", "hud", "line", "cps"), &PS2::say);
	ClassDB::bind_method(D_METHOD("can_load_scene", "path"), &PS2::can_load_scene);
	ClassDB::bind_method(D_METHOD("is_scene_loaded", "path"), &PS2::is_scene_loaded);
	ClassDB::bind_method(D_METHOD("get_loaded_scene_count"), &PS2::get_loaded_scene_count);
	ClassDB::bind_method(D_METHOD("unload_pack", "path"), &PS2::unload_pack);
	ClassDB::bind_method(D_METHOD("load_hud", "path"), &PS2::load_hud);
	ClassDB::bind_method(D_METHOD("unload_hud", "path"), &PS2::unload_hud);
	ClassDB::bind_method(D_METHOD("can_load_hud", "path"), &PS2::can_load_hud);
	ClassDB::bind_method(D_METHOD("is_hud_loaded", "path"), &PS2::is_hud_loaded);
	ClassDB::bind_method(D_METHOD("load_tiles", "path"), &PS2::load_tiles);
	ClassDB::bind_method(D_METHOD("unload_tiles", "path"), &PS2::unload_tiles);
	ClassDB::bind_method(D_METHOD("can_load_tiles", "path"), &PS2::can_load_tiles);
	ClassDB::bind_method(D_METHOD("is_tiles_loaded", "path"), &PS2::is_tiles_loaded);
	ClassDB::bind_method(D_METHOD("nav_follow", "node", "goal", "speed"), &PS2::nav_follow);
	ClassDB::bind_method(D_METHOD("nav_next_node", "from", "to"), &PS2::nav_next_node);
	ClassDB::bind_method(D_METHOD("set_hp", "node", "n"), &PS2::set_hp);
	ClassDB::bind_method(D_METHOD("get_hp", "node"), &PS2::get_hp);
	ClassDB::bind_method(D_METHOD("set_frame", "node", "i"), &PS2::set_frame);
	ClassDB::bind_method(D_METHOD("play_fmv", "path"), &PS2::play_fmv);
	ClassDB::bind_method(D_METHOD("save_slot", "slot"), &PS2::save_slot);
	ClassDB::bind_method(D_METHOD("load_slot", "slot"), &PS2::load_slot);
	ClassDB::bind_method(D_METHOD("memcard_format"), &PS2::memcard_format);
	ClassDB::bind_method(D_METHOD("load_music", "path"), &PS2::load_music);
	ClassDB::bind_method(D_METHOD("unload_music", "path"), &PS2::unload_music);
	ClassDB::bind_method(D_METHOD("can_load_music", "path"), &PS2::can_load_music);
	ClassDB::bind_method(D_METHOD("is_music_loaded", "path"), &PS2::is_music_loaded);
	ClassDB::bind_method(D_METHOD("set_music_volume", "vol"), &PS2::set_music_volume);
	ClassDB::bind_method(D_METHOD("set_music_fade", "to_vol", "ms"), &PS2::set_music_fade);
	ClassDB::bind_method(D_METHOD("stop_music"), &PS2::stop_music);
	ClassDB::bind_method(D_METHOD("look_camera", "yaw", "pitch", "roll"), &PS2::look_camera, DEFVAL(0.0f));
	ClassDB::bind_method(D_METHOD("orbit_camera", "target", "yaw", "pitch", "dist", "clip"), &PS2::orbit_camera, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("attach_camera", "node", "offset"), &PS2::attach_camera, DEFVAL(Vector3()));
	ClassDB::bind_method(D_METHOD("shake_camera", "amp", "ms"), &PS2::shake_camera);
	ClassDB::bind_method(D_METHOD("next_camera"), &PS2::next_camera);
	ClassDB::bind_method(D_METHOD("prev_camera"), &PS2::prev_camera);
	ClassDB::bind_method(D_METHOD("set_camera", "index"), &PS2::set_camera);
	ClassDB::bind_method(D_METHOD("set_camera_node", "name"), &PS2::set_camera_node);
	ClassDB::bind_method(D_METHOD("get_camera"), &PS2::get_camera);
	ClassDB::bind_method(D_METHOD("get_camera_count"), &PS2::get_camera_count);
	ClassDB::bind_method(D_METHOD("make_current", "node"), &PS2::make_current);
	ClassDB::bind_method(D_METHOD("create_tween"), &PS2::create_tween);
	ClassDB::bind_method(D_METHOD("tween_property", "tween", "node", "prop", "to", "sec"), &PS2::tween_property);
	ClassDB::bind_method(D_METHOD("kill_tweens", "node"), &PS2::kill_tweens);
	ClassDB::bind_method(D_METHOD("tween_is_done", "tween"), &PS2::tween_is_done);
	ClassDB::bind_method(D_METHOD("tween_count"), &PS2::tween_count);
	ClassDB::bind_method(D_METHOD("create_timer", "sec"), &PS2::create_timer);
	ClassDB::bind_method(D_METHOD("is_timer_done", "timer"), &PS2::is_timer_done);
	ClassDB::bind_method(D_METHOD("move_and_slide", "node", "velocity", "mask"), &PS2::move_and_slide, DEFVAL(255));
	ClassDB::bind_method(D_METHOD("is_on_floor"), &PS2::is_on_floor);
	ClassDB::bind_method(D_METHOD("is_on_wall"), &PS2::is_on_wall);
	ClassDB::bind_method(D_METHOD("is_on_ceiling"), &PS2::is_on_ceiling);
	ClassDB::bind_method(D_METHOD("overlaps", "a", "b"), &PS2::overlaps);
	ClassDB::bind_method(D_METHOD("overlaps_entered", "a", "b"), &PS2::overlaps_entered);
	ClassDB::bind_method(D_METHOD("has_overlapping_areas"), &PS2::has_overlapping_areas);
	ClassDB::bind_method(D_METHOD("hitbox_kind", "node"), &PS2::hitbox_kind);
	ClassDB::bind_method(D_METHOD("raycast", "from", "dir", "dist", "mask"), &PS2::raycast, DEFVAL(255));
	ClassDB::bind_method(D_METHOD("raycast_point", "from", "dir", "dist", "mask"), &PS2::raycast_point, DEFVAL(255));
	ClassDB::bind_method(D_METHOD("intersects_ray", "from", "dir"), &PS2::intersects_ray);
	ClassDB::bind_method(D_METHOD("tile_solid_at", "x", "y"), &PS2::tile_solid_at);
	ClassDB::bind_method(D_METHOD("tile_at", "x", "y"), &PS2::tile_at);
	ClassDB::bind_method(D_METHOD("set_fade", "alpha", "color"), &PS2::set_fade, DEFVAL(Color()));
	ClassDB::bind_method(D_METHOD("change_scene_fade", "path", "seconds"), &PS2::change_scene_fade);
	ClassDB::bind_method(D_METHOD("change_scene", "path"), &PS2::change_scene);
	ClassDB::bind_method(D_METHOD("instantiate", "path_or_pack"), &PS2::instantiate);
	ClassDB::bind_method(D_METHOD("can_instantiate", "path_or_pack"), &PS2::can_instantiate);
	ClassDB::bind_method(D_METHOD("say_done"), &PS2::say_done);
	ClassDB::bind_method(D_METHOD("set_hud_text", "hud", "text"), &PS2::set_hud_text);
	ClassDB::bind_method(D_METHOD("seek_animation", "sec"), &PS2::seek_animation);
	ClassDB::bind_method(D_METHOD("play_animation", "name_or_index"), &PS2::play_animation);
	ClassDB::bind_method(D_METHOD("stop_animation"), &PS2::stop_animation);
	ClassDB::bind_method(D_METHOD("set_animation_speed", "speed"), &PS2::set_animation_speed);
	ClassDB::bind_method(D_METHOD("get_ee_ram_used"), &PS2::get_ee_ram_used);
	ClassDB::bind_method(D_METHOD("get_ee_ram_free"), &PS2::get_ee_ram_free);
	ClassDB::bind_method(D_METHOD("get_ee_ram_limit"), &PS2::get_ee_ram_limit);
	ClassDB::bind_method(D_METHOD("get_gs_vram_used"), &PS2::get_gs_vram_used);
	ClassDB::bind_method(D_METHOD("get_gs_vram_free"), &PS2::get_gs_vram_free);
	ClassDB::bind_method(D_METHOD("get_gs_vram_limit"), &PS2::get_gs_vram_limit);
	ClassDB::bind_method(D_METHOD("get_pack_cost", "path"), &PS2::get_pack_cost);
	ClassDB::bind_method(D_METHOD("set_default_camera", "index_or_name"), &PS2::set_default_camera);
	ClassDB::bind_method(D_METHOD("get_default_camera"), &PS2::get_default_camera);
	ClassDB::bind_method(D_METHOD("load_sprites", "path"), &PS2::load_sprites);
	ClassDB::bind_method(D_METHOD("unload_sprites", "path"), &PS2::unload_sprites, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("can_load_sprites", "path"), &PS2::can_load_sprites);
	ClassDB::bind_method(D_METHOD("is_sprites_loaded", "path"), &PS2::is_sprites_loaded, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("load_animations", "path"), &PS2::load_animations);
	ClassDB::bind_method(D_METHOD("unload_animations", "path"), &PS2::unload_animations, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("can_load_animations", "path"), &PS2::can_load_animations);
	ClassDB::bind_method(D_METHOD("is_animations_loaded", "path"), &PS2::is_animations_loaded, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("load_hitboxes", "path"), &PS2::load_hitboxes);
	ClassDB::bind_method(D_METHOD("unload_hitboxes", "path"), &PS2::unload_hitboxes, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("can_load_hitboxes", "path"), &PS2::can_load_hitboxes);
	ClassDB::bind_method(D_METHOD("is_hitboxes_loaded", "path"), &PS2::is_hitboxes_loaded, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("spawn", "path", "pos"), &PS2::spawn, DEFVAL(Vector3()));
	ClassDB::bind_method(D_METHOD("move_planar", "node", "ax", "ay", "speed"), &PS2::move_planar);
	ClassDB::bind_method(D_METHOD("follow_node", "node", "target", "speed"), &PS2::follow_node);
	ClassDB::bind_method(D_METHOD("set_hitbox_enabled", "node", "on"), &PS2::set_hitbox_enabled);
	ClassDB::bind_method(D_METHOD("set_hitbox_layer", "node", "layer"), &PS2::set_hitbox_layer);
	ClassDB::bind_method(D_METHOD("get_hitbox_layer", "node"), &PS2::get_hitbox_layer);
	ClassDB::bind_method(D_METHOD("set_hitstop", "ms"), &PS2::set_hitstop);
	ClassDB::bind_method(D_METHOD("set_invuln", "node", "ms"), &PS2::set_invuln);
	ClassDB::bind_method(D_METHOD("is_invuln", "node"), &PS2::is_invuln);
	ClassDB::bind_method(D_METHOD("knockback", "node", "vx", "vy", "vz"), &PS2::knockback);
	ClassDB::bind_method(D_METHOD("set_cell", "x", "y", "flags"), &PS2::set_cell, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("erase_cell", "x", "y"), &PS2::erase_cell);
	ClassDB::bind_method(D_METHOD("set_flip", "node", "flip"), &PS2::set_flip);
	ClassDB::bind_method(D_METHOD("poke", "off", "byte"), &PS2::poke);
	ClassDB::bind_method(D_METHOD("peek", "off"), &PS2::peek);
	ClassDB::bind_method(D_METHOD("godot_save_bytes", "value"), &PS2::godot_save_bytes);
	ClassDB::bind_method(D_METHOD("godot_load_bytes", "bytes"), &PS2::godot_load_bytes);
	ClassDB::bind_method(D_METHOD("move_6dof", "node", "ax", "ay", "az", "pitch", "yaw", "roll", "speed"), &PS2::move_6dof);
	ClassDB::bind_method(D_METHOD("set_steer", "node", "steer"), &PS2::set_steer);
	ClassDB::bind_method(D_METHOD("set_thrust", "node", "thrust"), &PS2::set_thrust);
	ClassDB::bind_method(D_METHOD("set_eye_height", "height"), &PS2::set_eye_height);
	ClassDB::bind_method(D_METHOD("load_particles", "path"), &PS2::load_particles);
	ClassDB::bind_method(D_METHOD("unload_particles", "path"), &PS2::unload_particles, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("can_load_particles", "path"), &PS2::can_load_particles);
	ClassDB::bind_method(D_METHOD("is_particles_loaded", "path"), &PS2::is_particles_loaded, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("path_follow", "node", "speed"), &PS2::path_follow);
	ClassDB::bind_method(D_METHOD("set_path_offset", "node", "offset"), &PS2::set_path_offset);
	ClassDB::bind_method(D_METHOD("prefetch_scene", "path"), &PS2::prefetch_scene);
	ClassDB::bind_method(D_METHOD("navmesh", "node", "goal"), &PS2::navmesh);
	ClassDB::bind_method(D_METHOD("tween_play", "tween"), &PS2::tween_play);
	ClassDB::bind_method(D_METHOD("tween_kill", "tween"), &PS2::tween_kill);
}

void PS2Spawner::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_scene", "path"), &PS2Spawner::set_scene);
	ClassDB::bind_method(D_METHOD("get_scene"), &PS2Spawner::get_scene);
	ClassDB::bind_method(D_METHOD("spawn"), &PS2Spawner::spawn);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "scene", PROPERTY_HINT_FILE, "*.tscn"), "set_scene", "get_scene");
}

void PS2CameraVolume::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_camera_index", "index"), &PS2CameraVolume::set_camera_index);
	ClassDB::bind_method(D_METHOD("get_camera_index"), &PS2CameraVolume::get_camera_index);
	ClassDB::bind_method(D_METHOD("make_current"), &PS2CameraVolume::make_current);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "camera_index"), "set_camera_index", "get_camera_index");
}

void PS2Follow::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_target_path", "path"), &PS2Follow::set_target_path);
	ClassDB::bind_method(D_METHOD("get_target_path"), &PS2Follow::get_target_path);
	ClassDB::bind_method(D_METHOD("set_speed", "speed"), &PS2Follow::set_speed);
	ClassDB::bind_method(D_METHOD("get_speed"), &PS2Follow::get_speed);
	ClassDB::bind_method(D_METHOD("follow"), &PS2Follow::follow);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "target_path"), "set_target_path", "get_target_path");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed"), "set_speed", "get_speed");
}

void PS2Waypoint::_bind_methods() {}

void PS2Path::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_path_offset", "offset"), &PS2Path::set_path_offset);
	ClassDB::bind_method(D_METHOD("get_path_offset"), &PS2Path::get_path_offset);
	ClassDB::bind_method(D_METHOD("path_follow", "speed"), &PS2Path::path_follow);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "path_offset"), "set_path_offset", "get_path_offset");
}

void PS2NavRegion::_bind_methods() {
	ClassDB::bind_method(D_METHOD("nav_follow", "goal", "speed"), &PS2NavRegion::nav_follow);
	ClassDB::bind_method(D_METHOD("nav_next"), &PS2NavRegion::nav_next);
}

void PS2Vehicle::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mode", "mode"), &PS2Vehicle::set_mode);
	ClassDB::bind_method(D_METHOD("get_mode"), &PS2Vehicle::get_mode);
	ClassDB::bind_method(D_METHOD("set_mass", "mass"), &PS2Vehicle::set_mass);
	ClassDB::bind_method(D_METHOD("get_mass"), &PS2Vehicle::get_mass);
	ClassDB::bind_method(D_METHOD("set_thrust", "thrust"), &PS2Vehicle::set_thrust);
	ClassDB::bind_method(D_METHOD("get_thrust"), &PS2Vehicle::get_thrust);
	ClassDB::bind_method(D_METHOD("set_steer", "steer"), &PS2Vehicle::set_steer);
	ClassDB::bind_method(D_METHOD("get_steer"), &PS2Vehicle::get_steer);
	ClassDB::bind_method(D_METHOD("move_6dof", "ax", "ay", "az", "pitch", "yaw", "roll", "speed"), &PS2Vehicle::move_6dof);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mode"), "set_mode", "get_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mass"), "set_mass", "get_mass");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "thrust"), "set_thrust", "get_thrust");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "steer"), "set_steer", "get_steer");
}

void PS2Particles::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mode", "mode"), &PS2Particles::set_mode);
	ClassDB::bind_method(D_METHOD("get_mode"), &PS2Particles::get_mode);
	ClassDB::bind_method(D_METHOD("set_amount", "amount"), &PS2Particles::set_amount);
	ClassDB::bind_method(D_METHOD("get_amount"), &PS2Particles::get_amount);
	ClassDB::bind_method(D_METHOD("set_lifetime", "lifetime"), &PS2Particles::set_lifetime);
	ClassDB::bind_method(D_METHOD("get_lifetime"), &PS2Particles::get_lifetime);
	ClassDB::bind_method(D_METHOD("load_particles"), &PS2Particles::load_particles);
	ClassDB::bind_method(D_METHOD("unload_particles"), &PS2Particles::unload_particles);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mode"), "set_mode", "get_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "amount"), "set_amount", "get_amount");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lifetime"), "set_lifetime", "get_lifetime");
}

void PS2Tween::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_duration", "duration"), &PS2Tween::set_duration);
	ClassDB::bind_method(D_METHOD("get_duration"), &PS2Tween::get_duration);
	ClassDB::bind_method(D_METHOD("play"), &PS2Tween::play);
	ClassDB::bind_method(D_METHOD("kill"), &PS2Tween::kill);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "duration"), "set_duration", "get_duration");
}

static void _count_scene(Node *p_node, int &r_nodes, int &r_tris, int &r_tex, int &r_hud, int &r_tiles, int &r_cams, int &r_hits) {
	if (!p_node) {
		return;
	}
	r_nodes++;
	if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(p_node)) {
		const Ref<Mesh> mesh = mi->get_mesh();
		if (mesh.is_valid()) {
			r_tris += mesh->get_faces().size();
			r_tex += MAX(1, mesh->get_surface_count());
		} else {
			r_tex++;
		}
	}
	if (Object::cast_to<Sprite2D>(p_node)) {
		r_tex++;
	}
	if (Object::cast_to<Control>(p_node)) {
		r_hud++;
	}
	if (TileMap *tm = Object::cast_to<TileMap>(p_node)) {
		r_tiles += tm->get_used_cells(0).size();
	}
	if (TileMapLayer *tl = Object::cast_to<TileMapLayer>(p_node)) {
		r_tiles += tl->get_used_cells().size();
	}
	if (Object::cast_to<Camera3D>(p_node) || Object::cast_to<Camera2D>(p_node)) {
		r_cams++;
	}
	if (Object::cast_to<CollisionObject3D>(p_node)) {
		r_hits++;
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_count_scene(p_node->get_child(i), r_nodes, r_tris, r_tex, r_hud, r_tiles, r_cams, r_hits);
	}
}

static int _count_literal_packs() {
	int n = 0;
	Vector<String> stack;
	stack.push_back("res://");
	HashSet<String> seen;
	while (!stack.is_empty()) {
		const String dir = stack[stack.size() - 1];
		stack.resize(stack.size() - 1);
		Ref<DirAccess> da = DirAccess::open(dir);
		if (da.is_null()) {
			continue;
		}
		da->list_dir_begin();
		for (String name = da->get_next(); !name.is_empty(); name = da->get_next()) {
			if (name == "." || name == "..") {
				continue;
			}
			const String path = dir.path_join(name);
			if (da->current_is_dir()) {
				if (name == "export" || name == ".godot" || name == "addons" || name == "tests" || name == "tools") {
					continue;
				}
				stack.push_back(path);
				continue;
			}
			if (name.ends_with(".tscn")) {
				if (!seen.has(path)) {
					seen.insert(path);
					n++;
				}
				continue;
			}
			if (!(name.ends_with(".gd") || name.ends_with(".luau"))) {
				continue;
			}
			if (name == "run_tests.gd" || name.begins_with("test_")) {
				continue;
			}
			const String src = FileAccess::get_file_as_string(path);
			Vector<String> scenes;
			ps2_cook::collect_literal_scenes(src, scenes);
			for (int i = 0; i < scenes.size(); i++) {
				if (!seen.has(scenes[i])) {
					seen.insert(scenes[i]);
					n++;
				}
			}
		}
	}
	return n;
}

void PS2FlavorEditorPlugin::_sync_singleton(bool p_enabled) {
	Engine *eng = Engine::get_singleton();
	if (!eng) {
		return;
	}
	if (p_enabled) {
		if (!ps2_stub) {
			ps2_stub = memnew(PS2);
		}
		if (!eng->has_singleton("PS2")) {
			eng->add_singleton(Engine::Singleton("PS2", ps2_stub));
		}
		return;
	}
	if (eng->has_singleton("PS2")) {
		eng->remove_singleton("PS2");
	}
}

void PS2FlavorEditorPlugin::_refresh() {
	const bool enabled = ProjectSettings::get_singleton() && bool(ProjectSettings::get_singleton()->get("ps2/enabled"));
	if (enabled) {
		_sync_singleton(true);
	}
	if (!status) {
		return;
	}
	last_enabled = enabled;
	if (!enabled) {
		status->set_text("Turn on Project Settings → ps2/enabled for the PS2 flavor dock.\nPS2.* is PS2 export only; use OS.has_feature(\"ps2\").\nExport writes an EE ELF via blazium-toolchain (no Godot templates).");
		if (warn) {
			warn->set_text("");
		}
		return;
	}
	Node *root = EditorInterface::get_singleton() ? EditorInterface::get_singleton()->get_edited_scene_root() : nullptr;
	int nodes = 0, tris = 0, tex = 0, hud = 0, tiles = 0, cams = 0, hits = 0;
	_count_scene(root, nodes, tris, tex, hud, tiles, cams, hits);
	const int packs = _count_literal_packs();
	const int extras = MAX(0, packs - 1);
	const int est_ee = nodes * 80 + tris * 32 + 2 * 1024 * 1024;
	const int est_gs = 640 * 448 * 6 + tex * 64 * 64 * 2;
	const int load_ee = est_ee + extras * (tris > 0 ? MIN(est_ee / 4, 2 * 1024 * 1024) : 256 * 1024);
	const int load_gs = est_gs + extras * MIN(est_gs / 4, 256 * 1024);
	const String bios = OS::get_singleton() ? String(OS::get_singleton()->get_environment("PS2_BIOS_DIR")) : String();
	const String oneclick = bios.is_empty() ? "One-click disabled: set PS2_BIOS_DIR to a legal BIOS folder (export does not need BIOS)." : "One-click: PS2_BIOS_DIR is set (ps2 run / PCSX2 -elf).";
	status->set_text(vformat(
			"nodes %d/%d\ntris %d/%d\ntextures %d/%d\ncameras %d/%d\nHIT %d/%d\nHUD %d/%d\ntiles %d\ncatalog extras %d/%d (resident max %d + pack 0)\nest EE %d/%d  GS %d/%d\nEE 24 MiB and GS 4 MiB are shared. Extra STREAM packs cap at 2 MiB EE. Unload rooms/NPCs you are not using.\nPS2.* is PS2 export only; use OS.has_feature(\"ps2\").\nWorks: STREAM host:/cdrom0:, HIT, camera2D/3D, 6DoF/vehicle, CPU particles, nav/path, 16 tweens, IMA SFX/MUSIC, set_fade, DISP.\nPartial: change_scene swap / load_scene+instantiate additive, Button HUD, AnimationPlayer TRS, custom IR.\nRefused: play_xa, play_fmv, set_fog, spawn_shot, memcard_wrap, gsKit, official VMs, live GPUParticles/RigidBody.\n%s",
			nodes, ps2_cook::kMaxNodes, tris, ps2_cook::kMaxTris, tex, ps2_cook::kMaxTextures,
			cams, ps2_cook::kMaxCameras, hits, ps2_cook::kMaxHits, hud, ps2_cook::kMaxHud, tiles,
			extras, ps2_cook::kMaxScenePacks, ps2_cook::kMaxResidentExtras,
			est_ee, ps2_cook::kEeRamLimit, est_gs, ps2_cook::kGsVramLimit, oneclick));
	PackedStringArray hard;
	PackedStringArray soft;
	auto over = [&](int v, int cap, const String &label) {
		if (v > cap) {
			hard.push_back(vformat("%s (%d/%d)", label, v, cap));
		} else if (v > (cap * 4) / 5) {
			soft.push_back(vformat("%s 80%% (%d/%d)", label, v, cap));
		}
	};
	over(nodes, ps2_cook::kMaxNodes, "nodes");
	over(tris, ps2_cook::kMaxTris, "tris");
	over(tex, ps2_cook::kMaxTextures, "textures");
	over(cams, ps2_cook::kMaxCameras, "cameras");
	over(hits, ps2_cook::kMaxHits, "HIT");
	over(hud, ps2_cook::kMaxHud, "HUD");
	over(extras, ps2_cook::kMaxScenePacks, "catalog extras");
	over(est_ee, ps2_cook::kEeRamLimit, "est EE");
	over(est_gs, ps2_cook::kGsVramLimit, "est GS");
	if (load_ee > ps2_cook::kEeRamLimit || load_gs > ps2_cook::kGsVramLimit) {
		hard.push_back(vformat("Loading this scene plus %d already-referenced packs would exceed 24 MiB EE or 4 MiB GS — unload or split", extras));
	}
	if (!warn) {
		return;
	}
	if (!hard.is_empty()) {
		const String msg = "PS2 budget: " + String(", ").join(hard);
		warn->add_theme_color_override("font_color", Color(1, 0.35f, 0.35f));
		warn->set_text(msg);
		WARN_PRINT(msg);
	} else if (!soft.is_empty()) {
		const String msg = "PS2 budget 80%: " + String(", ").join(soft);
		warn->add_theme_color_override("font_color", Color(1, 0.82f, 0.25f));
		warn->set_text(msg);
		WARN_PRINT(msg);
	} else {
		warn->set_text("");
	}
}

void PS2FlavorEditorPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_PROCESS && dock) {
		Node *root = EditorInterface::get_singleton() ? EditorInterface::get_singleton()->get_edited_scene_root() : nullptr;
		const ObjectID id = root ? root->get_instance_id() : ObjectID();
		int nodes = 0, tris = 0, tex = 0, hud = 0, tiles = 0, cams = 0, hits = 0;
		_count_scene(root, nodes, tris, tex, hud, tiles, cams, hits);
		const bool enabled = ProjectSettings::get_singleton() && bool(ProjectSettings::get_singleton()->get("ps2/enabled"));
		if (id != last_root || nodes != last_nodes || enabled != last_enabled) {
			last_root = id;
			last_nodes = nodes;
			_refresh();
		}
	}
}

PS2FlavorEditorPlugin::PS2FlavorEditorPlugin() {
	const bool headless = DisplayServer::get_singleton() && String(DisplayServer::get_singleton()->get_name()) == "headless";
	if (!headless) {
		dock = memnew(VBoxContainer);
		status = memnew(Label);
		status->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
		dock->add_child(status);
		warn = memnew(Label);
		warn->add_theme_color_override("font_color", Color(1, 0.35f, 0.35f));
		warn->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
		dock->add_child(warn);
		add_control_to_dock(DOCK_SLOT_RIGHT_UL, dock);
		set_process(true);
	}
	_sync_singleton(true);
	if (!headless) {
		_refresh();
	}
}

PS2FlavorEditorPlugin::~PS2FlavorEditorPlugin() {
	ps2_stub = nullptr;
	if (dock) {
		remove_control_from_docks(dock);
		dock = nullptr;
	}
}
