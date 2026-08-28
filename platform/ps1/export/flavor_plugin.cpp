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
		"Affine UVs — warp is expected",
		"Guest: SPRITE HUD, AABB, 3 lights, fog",
		"FMV: blazium-toolchain ps1 fmv (MDEC/psxpress)",
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
