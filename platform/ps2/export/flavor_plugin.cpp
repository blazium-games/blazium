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

#include "core/config/engine.h"
#include "core/config/project_settings.h"
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
}

void PS2FlavorEditorPlugin::_sync_singleton() {
	Engine *eng = Engine::get_singleton();
	if (!eng) {
		return;
	}
	if (!ps2_stub) {
		ps2_stub = memnew(PS2);
	}
	if (!eng->has_singleton("PS2")) {
		eng->add_singleton(Engine::Singleton("PS2", ps2_stub));
	}
}

void PS2FlavorEditorPlugin::_refresh() {
	if (!status) {
		return;
	}
	const bool enabled = ProjectSettings::get_singleton() && bool(ProjectSettings::get_singleton()->get("ps2/enabled"));
	_sync_singleton();
	last_enabled = enabled;
	if (!enabled) {
		status->set_text("Turn on Project Settings → ps2/enabled for the PS2 flavor dock.\nPS2.* is PS2 export only; use OS.has_feature(\"ps2\").\nExport writes an EE ELF via blazium-toolchain (no Godot templates).");
		return;
	}
	status->set_text("PS2 export is on. Project → Export → PS2 cooks ABI 1 and writes Game.elf via blazium-toolchain.\nHUD/TILE/NAV/CAM/HIT/ANIM cook as ABI 1 sidecars. FMV/IPU is a named no-op. BIOS is only for One-click.\nPS2.* only works on a PS2 export. Guard with OS.has_feature(\"ps2\").");
}

void PS2FlavorEditorPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_PROCESS && dock) {
		const bool enabled = ProjectSettings::get_singleton() && bool(ProjectSettings::get_singleton()->get("ps2/enabled"));
		if (enabled != last_enabled) {
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
		add_control_to_dock(DOCK_SLOT_RIGHT_UL, dock);
		set_process(true);
	}
	_sync_singleton();
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
