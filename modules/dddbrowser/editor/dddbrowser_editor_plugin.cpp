/**************************************************************************/
/*  dddbrowser_editor_plugin.cpp                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#ifdef TOOLS_ENABLED

#include "dddbrowser_editor_plugin.h"

#include "../ddd_luau_check.h"
#include "../dddbrowser_level.h"
#include "../dddbrowser_portal.h"
#include "../dddbrowser_script.h"
#include "../dddbrowser_spawn.h"
#include "../dddbrowser_volume.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/os/os.h"
#include "editor/editor_data.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/gui/editor_file_dialog.h"
#include "scene/gui/popup_menu.h"
#include "scene/main/window.h"
#include "scene/resources/packed_scene.h"
#include "servers/display_server.h"

void DDDBrowserFilesystemContextPlugin::set_callbacks(const Callable &p_export, const Callable &p_test, const Callable &p_create, const Callable &p_check_luau) {
	export_cb = p_export;
	test_cb = p_test;
	create_cb = p_create;
	check_luau_cb = p_check_luau;
}

void DDDBrowserFilesystemContextPlugin::get_options(const Vector<String> &p_paths) {
	bool has_tscn = false;
	bool has_luau = false;
	for (int i = 0; i < p_paths.size(); i++) {
		if (p_paths[i].ends_with(".tscn") || p_paths[i].ends_with(".scn")) {
			has_tscn = true;
		}
		if (p_paths[i].ends_with(".luau")) {
			has_luau = true;
		}
	}
	if (has_tscn) {
		add_context_menu_item(TTR("Export as DDDBrowser Page"), export_cb, Ref<Texture2D>());
		add_context_menu_item(TTR("Test DDDBrowser Page (HTTPServer)"), test_cb, Ref<Texture2D>());
	}
	if (has_luau && check_luau_cb.is_valid()) {
		add_context_menu_item(TTR("Check DDD Luau"), check_luau_cb, Ref<Texture2D>());
	}
	if (create_cb.is_valid()) {
		add_context_menu_item(TTR("New DDDBrowser Level"), create_cb, Ref<Texture2D>());
	}
}

void DDDBrowserSceneTreeContextPlugin::set_callbacks(const Callable &p_export, const Callable &p_test, const Callable &p_check_luau) {
	export_cb = p_export;
	test_cb = p_test;
	check_luau_cb = p_check_luau;
}

void DDDBrowserSceneTreeContextPlugin::get_options(const Vector<String> &p_paths) {
	add_context_menu_item(TTR("Export as DDDBrowser Page"), export_cb, Ref<Texture2D>());
	add_context_menu_item(TTR("Test DDDBrowser Page (HTTPServer)"), test_cb, Ref<Texture2D>());
	if (check_luau_cb.is_valid()) {
		add_context_menu_item(TTR("Check DDD Luau"), check_luau_cb, Ref<Texture2D>());
	}
}

void DDDBrowserEditorPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_export_paths", "paths"), &DDDBrowserEditorPlugin::_export_paths);
	ClassDB::bind_method(D_METHOD("_test_paths", "paths"), &DDDBrowserEditorPlugin::_test_paths);
	ClassDB::bind_method(D_METHOD("_create_level_paths", "paths"), &DDDBrowserEditorPlugin::_create_level_paths);
	ClassDB::bind_method(D_METHOD("_check_luau_paths", "paths"), &DDDBrowserEditorPlugin::_check_luau_paths);
	ClassDB::bind_method(D_METHOD("_export_to_path", "path"), &DDDBrowserEditorPlugin::_export_to_path);
}

String DDDBrowserEditorPlugin::get_plugin_name() const {
	return "DDDBrowser";
}

String DDDBrowserEditorPlugin::_default_export_dir(const String &p_scene_path) const {
	String base = ProjectSettings::get_singleton()->globalize_path("user://dddbrowser_preview");
	String name = p_scene_path.get_file().get_basename();
	if (name.is_empty()) {
		name = "scene";
	}
	return base.path_join(name);
}

Node *DDDBrowserEditorPlugin::_load_scene_root(const String &p_path) {
	if (p_path.is_empty()) {
		return EditorNode::get_singleton()->get_edited_scene();
	}
	Ref<PackedScene> ps = ResourceLoader::load(p_path);
	if (ps.is_null()) {
		return nullptr;
	}
	return ps->instantiate();
}

void DDDBrowserEditorPlugin::_popup_export_dialog() {
	Node *root = EditorNode::get_singleton()->get_edited_scene();
	if (!root) {
		EditorNode::get_singleton()->show_accept(TTR("This operation can't be done without a scene."), TTR("OK"));
		return;
	}
	String filename = root->get_scene_file_path().get_file().get_basename();
	if (filename.is_empty()) {
		filename = root->get_name();
	}
	export_dialog->set_current_dir(ProjectSettings::get_singleton()->globalize_path("res://"));
	export_dialog->set_current_file(filename);
	export_dialog->popup_file_dialog();
}

void DDDBrowserEditorPlugin::_export_to_path(const String &p_path) {
	Node *root = EditorNode::get_singleton()->get_edited_scene();
	if (!root) {
		return;
	}
	Error err = exporter->export_scene(root, p_path, true);
	if (err != OK) {
		EditorNode::get_singleton()->show_accept(vformat(TTR("DDDBrowser export failed: %s"), exporter->get_last_error()), TTR("OK"));
		return;
	}
	EditorNode::get_singleton()->show_accept(vformat(TTR("Exported DDDBrowser page to:\n%s"), p_path), TTR("OK"));
}

void DDDBrowserEditorPlugin::_export_paths(const Variant &p_paths) {
	Vector<String> paths = p_paths;
	if (paths.is_empty()) {
		_popup_export_dialog();
		return;
	}
	String scene_path = paths[0];
	Node *root = _load_scene_root(scene_path);
	if (!root) {
		root = EditorNode::get_singleton()->get_edited_scene();
	}
	if (!root) {
		EditorNode::get_singleton()->show_accept(TTR("Could not load scene for DDDBrowser export."), TTR("OK"));
		return;
	}
	String out_dir = _default_export_dir(scene_path.is_empty() ? root->get_scene_file_path() : scene_path);
	Error err = exporter->export_scene(root, out_dir, true);
	bool owned = root != EditorNode::get_singleton()->get_edited_scene();
	if (owned) {
		root->queue_free();
	}
	if (err != OK) {
		EditorNode::get_singleton()->show_accept(vformat(TTR("DDDBrowser export failed: %s"), exporter->get_last_error()), TTR("OK"));
		return;
	}
	EditorNode::get_singleton()->show_accept(vformat(TTR("Exported DDDBrowser page to:\n%s"), out_dir), TTR("OK"));
}

void DDDBrowserEditorPlugin::_test_scene_root(Node *p_root) {
	ERR_FAIL_NULL(p_root);
	String out_dir = _default_export_dir(p_root->get_scene_file_path());
	Error err = exporter->export_scene(p_root, out_dir, true);
	if (err != OK) {
		EditorNode::get_singleton()->show_accept(vformat(TTR("DDDBrowser export failed: %s"), exporter->get_last_error()), TTR("OK"));
		return;
	}
	err = preview_server->start(out_dir, 8081);
	if (err != OK) {
		EditorNode::get_singleton()->show_accept(TTR("Failed to start HTTPServer preview on port 8081."), TTR("OK"));
		return;
	}
	String url = preview_server->get_index_url();
	DisplayServer::get_singleton()->clipboard_set(url);
	String msg = vformat(TTR("Preview server running.\nOpen in DDDBrowser (Allow HTTP):\n%s\n\nURL copied to clipboard."), url);
	String exe = EDITOR_GET("dddbrowser/executable_path");
	if (!exe.is_empty() && FileAccess::exists(exe)) {
		List<String> args;
		args.push_back(url);
		OS::get_singleton()->create_process(exe, args);
		msg += TTR("\nLaunched DDDBrowser executable.");
	}
	EditorNode::get_singleton()->show_accept(msg, TTR("OK"));
}

void DDDBrowserEditorPlugin::_test_paths(const Variant &p_paths) {
	Vector<String> paths = p_paths;
	Node *root = nullptr;
	bool owned = false;
	if (!paths.is_empty() && (paths[0].ends_with(".tscn") || paths[0].ends_with(".scn"))) {
		root = _load_scene_root(paths[0]);
		owned = root != nullptr;
	}
	if (!root) {
		root = EditorNode::get_singleton()->get_edited_scene();
	}
	if (!root) {
		EditorNode::get_singleton()->show_accept(TTR("Could not load scene for DDDBrowser test."), TTR("OK"));
		return;
	}
	_test_scene_root(root);
	if (owned) {
		root->queue_free();
	}
}

Error DDDBrowserEditorPlugin::_write_template_file(const String &p_res_path, const String &p_contents) const {
	String abs = ProjectSettings::get_singleton()->globalize_path(p_res_path);
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(abs.get_base_dir());
	Ref<FileAccess> f = FileAccess::open(abs, FileAccess::WRITE);
	ERR_FAIL_COND_V(f.is_null(), ERR_CANT_CREATE);
	f->store_string(p_contents);
	return OK;
}

void DDDBrowserEditorPlugin::_check_luau_paths(const Variant &p_paths) {
	Vector<String> paths = p_paths;
	String script_path;
	for (int i = 0; i < paths.size(); i++) {
		if (paths[i].ends_with(".luau")) {
			script_path = paths[i];
			break;
		}
	}
	if (script_path.is_empty()) {
		List<Node *> selection = EditorNode::get_singleton()->get_editor_selection()->get_selected_node_list();
		for (Node *n : selection) {
			if (DDDBrowserScript *script_node = Object::cast_to<DDDBrowserScript>(n)) {
				script_path = script_node->get_source_path();
				break;
			}
		}
	}
	if (script_path.is_empty()) {
		EditorNode::get_singleton()->show_accept(TTR("Select a .luau file or DDDBrowserScript node to check."), TTR("OK"));
		return;
	}
	Dictionary result = DDDLuauCheck::check_file(script_path);
	const bool ok = result.get("ok", false);
	const String message = result.get("message", "");
	EditorNode::get_singleton()->show_accept(ok ? message : vformat(TTR("DDD Luau check failed:\n%s"), message), TTR("OK"));
}

void DDDBrowserEditorPlugin::_create_level_paths(const Variant &p_paths) {
	Vector<String> paths = p_paths;
	String dir = "res://";
	if (!paths.is_empty()) {
		String p = paths[0];
		String abs = ProjectSettings::get_singleton()->globalize_path(p);
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		if (da.is_valid() && da->dir_exists(abs)) {
			dir = p;
		} else {
			dir = p.get_base_dir();
		}
	}
	if (!dir.ends_with("/")) {
		dir += "/";
	}
	String scripts_dir = dir.path_join("dddbrowser_scripts");
	String entity_script_res = scripts_dir.path_join("entity_template.luau");
	String gamemode_script_res = scripts_dir.path_join("gamemode_template.luau");
	_write_template_file(entity_script_res, DDDLuauCheck::entity_template());
	_write_template_file(gamemode_script_res, DDDLuauCheck::gamemode_template());

	String save_path = dir.path_join("dddbrowser_level.tscn");

	DDDBrowserLevel *level = memnew(DDDBrowserLevel);
	level->set_name("DDDBrowserLevel");
	level->set_scene_name("New DDDBrowser Level");
	level->set_gamemode_file(gamemode_script_res);
	DDDBrowserSpawn *spawn = memnew(DDDBrowserSpawn);
	spawn->set_name("Spawn");
	spawn->set_position(Vector3(0, 1.6, 4));
	level->add_child(spawn);
	spawn->set_owner(level);

	DDDBrowserPortal *portal = memnew(DDDBrowserPortal);
	portal->set_name("Portal");
	portal->set_position(Vector3(3, 1, 0));
	portal->set_destination_url("https://example.com/");
	level->add_child(portal);
	portal->set_owner(level);

	DDDBrowserVolume *volume = memnew(DDDBrowserVolume);
	volume->set_name("TriggerVolume");
	volume->set_position(Vector3(-3, 1, 0));
	volume->set_volume_type(DDDBrowserVolume::VOLUME_SINGLE_TRIGGER);
	volume->set_event_name("on_enter");
	level->add_child(volume);
	volume->set_owner(level);

	DDDBrowserScript *script_asset = memnew(DDDBrowserScript);
	script_asset->set_name("EntityScript");
	script_asset->set_asset_id("entity_template");
	script_asset->set_source_path(entity_script_res);
	level->add_child(script_asset);
	script_asset->set_owner(level);

	Ref<PackedScene> ps;
	ps.instantiate();
	ps->pack(level);
	Error err = ResourceSaver::save(ps, save_path);
	memdelete(level);
	if (err != OK) {
		EditorNode::get_singleton()->show_accept(TTR("Failed to create DDDBrowser level scene."), TTR("OK"));
		return;
	}
	EditorNode::get_singleton()->load_scene(save_path);
}

DDDBrowserEditorPlugin::DDDBrowserEditorPlugin() {
	exporter.instantiate();
	preview_server.instantiate();

	if (!EditorSettings::get_singleton()->has_setting("dddbrowser/executable_path")) {
		EditorSettings::get_singleton()->set_setting("dddbrowser/executable_path", "");
		EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "dddbrowser/executable_path", PROPERTY_HINT_GLOBAL_FILE, "*.exe"));
	}

	export_dialog = memnew(EditorFileDialog);
	export_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_DIR);
	export_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	export_dialog->set_title(TTR("Export DDDBrowser Page"));
	export_dialog->connect("dir_selected", callable_mp(this, &DDDBrowserEditorPlugin::_export_to_path));
	EditorNode::get_singleton()->get_gui_base()->add_child(export_dialog);

	PopupMenu *menu = get_export_as_menu();
	int idx = menu->get_item_count();
	menu->add_item(TTR("DDDBrowser Page..."));
	menu->set_item_metadata(idx, callable_mp(this, &DDDBrowserEditorPlugin::_popup_export_dialog));

	fs_plugin.instantiate();
	fs_plugin->set_callbacks(
			callable_mp(this, &DDDBrowserEditorPlugin::_export_paths),
			callable_mp(this, &DDDBrowserEditorPlugin::_test_paths),
			Callable(),
			callable_mp(this, &DDDBrowserEditorPlugin::_check_luau_paths));
	add_context_menu_plugin(EditorContextMenuPlugin::CONTEXT_SLOT_FILESYSTEM, fs_plugin);

	fs_create_plugin.instantiate();
	fs_create_plugin->set_callbacks(Callable(), Callable(), callable_mp(this, &DDDBrowserEditorPlugin::_create_level_paths));
	add_context_menu_plugin(EditorContextMenuPlugin::CONTEXT_SLOT_FILESYSTEM_CREATE, fs_create_plugin);

	tree_plugin.instantiate();
	tree_plugin->set_callbacks(
			callable_mp(this, &DDDBrowserEditorPlugin::_export_paths),
			callable_mp(this, &DDDBrowserEditorPlugin::_test_paths),
			callable_mp(this, &DDDBrowserEditorPlugin::_check_luau_paths));
	add_context_menu_plugin(EditorContextMenuPlugin::CONTEXT_SLOT_SCENE_TREE, tree_plugin);
}

DDDBrowserEditorPlugin::~DDDBrowserEditorPlugin() {
	if (preview_server.is_valid()) {
		preview_server->stop();
	}
	remove_context_menu_plugin(fs_plugin);
	remove_context_menu_plugin(fs_create_plugin);
	remove_context_menu_plugin(tree_plugin);
}

#endif
