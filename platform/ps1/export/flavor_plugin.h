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
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/box_container.h"
#include "scene/gui/label.h"

class PS1 : public Object {
	GDCLASS(PS1, Object);

	static void _fail_ps1() {
		ERR_PRINT("PS1.* only works on a PS1 export. Guard with OS.has_feature(\"ps1\").");
	}

protected:
	static void _bind_methods();

public:
	bool load_scene(const String &p_path) {
		_fail_ps1();
		return false;
	}
	bool unload_scene(const String &p_path) {
		_fail_ps1();
		return false;
	}
	bool is_scene_loaded(const String &p_path) const {
		_fail_ps1();
		return false;
	}
	int get_loaded_scene_count() const {
		_fail_ps1();
		return 0;
	}
	String get_loaded_scene(int p_index) const {
		_fail_ps1();
		return String();
	}
	int get_static_memory_usage() const {
		_fail_ps1();
		return 0;
	}
	int get_static_memory_peak_usage() const {
		_fail_ps1();
		return 0;
	}
	int get_ram_budget() const {
		_fail_ps1();
		return 0;
	}
	int get_free_ram() const {
		_fail_ps1();
		return 0;
	}
	bool can_load_scene(const String &p_path) const {
		_fail_ps1();
		return false;
	}
	bool can_instantiate(const Variant &p_path_or_pack) const {
		_fail_ps1();
		return false;
	}
	int get_node_count() const {
		_fail_ps1();
		return 0;
	}
	int get_node_budget() const {
		_fail_ps1();
		return 0;
	}
	int get_triangle_count() const {
		_fail_ps1();
		return 0;
	}
	int get_triangle_budget() const {
		_fail_ps1();
		return 0;
	}
	int get_free_nodes() const {
		_fail_ps1();
		return 0;
	}
	int get_free_hud() const {
		_fail_ps1();
		return 0;
	}
	int get_free_tiles() const {
		_fail_ps1();
		return 0;
	}
};

class PS1FlavorEditorPlugin : public EditorPlugin {
	GDCLASS(PS1FlavorEditorPlugin, EditorPlugin);

	VBoxContainer *dock = nullptr;
	Label *status = nullptr;
	Label *warn = nullptr;
	PS1 *ps1_stub = nullptr;
	ObjectID last_root;
	int last_nodes = -1;
	bool last_enabled = false;

	void _refresh();
	void _sync_singleton(bool p_enabled);

protected:
	static void _bind_methods() {}
	void _notification(int p_what);

public:
	virtual String get_plugin_name() const override { return "PS1"; }
	virtual bool has_main_screen() const override { return false; }

	PS1FlavorEditorPlugin();
	~PS1FlavorEditorPlugin();
};
