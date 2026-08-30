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
#include "core/object/object.h"
#include "core/string/ustring.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/box_container.h"
#include "scene/gui/label.h"

class PS2 : public Object {
	GDCLASS(PS2, Object);

	static void _fail_ps2() {
		ERR_PRINT("PS2.* only works on a PS2 export. Guard with OS.has_feature(\"ps2\").");
	}

protected:
	static void _bind_methods();

public:
	int get_abi() const {
		_fail_ps2();
		return 0;
	}
	bool is_ready() const {
		_fail_ps2();
		return false;
	}
	bool set_rumble(int p_device, int p_small, int p_large) {
		_fail_ps2();
		return false;
	}
	bool stop_rumble(int p_device) {
		_fail_ps2();
		return false;
	}
	int get_pressure(int p_device, int p_button) const {
		_fail_ps2();
		return 0;
	}
	bool play_sfx(const String &p_name) {
		_fail_ps2();
		(void)p_name;
		return false;
	}
	bool stop_sfx() {
		_fail_ps2();
		return false;
	}
	bool swap_pack(const String &p_unload, const String &p_load) {
		_fail_ps2();
		(void)p_unload;
		(void)p_load;
		return false;
	}
	bool load_scene(const String &p_path) {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	bool unload_scene(const String &p_path) {
		_fail_ps2();
		(void)p_path;
		return false;
	}
	int get_display_width() const {
		_fail_ps2();
		return 0;
	}
	int get_display_height() const {
		_fail_ps2();
		return 0;
	}
};

class PS2FlavorEditorPlugin : public EditorPlugin {
	GDCLASS(PS2FlavorEditorPlugin, EditorPlugin);

	VBoxContainer *dock = nullptr;
	Label *status = nullptr;
	PS2 *ps2_stub = nullptr;
	bool last_enabled = false;

	void _refresh();
	void _sync_singleton();

protected:
	static void _bind_methods() {}
	void _notification(int p_what);

public:
	virtual String get_plugin_name() const override { return "PS2"; }
	virtual bool has_main_screen() const override { return false; }

	PS2FlavorEditorPlugin();
	~PS2FlavorEditorPlugin();
};
