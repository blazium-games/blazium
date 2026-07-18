/**************************************************************************/
/*  justamcp_scene_tools_read.cpp                                         */
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

#include "../justamcp_editor_plugin.h"
#include "../justamcp_editor_scene_access.h"
#include "../justamcp_read_limits.h"
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
#include "editor/editor_file_system.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "scene/2d/sprite_2d.h"
#include "scene/3d/sprite_3d.h"
#include "scene/resources/packed_scene.h"

Dictionary JustAMCPSceneTools::_build_node_tree(Node *p_node, bool p_include_properties, int p_depth, int p_current_depth, const String &p_node_path) {
	Dictionary tree_data;
	tree_data["name"] = String(p_node->get_name());
	tree_data["type"] = p_node->get_class();
	tree_data["path"] = p_node_path;
	Array children;
	tree_data["children"] = children;

	if (p_include_properties) {
		Dictionary props;
		List<PropertyInfo> plist;
		p_node->get_property_list(&plist);
		for (const PropertyInfo &E : plist) {
			if (!(E.usage & PROPERTY_USAGE_STORAGE)) {
				continue;
			}
			String pn = E.name;
			if (pn.is_empty()) {
				continue;
			}
			props[pn] = _serialize_value(p_node->get(pn));
		}
		tree_data["properties"] = props;
	}

	if (p_depth >= 0 && p_current_depth >= p_depth) {
		return tree_data;
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *child = p_node->get_child(i);
		String child_path = (p_node_path == ".") ? String(child->get_name()) : p_node_path + "/" + String(child->get_name());
		Dictionary child_tree = _build_node_tree(child, p_include_properties, p_depth, p_current_depth + 1, child_path);
		Array c = tree_data["children"];
		c.push_back(child_tree);
		tree_data["children"] = c;
	}

	return tree_data;
}

void JustAMCPSceneTools::_collect_nodes_recursive(Node *p_node, const String &p_path, Array &r_out_nodes, int p_max_nodes) {
	if (r_out_nodes.size() >= p_max_nodes) {
		return;
	}
	Dictionary entry;
	entry["path"] = p_path;
	entry["node"] = p_node;
	r_out_nodes.push_back(entry);

	for (int i = 0; i < p_node->get_child_count(); i++) {
		if (r_out_nodes.size() >= p_max_nodes) {
			return;
		}
		Node *child = p_node->get_child(i);
		String child_path = (p_path == ".") ? String(child->get_name()) : p_path + "/" + String(child->get_name());
		_collect_nodes_recursive(child, child_path, r_out_nodes, p_max_nodes);
	}
}

Dictionary JustAMCPSceneTools::list_scene_nodes(const Dictionary &p_args) {
	String project_path = p_args.get("projectPath", "");
	String scene_path = _to_scene_res_path(project_path, p_args.get("scenePath", p_args.get("scene_path", "")));
	int depth = p_args.get("depth", -1);
	bool include_properties = p_args.get("includeProperties", false);

	Array result = _load_scene(scene_path);
	Dictionary err = result[1];
	if (!err.is_empty()) {
		return err;
	}

	Node *root = Object::cast_to<Node>(result[0]);
	Dictionary tree = _build_node_tree(root, include_properties, depth, 0, ".");
	memdelete(root);

	Dictionary ret;
	ret["ok"] = true;
	ret["tree"] = tree;
	return ret;
}

Dictionary JustAMCPSceneTools::get_scene_file_content(const Dictionary &p_args) {
	String project_path = p_args.get("projectPath", "");
	String scene_path = _to_scene_res_path(project_path, p_args.get("scenePath", p_args.get("path", "")));
	if (scene_path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "scenePath is required.";
		return ret;
	}
	if (!FileAccess::exists(scene_path)) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Scene not found: " + scene_path;
		return ret;
	}
	const int max_bytes = int(p_args.get("max_bytes", JUSTAMCP_MAX_SYNC_READ_BYTES));
	int64_t file_size = 0;
	if (!justamcp_file_within_read_limit(scene_path, max_bytes, file_size)) {
		return justamcp_read_limit_error(scene_path, file_size, max_bytes);
	}
	Ref<FileAccess> file = FileAccess::open(scene_path, FileAccess::READ);
	if (file.is_null()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to read scene: " + scene_path;
		return ret;
	}
	String content = file->get_as_text();
	file->close();

	Dictionary ret;
	ret["ok"] = true;
	ret["path"] = scene_path;
	ret["sceneContent"] = content;
	ret["size"] = content.length();
	return ret;
}

Dictionary JustAMCPSceneTools::get_scene_exports(const Dictionary &p_args) {
	String project_path = p_args.get("projectPath", "");
	String scene_path = _to_scene_res_path(project_path, p_args.get("scenePath", p_args.get("path", "")));
	Array loaded = _load_scene(scene_path);
	Dictionary err = loaded[1];
	if (!err.is_empty()) {
		return err;
	}
	Node *root = Object::cast_to<Node>(loaded[0]);
	Array nodes;
	const int max_nodes = CLAMP(int(p_args.get("max_nodes", 4096)), 1, 65536);
	_collect_nodes_recursive(root, ".", nodes, max_nodes);
	Array exports;
	for (int i = 0; i < nodes.size(); i++) {
		Dictionary entry = nodes[i];
		Node *node = Object::cast_to<Node>(entry["node"]);
		if (!node) {
			continue;
		}
		Dictionary props;
		List<PropertyInfo> plist;
		node->get_property_list(&plist);
		for (const PropertyInfo &prop : plist) {
			if ((prop.usage & PROPERTY_USAGE_SCRIPT_VARIABLE) || (prop.usage & PROPERTY_USAGE_EDITOR)) {
				String prop_name = prop.name;
				if (!prop_name.begins_with("_")) {
					Dictionary info;
					info["value"] = _serialize_value(node->get(prop_name));
					info["type"] = Variant::get_type_name(prop.type);
					info["hint"] = prop.hint;
					info["hint_string"] = prop.hint_string;
					props[prop_name] = info;
				}
			}
		}
		if (!props.is_empty()) {
			Dictionary item;
			item["node_path"] = entry["path"];
			item["node_name"] = node->get_name();
			item["node_type"] = node->get_class();
			if (node->get_script()) {
				Ref<Script> node_script = node->get_script();
				item["script_path"] = node_script.is_valid() ? node_script->get_path() : String();
			}
			item["exports"] = props;
			exports.push_back(item);
		}
	}
	memdelete(root);

	Dictionary ret;
	ret["ok"] = true;
	ret["path"] = scene_path;
	ret["nodes"] = exports;
	ret["count"] = exports.size();
	return ret;
}

Dictionary JustAMCPSceneTools::get_current_scene(const Dictionary &p_args) {
	bool editor_ready = EditorNode::get_singleton() && EditorInterface::get_singleton();
	Node *root = editor_ready ? JustAMCPEditorSceneAccess::get_edited_root() : nullptr;
	if (!root) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "No scene is currently open.";
		return ret;
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["path"] = root->get_scene_file_path();
	ret["root_name"] = root->get_name();
	ret["root_type"] = root->get_class();
	return ret;
}

Dictionary JustAMCPSceneTools::list_open_scenes(const Dictionary &p_args) {
	Array scenes;
	String current_path;
	if (EditorNode::get_singleton() && EditorInterface::get_singleton()) {
		PackedStringArray open_scenes = EditorInterface::get_singleton()->get_open_scenes();
		for (int i = 0; i < open_scenes.size(); i++) {
			scenes.push_back(open_scenes[i]);
		}
		Node *root = JustAMCPEditorSceneAccess::get_edited_root();
		if (root) {
			current_path = root->get_scene_file_path();
		}
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["scenes"] = scenes;
	ret["current"] = current_path;
	ret["count"] = scenes.size();
	return ret;
}

Dictionary JustAMCPSceneTools::get_node_properties(const Dictionary &p_args) {
	String project_path = p_args.get("projectPath", "");
	String scene_path = _to_scene_res_path(project_path, p_args.get("scenePath", p_args.get("scene_path", "")));
	String node_path = p_args.get("nodePath", ".");
	bool include_defaults = p_args.get("includeDefaults", false);

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

	Node *defaults = nullptr;
	if (!include_defaults && ClassDB::class_exists(node->get_class_name())) {
		Object *_defaults_obj = ClassDB::instantiate(node->get_class_name());
		defaults = Object::cast_to<Node>(_defaults_obj);
		if (!defaults && _defaults_obj) {
			memdelete(_defaults_obj);
		}
	}

	Dictionary props;
	List<PropertyInfo> plist;
	node->get_property_list(&plist);
	for (const PropertyInfo &E : plist) {
		if (!(E.usage & PROPERTY_USAGE_STORAGE)) {
			continue;
		}
		String prop_name = E.name;
		if (prop_name.is_empty()) {
			continue;
		}

		Variant current_val = node->get(prop_name);
		if (!include_defaults && defaults) {
			Variant default_val = defaults->get(prop_name);
			if (current_val == default_val) {
				continue;
			}
		}
		props[prop_name] = _serialize_value(current_val);
	}

	if (defaults) {
		memdelete(defaults);
	}
	memdelete(root);

	Dictionary ret;
	ret["ok"] = true;
	ret["nodePath"] = node_path;
	ret["properties"] = props;
	return ret;
}

Dictionary JustAMCPSceneTools::list_connections(const Dictionary &p_args) {
	String project_path = p_args.get("projectPath", "");
	String scene_path = _to_scene_res_path(project_path, p_args.get("scenePath", p_args.get("scene_path", "")));
	String filter_path = p_args.get("nodePath", "");

	Array result = _load_scene(scene_path);
	Dictionary err = result[1];
	if (!err.is_empty()) {
		return err;
	}

	Node *root = Object::cast_to<Node>(result[0]);
	Array nodes;
	const int max_nodes = CLAMP(int(p_args.get("max_nodes", 4096)), 1, 65536);
	_collect_nodes_recursive(root, ".", nodes, max_nodes);

	Array conn_arr;
	for (int i = 0; i < nodes.size(); i++) {
		Dictionary entry = nodes[i];
		String path = entry["path"];
		if (!filter_path.is_empty() && filter_path != path) {
			continue;
		}
		Node *node = Object::cast_to<Node>(entry["node"]);
		List<MethodInfo> signals;
		node->get_signal_list(&signals);
		for (const MethodInfo &si : signals) {
			String signal_name = si.name;
			if (signal_name.is_empty()) {
				continue;
			}

			List<Connection> conns;
			node->get_signal_connection_list(signal_name, &conns);
			for (const Connection &conn : conns) {
				Callable callable = conn.callable;
				Object *target_obj = callable.get_object();
				String target_path = "";
				if (target_obj && Object::cast_to<Node>(target_obj)) {
					target_path = root->get_path_to(Object::cast_to<Node>(target_obj));
				}
				Dictionary c_info;
				c_info["sourceNodePath"] = path;
				c_info["signalName"] = signal_name;
				c_info["targetNodePath"] = target_path;
				c_info["methodName"] = callable.get_method();
				c_info["flags"] = conn.flags;
				conn_arr.push_back(c_info);
			}
		}
	}

	memdelete(root);

	Dictionary ret;
	ret["ok"] = true;
	ret["connections"] = conn_arr;
	return ret;
}

Dictionary JustAMCPSceneTools::list_node_signals(const Dictionary &p_args) {
	String project_path = p_args.get("projectPath", "");
	String scene_path = _to_scene_res_path(project_path, p_args.get("scenePath", p_args.get("scene_path", "")));
	String node_path = p_args.get("nodePath", p_args.get("node_path", "."));

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

	Array signals;
	List<MethodInfo> signal_list;
	node->get_signal_list(&signal_list);
	for (const MethodInfo &signal : signal_list) {
		Dictionary info;
		info["name"] = signal.name;
		Array args;
		for (const PropertyInfo &arg : signal.arguments) {
			Dictionary arg_info;
			arg_info["name"] = arg.name;
			arg_info["type"] = Variant::get_type_name(arg.type);
			args.push_back(arg_info);
		}
		info["args"] = args;
		signals.push_back(info);
	}
	memdelete(root);

	Dictionary ret;
	ret["ok"] = true;
	ret["node_path"] = node_path;
	ret["signals"] = signals;
	ret["count"] = signals.size();
	return ret;
}

Dictionary JustAMCPSceneTools::has_signal_connection(const Dictionary &p_args) {
	String project_path = p_args.get("projectPath", "");
	String scene_path = _to_scene_res_path(project_path, p_args.get("scenePath", p_args.get("scene_path", "")));
	String source_node_path = p_args.get("sourceNodePath", p_args.get("source_path", ""));
	String signal_name = p_args.get("signalName", p_args.get("signal_name", ""));
	String target_node_path = p_args.get("targetNodePath", p_args.get("target_path", ""));
	String method_name = p_args.get("methodName", p_args.get("method_name", ""));

	Array result = _load_scene(scene_path);
	Dictionary err = result[1];
	if (!err.is_empty()) {
		return err;
	}
	Node *root = Object::cast_to<Node>(result[0]);
	Node *source = _find_node(root, source_node_path);
	Node *target = _find_node(root, target_node_path);
	if (!source || !target || signal_name.is_empty() || method_name.is_empty()) {
		memdelete(root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "source, target, signal and method are required.";
		return ret;
	}

	bool connected = source->is_connected(signal_name, Callable(target, StringName(method_name)));
	memdelete(root);
	Dictionary ret;
	ret["ok"] = true;
	ret["source"] = source_node_path;
	ret["signal"] = signal_name;
	ret["target"] = target_node_path;
	ret["method"] = method_name;
	ret["connected"] = connected;
	return ret;
}

#endif
