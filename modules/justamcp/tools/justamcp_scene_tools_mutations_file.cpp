/**************************************************************************/
/*  justamcp_scene_tools_mutations_file.cpp                               */
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

#include "../justamcp_editor_plugin.h" // IWYU pragma: keep
#include "../justamcp_editor_scene_access.h"
#include "justamcp_scene_path_lock.h"
#include "justamcp_scene_tools.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h" // IWYU pragma: keep
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h" // IWYU pragma: keep
#include "core/object/class_db.h"
#include "core/object/message_queue.h"
#include "core/object/script_language.h"
#include "core/os/mutex.h" // IWYU pragma: keep
#include "core/os/os.h" // IWYU pragma: keep
#include "core/os/thread.h"
#include "core/templates/hash_map.h" // IWYU pragma: keep
#include "editor/editor_data.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h" // IWYU pragma: keep
#include "editor/file_system/editor_file_system.h"
#include "scene/2d/sprite_2d.h" // IWYU pragma: keep
#include "scene/3d/sprite_3d.h" // IWYU pragma: keep
#include "scene/resources/packed_scene.h"

Dictionary JustAMCPSceneTools::create_scene(const Dictionary &p_args) {
	String project_path = p_args.get("projectPath", "");
	String scene_path = _to_scene_res_path(project_path, p_args.get("scenePath", p_args.get("scene_path", "")));
	String root_node_type = p_args.get("rootNodeType", "Node");
	String script_path = p_args.get("scriptPath", "");

	if (scene_path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Missing scenePath";
		return ret;
	}
	if (!scene_path.ends_with(".tscn")) {
		scene_path += ".tscn";
	}
	if (!ClassDB::class_exists(StringName(root_node_type))) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Invalid rootNodeType: " + root_node_type;
		return ret;
	}

	JustAMCPScenePathLock scene_lock(scene_path);
	if (!scene_lock.is_locked()) {
		return justamcp_scene_lock_busy_response();
	}

	_ensure_parent_dir_for_scene(scene_path);

	Object *_root_obj = ClassDB::instantiate(StringName(root_node_type));
	Node *root = Object::cast_to<Node>(_root_obj);
	if (!root) {
		if (_root_obj) {
			memdelete(_root_obj);
		}
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to instantiate root node: " + root_node_type;
		return ret;
	}
	String root_node_name = p_args.get("rootNodeName", p_args.get("root_node_name", ""));
	if (root_node_name.is_empty()) {
		String file_stem = scene_path.get_file().get_basename();
		if (!file_stem.is_empty()) {
			PackedStringArray parts = file_stem.split("_");
			for (int i = 0; i < parts.size(); i++) {
				String part = parts[i];
				if (part.is_empty()) {
					continue;
				}
				root_node_name += part.substr(0, 1).to_upper() + part.substr(1);
			}
		}
	}
	if (root_node_name.is_empty()) {
		root_node_name = root_node_type;
	}
	root->set_name(root_node_name);

	if (!script_path.is_empty()) {
		String full_script_path = _to_scene_res_path(project_path, script_path);
		Ref<Script> p_script_res = ResourceLoader::load(full_script_path);
		if (p_script_res.is_null()) {
			memdelete(root);
			Dictionary ret;
			ret["ok"] = false;
			ret["error"] = "Failed to load script: " + full_script_path;
			return ret;
		}
		root->set_script(p_script_res);
	}

	Dictionary err = _save_scene(root, scene_path);
	if (!err.is_empty()) {
		return err;
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["scenePath"] = scene_path;
	ret["rootNodeType"] = root_node_type;
	return ret;
}

Dictionary JustAMCPSceneTools::delete_scene_file(const Dictionary &p_args) {
	String project_path = p_args.get("projectPath", "");
	String scene_path = _to_scene_res_path(project_path, p_args.get("scenePath", p_args.get("path", "")));
	if (scene_path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "scenePath is required.";
		return ret;
	}
	const String main_scene = ProjectSettings::get_singleton() ? String(ProjectSettings::get_singleton()->get_setting("application/run/main_scene", "")) : String();
	const String scene_file = scene_path.get_file();
	if ((!main_scene.is_empty() && scene_path == main_scene) || scene_file == "project.godot" || scene_file == "project.blazium" || scene_file == "project.binary" || scene_path == "res://export_presets.cfg") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Refusing to delete protected project path: " + scene_path;
		return ret;
	}
	if (!FileAccess::exists(scene_path)) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Scene not found: " + scene_path;
		return ret;
	}

	JustAMCPScenePathLock scene_lock(scene_path);
	if (!scene_lock.is_locked()) {
		return justamcp_scene_lock_busy_response();
	}

	Error err = DirAccess::remove_absolute(scene_path);
	if (err != OK) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to delete scene: " + itos(err);
		return ret;
	}
	_refresh_filesystem(scene_path);

	Dictionary ret;
	ret["ok"] = true;
	ret["path"] = scene_path;
	ret["deleted"] = true;
	return ret;
}

Dictionary JustAMCPSceneTools::set_current_scene(const Dictionary &p_args) {
	String path = _to_scene_res_path(p_args.get("projectPath", ""), p_args.get("path", p_args.get("scenePath", "")));
	if (path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "path is required.";
		return ret;
	}
	if (!FileAccess::exists(path)) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Scene not found: " + path;
		return ret;
	}
	if (EditorInterface::get_singleton()) {
		EditorInterface::get_singleton()->open_scene_from_path(path);
	}
	Dictionary ret;
	ret["ok"] = true;
	ret["path"] = path;
	return ret;
}

Dictionary JustAMCPSceneTools::reload_scene(const Dictionary &p_args) {
	String path = _to_scene_res_path(p_args.get("projectPath", ""), p_args.get("path", p_args.get("scenePath", "")));
	if (path == "res://") {
		Node *root = JustAMCPEditorSceneAccess::get_edited_root();
		if (root) {
			path = root->get_scene_file_path();
		}
	}
	if (path.is_empty() || path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "No scene to reload.";
		return ret;
	}
	if (EditorInterface::get_singleton()) {
		EditorInterface::get_singleton()->reload_scene_from_path(path);
	}
	Dictionary ret;
	ret["ok"] = true;
	ret["path"] = path;
	return ret;
}

Dictionary JustAMCPSceneTools::duplicate_scene_file(const Dictionary &p_args) {
	String project_path = p_args.get("projectPath", "");
	String source_path = _to_scene_res_path(project_path, p_args.get("source_path", p_args.get("sourcePath", "")));
	String dest_path = _to_scene_res_path(project_path, p_args.get("dest_path", p_args.get("destPath", "")));
	if (source_path == "res://" || dest_path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "source_path and dest_path are required.";
		return ret;
	}
	if (!FileAccess::exists(source_path)) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Source scene not found: " + source_path;
		return ret;
	}

	JustAMCPScenePathLock scene_lock(dest_path);
	if (!scene_lock.is_locked()) {
		return justamcp_scene_lock_busy_response();
	}

	String dir_path = dest_path.get_base_dir();
	if (!DirAccess::dir_exists_absolute(dir_path)) {
		DirAccess::make_dir_recursive_absolute(dir_path);
	}
	Error err = DirAccess::copy_absolute(source_path, dest_path);
	_refresh_filesystem(dest_path);
	_refresh_filesystem(source_path);

	Dictionary ret;
	ret["ok"] = err == OK;
	ret["source"] = source_path;
	ret["destination"] = dest_path;
	if (err != OK) {
		ret["error"] = "Failed to duplicate scene: " + itos(err);
	}
	return ret;
}

Dictionary JustAMCPSceneTools::close_scene(const Dictionary &p_args) {
	Dictionary ret;
	if (!EditorNode::get_singleton()) {
		ret["ok"] = false;
		ret["error"] = "EditorNode unavailable.";
		return ret;
	}

	EditorData &editor_data = EditorNode::get_editor_data();
	const int scene_count = editor_data.get_edited_scene_count();
	if (scene_count <= 0) {
		ret["ok"] = false;
		ret["error"] = "No open scenes to close.";
		return ret;
	}

	String requested = String(p_args.get("path", p_args.get("scenePath", p_args.get("scene_path", ""))));
	String project_path = p_args.get("projectPath", "");
	if (!requested.is_empty()) {
		requested = _to_scene_res_path(project_path, requested);
	}

	int idx = -1;
	if (!requested.is_empty() && requested != "res://") {
		idx = editor_data.get_edited_scene_from_path(requested);
		if (idx < 0) {
			for (int i = 0; i < scene_count; i++) {
				Node *root = editor_data.get_edited_scene_root(i);
				if (root && root->get_scene_file_path() == requested) {
					idx = i;
					break;
				}
			}
		}
		if (idx < 0) {
			ret["ok"] = false;
			ret["error"] = "Scene is not open in the editor: " + requested;
			ret["path"] = requested;
			return ret;
		}
	} else {
		idx = editor_data.get_edited_scene();
		if (idx < 0 || idx >= scene_count) {
			ret["ok"] = false;
			ret["error"] = "No current scene to close.";
			return ret;
		}
	}

	String closed_path = editor_data.get_scene_path(idx);
	Node *root = editor_data.get_edited_scene_root(idx);
	if (closed_path.is_empty() && root) {
		closed_path = root->get_scene_file_path();
	}

	const bool save = bool(p_args.get("save", false));
	if (save && !closed_path.is_empty()) {
		EditorNode::get_singleton()->save_scene_if_open(closed_path);
	}

	editor_data.set_edited_scene(idx);
	EditorNode::get_singleton()->close_scene();

	ret["ok"] = true;
	ret["path"] = closed_path;
	ret["saved"] = save && !closed_path.is_empty();
	ret["message"] = closed_path.is_empty() ? String("Closed untitled scene tab.") : String("Closed scene: ") + closed_path;
	return ret;
}

Dictionary JustAMCPSceneTools::save_scene(const Dictionary &p_args) {
	String project_path = p_args.get("projectPath", "");
	String scene_path = _to_scene_res_path(project_path, p_args.get("scenePath", p_args.get("scene_path", "")));
	String new_path_raw = p_args.get("newPath", "");
	String target_path = scene_path;
	if (!new_path_raw.is_empty()) {
		target_path = _to_scene_res_path(project_path, new_path_raw);
	}

	JustAMCPScenePathLock scene_lock(target_path);
	if (!scene_lock.is_locked()) {
		return justamcp_scene_lock_busy_response();
	}

	Array result = _load_scene(scene_path);
	Dictionary err = result[1];
	if (!err.is_empty()) {
		return err;
	}

	_ensure_parent_dir_for_scene(target_path);

	Node *root = Object::cast_to<Node>(result[0]);
	err = _save_scene(root, target_path);
	if (!err.is_empty()) {
		return err;
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["scenePath"] = scene_path;
	ret["savedPath"] = target_path;
	return ret;
}

Dictionary JustAMCPSceneTools::create_inherited_scene(const Dictionary &p_args) {
	String project_path = p_args.get("projectPath", "");
	String base_scene_path = _to_scene_res_path(project_path, p_args.get("baseScenePath", ""));
	String new_scene_path = _to_scene_res_path(project_path, p_args.get("newScenePath", ""));

	if (base_scene_path == "res://" || new_scene_path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Missing baseScenePath or newScenePath parameter.";
		return ret;
	}

	JustAMCPScenePathLock scene_lock(new_scene_path);
	if (!scene_lock.is_locked()) {
		return justamcp_scene_lock_busy_response();
	}

	Ref<PackedScene> base_pack = ResourceLoader::load(base_scene_path);
	if (base_pack.is_null()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to load base scene packed resource.";
		return ret;
	}

	Node *instance = base_pack->instantiate(PackedScene::GEN_EDIT_STATE_INSTANCE);
	if (!instance) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to instantiate base scene for inheritance.";
		return ret;
	}

	instance->set_scene_file_path(base_scene_path);

	Dictionary err = _save_scene(instance, new_scene_path);
	if (!err.is_empty()) {
		return err;
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["scenePath"] = p_args.get("newScenePath", "");
	return ret;
}

#endif
