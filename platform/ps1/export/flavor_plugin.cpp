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

#include "core/input/shortcut.h"
#include "servers/display_server.h"
#include "scene/gui/label.h"

PS1FlavorEditorPlugin::PS1FlavorEditorPlugin() {
	const bool headless = DisplayServer::get_singleton() && String(DisplayServer::get_singleton()->get_name()) == "headless";
	if (headless) {
		return;
	}
	dock = memnew(VBoxContainer);
	const char *lines[] = {
		"Blazium PS1 flavor (same exporter)",
		"Slots: 16 TIM, 8192 tris, 3 lights, 16 Sprite2D, 128 nodes",
		"32 HUD widgets (pad-focus; poll pressed/value, no signals)",
		"256 TileMap tiles; first-slice VM (no official Luau, no skinning)",
		"8 packs; change_scene isolates scripts + sprites + stops anim",
		"AnimationPlayer xform clips; PAL 50 Hz / NTSC 60",
		"VAG loop/stop + replayable FMV (play() on VideoStreamPlayer)",
		"FMV: in-editor MIT STR/XA (not psxpress)",
		"Not DuckStation / Sony BIOS / hardware",
	};
	for (const char *t : lines) {
		Label *l = memnew(Label);
		l->set_text(t);
		dock->add_child(l);
	}
	add_control_to_dock(DOCK_SLOT_RIGHT_UL, dock);
}

PS1FlavorEditorPlugin::~PS1FlavorEditorPlugin() {
	if (dock) {
		remove_control_from_docks(dock);
		dock = nullptr;
	}
}
