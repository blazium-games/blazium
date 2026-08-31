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
	ClassDB::bind_method(D_METHOD("godot_save_bytes", "value"), &N64::godot_save_bytes);
	ClassDB::bind_method(D_METHOD("godot_load_bytes", "bytes"), &N64::godot_load_bytes);
}

void N64Checkpoint::_bind_methods() {}

void N64FlavorEditorPlugin::_sync_singleton(bool p_enabled) {
	Engine *eng = Engine::get_singleton();
	if (!eng) {
		return;
	}
	if (p_enabled) {
		if (!n64_stub) {
			n64_stub = memnew(N64);
		}
		if (!eng->has_singleton("N64")) {
			eng->add_singleton(Engine::Singleton("N64", n64_stub));
		}
		return;
	}
	if (eng->has_singleton("N64")) {
		eng->remove_singleton("N64");
	}
}

void N64FlavorEditorPlugin::_refresh() {
	const bool enabled = ProjectSettings::get_singleton() && bool(ProjectSettings::get_singleton()->get("n64/enabled"));
	if (enabled) {
		_sync_singleton(true);
	}
	if (!status) {
		return;
	}
	last_enabled = enabled;
	if (!enabled) {
		status->set_text("Turn on Project Settings → n64/enabled for the N64 flavor dock.\nN64.* is N64 export only; use OS.has_feature(\"n64\").\nExport writes a .z64 via blazium-toolchain (no Godot templates, no ISO).");
		return;
	}
	status->set_text("N64 v1: rdpq 320×240 (640×480 is P8 optional), 8 MiB Expansion Pak, DragonFS rom://, EEPROM user://.\nN64.* is N64 export only; use OS.has_feature(\"n64\").\nProduct is .z64 (80 37 12 40). Cook (NTEX/MESH/NODE) is P3.\nValidators: Ares and Project64. No PIF dump. Never C:\\ultra.");
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
	_sync_singleton(true);
	if (!headless) {
		_refresh();
	}
}

N64FlavorEditorPlugin::~N64FlavorEditorPlugin() {
	n64_stub = nullptr;
	if (dock) {
		remove_control_from_docks(dock);
		dock = nullptr;
	}
}
