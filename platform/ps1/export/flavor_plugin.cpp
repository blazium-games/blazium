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
#include "core/templates/hash_set.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "scene/2d/sprite_2d.h"
#include "scene/2d/tile_map.h"
#include "scene/2d/tile_map_layer.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/gui/control.h"
#include "scene/resources/mesh.h"
#include "servers/display_server.h"

void PS1::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load_scene", "path"), &PS1::load_scene);
	ClassDB::bind_method(D_METHOD("unload_scene", "path"), &PS1::unload_scene);
	ClassDB::bind_method(D_METHOD("is_scene_loaded", "path"), &PS1::is_scene_loaded);
	ClassDB::bind_method(D_METHOD("get_loaded_scene_count"), &PS1::get_loaded_scene_count);
	ClassDB::bind_method(D_METHOD("get_loaded_scene", "index"), &PS1::get_loaded_scene);
	ClassDB::bind_method(D_METHOD("get_static_memory_usage"), &PS1::get_static_memory_usage);
	ClassDB::bind_method(D_METHOD("get_static_memory_peak_usage"), &PS1::get_static_memory_peak_usage);
	ClassDB::bind_method(D_METHOD("get_ram_budget"), &PS1::get_ram_budget);
	ClassDB::bind_method(D_METHOD("get_free_ram"), &PS1::get_free_ram);
	ClassDB::bind_method(D_METHOD("can_load_scene", "path"), &PS1::can_load_scene);
	ClassDB::bind_method(D_METHOD("can_instantiate", "path_or_pack"), &PS1::can_instantiate);
	ClassDB::bind_method(D_METHOD("get_node_count"), &PS1::get_node_count);
	ClassDB::bind_method(D_METHOD("get_node_budget"), &PS1::get_node_budget);
	ClassDB::bind_method(D_METHOD("get_triangle_count"), &PS1::get_triangle_count);
	ClassDB::bind_method(D_METHOD("get_triangle_budget"), &PS1::get_triangle_budget);
	ClassDB::bind_method(D_METHOD("get_free_nodes"), &PS1::get_free_nodes);
	ClassDB::bind_method(D_METHOD("get_free_hud"), &PS1::get_free_hud);
	ClassDB::bind_method(D_METHOD("get_free_tiles"), &PS1::get_free_tiles);
}

static void _count_scene(Node *p_node, int &r_nodes, int &r_tris, int &r_sprites, int &r_hud, int &r_tiles) {
	if (!p_node) {
		return;
	}
	r_nodes++;
	if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(p_node)) {
		const Ref<Mesh> mesh = mi->get_mesh();
		if (mesh.is_valid()) {
			r_tris += mesh->get_faces().size();
		}
	}
	if (Object::cast_to<Sprite2D>(p_node)) {
		r_sprites++;
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
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_count_scene(p_node->get_child(i), r_nodes, r_tris, r_sprites, r_hud, r_tiles);
	}
}

static int _count_literal_packs() {
	int n = 1;
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
			if (!(name.ends_with(".gd") || name.ends_with(".luau"))) {
				continue;
			}
			if (name == "run_tests.gd" || name.begins_with("test_")) {
				continue;
			}
			const String src = FileAccess::get_file_as_string(path);
			Vector<String> scenes;
			ps1_cook::collect_literal_scenes(src, scenes);
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

void PS1FlavorEditorPlugin::_sync_singleton(bool p_enabled) {
	Engine *eng = Engine::get_singleton();
	if (!eng) {
		return;
	}
	if (p_enabled) {
		if (!ps1_stub) {
			ps1_stub = memnew(PS1);
		}
		if (!eng->has_singleton("PS1")) {
			eng->add_singleton(Engine::Singleton("PS1", ps1_stub));
		}
	} else if (eng->has_singleton("PS1")) {
		eng->remove_singleton("PS1");
	}
}

void PS1FlavorEditorPlugin::_refresh() {
	if (!status || !warn) {
		return;
	}
	const bool enabled = ProjectSettings::get_singleton() && bool(ProjectSettings::get_singleton()->get("ps1/enabled"));
	_sync_singleton(enabled);
	last_enabled = enabled;
	if (!enabled) {
		status->set_text("Turn on Project Settings → ps1/enabled for live PS1 budget warnings.\nPS1.* is PS1 export only; use OS.has_feature(\"ps1\").");
		warn->set_text("");
		return;
	}
	Node *root = EditorInterface::get_singleton() ? EditorInterface::get_singleton()->get_edited_scene_root() : nullptr;
	int nodes = 0, tris = 0, sprites = 0, hud = 0, tiles = 0;
	_count_scene(root, nodes, tris, sprites, hud, tiles);
	const int packs = _count_literal_packs();
	const int est_ram = nodes * 56 + tris * 32 + hud * 80 + tiles * 8;
	status->set_text(vformat(
			"nodes %d/%d\ntris %d/%d\nHUD %d/%d\ntiles %d/%d\nSprite2D %d/%d\npacks %d/%d\nest RAM %d/%d\nPS1.* is PS1 export only; use OS.has_feature(\"ps1\").\n24 packs; multi-resident load/unload; budgeted instantiate; first-slice VM.",
			nodes, ps1_cook::kMaxNodes, tris, ps1_cook::kMaxTris, hud, ps1_cook::kMaxHud, tiles, ps1_cook::kMaxTiles,
			sprites, ps1_cook::kMaxSprites, packs, ps1_cook::kMaxScenePacks, est_ram, 2 * 1024 * 1024));
	PackedStringArray reasons;
	if (nodes > ps1_cook::kMaxNodes) {
		reasons.push_back(vformat("too many nodes (%d/%d)", nodes, ps1_cook::kMaxNodes));
	}
	if (tris > ps1_cook::kMaxTris) {
		reasons.push_back(vformat("too many tris (%d/%d)", tris, ps1_cook::kMaxTris));
	}
	if (hud > ps1_cook::kMaxHud) {
		reasons.push_back(vformat("too many HUD widgets (%d/%d)", hud, ps1_cook::kMaxHud));
	}
	if (tiles > ps1_cook::kMaxTiles) {
		reasons.push_back(vformat("too many tiles (%d/%d)", tiles, ps1_cook::kMaxTiles));
	}
	if (sprites > ps1_cook::kMaxSprites) {
		reasons.push_back(vformat("too many Sprite2D (%d/%d)", sprites, ps1_cook::kMaxSprites));
	}
	if (packs > ps1_cook::kMaxScenePacks) {
		reasons.push_back(vformat("too many packs (%d/%d)", packs, ps1_cook::kMaxScenePacks));
	}
	if (est_ram > 2 * 1024 * 1024) {
		reasons.push_back("est RAM blob exceeds 2 MiB");
	}
	if (root && nodes == 0) {
		reasons.push_back("empty scene");
	}
	if (reasons.is_empty()) {
		warn->set_text("");
	} else {
		const String msg = "PS1 budget: " + String(", ").join(reasons);
		warn->set_text(msg);
		WARN_PRINT(msg);
	}
}

void PS1FlavorEditorPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_PROCESS && dock) {
		Node *root = EditorInterface::get_singleton() ? EditorInterface::get_singleton()->get_edited_scene_root() : nullptr;
		const ObjectID id = root ? root->get_instance_id() : ObjectID();
		int nodes = 0, tris = 0, sprites = 0, hud = 0, tiles = 0;
		_count_scene(root, nodes, tris, sprites, hud, tiles);
		const bool enabled = ProjectSettings::get_singleton() && bool(ProjectSettings::get_singleton()->get("ps1/enabled"));
		if (id != last_root || nodes != last_nodes || enabled != last_enabled) {
			last_root = id;
			last_nodes = nodes;
			_refresh();
		}
	}
}

PS1FlavorEditorPlugin::PS1FlavorEditorPlugin() {
	const bool headless = DisplayServer::get_singleton() && String(DisplayServer::get_singleton()->get_name()) == "headless";
	if (headless) {
		return;
	}
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
	_refresh();
}

PS1FlavorEditorPlugin::~PS1FlavorEditorPlugin() {
	_sync_singleton(false);
	if (ps1_stub) {
		memdelete(ps1_stub);
		ps1_stub = nullptr;
	}
	if (dock) {
		remove_control_from_docks(dock);
		dock = nullptr;
	}
}
