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

#include "core/config/project_settings.h"
#include "servers/display_server.h"

void N64::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_abi"), &N64::get_abi);
	ClassDB::bind_method(D_METHOD("is_ready"), &N64::is_ready);
	ClassDB::bind_method(D_METHOD("play_sfx", "name"), &N64::play_sfx);
	ClassDB::bind_method(D_METHOD("stop_sfx"), &N64::stop_sfx);
	ClassDB::bind_method(D_METHOD("set_fade", "alpha", "color"), &N64::set_fade, DEFVAL(Color()));
	ClassDB::bind_method(D_METHOD("user_save", "data"), &N64::user_save);
	ClassDB::bind_method(D_METHOD("user_load"), &N64::user_load);
	ClassDB::bind_method(D_METHOD("get_rdram_used"), &N64::get_rdram_used);
	ClassDB::bind_method(D_METHOD("get_rdram_free"), &N64::get_rdram_free);
	ClassDB::bind_method(D_METHOD("get_rdram_limit"), &N64::get_rdram_limit);
	ClassDB::bind_method(D_METHOD("get_tmem_used"), &N64::get_tmem_used);
	ClassDB::bind_method(D_METHOD("get_tmem_free"), &N64::get_tmem_free);
	ClassDB::bind_method(D_METHOD("get_tmem_limit"), &N64::get_tmem_limit);
	ClassDB::bind_method(D_METHOD("change_scene", "path"), &N64::change_scene);
	ClassDB::bind_method(D_METHOD("instantiate", "path_or_pack"), &N64::instantiate);
	ClassDB::bind_method(D_METHOD("set_checkpoint", "node"), &N64::set_checkpoint);
	ClassDB::bind_method(D_METHOD("respawn", "node"), &N64::respawn);
	ClassDB::bind_method(D_METHOD("look_camera", "yaw", "pitch", "roll"), &N64::look_camera, DEFVAL(0.0f));
	ClassDB::bind_method(D_METHOD("set_camera", "id"), &N64::set_camera);
	ClassDB::bind_method(D_METHOD("set_default_camera", "id"), &N64::set_default_camera);
	ClassDB::bind_method(D_METHOD("next_camera"), &N64::next_camera);
	ClassDB::bind_method(D_METHOD("play_anim", "name"), &N64::play_anim);
	ClassDB::bind_method(D_METHOD("load_anims"), &N64::load_anims);
	ClassDB::bind_method(D_METHOD("unload_anims"), &N64::unload_anims);
	ClassDB::bind_method(D_METHOD("load_sprites"), &N64::load_sprites);
	ClassDB::bind_method(D_METHOD("overlaps", "x", "y", "z"), &N64::overlaps);
	ClassDB::bind_method(D_METHOD("mempak_save", "data"), &N64::mempak_save);
	ClassDB::bind_method(D_METHOD("mempak_load"), &N64::mempak_load);
	ClassDB::bind_method(D_METHOD("godot_save_bytes", "value"), &N64::godot_save_bytes);
	ClassDB::bind_method(D_METHOD("godot_load_bytes", "bytes"), &N64::godot_load_bytes);
	ClassDB::bind_method(D_METHOD("path_follow", "id"), &N64::path_follow);
	ClassDB::bind_method(D_METHOD("follow_node", "id"), &N64::follow_node);
	ClassDB::bind_method(D_METHOD("load_particles"), &N64::load_particles);
	ClassDB::bind_method(D_METHOD("unload_particles"), &N64::unload_particles);
	ClassDB::bind_method(D_METHOD("play_particles", "id"), &N64::play_particles);
	ClassDB::bind_method(D_METHOD("navmesh_next"), &N64::navmesh_next);
	ClassDB::bind_method(D_METHOD("create_tween", "id", "dur"), &N64::create_tween);
	ClassDB::bind_method(D_METHOD("kill_tweens"), &N64::kill_tweens);
	ClassDB::bind_method(D_METHOD("drive_vehicle", "id"), &N64::drive_vehicle);
	ClassDB::bind_method(D_METHOD("set_orbit", "az", "el", "rad"), &N64::set_orbit);
	ClassDB::bind_method(D_METHOD("move_6dof", "dx", "dy", "dz"), &N64::move_6dof);
}

void N64Checkpoint::_bind_methods() {}
void N64CameraVolume::_bind_methods() {}
void N64Follow::_bind_methods() {}
void N64Path::_bind_methods() {}
void N64Waypoint::_bind_methods() {}
void N64NavRegion::_bind_methods() {}
void N64Particles::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mode", "mode"), &N64Particles::set_mode);
	ClassDB::bind_method(D_METHOD("get_mode"), &N64Particles::get_mode);
	ClassDB::bind_method(D_METHOD("set_rate", "rate"), &N64Particles::set_rate);
	ClassDB::bind_method(D_METHOD("get_rate"), &N64Particles::get_rate);
	ClassDB::bind_method(D_METHOD("set_life", "life"), &N64Particles::set_life);
	ClassDB::bind_method(D_METHOD("get_life"), &N64Particles::get_life);
	ClassDB::bind_method(D_METHOD("set_speed", "speed"), &N64Particles::set_speed);
	ClassDB::bind_method(D_METHOD("get_speed"), &N64Particles::get_speed);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mode"), "set_mode", "get_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rate"), "set_rate", "get_rate");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "life"), "set_life", "get_life");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed"), "set_speed", "get_speed");
}
void N64Vehicle::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mode", "mode"), &N64Vehicle::set_mode);
	ClassDB::bind_method(D_METHOD("get_mode"), &N64Vehicle::get_mode);
	ClassDB::bind_method(D_METHOD("set_speed", "speed"), &N64Vehicle::set_speed);
	ClassDB::bind_method(D_METHOD("get_speed"), &N64Vehicle::get_speed);
	ClassDB::bind_method(D_METHOD("set_turn", "turn"), &N64Vehicle::set_turn);
	ClassDB::bind_method(D_METHOD("get_turn"), &N64Vehicle::get_turn);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mode"), "set_mode", "get_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed"), "set_speed", "get_speed");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "turn"), "set_turn", "get_turn");
}
void N64Tween::_bind_methods() {}
void N64Spawner::_bind_methods() {}
void N64LoadZone::_bind_methods() {}
void N64UnloadZone::_bind_methods() {}

void N64FlavorEditorPlugin::_refresh() {
	const bool enabled = ProjectSettings::get_singleton() && bool(ProjectSettings::get_singleton()->get("n64/enabled"));
	if (!status) {
		return;
	}
	last_enabled = enabled;
	if (!enabled) {
		status->set_text("Turn on Project Settings → n64/enabled for the N64 flavor dock.\nN64.* is N64 export only; use OS.has_feature(\"n64\").\nExport writes a .z64 via blazium-toolchain (no Godot templates, no ISO).");
		return;
	}
	status->set_text("N64 v1: rdpq 320×240 (640×480 is P8 optional), 8 MiB Expansion Pak (4 MiB no-pak is P8 optional; default stays 8 MiB). Camera2D cooks CAMN ortho; SPRT/TILE draw textured. N64Follow/Path/Waypoint/NavRegion/Particles/Vehicle/Tween/Spawner/LoadZone nodes. PART modes billboard/flipbook/trail/point/stream. Arcade vehicle (speed/turn/hover/space), not Godot RigidBody. Path/navmesh/tween; extra-pack load/unload vs 2 MiB RDRAM (1 MiB if --rdram 4). Skeletal skinning and Godot Controls fail cook. DragonFS rom://, 4 KiB EEPROM user:// stores Godot marshalls bytes. Multi-scene is pack 0 plus rom:// packs 1–23.\nN64.* is N64 export only; use OS.has_feature(\"n64\").\nProduct is .z64 (80 37 12 40). Cook (NTEX/MESH/NODE/CAMN/HITN/ANIM/PRTN/PTHN/NAVM/VEHN).\nValidators: Ares and Project64. No PIF dump. Never C:\\ultra.");
}

void N64FlavorEditorPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_PROCESS && dock) {
		const bool enabled = ProjectSettings::get_singleton() && bool(ProjectSettings::get_singleton()->get("n64/enabled"));
		if (enabled != last_enabled) {
			_refresh();
		}
	}
}

N64FlavorEditorPlugin::N64FlavorEditorPlugin() {
	const bool headless = DisplayServer::get_singleton() && String(DisplayServer::get_singleton()->get_name()) == "headless";
	if (!headless) {
		dock = memnew(VBoxContainer);
		status = memnew(Label);
		status->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
		dock->add_child(status);
		add_control_to_dock(DOCK_SLOT_RIGHT_UL, dock);
		set_process(true);
	}
	if (!headless) {
		_refresh();
	}
}

N64FlavorEditorPlugin::~N64FlavorEditorPlugin() {
	if (dock) {
		remove_control_from_docks(dock);
		dock = nullptr;
	}
}
