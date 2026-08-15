/**************************************************************************/
/*  justamcp_scene_tools_mutations_nodes.cpp                              */
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

#include "core/object/class_db.h"
#include "../justamcp_editor_plugin.h"
#include "../justamcp_editor_scene_access.h"
#include "justamcp_scene_path_lock.h"
#include "justamcp_scene_tools.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/message_queue.h"
#include "core/object/script_language.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/templates/hash_map.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "scene/2d/sprite_2d.h"
#include "scene/3d/sprite_3d.h"
#include "scene/resources/packed_scene.h"

Dictionary JustAMCPSceneTools::add_node(const Dictionary &p_args) {
	String project_path = p_args.get("projectPath", "");
	String scene_path = _to_scene_res_path(project_path, p_args.get("scenePath", p_args.get("scene_path", "")));
	String node_type = p_args.get("nodeType", p_args.get("node_type", ""));
	String node_name = p_args.get("nodeName", p_args.get("node_name", ""));
	String parent_node_path = p_args.get("parentNodePath", p_args.get("parent_path", "."));
	Dictionary properties = _parse_properties_arg(p_args.get("properties", Dictionary()));

	if (node_type.is_empty() || node_name.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Missing nodeType or nodeName";
		return ret;
	}
	if (!ClassDB::class_exists(StringName(node_type))) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Invalid nodeType: " + node_type;
		return ret;
	}

	JustAMCPScenePathLock scene_lock(scene_path);
	if (!scene_lock.is_locked()) {
		return justamcp_scene_lock_busy_response();
	}

	bool is_active = justamcp_is_active_scene(scene_path);
	Node *root = nullptr;
	if (is_active) {
		root = JustAMCPEditorSceneAccess::get_edited_root();
	} else {
		Array result = _load_scene(scene_path);
		Dictionary load_err = result[1];
		if (!load_err.is_empty()) {
			return load_err;
		}
		root = Object::cast_to<Node>(result[0]);
	}

	Node *parent = _find_node(root, parent_node_path);
	if (!parent) {
		if (!is_active) {
			memdelete(root);
		}
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Parent node not found: " + parent_node_path;
		return ret;
	}

	Object *_new_node_obj = ClassDB::instantiate(StringName(node_type));
	Node *new_node = Object::cast_to<Node>(_new_node_obj);
	if (!new_node) {
		if (_new_node_obj) {
			memdelete(_new_node_obj);
		}
		if (!is_active) {
			memdelete(root);
		}
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to instantiate nodeType: " + node_type;
		return ret;
	}

	new_node->set_name(node_name);
	_set_node_properties(new_node, properties);

	if (parent != root && !root->is_ancestor_of(parent)) {
		memdelete(new_node);
		if (!is_active) {
			memdelete(root);
		}
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Parent must be inside the scene tree rooted at the edited scene";
		return ret;
	}

	if (is_active) {
		EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
		ur->create_action(TTR("AI Local: Add Node"), UndoRedo::MERGE_DISABLE, parent);
		ur->add_do_method(parent, "add_child", new_node, true);
		ur->add_do_method(new_node, "set_owner", root);
		ur->add_do_reference(new_node);
		ur->add_undo_method(parent, "remove_child", new_node);
		ur->commit_action(true);
		Dictionary save_err = _pack_and_save_scene(root, scene_path, false);
		if (!save_err.is_empty()) {
			return save_err;
		}
	} else {
		parent->add_child(new_node);
		_set_owner_recursive(new_node, root);
		Dictionary save_err = _save_scene(root, scene_path);
		if (!save_err.is_empty()) {
			return save_err;
		}
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["nodeName"] = node_name;
	ret["nodeType"] = node_type;
	return ret;
}

Dictionary JustAMCPSceneTools::instance_scene(const Dictionary &p_args) {
	String project_path = p_args.get("projectPath", "");
	String target_scene_path = _to_scene_res_path(project_path, p_args.get("scenePath", p_args.get("scene_path", "")));
	String instance_scene_path = _to_scene_res_path(project_path, p_args.get("instanceScenePath", p_args.get("instance_scene_path", "")));
	String parent_node_path = p_args.get("parentNodePath", p_args.get("parent_path", p_args.get("parent_node_path", ".")));
	String requested_node_name = p_args.get("nodeName", p_args.get("node_name", ""));

	if (instance_scene_path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Missing instanceScenePath";
		return ret;
	}

	JustAMCPScenePathLock scene_lock(target_scene_path);
	if (!scene_lock.is_locked()) {
		return justamcp_scene_lock_busy_response();
	}

	Ref<PackedScene> packed = ResourceLoader::load(instance_scene_path);
	if (packed.is_null()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to load instance scene: " + instance_scene_path;
		return ret;
	}

	bool is_active = justamcp_is_active_scene(target_scene_path);
	Node *root = nullptr;
	if (is_active) {
		root = JustAMCPEditorSceneAccess::get_edited_root();
	} else {
		Array result = _load_scene(target_scene_path);
		Dictionary err = result[1];
		if (!err.is_empty()) {
			return err;
		}
		root = Object::cast_to<Node>(result[0]);
	}

	Node *parent = _find_node(root, parent_node_path);
	if (!parent) {
		if (!is_active) {
			memdelete(root);
		}
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Parent node not found: " + parent_node_path;
		return ret;
	}

	if (!requested_node_name.is_empty() && parent->has_node(requested_node_name)) {
		Node *existing = parent->get_node(requested_node_name);
		const String result_node_name = existing->get_name();
		if (!is_active) {
			memdelete(root);
		}
		Dictionary ret;
		ret["ok"] = true;
		ret["nodeName"] = result_node_name;
		ret["instanceScenePath"] = instance_scene_path;
		ret["parentNodePath"] = parent_node_path;
		ret["alreadyExists"] = true;
		return ret;
	}

	Node *new_node = packed->instantiate(PackedScene::GEN_EDIT_STATE_INSTANCE);
	if (!new_node) {
		if (!is_active) {
			memdelete(root);
		}
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to instantiate packed scene.";
		return ret;
	}

	new_node->set_scene_file_path(instance_scene_path);

	if (!requested_node_name.is_empty()) {
		new_node->set_name(requested_node_name);
	}

	Dictionary properties = _parse_properties_arg(p_args.get("properties", Dictionary()));
	_set_node_properties(new_node, properties);

	const String result_node_name = new_node->get_name();

	if (is_active) {
		EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
		ur->create_action(TTR("AI Local: Instance Scene"), UndoRedo::MERGE_DISABLE, parent);
		ur->add_do_method(parent, "add_child", new_node, true);
		ur->add_do_method(new_node, "set_owner", root);
		ur->add_do_reference(new_node);
		ur->add_undo_method(parent, "remove_child", new_node);
		ur->commit_action(true);
		Dictionary save_err = _pack_and_save_scene(root, target_scene_path, false);
		if (!save_err.is_empty()) {
			return save_err;
		}
	} else {
		parent->add_child(new_node, true);
		_set_owner_recursive(new_node, root);
		Dictionary save_err = _save_scene(root, target_scene_path);
		if (!save_err.is_empty()) {
			return save_err;
		}
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["nodeName"] = result_node_name;
	ret["instanceScenePath"] = instance_scene_path;
	ret["parentNodePath"] = parent_node_path;
	return ret;
}

Dictionary JustAMCPSceneTools::delete_node(const Dictionary &p_args) {
	String project_path = p_args.get("projectPath", "");
	String scene_path = _to_scene_res_path(project_path, p_args.get("scenePath", p_args.get("scene_path", "")));
	String node_path = p_args.get("nodePath", "");

	if (node_path.is_empty() || node_path == ".") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Cannot delete root node";
		return ret;
	}

	JustAMCPScenePathLock scene_lock(scene_path);
	if (!scene_lock.is_locked()) {
		return justamcp_scene_lock_busy_response();
	}

	bool is_active = justamcp_is_active_scene(scene_path);
	Node *root = nullptr;
	if (is_active) {
		root = JustAMCPEditorSceneAccess::get_edited_root();
	} else {
		Array result = _load_scene(scene_path);
		Dictionary err = result[1];
		if (!err.is_empty()) {
			return err;
		}
		root = Object::cast_to<Node>(result[0]);
	}

	Node *node = _find_node(root, node_path);
	if (!node) {
		if (!is_active) {
			memdelete(root);
		}
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Node not found: " + node_path;
		return ret;
	}

	Node *parent = node->get_parent();
	if (!parent) {
		if (!is_active) {
			memdelete(root);
		}
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Cannot delete root node";
		return ret;
	}

	if (is_active) {
		EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
		ur->create_action(TTR("AI Local: Delete Node"), UndoRedo::MERGE_DISABLE, node);
		ur->add_do_method(parent, "remove_child", node);

		ur->add_undo_method(node, "set_owner", root);
		ur->add_undo_method(parent, "add_child", node, true);
		ur->add_undo_reference(node);
		ur->commit_action(true);
	} else {
		parent->remove_child(node);
		memdelete(node);
		Dictionary save_err = _save_scene(root, scene_path);
		if (!save_err.is_empty()) {
			return save_err;
		}
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["deletedNodePath"] = node_path;
	return ret;
}

Dictionary JustAMCPSceneTools::duplicate_node(const Dictionary &p_args) {
	String project_path = p_args.get("projectPath", "");
	String scene_path = _to_scene_res_path(project_path, p_args.get("scenePath", p_args.get("scene_path", "")));
	String node_path = p_args.get("nodePath", "");
	String new_name = p_args.get("newName", "");
	String parent_path = p_args.get("parentPath", "");

	if (node_path.is_empty() || new_name.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Missing nodePath or newName";
		return ret;
	}

	JustAMCPScenePathLock scene_lock(scene_path);
	if (!scene_lock.is_locked()) {
		return justamcp_scene_lock_busy_response();
	}

	bool is_active = justamcp_is_active_scene(scene_path);
	Node *root = nullptr;
	if (is_active) {
		root = JustAMCPEditorSceneAccess::get_edited_root();
	} else {
		Array result = _load_scene(scene_path);
		Dictionary err = result[1];
		if (!err.is_empty()) {
			return err;
		}
		root = Object::cast_to<Node>(result[0]);
	}

	Node *source = _find_node(root, node_path);
	if (!source) {
		if (!is_active) {
			memdelete(root);
		}
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Node not found: " + node_path;
		return ret;
	}

	if (source == root) {
		if (!is_active) {
			memdelete(root);
		}
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Cannot duplicate the scene root; choose a child node_path";
		return ret;
	}

	Node *target_parent = source->get_parent();
	if (!parent_path.is_empty()) {
		target_parent = _find_node(root, parent_path);
	}
	if (!target_parent) {
		if (!is_active) {
			memdelete(root);
		}
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Parent not found: " + parent_path;
		return ret;
	}
	if (target_parent != root && !root->is_ancestor_of(target_parent)) {
		if (!is_active) {
			memdelete(root);
		}
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Parent must be inside the scene tree rooted at the edited scene";
		return ret;
	}

	Node *duplicated_node = source->duplicate();
	if (!duplicated_node) {
		if (!is_active) {
			memdelete(root);
		}
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to duplicate node: " + node_path;
		return ret;
	}

	duplicated_node->set_name(new_name);

	if (is_active) {
		EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
		ur->create_action(TTR("AI Local: Duplicate Node"), UndoRedo::MERGE_DISABLE, duplicated_node);
		ur->add_do_method(target_parent, "add_child", duplicated_node, true);
		if (root->is_ancestor_of(target_parent) || target_parent == root) {
			ur->add_do_method(duplicated_node, "set_owner", root);
		}
		ur->add_do_reference(duplicated_node);
		ur->add_undo_method(target_parent, "remove_child", duplicated_node);
		ur->commit_action(true);
	} else {
		target_parent->add_child(duplicated_node);
		_set_owner_recursive(duplicated_node, root);
		Dictionary save_err = _save_scene(root, scene_path);
		if (!save_err.is_empty()) {
			return save_err;
		}
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["nodePath"] = node_path;
	ret["newName"] = new_name;
	return ret;
}

Dictionary JustAMCPSceneTools::reparent_node(const Dictionary &p_args) {
	String project_path = p_args.get("projectPath", "");
	String scene_path = _to_scene_res_path(project_path, p_args.get("scenePath", p_args.get("scene_path", "")));
	String node_path = p_args.get("nodePath", "");
	String new_parent_path = p_args.get("newParentPath", "");

	if (node_path.is_empty() || node_path == ".") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Cannot reparent root node";
		return ret;
	}
	if (new_parent_path.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Missing newParentPath";
		return ret;
	}

	JustAMCPScenePathLock scene_lock(scene_path);
	if (!scene_lock.is_locked()) {
		return justamcp_scene_lock_busy_response();
	}

	Array result = _load_scene(scene_path);
	Dictionary err = result[1];
	if (!err.is_empty()) {
		return err;
	}

	Node *root = Object::cast_to<Node>(result[0]);
	Node *node = _find_node(root, node_path);
	Node *new_parent = _find_node(root, new_parent_path);
	if (!node) {
		memdelete(root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Node not found: " + node_path;
		return ret;
	}
	if (!new_parent) {
		memdelete(root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "New parent not found: " + new_parent_path;
		return ret;
	}

	Node *old_parent = node->get_parent();
	if (!old_parent) {
		memdelete(root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Cannot reparent root node";
		return ret;
	}

	old_parent->remove_child(node);
	new_parent->add_child(node);
	_set_owner_recursive(node, root);

	err = _save_scene(root, scene_path);
	if (!err.is_empty()) {
		return err;
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["nodePath"] = node_path;
	ret["newParentPath"] = new_parent_path;
	return ret;
}

Dictionary JustAMCPSceneTools::set_node_properties(const Dictionary &p_args) {
	String project_path = p_args.get("projectPath", "");
	String scene_path = _to_scene_res_path(project_path, p_args.get("scenePath", p_args.get("scene_path", "")));
	String node_path = p_args.get("nodePath", p_args.get("node_path", "."));
	Dictionary properties = _parse_properties_arg(p_args.get("properties", Dictionary()));

	JustAMCPScenePathLock scene_lock(scene_path);
	if (!scene_lock.is_locked()) {
		return justamcp_scene_lock_busy_response();
	}

	bool is_active = justamcp_is_active_scene(scene_path);
	Node *root = nullptr;
	if (is_active) {
		root = JustAMCPEditorSceneAccess::get_edited_root();
	} else {
		Array result = _load_scene(scene_path);
		Dictionary err = result[1];
		if (!err.is_empty()) {
			return err;
		}
		root = Object::cast_to<Node>(result[0]);
	}

	Node *node = _find_node(root, node_path);
	if (!node) {
		if (!is_active) {
			memdelete(root);
		}
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Node not found: " + node_path;
		return ret;
	}

	if (is_active) {
		EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
		ur->create_action(TTR("AI Local: Set Properties"), UndoRedo::MERGE_DISABLE, node);
		Array keys = properties.keys();
		for (int i = 0; i < keys.size(); i++) {
			String prop_name = keys[i];
			Variant new_val = _parse_value(properties[prop_name]);
			Variant old_val = node->get(prop_name);
			ur->add_do_property(node, prop_name, new_val);
			ur->add_undo_property(node, prop_name, old_val);
		}
		ur->commit_action(true);
	} else {
		_set_node_properties(node, properties);
		Dictionary save_err = _save_scene(root, scene_path);
		if (!save_err.is_empty()) {
			return save_err;
		}
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["nodePath"] = node_path;
	return ret;
}

Dictionary JustAMCPSceneTools::load_sprite(const Dictionary &p_args) {
	String project_path = p_args.get("projectPath", "");
	String scene_path = _to_scene_res_path(project_path, p_args.get("scenePath", p_args.get("scene_path", "")));
	String node_path = p_args.get("nodePath", ".");
	String texture_path = _to_scene_res_path(project_path, p_args.get("texturePath", ""));

	if (texture_path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Missing texturePath";
		return ret;
	}

	Ref<Texture2D> texture = ResourceLoader::load(texture_path);
	if (texture.is_null()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to load texture: " + texture_path;
		return ret;
	}

	JustAMCPScenePathLock scene_lock(scene_path);
	if (!scene_lock.is_locked()) {
		return justamcp_scene_lock_busy_response();
	}

	Array result = _load_scene(scene_path);
	Dictionary err = result[1];
	if (!err.is_empty()) {
		return err;
	}

	Node *root = Object::cast_to<Node>(result[0]);
	Node *node = _find_node(root, node_path);
	if (!node) {
		memdelete(root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Node not found: " + node_path;
		return ret;
	}

	if (Sprite2D *s2d = Object::cast_to<Sprite2D>(node)) {
		s2d->set_texture(texture);
	} else if (Sprite3D *s3d = Object::cast_to<Sprite3D>(node)) {
		s3d->set_texture(texture);
	} else {
		memdelete(root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Node is not Sprite2D or Sprite3D: " + node_path;
		return ret;
	}

	err = _save_scene(root, scene_path);
	if (!err.is_empty()) {
		return err;
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["nodePath"] = node_path;
	ret["texturePath"] = texture_path;
	return ret;
}

Dictionary JustAMCPSceneTools::connect_signal(const Dictionary &p_args) {
	String project_path = p_args.get("projectPath", "");
	String scene_path = _to_scene_res_path(project_path, p_args.get("scenePath", p_args.get("scene_path", "")));
	String source_node_path = p_args.get("sourceNodePath", "");
	String signal_name = p_args.get("signalName", "");
	String target_node_path = p_args.get("targetNodePath", "");
	String method_name = p_args.get("methodName", "");
	int flags = p_args.get("flags", 0);

	if (source_node_path.is_empty() || signal_name.is_empty() || target_node_path.is_empty() || method_name.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Missing required signal connection arguments";
		return ret;
	}

	JustAMCPScenePathLock scene_lock(scene_path);
	if (!scene_lock.is_locked()) {
		return justamcp_scene_lock_busy_response();
	}

	Array result = _load_scene(scene_path);
	Dictionary err = result[1];
	if (!err.is_empty()) {
		return err;
	}

	Node *root = Object::cast_to<Node>(result[0]);
	Node *source = _find_node(root, source_node_path);
	Node *target = _find_node(root, target_node_path);
	if (!source) {
		memdelete(root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Source node not found: " + source_node_path;
		return ret;
	}
	if (!target) {
		memdelete(root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Target node not found: " + target_node_path;
		return ret;
	}
	if (!source->has_signal(signal_name)) {
		memdelete(root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Signal not found on source: " + signal_name;
		return ret;
	}

	Callable callable(target, StringName(method_name));
	if (!source->is_connected(signal_name, callable)) {
		Error connect_result = source->connect(signal_name, callable, flags);
		if (connect_result != OK) {
			memdelete(root);
			Dictionary ret;
			ret["ok"] = false;
			ret["error"] = "Failed to connect signal: " + itos(connect_result);
			return ret;
		}
	}

	err = _save_scene(root, scene_path);
	if (!err.is_empty()) {
		return err;
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["sourceNodePath"] = source_node_path;
	ret["signalName"] = signal_name;
	ret["targetNodePath"] = target_node_path;
	ret["methodName"] = method_name;
	ret["flags"] = flags;
	return ret;
}

Dictionary JustAMCPSceneTools::disconnect_signal(const Dictionary &p_args) {
	String project_path = p_args.get("projectPath", "");
	String scene_path = _to_scene_res_path(project_path, p_args.get("scenePath", p_args.get("scene_path", "")));
	String source_node_path = p_args.get("sourceNodePath", "");
	String signal_name = p_args.get("signalName", "");
	String target_node_path = p_args.get("targetNodePath", "");
	String method_name = p_args.get("methodName", "");

	if (source_node_path.is_empty() || signal_name.is_empty() || target_node_path.is_empty() || method_name.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Missing required signal disconnection arguments";
		return ret;
	}

	JustAMCPScenePathLock scene_lock(scene_path);
	if (!scene_lock.is_locked()) {
		return justamcp_scene_lock_busy_response();
	}

	Array result = _load_scene(scene_path);
	Dictionary err = result[1];
	if (!err.is_empty()) {
		return err;
	}

	Node *root = Object::cast_to<Node>(result[0]);
	Node *source = _find_node(root, source_node_path);
	Node *target = _find_node(root, target_node_path);
	if (!source) {
		memdelete(root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Source node not found: " + source_node_path;
		return ret;
	}
	if (!target) {
		memdelete(root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Target node not found: " + target_node_path;
		return ret;
	}

	Callable callable(target, StringName(method_name));
	if (source->is_connected(signal_name, callable)) {
		source->disconnect(signal_name, callable);
	}

	err = _save_scene(root, scene_path);
	if (!err.is_empty()) {
		return err;
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["sourceNodePath"] = source_node_path;
	ret["signalName"] = signal_name;
	ret["targetNodePath"] = target_node_path;
	ret["methodName"] = method_name;
	return ret;
}

#endif
