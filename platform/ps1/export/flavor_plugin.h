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
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/box_container.h"

class PS1FlavorEditorPlugin : public EditorPlugin {
	GDCLASS(PS1FlavorEditorPlugin, EditorPlugin);

	VBoxContainer *dock = nullptr;

protected:
	static void _bind_methods() {}

public:
	virtual String get_plugin_name() const override { return "PS1"; }
	virtual bool has_main_screen() const override { return false; }

	PS1FlavorEditorPlugin();
	~PS1FlavorEditorPlugin();
};
