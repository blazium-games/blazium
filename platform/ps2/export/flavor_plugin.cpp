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
	status->set_text("PS2 export is on. Project → Export → PS2 cooks ABI 1 and writes Game.elf via blazium-toolchain.\nGS mesh, scripts, pad, and CI gates are in. BIOS is only required for One-click.\nPS2.* (rumble/pressure) only works on a PS2 export. Guard with OS.has_feature(\"ps2\").");
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
