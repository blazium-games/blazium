/**************************************************************************/
/*  justamcp_scene_tools.cpp                                              */
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

#include "justamcp_scene_tools.h"
#include "../justamcp_editor_filesystem.h"
#include "../justamcp_editor_plugin.h"
#include "../justamcp_editor_scene_access.h"

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
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "editor/editor_file_system.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "scene/2d/sprite_2d.h"
#include "scene/3d/sprite_3d.h"
#include "scene/resources/packed_scene.h"

void JustAMCPSceneTools::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_deferred_refresh_filesystem"), &JustAMCPSceneTools::_deferred_refresh_filesystem);
}

static bool _filesystem_scan_pending = false;
static HashSet<String> _pending_efs_update_paths;

void JustAMCPSceneTools::_refresh_filesystem(const String &p_changed_path) {
	if (!editor_plugin) {
		return;
	}
	if (!p_changed_path.is_empty()) {
		_pending_efs_update_paths.insert(p_changed_path);
	}
	if (_filesystem_scan_pending) {
		return;
	}
	_filesystem_scan_pending = true;
	MessageQueue::get_singleton()->push_callable(callable_mp(this, &JustAMCPSceneTools::_deferred_refresh_filesystem));
}

void JustAMCPSceneTools::_deferred_refresh_filesystem() {
	_filesystem_scan_pending = false;
	if (!EditorFileSystem::get_singleton()) {
		_pending_efs_update_paths.clear();
		return;
	}
	if (_pending_efs_update_paths.is_empty()) {
		return;
	}
	for (const String &path : _pending_efs_update_paths) {
		JustAMCPEditorFilesystem::refresh_path(path);
	}
	_pending_efs_update_paths.clear();
}

JustAMCPSceneTools::JustAMCPSceneTools() {
}

JustAMCPSceneTools::~JustAMCPSceneTools() {
}

void JustAMCPSceneTools::_refresh_and_reload(const String &p_scene_path) {
	_refresh_filesystem(p_scene_path);
	_reload_scene_in_editor(p_scene_path);
}

void JustAMCPSceneTools::_reload_scene_in_editor(const String &p_scene_path) {
	if (!editor_plugin) {
		return;
	}
	Node *edited = JustAMCPEditorSceneAccess::get_edited_root();
	if (edited && edited->get_scene_file_path() == p_scene_path) {
		EditorInterface::get_singleton()->reload_scene_from_path(p_scene_path);
	}
}

String JustAMCPSceneTools::_ensure_res_path(const String &p_path) {
	if (!p_path.begins_with("res://")) {
		return "res://" + p_path;
	}
	return p_path;
}

String JustAMCPSceneTools::_to_scene_res_path(const String &p_project_path, const String &p_scene_path) {
	String p = p_scene_path.strip_edges();
	if (p.begins_with("res://") || p.begins_with("user://")) {
		return p;
	}

	// Absolute filesystem paths (tests / external tooling) stay absolute when present.
	if (p.is_absolute_path() && FileAccess::exists(p)) {
		return p;
	}

	if (!p_project_path.strip_edges().is_empty()) {
		String normalized_project = p_project_path.replace("\\", "/");
		String normalized_scene = p.replace("\\", "/");
		if (normalized_scene.begins_with(normalized_project)) {
			String rel = normalized_scene.substr(normalized_project.length());
			if (rel.begins_with("/")) {
				rel = rel.substr(1);
			}
			return _ensure_res_path(rel);
		}
	}

	return _ensure_res_path(p);
}

Array JustAMCPSceneTools::_load_scene(const String &p_scene_path) {
	Array ret;
	ret.resize(2);
	ret[0] = (Object *)nullptr;
	Dictionary err;

	if (!FileAccess::exists(p_scene_path)) {
		err["ok"] = false;
		err["error"] = "Scene not found: " + p_scene_path;
		ret[1] = err;
		return ret;
	}

	Ref<PackedScene> packed = ResourceLoader::load(p_scene_path);
	if (packed.is_null()) {
		err["ok"] = false;
		err["error"] = "Failed to load: " + p_scene_path;
		ret[1] = err;
		return ret;
	}

	Node *root = packed->instantiate();
	if (!root) {
		err["ok"] = false;
		err["error"] = "Failed to instantiate: " + p_scene_path;
		ret[1] = err;
		return ret;
	}

	ret[0] = root;
	ret[1] = Dictionary();
	return ret;
}

Dictionary JustAMCPSceneTools::_pack_and_save_scene(Node *p_scene_root, const String &p_scene_path, bool p_free_root) {
	Dictionary ret;
	Ref<PackedScene> packed;
	packed.instantiate();
	if (packed->pack(p_scene_root) != OK) {
		if (p_free_root) {
			memdelete(p_scene_root);
		}
		ret["ok"] = false;
		ret["error"] = "Failed to pack scene";
		return ret;
	}
	if (ResourceSaver::save(packed, p_scene_path) != OK) {
		if (p_free_root) {
			memdelete(p_scene_root);
		}
		ret["ok"] = false;
		ret["error"] = "Failed to save scene";
		return ret;
	}
	if (p_free_root) {
		memdelete(p_scene_root);
	}
	_refresh_and_reload(p_scene_path);
	return Dictionary();
}

Dictionary JustAMCPSceneTools::_save_scene(Node *p_scene_root, const String &p_scene_path) {
	return _pack_and_save_scene(p_scene_root, p_scene_path, true);
}

Node *JustAMCPSceneTools::_find_node(Node *p_root, const String &p_path) {
	if (p_path == "." || p_path.is_empty()) {
		return p_root;
	}
	return p_root->get_node_or_null(p_path);
}

Variant JustAMCPSceneTools::_parse_value(const Variant &p_value) {
	if (p_value.get_type() == Variant::DICTIONARY) {
		Dictionary value = p_value;
		if (value.has("type")) {
			String t = value["type"];
			if (t == "Vector2") {
				return Vector2(value.get("x", 0), value.get("y", 0));
			}
			if (t == "Vector3") {
				return Vector3(value.get("x", 0), value.get("y", 0), value.get("z", 0));
			}
			if (t == "Color") {
				return Color(value.get("r", 1), value.get("g", 1), value.get("b", 1), value.get("a", 1));
			}
			if (t == "Vector2i") {
				return Vector2i(value.get("x", 0), value.get("y", 0));
			}
			if (t == "Vector3i") {
				return Vector3i(value.get("x", 0), value.get("y", 0), value.get("z", 0));
			}
			if (t == "Rect2") {
				return Rect2(value.get("x", 0), value.get("y", 0), value.get("width", 0), value.get("height", 0));
			}
			if (t == "Transform2D") {
				if (value.has("x") && value.has("y") && value.has("origin")) {
					Dictionary xx = value["x"];
					Dictionary yy = value["y"];
					Dictionary oo = value["origin"];
					return Transform2D(
							Vector2(xx.get("x", 1), xx.get("y", 0)),
							Vector2(yy.get("x", 0), yy.get("y", 1)),
							Vector2(oo.get("x", 0), oo.get("y", 0)));
				}
			}
			if (t == "Transform3D") {
				if (value.has("basis") && value.has("origin")) {
					Dictionary b = value["basis"];
					Dictionary o = value["origin"];
					Dictionary bx = b.get("x", Dictionary());
					Dictionary by = b.get("y", Dictionary());
					Dictionary bz = b.get("z", Dictionary());
					Basis basis = Basis(
							Vector3(bx.get("x", 1), bx.get("y", 0), bx.get("z", 0)),
							Vector3(by.get("x", 0), by.get("y", 1), by.get("z", 0)),
							Vector3(bz.get("x", 0), bz.get("y", 0), bz.get("z", 1)));
					return Transform3D(basis, Vector3(o.get("x", 0), o.get("y", 0), o.get("z", 0)));
				}
			}
			if (t == "NodePath") {
				return NodePath(String(value.get("path", "")));
			}
			if (t == "Resource") {
				String resource_path = value.get("path", "");
				if (resource_path.is_empty()) {
					return Variant();
				}
				return ResourceLoader::load(resource_path);
			}
		}
	} else if (p_value.get_type() == Variant::ARRAY) {
		Array arr = p_value;
		Array result;
		for (int i = 0; i < arr.size(); i++) {
			result.push_back(_parse_value(arr[i]));
		}
		return result;
	}
	return p_value;
}

Variant JustAMCPSceneTools::_serialize_value(const Variant &p_value) {
	switch (p_value.get_type()) {
		case Variant::VECTOR2: {
			Vector2 v = p_value;
			Dictionary d;
			d["type"] = "Vector2";
			d["x"] = v.x;
			d["y"] = v.y;
			return d;
		}
		case Variant::VECTOR3: {
			Vector3 v = p_value;
			Dictionary d;
			d["type"] = "Vector3";
			d["x"] = v.x;
			d["y"] = v.y;
			d["z"] = v.z;
			return d;
		}
		case Variant::COLOR: {
			Color c = p_value;
			Dictionary d;
			d["type"] = "Color";
			d["r"] = c.r;
			d["g"] = c.g;
			d["b"] = c.b;
			d["a"] = c.a;
			return d;
		}
		case Variant::VECTOR2I: {
			Vector2i v = p_value;
			Dictionary d;
			d["type"] = "Vector2i";
			d["x"] = v.x;
			d["y"] = v.y;
			return d;
		}
		case Variant::VECTOR3I: {
			Vector3i v = p_value;
			Dictionary d;
			d["type"] = "Vector3i";
			d["x"] = v.x;
			d["y"] = v.y;
			d["z"] = v.z;
			return d;
		}
		case Variant::RECT2: {
			Rect2 r = p_value;
			Dictionary d;
			d["type"] = "Rect2";
			d["x"] = r.position.x;
			d["y"] = r.position.y;
			d["width"] = r.size.width;
			d["height"] = r.size.height;
			return d;
		}
		case Variant::NODE_PATH: {
			NodePath p = p_value;
			Dictionary d;
			d["type"] = "NodePath";
			d["path"] = String(p);
			return d;
		}
		case Variant::TRANSFORM2D: {
			Transform2D t = p_value;
			Dictionary d;
			d["type"] = "Transform2D";
			Dictionary xx;
			xx["x"] = t.columns[0].x;
			xx["y"] = t.columns[0].y;
			Dictionary yy;
			yy["x"] = t.columns[1].x;
			yy["y"] = t.columns[1].y;
			Dictionary oo;
			oo["x"] = t.get_origin().x;
			oo["y"] = t.get_origin().y;
			d["x"] = xx;
			d["y"] = yy;
			d["origin"] = oo;
			return d;
		}
		case Variant::TRANSFORM3D: {
			Transform3D t = p_value;
			Dictionary d;
			d["type"] = "Transform3D";
			Dictionary bx;
			bx["x"] = t.basis.get_column(0).x;
			bx["y"] = t.basis.get_column(0).y;
			bx["z"] = t.basis.get_column(0).z;
			Dictionary by;
			by["x"] = t.basis.get_column(1).x;
			by["y"] = t.basis.get_column(1).y;
			by["z"] = t.basis.get_column(1).z;
			Dictionary bz;
			bz["x"] = t.basis.get_column(2).x;
			bz["y"] = t.basis.get_column(2).y;
			bz["z"] = t.basis.get_column(2).z;
			Dictionary basis;
			basis["x"] = bx;
			basis["y"] = by;
			basis["z"] = bz;
			Dictionary origin;
			origin["x"] = t.origin.x;
			origin["y"] = t.origin.y;
			origin["z"] = t.origin.z;
			d["basis"] = basis;
			d["origin"] = origin;
			return d;
		}
		case Variant::OBJECT: {
			Ref<Resource> res = p_value;
			if (res.is_valid() && !res->get_path().is_empty()) {
				Dictionary d;
				d["type"] = "Resource";
				d["path"] = res->get_path();
				return d;
			}
			return Variant();
		}
		default:
			return p_value;
	}
}

void JustAMCPSceneTools::_set_node_properties(Node *p_node, const Dictionary &p_properties) {
	Array keys = p_properties.keys();
	for (int i = 0; i < keys.size(); i++) {
		String prop_name = keys[i];
		Variant val = _parse_value(p_properties[prop_name]);
		if (prop_name == "script" && val.get_type() == Variant::STRING) {
			String script_path = val;
			if (script_path.begins_with("res://")) {
				Ref<Script> script_res = ResourceLoader::load(script_path);
				if (script_res.is_valid()) {
					val = script_res;
				}
			}
		}
		p_node->set(prop_name, val);
	}
}

Dictionary JustAMCPSceneTools::_parse_properties_arg(const Variant &p_raw_properties) {
	if (p_raw_properties.get_type() == Variant::DICTIONARY) {
		return p_raw_properties;
	}
	if (p_raw_properties.get_type() == Variant::STRING) {
		String text = p_raw_properties;
		if (text.strip_edges().is_empty()) {
			return Dictionary();
		}
		Ref<JSON> json;
		json.instantiate();
		if (json->parse(text) == OK) {
			Variant parsed = json->get_data();
			if (parsed.get_type() == Variant::DICTIONARY) {
				return parsed;
			}
		}
	}
	return Dictionary();
}

void JustAMCPSceneTools::_ensure_parent_dir_for_scene(const String &p_scene_path) {
	String base_dir = p_scene_path.get_base_dir();
	Ref<DirAccess> dir = DirAccess::open(base_dir);
	if (dir.is_null()) {
		DirAccess::make_dir_recursive_absolute(base_dir);
	}
}

void JustAMCPSceneTools::_set_owner_recursive(Node *p_node, Node *p_scene_owner) {
	p_node->set_owner(p_scene_owner);
	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *child = p_node->get_child(i);
		_set_owner_recursive(child, p_scene_owner);
	}
}

Dictionary JustAMCPSceneTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "create_scene") {
		return create_scene(p_args);
	}
	if (p_tool_name == "scene_create_inherited") {
		return create_inherited_scene(p_args);
	}
	if (p_tool_name == "list_scene_nodes") {
		return list_scene_nodes(p_args);
	}
	if (p_tool_name == "get_scene_file_content") {
		return get_scene_file_content(p_args);
	}
	if (p_tool_name == "delete_scene") {
		return delete_scene_file(p_args);
	}
	if (p_tool_name == "get_scene_exports") {
		return get_scene_exports(p_args);
	}
	if (p_tool_name == "scene_get_current") {
		return get_current_scene(p_args);
	}
	if (p_tool_name == "scene_list_open") {
		return list_open_scenes(p_args);
	}
	if (p_tool_name == "scene_set_current") {
		return set_current_scene(p_args);
	}
	if (p_tool_name == "scene_reload") {
		return reload_scene(p_args);
	}
	if (p_tool_name == "scene_duplicate_file") {
		return duplicate_scene_file(p_args);
	}
	if (p_tool_name == "scene_close") {
		return close_scene(p_args);
	}
	if (p_tool_name == "add_node") {
		return add_node(p_args);
	}
	if (p_tool_name == "delete_node") {
		return delete_node(p_args);
	}
	if (p_tool_name == "duplicate_node") {
		return duplicate_node(p_args);
	}
	if (p_tool_name == "reparent_node") {
		return reparent_node(p_args);
	}
	if (p_tool_name == "modify_node_property") {
		Dictionary wrapper_args = p_args;
		Dictionary properties;
		properties[p_args.get("property", "")] = p_args.get("value", Variant());
		wrapper_args["properties"] = properties;
		return set_node_properties(wrapper_args);
	}
	if (p_tool_name == "set_node_properties") {
		return set_node_properties(p_args);
	}
	if (p_tool_name == "get_node_properties") {
		return get_node_properties(p_args);
	}
	if (p_tool_name == "create_area_2d") {
		return create_area_2d(p_args);
	}
	if (p_tool_name == "create_line_2d") {
		return create_line_2d(p_args);
	}
	if (p_tool_name == "create_polygon_2d") {
		return create_polygon_2d(p_args);
	}
	if (p_tool_name == "create_csg_shape") {
		return create_csg_shape(p_args);
	}
	if (p_tool_name == "instance_scene") {
		return instance_scene(p_args);
	}
	if (p_tool_name == "setup_camera_2d") {
		return setup_camera_2d(p_args);
	}
	if (p_tool_name == "setup_parallax_2d") {
		return setup_parallax_2d(p_args);
	}
	if (p_tool_name == "create_multimesh") {
		return create_multimesh(p_args);
	}
	if (p_tool_name == "setup_skeleton") {
		return setup_skeleton(p_args);
	}
	if (p_tool_name == "setup_occlusion") {
		return setup_occlusion(p_args);
	}
	if (p_tool_name == "load_sprite") {
		return load_sprite(p_args);
	}
	if (p_tool_name == "save_scene") {
		return save_scene(p_args);
	}
	if (p_tool_name == "connect_signal") {
		return connect_signal(p_args);
	}
	if (p_tool_name == "disconnect_signal") {
		return disconnect_signal(p_args);
	}
	if (p_tool_name == "list_connections") {
		return list_connections(p_args);
	}
	if (p_tool_name == "list_node_signals") {
		return list_node_signals(p_args);
	}
	if (p_tool_name == "has_signal_connection") {
		return has_signal_connection(p_args);
	}
	return Dictionary();
}

#endif
