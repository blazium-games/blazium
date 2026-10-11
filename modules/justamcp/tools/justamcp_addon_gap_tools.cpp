/**************************************************************************/
/*  justamcp_addon_gap_tools.cpp                                          */
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

#include "justamcp_addon_gap_tools.h"

#include "../justamcp_editor_scene_access.h"
#include "../justamcp_server.h"
#include "../justamcp_tool_context.h"
#include "justamcp_agent_policy.h"

#include "core/config/project_settings.h"
#include "core/io/config_file.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/class_db.h"
#include "core/object/worker_thread_pool.h"
#include "core/os/thread.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "scene/2d/camera_2d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/multimesh_instance_3d.h"
#include "scene/3d/skeleton_3d.h"
#include "scene/animation/animation_blend_tree.h"
#include "scene/animation/animation_player.h"
#include "scene/animation/animation_tree.h"
#include "scene/gui/button.h"
#include "scene/gui/item_list.h"
#include "scene/gui/option_button.h"
#include "scene/gui/popup_menu.h"
#include "scene/resources/3d/box_shape_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/animation.h"
#include "scene/resources/animation_library.h"
#include "scene/resources/audio/audio_stream_randomizer.h"
#include "scene/resources/bone_map.h"
#include "scene/resources/font.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "scene/resources/multimesh.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/particle_process_material.h"
#include "scene/resources/physics_material.h"
#include "scene/resources/style_box_flat.h"
#include "scene/resources/theme.h"
#include "servers/audio/audio_bus_layout.h"
#include "servers/audio/audio_server.h"
#include "servers/rendering/rendering_server.h"

#include "modules/gdscript/gdscript.h"
#include "modules/tilemap/tile_map_layer.h"
#include "modules/tilemap/tile_set.h"
#include "modules/visual_shader/visual_shader.h"
#include "modules/visual_shader/vs_nodes/visual_shader_nodes.h"

static Dictionary _err(const String &p_message) {
	Dictionary result;
	result["ok"] = false;
	result["error"] = p_message;
	return result;
}

static Dictionary _ok() {
	Dictionary result;
	result["ok"] = true;
	return result;
}

static String _arg_string(const Dictionary &p_args, const String &p_a, const String &p_b, const String &p_fallback) {
	if (p_args.has(p_a) && p_args[p_a].get_type() == Variant::STRING && !String(p_args[p_a]).is_empty()) {
		return p_args[p_a];
	}
	if (!p_b.is_empty() && p_args.has(p_b) && p_args[p_b].get_type() == Variant::STRING && !String(p_args[p_b]).is_empty()) {
		return p_args[p_b];
	}
	return p_fallback;
}

static Node *_root_or_error(Dictionary &r_error) {
	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		r_error = _err("No scene is currently open");
	}
	return root;
}

static Node *_need_node(const Dictionary &p_args, const String &p_fallback, Dictionary &r_error) {
	Node *root = _root_or_error(r_error);
	if (!root) {
		return nullptr;
	}
	const String path = _arg_string(p_args, "node_path", "nodePath", p_fallback);
	Node *node = JustAMCPEditorSceneAccess::find_node(root, path);
	if (!node) {
		r_error = _err("Node not found: " + path);
	}
	return node;
}

static Dictionary _spawn(const Dictionary &p_args, const String &p_type, const String &p_default_name) {
	Dictionary error;
	Node *root = _root_or_error(error);
	if (!root) {
		return error;
	}
	const String parent_path = _arg_string(p_args, "parent_path", "parentNodePath", ".");
	Node *parent = JustAMCPEditorSceneAccess::find_node(root, parent_path);
	if (!parent) {
		return _err("Parent node not found: " + parent_path);
	}
	Object *created = ClassDB::instantiate(p_type);
	Node *node = Object::cast_to<Node>(created);
	if (!node) {
		if (created) {
			memdelete(created);
		}
		return _err("Failed to instantiate " + p_type);
	}
	node->set_name(_arg_string(p_args, "node_name", "nodeName", p_default_name));
	if (p_args.has("properties") && p_args["properties"].get_type() == Variant::DICTIONARY) {
		const Dictionary props = p_args["properties"];
		const Array keys = props.keys();
		for (int i = 0; i < keys.size(); i++) {
			node->set(keys[i], props[keys[i]]);
		}
	}
	JustAMCPEditorSceneAccess::add_child_with_undo(node, parent, root, "Add " + p_type);
	Dictionary result = _ok();
	result["node_type"] = p_type;
	result["node_name"] = String(node->get_name());
	result["node_path"] = JustAMCPEditorSceneAccess::safe_path_to(root, node);
	return result;
}

static bool _is_3d(const Dictionary &p_args) {
	return p_args.has("is_3d") && bool(p_args["is_3d"]);
}

static void _snapshot_file(const String &p_path) {
	JustAMCPAgentPolicy::note_file_undo(p_path);
}

static void _snapshot_setting(const String &p_setting) {
	ProjectSettings *settings = ProjectSettings::get_singleton();
	if (!settings) {
		return;
	}
	const bool existed = settings->has_setting(p_setting);
	JustAMCPAgentPolicy::note_setting_undo(p_setting, existed ? settings->get_setting(p_setting) : Variant(), existed);
}

static Dictionary _save_branch(const Dictionary &p_args) {
	Dictionary error;
	Node *node = _need_node(p_args, ".", error);
	if (!node) {
		return error;
	}
	const String dest = _arg_string(p_args, "dest_path", "path", "");
	if (!dest.begins_with("res://") || !dest.ends_with(".tscn")) {
		return _err("dest_path must be a res:// .tscn path");
	}
	Ref<PackedScene> packed;
	packed.instantiate();
	const Error pack_err = packed->pack(node);
	if (pack_err != OK) {
		return _err("Failed to pack branch");
	}
	_snapshot_file(dest);
	const Error save_err = ResourceSaver::save(packed, dest);
	if (save_err != OK) {
		return _err("Failed to save branch scene");
	}
	Dictionary result = _ok();
	result["path"] = dest;
	return result;
}

static Dictionary _replace_type(const Dictionary &p_args) {
	Dictionary error;
	Node *node = _need_node(p_args, "Sprite2D", error);
	if (!node) {
		return error;
	}
	const String type_name = _arg_string(p_args, "node_type", "nodeType", "Node2D");
	if (type_name.is_empty() || !ClassDB::class_exists(type_name) || !ClassDB::is_parent_class(type_name, "Node")) {
		return _err("node_type must be a Node class");
	}
	Object *created = ClassDB::instantiate(type_name);
	Node *replacement = Object::cast_to<Node>(created);
	if (!replacement) {
		if (created) {
			memdelete(created);
		}
		return _err("Failed to instantiate " + type_name);
	}
	replacement->set_name(node->get_name());
	if (Node2D *src = Object::cast_to<Node2D>(node)) {
		if (Node2D *dst = Object::cast_to<Node2D>(replacement)) {
			dst->set_transform(src->get_transform());
		}
	}
	if (Node3D *src3 = Object::cast_to<Node3D>(node)) {
		if (Node3D *dst3 = Object::cast_to<Node3D>(replacement)) {
			dst3->set_transform(src3->get_transform());
		}
	}
	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	Node *parent = node->get_parent();
	EditorUndoRedoManager *undo = EditorUndoRedoManager::get_singleton();
	if (parent && undo && Thread::is_main_thread() && JustAMCPEditorSceneAccess::in_edited_scene(node)) {
		const Node *previous_owner = node->get_owner();
		undo->create_action(String("Replace Node Type [") + JustAMCPAgentPolicy::current_session_id() + "]", UndoRedo::MERGE_DISABLE);
		undo->add_do_method(node, "replace_by", replacement, true);
		if (root) {
			undo->add_do_method(replacement, "set_owner", root);
		}
		undo->add_do_reference(replacement);
		undo->add_undo_method(replacement, "replace_by", node, true);
		if (previous_owner) {
			undo->add_undo_method(node, "set_owner", previous_owner);
		}
		undo->add_undo_reference(node);
		undo->commit_action();
	} else {
		node->replace_by(replacement, true);
		memdelete(node);
		if (root) {
			replacement->set_owner(root);
		}
	}
	Dictionary result = _ok();
	result["node_type"] = type_name;
	result["node_name"] = String(replacement->get_name());
	return result;
}

static Dictionary _unique_name(const Dictionary &p_args) {
	Dictionary error;
	Node *node = _need_node(p_args, "", error);
	if (!node) {
		return error;
	}
	const bool unique = !p_args.has("unique") || bool(p_args["unique"]);
	JustAMCPEditorSceneAccess::set_unique_name_with_undo(node, unique);
	Dictionary result = _ok();
	result["unique"] = node->is_unique_name_in_owner();
	return result;
}

static Dictionary _add_timer(const Dictionary &p_args) {
	Dictionary spawned = _spawn(p_args, "Timer", "GapTimer");
	if (!bool(spawned.get("ok", false))) {
		return spawned;
	}
	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	Node *timer = JustAMCPEditorSceneAccess::find_node(root, spawned["node_path"]);
	if (timer) {
		JustAMCPEditorSceneAccess::set_property_with_undo(timer, "wait_time", p_args.has("wait_time") ? double(p_args["wait_time"]) : 1.0, "Set Timer Wait Time");
		JustAMCPEditorSceneAccess::set_property_with_undo(timer, "autostart", p_args.has("autostart") && bool(p_args["autostart"]), "Set Timer Autostart");
	}
	return spawned;
}

static Vector2 _node_pos(Node *p_node, bool &r_ok) {
	r_ok = false;
	if (Node2D *node2 = Object::cast_to<Node2D>(p_node)) {
		r_ok = true;
		return node2->get_position();
	}
	if (Control *control = Object::cast_to<Control>(p_node)) {
		r_ok = true;
		return control->get_position();
	}
	return Vector2();
}

static void _set_axis(Node *p_node, const String &p_axis, float p_value) {
	bool ok = false;
	Vector2 pos = _node_pos(p_node, ok);
	if (!ok) {
		return;
	}
	if (p_axis == "y") {
		pos.y = p_value;
	} else {
		pos.x = p_value;
	}
	JustAMCPEditorSceneAccess::set_property_with_undo(p_node, "position", pos, "Set Node Position");
}

static Array _node_list(const Dictionary &p_args, Node *p_root) {
	Array paths;
	if (p_args.has("node_paths") && p_args["node_paths"].get_type() == Variant::ARRAY) {
		paths = p_args["node_paths"];
	} else if (p_args.has("node_path")) {
		paths.push_back(p_args["node_path"]);
	} else {
		paths.push_back("Sprite2D");
		paths.push_back("Camera2D");
	}
	Array nodes;
	for (int i = 0; i < paths.size() && i < 64; i++) {
		Node *node = JustAMCPEditorSceneAccess::find_node(p_root, paths[i]);
		if (node) {
			nodes.push_back(node);
		}
	}
	return nodes;
}

static Dictionary _align(const Dictionary &p_args) {
	Dictionary error;
	Node *root = _root_or_error(error);
	if (!root) {
		return error;
	}
	const Array nodes = _node_list(p_args, root);
	if (nodes.is_empty()) {
		return _err("No nodes to align");
	}
	const String axis = _arg_string(p_args, "axis", "", "x");
	float target = 0.0;
	bool have = false;
	for (int i = 0; i < nodes.size(); i++) {
		bool ok = false;
		const Vector2 pos = _node_pos(Object::cast_to<Node>(nodes[i]), ok);
		if (!ok) {
			continue;
		}
		const float value = axis == "y" ? pos.y : pos.x;
		if (!have || value < target) {
			target = value;
			have = true;
		}
	}
	if (!have) {
		return _err("Nodes have no 2D position");
	}
	for (int i = 0; i < nodes.size(); i++) {
		_set_axis(Object::cast_to<Node>(nodes[i]), axis, target);
	}
	Dictionary result = _ok();
	result["count"] = nodes.size();
	result["axis"] = axis;
	return result;
}

static Dictionary _distribute(const Dictionary &p_args) {
	Dictionary error;
	Node *root = _root_or_error(error);
	if (!root) {
		return error;
	}
	Array nodes = _node_list(p_args, root);
	if (nodes.size() < 2) {
		return _err("Distribute needs at least two nodes");
	}
	const String axis = _arg_string(p_args, "axis", "", "x");
	struct Item {
		Node *node = nullptr;
		float value = 0.0;
	};
	Vector<Item> items;
	for (int i = 0; i < nodes.size(); i++) {
		Node *node = Object::cast_to<Node>(nodes[i]);
		bool ok = false;
		const Vector2 pos = _node_pos(node, ok);
		if (!ok) {
			continue;
		}
		Item item;
		item.node = node;
		item.value = axis == "y" ? pos.y : pos.x;
		items.push_back(item);
	}
	if (items.size() < 2) {
		return _err("Nodes have no 2D position");
	}
	Item *sorted = items.ptrw();
	for (int i = 1; i < items.size(); i++) {
		const Item key = sorted[i];
		int j = i - 1;
		while (j >= 0 && sorted[j].value > key.value) {
			sorted[j + 1] = sorted[j];
			j--;
		}
		sorted[j + 1] = key;
	}
	const float first = items[0].value;
	const float last = items[items.size() - 1].value;
	const float step = (last - first) / float(items.size() - 1);
	for (int i = 0; i < items.size(); i++) {
		_set_axis(items[i].node, axis, first + step * float(i));
	}
	Dictionary result = _ok();
	result["count"] = items.size();
	return result;
}

static Dictionary _transform_2d(const Dictionary &p_args) {
	Dictionary error;
	Node *node = _need_node(p_args, "Sprite2D", error);
	if (!node) {
		return error;
	}
	Node2D *node2 = Object::cast_to<Node2D>(node);
	Control *control = Object::cast_to<Control>(node);
	if (!node2 && !control) {
		return _err("Node is not a Node2D or Control");
	}
	Vector2 pos = node2 ? node2->get_position() : control->get_position();
	if (p_args.has("x")) {
		pos.x = p_args["x"];
	}
	if (p_args.has("y")) {
		pos.y = p_args["y"];
	}
	JustAMCPEditorSceneAccess::set_property_with_undo(node, "position", pos, "Set Node Transform");
	if (node2) {
		if (p_args.has("rotation")) {
			JustAMCPEditorSceneAccess::set_property_with_undo(node2, "rotation", Math::deg_to_rad(double(p_args["rotation"])), "Set Node Rotation");
		}
		if (p_args.has("scale")) {
			const float scale = p_args["scale"];
			JustAMCPEditorSceneAccess::set_property_with_undo(node2, "scale", Vector2(scale, scale), "Set Node Scale");
		}
	}
	return _ok();
}

static Dictionary _transform_3d(const Dictionary &p_args) {
	Dictionary error;
	Node *node = _need_node(p_args, "MeshInstance3D", error);
	if (!node) {
		return error;
	}
	Node3D *node3 = Object::cast_to<Node3D>(node);
	if (!node3) {
		return _err("Node is not a Node3D");
	}
	Vector3 pos = node3->get_position();
	if (p_args.has("x")) {
		pos.x = p_args["x"];
	}
	if (p_args.has("y")) {
		pos.y = p_args["y"];
	}
	if (p_args.has("z")) {
		pos.z = p_args["z"];
	}
	JustAMCPEditorSceneAccess::set_property_with_undo(node3, "position", pos, "Set Node Transform");
	if (p_args.has("rotation_x") || p_args.has("rotation_y") || p_args.has("rotation_z")) {
		Vector3 rot = node3->get_rotation_degrees();
		if (p_args.has("rotation_x")) {
			rot.x = p_args["rotation_x"];
		}
		if (p_args.has("rotation_y")) {
			rot.y = p_args["rotation_y"];
		}
		if (p_args.has("rotation_z")) {
			rot.z = p_args["rotation_z"];
		}
		node3->set_rotation_degrees(rot);
	}
	return _ok();
}

static Dictionary _look_at(const Dictionary &p_args) {
	Dictionary error;
	Node *node = _need_node(p_args, "MeshInstance3D", error);
	if (!node) {
		return error;
	}
	Node3D *node3 = Object::cast_to<Node3D>(node);
	if (!node3) {
		return _err("Node is not a Node3D");
	}
	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	const String target_path = _arg_string(p_args, "target_path", "target", "Camera3D");
	Node *target = JustAMCPEditorSceneAccess::find_node(root, target_path);
	Node3D *target3 = Object::cast_to<Node3D>(target);
	if (!target3) {
		return _err("Target is not a Node3D: " + target_path);
	}
	node3->look_at(target3->get_global_position());
	return _ok();
}

static Dictionary _batch_get(const Dictionary &p_args) {
	Dictionary error;
	Node *root = _root_or_error(error);
	if (!root) {
		return error;
	}
	Array names;
	if (p_args.has("properties") && p_args["properties"].get_type() == Variant::ARRAY) {
		names = p_args["properties"];
	} else if (p_args.has("property")) {
		names.push_back(p_args["property"]);
	} else {
		names.push_back("position");
	}
	const Array nodes = _node_list(p_args, root);
	Array rows;
	for (int i = 0; i < nodes.size(); i++) {
		Node *node = Object::cast_to<Node>(nodes[i]);
		Dictionary row;
		row["node_path"] = JustAMCPEditorSceneAccess::safe_path_to(root, node);
		Dictionary values;
		for (int j = 0; j < names.size() && j < 32; j++) {
			const String prop = names[j];
			bool valid = false;
			const Variant value = node->get(prop, &valid);
			if (valid) {
				values[prop] = value;
			}
		}
		row["values"] = values;
		rows.push_back(row);
	}
	Dictionary result = _ok();
	result["nodes"] = rows;
	return result;
}

static Dictionary _across_scenes(const Dictionary &p_args) {
	Array scenes;
	if (p_args.has("scene_paths") && p_args["scene_paths"].get_type() == Variant::ARRAY) {
		scenes = p_args["scene_paths"];
	} else if (p_args.has("scene_path")) {
		scenes.push_back(p_args["scene_path"]);
	}
	const String node_path = _arg_string(p_args, "node_path", "", ".");
	const String property = _arg_string(p_args, "property", "", "");
	if (property.is_empty() || scenes.is_empty()) {
		return _err("scene_paths and property are required");
	}
	if (!p_args.has("value")) {
		return _err("value is required");
	}
	int changed = 0;
	for (int i = 0; i < scenes.size() && i < 8; i++) {
		const String path = scenes[i];
		Ref<PackedScene> packed = ResourceLoader::load(path);
		if (packed.is_null()) {
			continue;
		}
		Node *root = packed->instantiate();
		Node *node = JustAMCPEditorSceneAccess::find_node(root, node_path);
		if (node) {
			node->set(property, p_args["value"]);
			Ref<PackedScene> out;
			out.instantiate();
			if (out->pack(root) == OK) {
				_snapshot_file(path);
				if (ResourceSaver::save(out, path) == OK) {
					changed++;
				}
			}
		}
		memdelete(root);
	}
	Dictionary result = _ok();
	result["changed"] = changed;
	return result;
}

static void _ensure_parent_dir(const String &p_path) {
	const String base = p_path.get_base_dir();
	if (base.is_empty()) {
		return;
	}
	String absolute = base;
	if (ProjectSettings::get_singleton()) {
		absolute = ProjectSettings::get_singleton()->globalize_path(base);
	}
	Ref<DirAccess> fs = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (fs.is_valid() && !fs->dir_exists(absolute)) {
		fs->make_dir_recursive(absolute);
	}
}

static Dictionary _placeholder(const Dictionary &p_args) {
	const String path = _arg_string(p_args, "path", "dest_path", "user://justamcp_gap_placeholder.png");
	_ensure_parent_dir(path);
	Ref<Image> image;
	image.instantiate();
	const int size = 8;
	image->initialize_data(size, size, false, Image::FORMAT_RGBA8);
	image->fill(Color(0.2, 0.6, 0.9));
	_snapshot_file(path);
	const Error err = image->save_png(path);
	if (err != OK) {
		return _err("Failed to write placeholder image");
	}
	Dictionary result = _ok();
	result["path"] = path;
	result["width"] = size;
	result["height"] = size;
	return result;
}

static Dictionary _image_info(const Dictionary &p_args) {
	String path = _arg_string(p_args, "path", "image_path", "");
	if (path.is_empty()) {
		Dictionary written = _placeholder(p_args);
		if (!bool(written.get("ok", false))) {
			return written;
		}
		path = written["path"];
	}
	Ref<Image> image;
	image.instantiate();
	const Error err = image->load(path);
	if (err != OK) {
		return _err("Failed to load image: " + path);
	}
	Dictionary result = _ok();
	result["path"] = path;
	result["width"] = image->get_width();
	result["height"] = image->get_height();
	result["format"] = Image::get_format_name(image->get_format());
	return result;
}

static Dictionary _diff_images(const Dictionary &p_args) {
	String left_path = _arg_string(p_args, "left", "path", "");
	String right_path = _arg_string(p_args, "right", "other_path", "");
	if (left_path.is_empty() || right_path.is_empty()) {
		Dictionary left_args;
		left_args["path"] = "user://justamcp_gap_diff_left.png";
		Dictionary right_args;
		right_args["path"] = "user://justamcp_gap_diff_right.png";
		Dictionary left_written = _placeholder(left_args);
		Dictionary right_written = _placeholder(right_args);
		if (!bool(left_written.get("ok", false)) || !bool(right_written.get("ok", false))) {
			return _err("Failed to write images to compare");
		}
		left_path = left_written["path"];
		right_path = right_written["path"];
	}
	Ref<Image> left;
	left.instantiate();
	Ref<Image> right;
	right.instantiate();
	if (left_path.is_empty() || right_path.is_empty() || left->load(left_path) != OK || right->load(right_path) != OK) {
		return _err("left and right must be readable images");
	}
	const int width = MIN(left->get_width(), right->get_width());
	const int height = MIN(left->get_height(), right->get_height());
	int different = 0;
	const int cap = 64;
	const int step_x = MAX(1, width / cap);
	const int step_y = MAX(1, height / cap);
	int sampled = 0;
	for (int y = 0; y < height; y += step_y) {
		for (int x = 0; x < width; x += step_x) {
			sampled++;
			if (left->get_pixel(x, y) != right->get_pixel(x, y)) {
				different++;
			}
		}
	}
	Dictionary result = _ok();
	result["sampled"] = sampled;
	result["different"] = different;
	result["match"] = different == 0 && left->get_width() == right->get_width() && left->get_height() == right->get_height();
	return result;
}

static Dictionary _camera_limits(const Dictionary &p_args) {
	Dictionary error;
	Node *node = _need_node(p_args, "Camera2D", error);
	if (!node) {
		return error;
	}
	Camera2D *camera = Object::cast_to<Camera2D>(node);
	if (!camera) {
		return _err("Node is not a Camera2D");
	}
	if (p_args.has("left")) {
		camera->set_limit(SIDE_LEFT, p_args["left"]);
	}
	if (p_args.has("top")) {
		camera->set_limit(SIDE_TOP, p_args["top"]);
	}
	if (p_args.has("right")) {
		camera->set_limit(SIDE_RIGHT, p_args["right"]);
	}
	if (p_args.has("bottom")) {
		camera->set_limit(SIDE_BOTTOM, p_args["bottom"]);
	}
	return _ok();
}

static Dictionary _wire_button(const Dictionary &p_args) {
	Dictionary error;
	Node *button = _need_node(p_args, "Button", error);
	if (!button) {
		return error;
	}
	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	const String target_path = _arg_string(p_args, "target_path", "target", ".");
	Node *target = JustAMCPEditorSceneAccess::find_node(root, target_path);
	const String method = _arg_string(p_args, "method", "method_name", "queue_free");
	if (!target) {
		return _err("target_path is required");
	}
	if (!button->has_signal("pressed")) {
		return _err("Node has no pressed signal");
	}
	button->connect("pressed", Callable(target, method));
	return _ok();
}

static Dictionary _list_items(const Dictionary &p_args, bool p_option) {
	Dictionary error;
	Node *node = _need_node(p_args, p_option ? "OptionButton" : "ItemList", error);
	if (!node) {
		return error;
	}
	Array items;
	if (p_args.has("items") && p_args["items"].get_type() == Variant::ARRAY) {
		items = p_args["items"];
	}
	if (ItemList *list = Object::cast_to<ItemList>(node)) {
		list->clear();
		for (int i = 0; i < items.size() && i < 64; i++) {
			list->add_item(items[i]);
		}
	} else if (OptionButton *option = Object::cast_to<OptionButton>(node)) {
		option->clear();
		for (int i = 0; i < items.size() && i < 64; i++) {
			option->add_item(items[i]);
		}
	} else if (PopupMenu *menu = Object::cast_to<PopupMenu>(node)) {
		menu->clear();
		for (int i = 0; i < items.size() && i < 64; i++) {
			menu->add_item(items[i]);
		}
	} else {
		return _err("Node is not an ItemList, OptionButton, or PopupMenu");
	}
	Dictionary result = _ok();
	result["count"] = items.size();
	return result;
}

static Dictionary _control_style(const Dictionary &p_args) {
	Dictionary error;
	Node *node = _need_node(p_args, "Button", error);
	if (!node) {
		return error;
	}
	Control *control = Object::cast_to<Control>(node);
	if (!control) {
		return _err("Node is not a Control");
	}
	if (p_args.has("color")) {
		control->set_modulate(p_args["color"]);
	} else {
		Ref<StyleBoxFlat> box;
		box.instantiate();
		box->set_bg_color(Color(0.15, 0.2, 0.28));
		control->add_theme_style_override("normal", box);
	}
	return _ok();
}

static Dictionary _load_font(const Dictionary &p_args) {
	Dictionary error;
	Node *node = _need_node(p_args, "Button", error);
	if (!node) {
		return error;
	}
	Control *control = Object::cast_to<Control>(node);
	if (!control) {
		return _err("Node is not a Control");
	}
	const String path = _arg_string(p_args, "font_path", "path", "");
	Ref<Resource> font = path.is_empty() ? Ref<Resource>() : ResourceLoader::load(path);
	if (font.is_null()) {
		Object *system = ClassDB::instantiate("SystemFont");
		font = Object::cast_to<Resource>(system);
		if (font.is_null() && system) {
			memdelete(system);
		}
	}
	Ref<Font> font_ref = Object::cast_to<Font>(font.ptr());
	if (font_ref.is_null()) {
		return _err("Failed to load a font");
	}
	control->add_theme_font_override("font", font_ref);
	return _ok();
}

static Dictionary _texture_rect(const Dictionary &p_args) {
	Dictionary spawned = _spawn(p_args, "TextureRect", "GapTextureRect");
	if (!bool(spawned.get("ok", false))) {
		return spawned;
	}
	const String path = _arg_string(p_args, "texture_path", "path", "");
	if (!path.is_empty()) {
		Node *node = JustAMCPEditorSceneAccess::find_node(JustAMCPEditorSceneAccess::get_edited_root(), spawned["node_path"]);
		Ref<Resource> texture = ResourceLoader::load(path);
		if (node && texture.is_valid()) {
			node->set("texture", texture);
		}
	}
	return spawned;
}

static Dictionary _control_icon(const Dictionary &p_args) {
	Dictionary error;
	Node *node = _need_node(p_args, "Button", error);
	if (!node) {
		return error;
	}
	Button *button = Object::cast_to<Button>(node);
	if (!button) {
		return _err("Node is not a Button");
	}
	const String path = _arg_string(p_args, "texture_path", "path", "");
	Ref<Texture2D> texture;
	if (!path.is_empty()) {
		texture = ResourceLoader::load(path);
	}
	if (texture.is_null()) {
		if (!RenderingServer::get_singleton()) {
			return _err("RenderingServer is not available");
		}
		Ref<Image> image;
		image.instantiate();
		image->initialize_data(8, 8, false, Image::FORMAT_RGBA8);
		image->fill(Color(0.9, 0.4, 0.2));
		texture = ImageTexture::create_from_image(image);
	}
	if (texture.is_null()) {
		return _err("Failed to load icon");
	}
	button->set_button_icon(texture);
	return _ok();
}

static Dictionary _csg_operation(const Dictionary &p_args) {
	Dictionary error;
	Node *node = nullptr;
	if (!p_args.has("node_path")) {
		const Dictionary spawned = _spawn(p_args, "CSGBox3D", "GapCSG");
		if (!bool(spawned.get("ok", false))) {
			return spawned;
		}
		node = JustAMCPEditorSceneAccess::find_node(JustAMCPEditorSceneAccess::get_edited_root(), spawned["node_path"]);
	} else {
		node = _need_node(p_args, "", error);
		if (!node) {
			return error;
		}
	}
	if (!ClassDB::is_parent_class(node->get_class(), "CSGShape3D") && String(node->get_class()) != "CSGShape3D") {
		return _err("Node is not a CSG shape");
	}
	const String op = _arg_string(p_args, "operation", "", "union");
	int value = 0;
	if (op == "intersection") {
		value = 1;
	} else if (op == "subtraction") {
		value = 2;
	}
	JustAMCPEditorSceneAccess::set_property_with_undo(node, "operation", value, "Set CSG Operation");
	Dictionary result = _ok();
	result["operation"] = op;
	return result;
}

static Dictionary _material_preset(const Dictionary &p_args) {
	if (!RenderingServer::get_singleton()) {
		return _err("RenderingServer is not available");
	}
	Ref<StandardMaterial3D> material;
	material.instantiate();
	const String preset = _arg_string(p_args, "preset", "", "metal");
	if (preset == "glass") {
		material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		material->set_albedo(Color(0.8, 0.9, 1.0, 0.35));
		material->set_roughness(0.05);
	} else if (preset == "wood") {
		material->set_albedo(Color(0.45, 0.28, 0.14));
		material->set_roughness(0.8);
	} else {
		material->set_albedo(Color(0.75, 0.75, 0.78));
		material->set_metallic(1.0);
		material->set_roughness(0.25);
	}
	const String path = _arg_string(p_args, "path", "dest_path", "");
	if (!path.is_empty()) {
		_snapshot_file(path);
		if (ResourceSaver::save(material, path) != OK) {
			return _err("Failed to save material");
		}
	}
	Dictionary error;
	if (p_args.has("node_path")) {
		Node *node = _need_node(p_args, "MeshInstance3D", error);
		MeshInstance3D *mesh = Object::cast_to<MeshInstance3D>(node);
		if (mesh) {
			JustAMCPEditorSceneAccess::set_property_with_undo(mesh, "material_override", material, "Set Material Override");
		}
	}
	Dictionary result = _ok();
	result["preset"] = preset;
	if (!path.is_empty()) {
		result["path"] = path;
	}
	return result;
}

static Dictionary _material_property(const Dictionary &p_args) {
	Dictionary error;
	Node *node = _need_node(p_args, "MeshInstance3D", error);
	if (!node) {
		return error;
	}
	if (!RenderingServer::get_singleton()) {
		return _err("RenderingServer is not available");
	}
	const String property = _arg_string(p_args, "property", "", "albedo_color");
	const Variant value = p_args.has("value") ? p_args["value"] : Variant(Color(1, 1, 1));
	Ref<StandardMaterial3D> standard = Object::cast_to<StandardMaterial3D>(static_cast<Object *>(node->get("material_override")));
	if (standard.is_null()) {
		standard.instantiate();
		JustAMCPEditorSceneAccess::set_property_with_undo(node, "material_override", standard, "Set Material Override");
	}
	standard->set(property, value);
	return _ok();
}

static Dictionary _grid_cell(const Dictionary &p_args, bool p_region) {
	Dictionary error;
	Node *node = nullptr;
	if (!p_args.has("node_path")) {
		const Dictionary spawned = _spawn(p_args, "GridMap", "GapGridMap");
		if (!bool(spawned.get("ok", false))) {
			return spawned;
		}
		node = JustAMCPEditorSceneAccess::find_node(JustAMCPEditorSceneAccess::get_edited_root(), spawned["node_path"]);
	} else {
		node = _need_node(p_args, "", error);
	}
	if (!node || String(node->get_class()) != "GridMap") {
		return node ? _err("Node is not a GridMap") : error;
	}
	const int item = p_args.has("item") ? int(p_args["item"]) : 0;
	int painted = 0;
	if (!p_region) {
		const Vector3i cell(int(p_args.get("x", 0)), int(p_args.get("y", 0)), int(p_args.get("z", 0)));
		node->call("set_cell_item", cell, item);
		painted = 1;
	} else {
		const int x0 = int(p_args.get("x0", 0));
		const int y0 = int(p_args.get("y0", 0));
		const int z0 = int(p_args.get("z0", 0));
		const int x1 = int(p_args.get("x1", x0));
		const int y1 = int(p_args.get("y1", y0));
		const int z1 = int(p_args.get("z1", z0));
		for (int x = MIN(x0, x1); x <= MAX(x0, x1) && painted < 256; x++) {
			for (int y = MIN(y0, y1); y <= MAX(y0, y1) && painted < 256; y++) {
				for (int z = MIN(z0, z1); z <= MAX(z0, z1) && painted < 256; z++) {
					node->call("set_cell_item", Vector3i(x, y, z), item);
					painted++;
				}
			}
		}
	}
	Dictionary result = _ok();
	result["painted"] = painted;
	return result;
}

static Dictionary _terrain_mesh(const Dictionary &p_args) {
	if (!RenderingServer::get_singleton()) {
		return _err("RenderingServer is not available");
	}
	Dictionary spawned = _spawn(p_args, "MeshInstance3D", "GapTerrain");
	if (!bool(spawned.get("ok", false))) {
		return spawned;
	}
	Node *node = JustAMCPEditorSceneAccess::find_node(JustAMCPEditorSceneAccess::get_edited_root(), spawned["node_path"]);
	MeshInstance3D *mesh_node = Object::cast_to<MeshInstance3D>(node);
	if (!mesh_node) {
		return spawned;
	}
	Ref<PlaneMesh> mesh;
	mesh.instantiate();
	const int sub = CLAMP(int(p_args.get("subdivide", 4)), 1, 16);
	mesh->set_size(Size2(float(p_args.get("size", 8.0)), float(p_args.get("size", 8.0))));
	mesh->set_subdivide_width(sub);
	mesh->set_subdivide_depth(sub);
	mesh_node->set_mesh(mesh);
	spawned["subdivide"] = sub;
	return spawned;
}

static Dictionary _scatter(const Dictionary &p_args) {
	if (!RenderingServer::get_singleton()) {
		return _err("RenderingServer is not available");
	}
	Dictionary spawned = _spawn(p_args, "MultiMeshInstance3D", "GapScatter");
	if (!bool(spawned.get("ok", false))) {
		return spawned;
	}
	Node *node = JustAMCPEditorSceneAccess::find_node(JustAMCPEditorSceneAccess::get_edited_root(), spawned["node_path"]);
	MultiMeshInstance3D *instance = Object::cast_to<MultiMeshInstance3D>(node);
	if (!instance) {
		return spawned;
	}
	const int count = CLAMP(int(p_args.get("count", 4)), 1, 32);
	Ref<MultiMesh> multi;
	multi.instantiate();
	multi->set_transform_format(MultiMesh::TRANSFORM_3D);
	Ref<BoxMesh> box;
	box.instantiate();
	box->set_size(Vector3(0.2, 0.2, 0.2));
	multi->set_mesh(box);
	multi->set_instance_count(count);
	for (int i = 0; i < count; i++) {
		Transform3D xf;
		xf.origin = Vector3(float(i) * 0.4, 0, 0);
		multi->set_instance_transform(i, xf);
	}
	instance->set_multimesh(multi);
	spawned["count"] = count;
	return spawned;
}

static Ref<TileSet> _load_tileset(const Dictionary &p_args, Dictionary &r_error) {
	const String path = _arg_string(p_args, "tileset_path", "path", "");
	Ref<TileSet> tileset;
	if (!path.is_empty()) {
		tileset = ResourceLoader::load(path);
	}
	Dictionary error;
	Node *node = _need_node(p_args, "MyTileMap", error);
	if (tileset.is_null()) {
		tileset.instantiate();
	}
	if (node) {
		JustAMCPEditorSceneAccess::set_property_with_undo(node, "tile_set", tileset, "Set TileSet");
	} else if (tileset.is_null()) {
		r_error = error.is_empty() ? _err("TileSet not found") : error;
	}
	return tileset;
}

static Dictionary _tileset_layers(const Dictionary &p_args) {
	Dictionary error;
	Ref<TileSet> tileset = _load_tileset(p_args, error);
	if (tileset.is_null()) {
		return error;
	}
	tileset->add_physics_layer();
	Dictionary result = _ok();
	result["physics_layers"] = tileset->get_physics_layers_count();
	return result;
}

static Dictionary _tileset_terrains(const Dictionary &p_args) {
	Dictionary error;
	Ref<TileSet> tileset = _load_tileset(p_args, error);
	if (tileset.is_null()) {
		return error;
	}
	tileset->add_terrain_set();
	const int set_index = tileset->get_terrain_sets_count() - 1;
	tileset->add_terrain(set_index);
	tileset->set_terrain_name(set_index, 0, _arg_string(p_args, "terrain_name", "", "GapTerrain"));
	Dictionary result = _ok();
	result["terrain_sets"] = tileset->get_terrain_sets_count();
	return result;
}

static Dictionary _tileset_tile_data(const Dictionary &p_args) {
	Dictionary error;
	Ref<TileSet> tileset = _load_tileset(p_args, error);
	if (tileset.is_null()) {
		return error;
	}
	Ref<TileSetAtlasSource> source;
	if (tileset->get_source_count() > 0) {
		source = Object::cast_to<TileSetAtlasSource>(tileset->get_source(tileset->get_source_id(0)).ptr());
	}
	if (source.is_null()) {
		source.instantiate();
		tileset->add_source(source, 0);
	}
	const Vector2i coords(int(p_args.get("x", 0)), int(p_args.get("y", 0)));
	if (!source->has_tile(coords)) {
		source->create_tile(coords);
	}
	Dictionary result = _ok();
	result["x"] = coords.x;
	result["y"] = coords.y;
	return result;
}

static Dictionary _tile_cells(const Dictionary &p_args) {
	Dictionary error;
	Node *node = _need_node(p_args, "MyTileMap", error);
	if (!node) {
		return error;
	}
	TileMapLayer *layer = Object::cast_to<TileMapLayer>(node);
	if (!layer) {
		return _err("Node is not a TileMapLayer");
	}
	Array cells;
	if (p_args.has("cells") && p_args["cells"].get_type() == Variant::ARRAY) {
		cells = p_args["cells"];
	} else {
		Dictionary one;
		one["x"] = int(p_args.get("x", 0));
		one["y"] = int(p_args.get("y", 0));
		cells.push_back(one);
	}
	const int source = int(p_args.get("source_id", 0));
	int painted = 0;
	for (int i = 0; i < cells.size() && i < 256; i++) {
		Vector2i coords;
		if (cells[i].get_type() == Variant::DICTIONARY) {
			const Dictionary cell = cells[i];
			coords = Vector2i(int(cell.get("x", 0)), int(cell.get("y", 0)));
		} else if (cells[i].get_type() == Variant::VECTOR2I) {
			coords = cells[i];
		}
		layer->set_cell(coords, source, coords);
		painted++;
	}
	Dictionary result = _ok();
	result["painted"] = painted;
	return result;
}

static Dictionary _paint_terrain(const Dictionary &p_args) {
	Dictionary error;
	Node *node = _need_node(p_args, "MyTileMap", error);
	if (!node) {
		return error;
	}
	TileMapLayer *layer = Object::cast_to<TileMapLayer>(node);
	if (!layer) {
		return _err("Node is not a TileMapLayer");
	}
	TypedArray<Vector2i> cells;
	if (p_args.has("cells") && p_args["cells"].get_type() == Variant::ARRAY) {
		const Array raw = p_args["cells"];
		for (int i = 0; i < raw.size() && i < 256; i++) {
			if (raw[i].get_type() == Variant::DICTIONARY) {
				const Dictionary cell = raw[i];
				cells.push_back(Vector2i(int(cell.get("x", 0)), int(cell.get("y", 0))));
			}
		}
	}
	if (cells.is_empty()) {
		cells.push_back(Vector2i(int(p_args.get("x", 0)), int(p_args.get("y", 0))));
	}
	layer->set_cells_terrain_connect(cells, int(p_args.get("terrain_set", 0)), int(p_args.get("terrain", 0)), true);
	Dictionary result = _ok();
	result["painted"] = cells.size();
	return result;
}

static Dictionary _assign_theme(const Dictionary &p_args) {
	Dictionary error;
	Node *node = _need_node(p_args, "Button", error);
	if (!node) {
		return error;
	}
	Control *control = Object::cast_to<Control>(node);
	if (!control) {
		return _err("Node is not a Control");
	}
	const String path = _arg_string(p_args, "theme_path", "path", "res://tests/fixtures/bench/sample_theme.tres");
	Ref<Theme> theme = ResourceLoader::load(path);
	if (theme.is_null()) {
		return _err("Failed to load theme");
	}
	JustAMCPEditorSceneAccess::set_property_with_undo(control, "theme", theme, "Assign Theme");
	Dictionary result = _ok();
	result["path"] = path;
	return result;
}

static Dictionary _merge_theme(const Dictionary &p_args) {
	Dictionary error;
	Node *node = _need_node(p_args, "Button", error);
	Control *control = Object::cast_to<Control>(node);
	if (!control) {
		return error.is_empty() ? _err("Node is not a Control") : error;
	}
	const String extra = _arg_string(p_args, "other_path", "source_path", "res://tests/fixtures/bench/sample_theme.tres");
	Ref<Theme> source = ResourceLoader::load(extra);
	if (source.is_null()) {
		return _err("Theme path must load");
	}
	Ref<Theme> theme = control->get_theme();
	if (theme.is_null()) {
		theme.instantiate();
	} else {
		theme = Object::cast_to<Theme>(theme->duplicate().ptr());
	}
	theme->merge_with(source);
	JustAMCPEditorSceneAccess::set_property_with_undo(control, "theme", theme, "Merge Theme");
	return _ok();
}

static Dictionary _configure_theme(const Dictionary &p_args) {
	Dictionary error;
	Node *node = _need_node(p_args, "Button", error);
	Control *control = Object::cast_to<Control>(node);
	if (!control) {
		return error.is_empty() ? _err("Node is not a Control") : error;
	}
	Ref<Theme> theme;
	theme.instantiate();
	const String type_name = _arg_string(p_args, "theme_type", "type", "Button");
	theme->set_color(_arg_string(p_args, "color_name", "", "font_color"), type_name, Color(0.9, 0.9, 0.95));
	theme->set_constant(_arg_string(p_args, "constant_name", "", "h_separation"), type_name, int(p_args.get("constant", 4)));
	JustAMCPEditorSceneAccess::set_property_with_undo(control, "theme", theme, "Configure Theme");
	const String path = _arg_string(p_args, "path", "theme_path", "");
	if (path.begins_with("user://") || (p_args.has("path") && path.begins_with("res://"))) {
		_snapshot_file(path);
		if (ResourceSaver::save(theme, path) != OK) {
			return _err("Failed to save theme");
		}
	}
	return _ok();
}

static String _shader_preset_text(const String &p_preset) {
	if (p_preset == "canvas") {
		return "shader_type canvas_item;\n\nvoid fragment() {\n\tCOLOR = vec4(1.0, 0.4, 0.2, 1.0);\n}\n";
	}
	if (p_preset == "outline") {
		return "shader_type canvas_item;\n\nvoid fragment() {\n\tCOLOR = texture(TEXTURE, UV);\n}\n";
	}
	return "shader_type spatial;\n\nvoid fragment() {\n\tALBEDO = vec3(0.8, 0.8, 0.85);\n\tROUGHNESS = 0.4;\n}\n";
}

static Dictionary _shader_preset(const Dictionary &p_args) {
	const String path = _arg_string(p_args, "path", "shader_path", "user://justamcp_gap_preset.gdshader");
	_ensure_parent_dir(path);
	_snapshot_file(path);
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
	if (file.is_null()) {
		return _err("Failed to write shader preset");
	}
	file->store_string(_shader_preset_text(_arg_string(p_args, "preset", "", "spatial")));
	Dictionary result = _ok();
	result["path"] = path;
	return result;
}

static VisualShader::Type _visual_type(const String &p_name) {
	if (p_name == "vertex") {
		return VisualShader::TYPE_VERTEX;
	}
	if (p_name == "light") {
		return VisualShader::TYPE_LIGHT;
	}
	return VisualShader::TYPE_FRAGMENT;
}

static Ref<VisualShader> _load_visual(const Dictionary &p_args, Dictionary &r_error, bool p_create) {
	const String path = _arg_string(p_args, "path", "shader_path", "user://justamcp_gap_visual.tres");
	Ref<VisualShader> shader = ResourceLoader::load(path);
	if (shader.is_null() && p_create) {
		if (!RenderingServer::get_singleton()) {
			r_error = _err("RenderingServer is not available");
			return Ref<VisualShader>();
		}
		shader.instantiate();
		shader->set_mode(Shader::MODE_SPATIAL);
	}
	if (shader.is_null()) {
		r_error = _err("Visual shader not found: " + path);
	}
	return shader;
}

static Dictionary _save_visual(const Ref<VisualShader> &p_shader, const String &p_path) {
	_ensure_parent_dir(p_path);
	_snapshot_file(p_path);
	if (ResourceSaver::save(p_shader, p_path) != OK) {
		return _err("Failed to save visual shader");
	}
	Dictionary result = _ok();
	result["path"] = p_path;
	return result;
}

static Dictionary _create_visual(const Dictionary &p_args) {
	if (!RenderingServer::get_singleton()) {
		return _err("RenderingServer is not available");
	}
	Ref<VisualShader> shader;
	shader.instantiate();
	const String mode = _arg_string(p_args, "mode", "", "spatial");
	shader->set_mode(mode == "canvas_item" ? Shader::MODE_CANVAS_ITEM : Shader::MODE_SPATIAL);
	return _save_visual(shader, _arg_string(p_args, "path", "shader_path", "user://justamcp_gap_visual.tres"));
}

static Dictionary _add_visual_node(const Dictionary &p_args) {
	Dictionary error;
	Ref<VisualShader> shader = _load_visual(p_args, error, true);
	if (shader.is_null()) {
		return error;
	}
	const VisualShader::Type type = _visual_type(_arg_string(p_args, "shader_type", "stage", "fragment"));
	Ref<VisualShaderNodeFloatConstant> node;
	node.instantiate();
	node->set_constant(float(p_args.get("value", 1.0)));
	const int id = shader->get_valid_node_id(type);
	shader->add_node(type, node, Vector2(float(p_args.get("x", 0)), float(p_args.get("y", 0))), id);
	Dictionary result = _save_visual(shader, _arg_string(p_args, "path", "shader_path", "user://justamcp_gap_visual.tres"));
	if (bool(result.get("ok", false))) {
		result["node_id"] = id;
	}
	return result;
}

static Dictionary _connect_visual(const Dictionary &p_args, bool p_disconnect) {
	Dictionary error;
	Ref<VisualShader> shader = _load_visual(p_args, error, true);
	if (shader.is_null()) {
		return error;
	}
	const VisualShader::Type type = _visual_type(_arg_string(p_args, "shader_type", "stage", "fragment"));
	int from_node = int(p_args.get("from_node", -1));
	const int from_port = int(p_args.get("from_port", 0));
	const int to_node = int(p_args.get("to_node", 0));
	const int to_port = int(p_args.get("to_port", 0));
	if (from_node < 0 || shader->get_node(type, from_node).is_null()) {
		Ref<VisualShaderNodeFloatConstant> node;
		node.instantiate();
		from_node = shader->get_valid_node_id(type);
		shader->add_node(type, node, Vector2(), from_node);
	}
	if (p_disconnect) {
		shader->disconnect_nodes(type, from_node, from_port, to_node, to_port);
	} else if (shader->can_connect_nodes(type, from_node, from_port, to_node, to_port)) {
		if (shader->connect_nodes(type, from_node, from_port, to_node, to_port) != OK) {
			return _err("Failed to connect visual shader nodes");
		}
	}
	return _save_visual(shader, _arg_string(p_args, "path", "shader_path", "user://justamcp_gap_visual.tres"));
}

static Dictionary _remove_visual_node(const Dictionary &p_args) {
	Dictionary error;
	Ref<VisualShader> shader = _load_visual(p_args, error, true);
	if (shader.is_null()) {
		return error;
	}
	const VisualShader::Type type = _visual_type(_arg_string(p_args, "shader_type", "stage", "fragment"));
	int node_id = int(p_args.get("node_id", -1));
	if (node_id < 0 || shader->get_node(type, node_id).is_null()) {
		Ref<VisualShaderNodeFloatConstant> created;
		created.instantiate();
		node_id = shader->get_valid_node_id(type);
		shader->add_node(type, created, Vector2(), node_id);
	}
	shader->remove_node(type, node_id);
	return _save_visual(shader, _arg_string(p_args, "path", "shader_path", "user://justamcp_gap_visual.tres"));
}

static Dictionary _visual_node_property(const Dictionary &p_args) {
	Dictionary error;
	Ref<VisualShader> shader = _load_visual(p_args, error, true);
	if (shader.is_null()) {
		return error;
	}
	const VisualShader::Type type = _visual_type(_arg_string(p_args, "shader_type", "stage", "fragment"));
	int node_id = int(p_args.get("node_id", -1));
	Ref<VisualShaderNode> node = node_id < 0 ? Ref<VisualShaderNode>() : shader->get_node(type, node_id);
	if (node.is_null()) {
		Ref<VisualShaderNodeFloatConstant> created;
		created.instantiate();
		node_id = shader->get_valid_node_id(type);
		shader->add_node(type, created, Vector2(), node_id);
		node = created;
	}
	if (node.is_null()) {
		return _err("Visual shader node not found");
	}
	const Variant value = p_args.get("value", 1.0);
	if (VisualShaderNodeFloatConstant *constant = Object::cast_to<VisualShaderNodeFloatConstant>(node.ptr())) {
		constant->set_constant(float(value));
	} else {
		node->set(_arg_string(p_args, "property", "", "constant"), value);
	}
	return _save_visual(shader, _arg_string(p_args, "path", "shader_path", "user://justamcp_gap_visual.tres"));
}

static Dictionary _visual_info(const Dictionary &p_args) {
	Dictionary error;
	Ref<VisualShader> shader = _load_visual(p_args, error, true);
	if (shader.is_null()) {
		return error;
	}
	const VisualShader::Type type = _visual_type(_arg_string(p_args, "shader_type", "stage", "fragment"));
	Dictionary result = _ok();
	result["node_count"] = shader->get_node_list(type).size();
	result["mode"] = shader->get_mode();
	return result;
}

static Skeleton3D *_skeleton(const Dictionary &p_args, Dictionary &r_error) {
	Node *node = _need_node(p_args, "Skeleton3D", r_error);
	Skeleton3D *skeleton = Object::cast_to<Skeleton3D>(node);
	if (node && !skeleton) {
		r_error = _err("Node is not a Skeleton3D");
	}
	return skeleton;
}

static Dictionary _add_bone(const Dictionary &p_args) {
	Dictionary error;
	Skeleton3D *skeleton = _skeleton(p_args, error);
	if (!skeleton) {
		return error;
	}
	const String name = _arg_string(p_args, "bone_name", "name", "GapBone");
	const int index = skeleton->add_bone(name);
	Dictionary result = _ok();
	result["bone"] = index;
	result["bone_name"] = name;
	return result;
}

static Dictionary _bone_rest(const Dictionary &p_args) {
	Dictionary error;
	Skeleton3D *skeleton = _skeleton(p_args, error);
	if (!skeleton) {
		return error;
	}
	int index = int(p_args.get("bone", 0));
	if (skeleton->get_bone_count() == 0) {
		index = skeleton->add_bone("GapBone");
	}
	if (index < 0 || index >= skeleton->get_bone_count()) {
		return _err("Bone index is out of range");
	}
	Transform3D rest = skeleton->get_bone_rest(index);
	rest.origin = Vector3(float(p_args.get("x", rest.origin.x)), float(p_args.get("y", rest.origin.y)), float(p_args.get("z", rest.origin.z)));
	skeleton->set_bone_rest(index, rest);
	return _ok();
}

static Dictionary _bone_pose(const Dictionary &p_args) {
	Dictionary error;
	Skeleton3D *skeleton = _skeleton(p_args, error);
	if (!skeleton) {
		return error;
	}
	int index = int(p_args.get("bone", 0));
	if (skeleton->get_bone_count() == 0) {
		index = skeleton->add_bone("GapBone");
	}
	if (index < 0 || index >= skeleton->get_bone_count()) {
		return _err("Bone index is out of range");
	}
	skeleton->set_bone_pose_position(index, Vector3(float(p_args.get("x", 0)), float(p_args.get("y", 0)), float(p_args.get("z", 0))));
	return _ok();
}

static Dictionary _reset_poses(const Dictionary &p_args) {
	Dictionary error;
	Skeleton3D *skeleton = _skeleton(p_args, error);
	if (!skeleton) {
		return error;
	}
	skeleton->reset_bone_poses();
	return _ok();
}

static Dictionary _skeleton_info(const Dictionary &p_args) {
	Dictionary error;
	Skeleton3D *skeleton = _skeleton(p_args, error);
	if (!skeleton) {
		return error;
	}
	Array names;
	for (int i = 0; i < skeleton->get_bone_count() && i < 64; i++) {
		names.push_back(skeleton->get_bone_name(i));
	}
	Dictionary result = _ok();
	result["bones"] = names;
	result["count"] = skeleton->get_bone_count();
	return result;
}

static Dictionary _blend_shape(const Dictionary &p_args) {
	Dictionary error;
	Node *node = _need_node(p_args, "MeshInstance3D", error);
	if (!node) {
		return error;
	}
	MeshInstance3D *mesh = Object::cast_to<MeshInstance3D>(node);
	if (!mesh) {
		return _err("Node is not a MeshInstance3D");
	}
	const String shape = _arg_string(p_args, "shape", "name", "gap");
	int index = mesh->find_blend_shape_by_name(shape);
	if (index < 0) {
		if (!RenderingServer::get_singleton()) {
			return _err("RenderingServer is not available");
		}
		Ref<ArrayMesh> array_mesh;
		array_mesh.instantiate();
		array_mesh->add_blend_shape(shape);
		mesh->set_mesh(array_mesh);
		index = mesh->find_blend_shape_by_name(shape);
	}
	if (index < 0) {
		return _err("Blend shape was not found");
	}
	mesh->set_blend_shape_value(index, float(p_args.get("value", 0.0)));
	return _ok();
}

static Dictionary _mesh_info(const Dictionary &p_args) {
	Dictionary error;
	Node *node = _need_node(p_args, "MeshInstance3D", error);
	if (!node) {
		return error;
	}
	MeshInstance3D *mesh_node = Object::cast_to<MeshInstance3D>(node);
	Ref<Mesh> mesh = mesh_node ? mesh_node->get_mesh() : Ref<Mesh>();
	if (mesh.is_null()) {
		return _err("Node has no mesh");
	}
	Dictionary result = _ok();
	result["surfaces"] = mesh->get_surface_count();
	Array shapes;
	for (int i = 0; i < mesh->get_blend_shape_count() && i < 32; i++) {
		shapes.push_back(mesh->get_blend_shape_name(i));
	}
	result["blend_shapes"] = shapes;
	return result;
}

static Dictionary _model_anims(const Dictionary &p_args) {
	const String path = _arg_string(p_args, "path", "model_path", "");
	if (path.is_empty()) {
		return _err("path is required");
	}
	Ref<PackedScene> packed = ResourceLoader::load(path);
	if (packed.is_null()) {
		return _err("Failed to load model scene");
	}
	Node *root = packed->instantiate();
	Array names;
	AnimationPlayer *player = Object::cast_to<AnimationPlayer>(root);
	if (!player) {
		player = Object::cast_to<AnimationPlayer>(root->find_child("AnimationPlayer", true, false));
	}
	if (player) {
		LocalVector<StringName> list;
		player->get_animation_list(&list);
		for (const StringName &name : list) {
			names.push_back(String(name));
		}
	}
	memdelete(root);
	Dictionary result = _ok();
	result["animations"] = names;
	return result;
}

static Dictionary _bone_map(const Dictionary &p_args, bool p_auto) {
	Ref<BoneMap> map;
	map.instantiate();
	if (p_auto) {
		map->set("profile", Variant());
	}
	const String path = _arg_string(p_args, "path", "dest_path", "user://justamcp_gap_bonemap.tres");
	_snapshot_file(path);
	if (ResourceSaver::save(map, path) != OK) {
		return _err("Failed to save bone map");
	}
	Dictionary result = _ok();
	result["path"] = path;
	result["auto"] = p_auto;
	return result;
}

static AnimationPlayer *_player(const Dictionary &p_args, Dictionary &r_error) {
	Node *node = _need_node(p_args, "AnimationPlayer", r_error);
	AnimationPlayer *player = Object::cast_to<AnimationPlayer>(node);
	if (node && !player) {
		r_error = _err("Node is not an AnimationPlayer");
	}
	return player;
}

static Ref<Animation> _animation(AnimationPlayer *p_player, const Dictionary &p_args, Dictionary &r_error) {
	const String name = _arg_string(p_args, "animation", "animation_name", "bench");
	Ref<Animation> animation = p_player->get_animation(name);
	if (animation.is_null()) {
		r_error = _err("Animation not found: " + name);
	}
	return animation;
}

static Dictionary _rename_animation(const Dictionary &p_args) {
	Dictionary error;
	AnimationPlayer *player = _player(p_args, error);
	if (!player) {
		return error;
	}
	Ref<AnimationLibrary> library = player->get_animation_library("");
	if (library.is_null()) {
		return _err("Animation library is missing");
	}
	const String from = _arg_string(p_args, "from", "animation", "bench");
	const String to = _arg_string(p_args, "to", "new_name", "bench_gap");
	if (!library->has_animation(from)) {
		return _err("Animation not found: " + from);
	}
	library->rename_animation(from, to);
	Dictionary result = _ok();
	result["name"] = to;
	return result;
}

static Dictionary _list_tracks(const Dictionary &p_args) {
	Dictionary error;
	AnimationPlayer *player = _player(p_args, error);
	if (!player) {
		return error;
	}
	Ref<Animation> animation = _animation(player, p_args, error);
	if (animation.is_null()) {
		return error;
	}
	Array tracks;
	for (int i = 0; i < animation->get_track_count() && i < 64; i++) {
		Dictionary track;
		track["index"] = i;
		track["path"] = String(animation->track_get_path(i));
		track["type"] = animation->track_get_type(i);
		tracks.push_back(track);
	}
	Dictionary result = _ok();
	result["tracks"] = tracks;
	return result;
}

static Dictionary _configure_track(const Dictionary &p_args) {
	Dictionary error;
	AnimationPlayer *player = _player(p_args, error);
	if (!player) {
		return error;
	}
	Ref<Animation> animation = _animation(player, p_args, error);
	if (animation.is_null()) {
		return error;
	}
	const int index = animation->add_track(Animation::TYPE_VALUE);
	animation->track_set_path(index, NodePath(_arg_string(p_args, "track_path", "path", "Sprite2D:position")));
	animation->track_insert_key(index, 0.0, Vector2());
	Dictionary result = _ok();
	result["track"] = index;
	return result;
}

static Dictionary _remove_track(const Dictionary &p_args) {
	Dictionary error;
	AnimationPlayer *player = _player(p_args, error);
	if (!player) {
		return error;
	}
	Ref<Animation> animation = _animation(player, p_args, error);
	if (animation.is_null()) {
		return error;
	}
	if (animation->get_track_count() <= 0) {
		return _err("Animation has no tracks");
	}
	const int index = int(p_args.get("track", animation->get_track_count() - 1));
	animation->remove_track(index);
	return _ok();
}

static Dictionary _set_keys(const Dictionary &p_args) {
	Dictionary error;
	AnimationPlayer *player = _player(p_args, error);
	if (!player) {
		return error;
	}
	Ref<Animation> animation = _animation(player, p_args, error);
	if (animation.is_null()) {
		return error;
	}
	int track = int(p_args.get("track", 0));
	if (animation->get_track_count() <= 0) {
		track = animation->add_track(Animation::TYPE_VALUE);
		animation->track_set_path(track, NodePath("Sprite2D:position"));
	}
	Array keys;
	if (p_args.has("keys") && p_args["keys"].get_type() == Variant::ARRAY) {
		keys = p_args["keys"];
	}
	if (keys.is_empty()) {
		animation->track_insert_key(track, double(p_args.get("time", 0.0)), p_args.get("value", Vector2()));
	} else {
		for (int i = 0; i < keys.size() && i < 32; i++) {
			if (keys[i].get_type() != Variant::DICTIONARY) {
				continue;
			}
			const Dictionary key = keys[i];
			animation->track_insert_key(track, double(key.get("time", 0.0)), key.get("value", Vector2()));
		}
	}
	return _ok();
}

static Dictionary _remove_key(const Dictionary &p_args) {
	Dictionary error;
	AnimationPlayer *player = _player(p_args, error);
	if (!player) {
		return error;
	}
	Ref<Animation> animation = _animation(player, p_args, error);
	if (animation.is_null()) {
		return error;
	}
	const int track = int(p_args.get("track", 0));
	const int key = int(p_args.get("key", 0));
	if (track < 0 || track >= animation->get_track_count() || key < 0 || key >= animation->track_get_key_count(track)) {
		return _err("Keyframe is out of range");
	}
	animation->track_remove_key(track, key);
	return _ok();
}

static Ref<AnimationNodeBlendTree> _blend_tree(const Dictionary &p_args, Dictionary &r_error) {
	Node *node = _need_node(p_args, "BlendTree", r_error);
	AnimationTree *tree = Object::cast_to<AnimationTree>(node);
	if (node && !tree) {
		r_error = _err("Node is not an AnimationTree");
		return Ref<AnimationNodeBlendTree>();
	}
	Ref<AnimationRootNode> root_node;
	if (tree) {
		root_node = tree->get_root_animation_node();
	}
	Ref<AnimationNodeBlendTree> blend = Object::cast_to<AnimationNodeBlendTree>(root_node.ptr());
	if (node && blend.is_null() && r_error.is_empty()) {
		r_error = _err("Node has no AnimationNodeBlendTree");
	}
	return blend;
}

static Dictionary _connect_blend(const Dictionary &p_args, bool p_remove) {
	Dictionary error;
	Ref<AnimationNodeBlendTree> tree = _blend_tree(p_args, error);
	if (tree.is_null()) {
		return error;
	}
	const String name = _arg_string(p_args, "node_name", "name", "GapClip");
	if (!tree->has_node(name)) {
		Ref<AnimationNodeAnimation> clip;
		clip.instantiate();
		clip->set_animation(_arg_string(p_args, "animation", "", "bench"));
		tree->add_node(name, clip, Vector2(160, 80));
	}
	if (p_remove) {
		tree->remove_node(name);
	} else {
		tree->connect_node("output", int(p_args.get("input", 0)), name);
	}
	return _ok();
}

static Dictionary _physics_material(const Dictionary &p_args) {
	Dictionary error;
	Node *node = _need_node(p_args, "CharacterBody2D", error);
	if (!node) {
		return error;
	}
	Ref<PhysicsMaterial> material;
	material.instantiate();
	material->set_friction(float(p_args.get("friction", 1.0)));
	material->set_bounce(float(p_args.get("bounce", 0.0)));
	JustAMCPEditorSceneAccess::set_property_with_undo(node, "physics_material_override", material, "Set Physics Material");
	return _ok();
}

static Dictionary _layers_by_name(const Dictionary &p_args) {
	Dictionary error;
	Node *node = _need_node(p_args, "CharacterBody2D", error);
	if (!node) {
		return error;
	}
	const String layer_name = _arg_string(p_args, "layer_name", "name", "gap");
	const String prefix = _is_3d(p_args) ? "layer_names/3d_physics/layer_" : "layer_names/2d_physics/layer_";
	int bit = 0;
	for (int i = 1; i <= 32; i++) {
		if (String(ProjectSettings::get_singleton()->get_setting(prefix + itos(i), "")) == layer_name) {
			bit = 1 << (i - 1);
			break;
		}
	}
	if (bit == 0) {
		const String setting_name = prefix + "1";
		_snapshot_setting(setting_name);
		ProjectSettings::get_singleton()->set_setting(setting_name, layer_name);
		bit = 1;
	}
	JustAMCPEditorSceneAccess::set_property_with_undo(node, "collision_layer", bit, "Set Collision Layer");
	JustAMCPEditorSceneAccess::set_property_with_undo(node, "collision_mask", bit, "Set Collision Mask");
	Dictionary result = _ok();
	result["layer"] = bit;
	return result;
}

static Dictionary _particle_process(const Dictionary &p_args) {
	Dictionary error;
	Node *node = _need_node(p_args, "GPUParticles2D", error);
	if (!node) {
		return error;
	}
	Ref<ParticleProcessMaterial> material = Object::cast_to<ParticleProcessMaterial>(static_cast<Object *>(node->get("process_material")));
	if (material.is_null()) {
		material.instantiate();
		JustAMCPEditorSceneAccess::set_property_with_undo(node, "process_material", material, "Set Particle Process");
	}
	if (p_args.has("gravity") && p_args["gravity"].get_type() == Variant::VECTOR3) {
		material->set_gravity(p_args["gravity"]);
	} else {
		material->set_gravity(Vector3(0, float(p_args.get("gravity_y", 98.0)), 0));
	}
	return _ok();
}

static Dictionary _audio_randomizer(const Dictionary &p_args) {
	Dictionary spawned = _spawn(p_args, "AudioStreamPlayer", "GapRandomizer");
	if (!bool(spawned.get("ok", false))) {
		return spawned;
	}
	Node *node = JustAMCPEditorSceneAccess::find_node(JustAMCPEditorSceneAccess::get_edited_root(), spawned["node_path"]);
	if (node) {
		Ref<AudioStreamRandomizer> stream;
		stream.instantiate();
		JustAMCPEditorSceneAccess::set_property_with_undo(node, "stream", stream, "Set Audio Stream");
	}
	return spawned;
}

static Dictionary _save_bus_layout(const Dictionary &p_args) {
	const String path = _arg_string(p_args, "path", "dest_path", "user://justamcp_gap_bus_layout.tres");
	Ref<AudioBusLayout> layout = AudioServer::get_singleton()->generate_bus_layout();
	_snapshot_file(path);
	if (ResourceSaver::save(layout, path) != OK) {
		return _err("Failed to save bus layout");
	}
	Dictionary result = _ok();
	result["path"] = path;
	return result;
}

static Dictionary _list_plugins(const Dictionary &) {
	Array enabled = ProjectSettings::get_singleton()->get_setting("editor_plugins/enabled", Array());
	Dictionary result = _ok();
	result["enabled"] = enabled;
	return result;
}

static Dictionary _enable_plugin(const Dictionary &p_args) {
	const String addon = _arg_string(p_args, "plugin", "path", "");
	if (addon.is_empty()) {
		return _err("plugin is required");
	}
	const bool enabled = !p_args.has("enabled") || bool(p_args["enabled"]);
	_snapshot_setting("editor_plugins/enabled");
	if (EditorNode::get_singleton()) {
		EditorNode::get_singleton()->set_addon_plugin_enabled(addon, enabled, true);
	} else {
		Array list = ProjectSettings::get_singleton()->get_setting("editor_plugins/enabled", Array());
		list.erase(addon);
		if (enabled) {
			list.push_back(addon);
		}
		ProjectSettings::get_singleton()->set_setting("editor_plugins/enabled", list);
	}
	Dictionary result = _ok();
	result["plugin"] = addon;
	result["enabled"] = enabled;
	return result;
}

static Dictionary _import_options(const Dictionary &p_args) {
	const String path = _arg_string(p_args, "path", "file_path", "");
	if (path.is_empty()) {
		return _err("path is required");
	}
	Ref<ConfigFile> config;
	config.instantiate();
	const String import_path = path + ".import";
	if (config->load(import_path) != OK) {
		config->set_value("remap", "importer", "texture");
	}
	if (p_args.has("options") && p_args["options"].get_type() == Variant::DICTIONARY) {
		const Dictionary options = p_args["options"];
		const Array keys = options.keys();
		for (int i = 0; i < keys.size() && i < 32; i++) {
			config->set_value("params", keys[i], options[keys[i]]);
		}
	}
	_snapshot_file(import_path);
	if (config->save(import_path) != OK) {
		return _err("Failed to save import options");
	}
	return _ok();
}

static Dictionary _check_scripts(const Dictionary &) {
	int checked = 0;
	int failed = 0;
	Ref<DirAccess> dir = DirAccess::open("res://");
	if (dir.is_valid()) {
		dir->list_dir_begin();
		String name = dir->get_next();
		while (!name.is_empty() && checked < 32) {
			if (!dir->current_is_dir() && name.ends_with(".gd")) {
				checked++;
				Ref<GDScript> script;
				script.instantiate();
				script->set_source_code(FileAccess::get_file_as_string("res://" + name));
				if (script->reload(false) != OK) {
					failed++;
				}
			}
			name = dir->get_next();
		}
	}
	Dictionary result = _ok();
	result["checked"] = checked;
	result["failed"] = failed;
	return result;
}

static const int k_scene_walk_cap = 4096;

struct GapWalkStats {
	int nodes = 0;
	int cameras = 0;
	int lights = 0;
	Rect2 bounds;
	bool have_bounds = false;
	bool capped = false;
};

static GapWalkStats _walk_scene(Node *p_root, bool p_visual) {
	GapWalkStats stats;
	if (!p_root) {
		return stats;
	}
	Vector<Node *> pending;
	pending.push_back(p_root);
	while (!pending.is_empty() && stats.nodes < k_scene_walk_cap) {
		Node *node = pending[pending.size() - 1];
		pending.remove_at(pending.size() - 1);
		stats.nodes++;
		if (p_visual) {
			const String cls = node->get_class();
			if (cls.contains("Camera")) {
				stats.cameras++;
			}
			if (cls.contains("Light")) {
				stats.lights++;
			}
			if (Node2D *node2 = Object::cast_to<Node2D>(node)) {
				const Vector2 pos = node2->get_position();
				if (!stats.have_bounds) {
					stats.bounds = Rect2(pos, Size2());
					stats.have_bounds = true;
				} else {
					stats.bounds = stats.bounds.expand(pos);
				}
			}
		}
		const int children = node->get_child_count();
		for (int i = 0; i < children; i++) {
			if (stats.nodes + pending.size() >= k_scene_walk_cap) {
				stats.capped = true;
				break;
			}
			pending.push_back(node->get_child(i));
		}
	}
	if (stats.nodes >= k_scene_walk_cap) {
		stats.capped = true;
	}
	return stats;
}

static Dictionary _scene_summary(bool p_visual) {
	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return Dictionary();
	}
	const GapWalkStats stats = _walk_scene(root, p_visual);
	Dictionary visual;
	visual["nodes"] = stats.nodes;
	visual["capped"] = stats.capped;
	if (p_visual) {
		visual["cameras"] = stats.cameras;
		visual["lights"] = stats.lights;
		visual["bounds"] = stats.bounds;
	}
	return visual;
}

static Dictionary _describe_visual(const Dictionary &) {
	if (!JustAMCPEditorSceneAccess::get_edited_root()) {
		return _err("No scene is currently open");
	}
	Dictionary result = _scene_summary(true);
	result["ok"] = true;
	return result;
}

static Dictionary _add_translation(const Dictionary &p_args) {
	const String path = _arg_string(p_args, "path", "file_path", "user://justamcp_gap_locale.csv");
	const String key = _arg_string(p_args, "key", "message", "GAP_HELLO");
	const String value = _arg_string(p_args, "value", "text", "Hello");
	String existing;
	if (FileAccess::exists(path)) {
		existing = FileAccess::get_file_as_string(path);
	}
	_snapshot_file(path);
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
	if (file.is_null()) {
		return _err("Failed to write translation file");
	}
	if (existing.is_empty()) {
		file->store_string("keys,en\n");
	} else {
		file->store_string(existing);
		if (!existing.ends_with("\n")) {
			file->store_string("\n");
		}
	}
	file->store_string(key + "," + value + "\n");
	if (path.begins_with("res://")) {
		_snapshot_setting("internationalization/locale/translations");
		Array translations = ProjectSettings::get_singleton()->get_setting("internationalization/locale/translations", Array());
		translations.push_back(path);
		ProjectSettings::get_singleton()->set_setting("internationalization/locale/translations", translations);
	}
	Dictionary result = _ok();
	result["path"] = path;
	result["key"] = key;
	return result;
}

static Dictionary _set_locale(const Dictionary &p_args) {
	const String locale = _arg_string(p_args, "locale", "code", "en");
	_snapshot_setting("internationalization/locale/locale");
	ProjectSettings::get_singleton()->set_setting("internationalization/locale/locale", locale);
	if (p_args.has("persist") && bool(p_args["persist"])) {
		ProjectSettings::get_singleton()->save();
	}
	Dictionary result = _ok();
	result["locale"] = locale;
	return result;
}

static Dictionary _export_preset(const Dictionary &p_args) {
	const String path = _arg_string(p_args, "path", "file_path", "res://export_presets.cfg");
	Ref<ConfigFile> config;
	config.instantiate();
	config->load(path);
	const String name = _arg_string(p_args, "name", "preset", "Gap Preset");
	const String platform = _arg_string(p_args, "platform", "", "Windows Desktop");
	int index = 0;
	while (config->has_section("preset." + itos(index)) && index < 32) {
		index++;
	}
	const String section = "preset." + itos(index);
	config->set_value(section, "name", name);
	config->set_value(section, "platform", platform);
	config->set_value(section, "runnable", true);
	config->set_value(section + ".options", "custom_template/debug", "");
	_snapshot_file(path);
	if (config->save(path) != OK) {
		return _err("Failed to write export presets");
	}
	Dictionary result = _ok();
	result["preset"] = name;
	result["index"] = index;
	return result;
}

struct GapIOJob {
	Variant request_id;
	String kind;
	Dictionary args;
	Dictionary scene;
};

static Dictionary _run_gap_io(const String &p_kind, const Dictionary &p_args, const Dictionary &p_scene) {
	if (p_kind == "image_info") {
		return _image_info(p_args);
	}
	if (p_kind == "diff_images") {
		return _diff_images(p_args);
	}
	if (p_kind == "scripts") {
		return _check_scripts(p_args);
	}
	if (p_kind == "across") {
		return _across_scenes(p_args);
	}
	if (p_kind == "health") {
		Dictionary result = _ok();
		result["scripts"] = _check_scripts(p_args);
		result["scene"] = p_scene;
		return result;
	}
	return _err("Unknown gap io job");
}

static void _gap_io_worker(void *p_userdata) {
	GapIOJob *job = static_cast<GapIOJob *>(p_userdata);
	Dictionary result = _run_gap_io(job->kind, job->args, job->scene);
	if (JustAMCPServer *server = JustAMCPServer::get_singleton()) {
		server->call_deferred("_deferred_complete_tool_dict", job->request_id, result);
	}
	memdelete(job);
}

static Dictionary _schedule_gap_io(const String &p_kind, const Dictionary &p_args, const Dictionary &p_scene = Dictionary()) {
	if (Thread::is_main_thread()) {
		const Variant request_id = justamcp_get_active_tool_request_id();
		WorkerThreadPool *pool = WorkerThreadPool::get_singleton();
		if (request_id.get_type() != Variant::NIL && pool) {
			GapIOJob *job = memnew(GapIOJob);
			job->request_id = request_id;
			job->kind = p_kind;
			job->args = p_args;
			job->scene = p_scene;
			pool->add_native_task(&_gap_io_worker, job, false, "JustAMCPGapIO");
			Dictionary pending;
			pending["_justamcp_async_pending"] = true;
			return pending;
		}
	}
	return _run_gap_io(p_kind, p_args, p_scene);
}

static const char *const _gap_tools[] = {
	"save_branch_as_scene",
	"replace_node_type",
	"set_unique_name",
	"add_timer",
	"align_nodes",
	"distribute_nodes",
	"set_node_transform_2d",
	"set_node_transform_3d",
	"look_at_node",
	"batch_get_properties",
	"set_property_across_scenes",
	"add_sprite",
	"add_animated_sprite",
	"add_canvas_modulate",
	"add_light_2d",
	"add_light_occluder_2d",
	"create_placeholder_texture",
	"set_camera_limits",
	"add_ui_control",
	"add_ui_container",
	"add_progress_bar",
	"add_ui_dialog",
	"configure_popup_menu",
	"configure_menu_bar",
	"set_item_list_items",
	"set_option_button_items",
	"set_texture_rect",
	"set_control_icon",
	"load_font",
	"set_control_style",
	"wire_button",
	"add_marker",
	"add_remote_transform",
	"add_path",
	"add_path_follow",
	"add_visibility_notifier",
	"add_decal",
	"add_reflection_probe",
	"add_subviewport",
	"add_bone_attachment_3d",
	"add_text_mesh",
	"add_spring_arm",
	"add_global_illumination",
	"create_material_preset",
	"set_material_property",
	"set_csg_operation",
	"gridmap_set_cell",
	"paint_gridmap_region",
	"create_terrain",
	"scatter_multimesh",
	"configure_tileset_layers",
	"configure_tileset_terrains",
	"set_tileset_tile_data",
	"tilemap_paint_terrain",
	"tilemap_set_cells",
	"assign_theme",
	"merge_theme",
	"configure_theme",
	"add_shader_preset",
	"create_visual_shader",
	"add_visual_shader_node",
	"connect_visual_shader_nodes",
	"disconnect_visual_shader_nodes",
	"remove_visual_shader_node",
	"set_visual_shader_node_property",
	"get_visual_shader_info",
	"add_bone",
	"set_bone_rest",
	"set_bone_pose",
	"reset_bone_poses",
	"get_skeleton_info",
	"set_blend_shape",
	"get_mesh_info",
	"list_model_animations",
	"create_skeleton_2d",
	"create_bone_map",
	"auto_bone_map",
	"add_ik_chain",
	"add_look_at_modifier",
	"rename_animation",
	"list_animation_tracks",
	"configure_animation_track",
	"remove_animation_track",
	"set_animation_keyframes",
	"remove_animation_keyframe",
	"connect_animation_blend_nodes",
	"remove_animation_blend_node",
	"add_collision_polygon",
	"add_mesh_collision",
	"add_shape_cast",
	"set_physics_material",
	"set_collision_layers_by_name",
	"add_particle_collision",
	"set_particle_process",
	"create_audio_randomizer",
	"save_bus_layout",
	"add_navigation_obstacle",
	"add_navigation_link",
	"list_plugins",
	"enable_plugin",
	"set_import_options",
	"get_image_info",
	"diff_images",
	"check_all_scripts",
	"project_health_report",
	"describe_scene_visual",
	"add_translation",
	"set_locale",
	"create_export_preset",
	nullptr
};

bool JustAMCPAddonGapTools::handles(const String &p_tool_name) {
	for (int i = 0; _gap_tools[i]; i++) {
		if (p_tool_name == _gap_tools[i]) {
			return true;
		}
	}
	return false;
}

Dictionary JustAMCPAddonGapTools::execute(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "save_branch_as_scene") {
		return _save_branch(p_args);
	}
	if (p_tool_name == "replace_node_type") {
		return _replace_type(p_args);
	}
	if (p_tool_name == "set_unique_name") {
		return _unique_name(p_args);
	}
	if (p_tool_name == "add_timer") {
		return _add_timer(p_args);
	}
	if (p_tool_name == "align_nodes") {
		return _align(p_args);
	}
	if (p_tool_name == "distribute_nodes") {
		return _distribute(p_args);
	}
	if (p_tool_name == "set_node_transform_2d") {
		return _transform_2d(p_args);
	}
	if (p_tool_name == "set_node_transform_3d") {
		return _transform_3d(p_args);
	}
	if (p_tool_name == "look_at_node") {
		return _look_at(p_args);
	}
	if (p_tool_name == "batch_get_properties") {
		return _batch_get(p_args);
	}
	if (p_tool_name == "set_property_across_scenes") {
		return _schedule_gap_io("across", p_args);
	}
	if (p_tool_name == "add_sprite") {
		return _spawn(p_args, "Sprite2D", "GapSprite");
	}
	if (p_tool_name == "add_animated_sprite") {
		return _spawn(p_args, "AnimatedSprite2D", "GapAnimatedSprite");
	}
	if (p_tool_name == "add_canvas_modulate") {
		return _spawn(p_args, "CanvasModulate", "GapCanvasModulate");
	}
	if (p_tool_name == "add_light_2d") {
		return _spawn(p_args, "PointLight2D", "GapLight2D");
	}
	if (p_tool_name == "add_light_occluder_2d") {
		return _spawn(p_args, "LightOccluder2D", "GapOccluder2D");
	}
	if (p_tool_name == "create_placeholder_texture") {
		return _placeholder(p_args);
	}
	if (p_tool_name == "set_camera_limits") {
		return _camera_limits(p_args);
	}
	if (p_tool_name == "add_ui_control") {
		return _spawn(p_args, _arg_string(p_args, "node_type", "control_type", "Label"), "GapControl");
	}
	if (p_tool_name == "add_ui_container") {
		return _spawn(p_args, _arg_string(p_args, "node_type", "container_type", "VBoxContainer"), "GapContainer");
	}
	if (p_tool_name == "add_progress_bar") {
		return _spawn(p_args, "ProgressBar", "GapProgress");
	}
	if (p_tool_name == "add_ui_dialog") {
		return _spawn(p_args, "AcceptDialog", "GapDialog");
	}
	if (p_tool_name == "configure_popup_menu") {
		if (p_args.has("node_path")) {
			return _list_items(p_args, false);
		}
		Dictionary spawned = _spawn(p_args, "PopupMenu", "GapPopup");
		if (bool(spawned.get("ok", false))) {
			Dictionary args = p_args;
			args["node_path"] = spawned["node_path"];
			_list_items(args, false);
		}
		return spawned;
	}
	if (p_tool_name == "configure_menu_bar") {
		return _spawn(p_args, "MenuBar", "GapMenuBar");
	}
	if (p_tool_name == "set_item_list_items") {
		if (p_args.has("node_path")) {
			return _list_items(p_args, false);
		}
		Dictionary spawned = _spawn(p_args, "ItemList", "GapItemList");
		if (!bool(spawned.get("ok", false))) {
			return spawned;
		}
		Dictionary args = p_args;
		args["node_path"] = spawned["node_path"];
		const Dictionary filled = _list_items(args, false);
		if (!bool(filled.get("ok", false))) {
			return filled;
		}
		spawned["count"] = filled.get("count", 0);
		return spawned;
	}
	if (p_tool_name == "set_option_button_items") {
		if (p_args.has("node_path")) {
			return _list_items(p_args, true);
		}
		Dictionary spawned = _spawn(p_args, "OptionButton", "GapOptionButton");
		if (!bool(spawned.get("ok", false))) {
			return spawned;
		}
		Dictionary args = p_args;
		args["node_path"] = spawned["node_path"];
		const Dictionary filled = _list_items(args, true);
		if (!bool(filled.get("ok", false))) {
			return filled;
		}
		spawned["count"] = filled.get("count", 0);
		return spawned;
	}
	if (p_tool_name == "set_texture_rect") {
		return _texture_rect(p_args);
	}
	if (p_tool_name == "set_control_icon") {
		return _control_icon(p_args);
	}
	if (p_tool_name == "load_font") {
		return _load_font(p_args);
	}
	if (p_tool_name == "set_control_style") {
		return _control_style(p_args);
	}
	if (p_tool_name == "wire_button") {
		return _wire_button(p_args);
	}
	if (p_tool_name == "add_marker") {
		return _spawn(p_args, _is_3d(p_args) ? "Marker3D" : "Marker2D", "GapMarker");
	}
	if (p_tool_name == "add_remote_transform") {
		return _spawn(p_args, _is_3d(p_args) ? "RemoteTransform3D" : "RemoteTransform2D", "GapRemote");
	}
	if (p_tool_name == "add_path") {
		return _spawn(p_args, _is_3d(p_args) ? "Path3D" : "Path2D", "GapPath");
	}
	if (p_tool_name == "add_path_follow") {
		return _spawn(p_args, _is_3d(p_args) ? "PathFollow3D" : "PathFollow2D", "GapPathFollow");
	}
	if (p_tool_name == "add_visibility_notifier") {
		return _spawn(p_args, _is_3d(p_args) ? "VisibleOnScreenNotifier3D" : "VisibleOnScreenNotifier2D", "GapNotifier");
	}
	if (p_tool_name == "add_decal") {
		return _spawn(p_args, "Decal", "GapDecal");
	}
	if (p_tool_name == "add_reflection_probe") {
		return _spawn(p_args, "ReflectionProbe", "GapReflectionProbe");
	}
	if (p_tool_name == "add_subviewport") {
		return _spawn(p_args, "SubViewport", "GapSubViewport");
	}
	if (p_tool_name == "add_bone_attachment_3d") {
		return _spawn(p_args, "BoneAttachment3D", "GapBoneAttachment");
	}
	if (p_tool_name == "add_text_mesh") {
		Dictionary spawned = _spawn(p_args, "MeshInstance3D", "GapTextMesh");
		if (!bool(spawned.get("ok", false))) {
			return spawned;
		}
		MeshInstance3D *mesh_node = Object::cast_to<MeshInstance3D>(JustAMCPEditorSceneAccess::find_node(JustAMCPEditorSceneAccess::get_edited_root(), spawned["node_path"]));
		if (mesh_node) {
			Ref<TextMesh> text;
			text.instantiate();
			text->set_text(_arg_string(p_args, "text", "", "Gap"));
			mesh_node->set_mesh(text);
		}
		spawned["node_type"] = "MeshInstance3D";
		return spawned;
	}
	if (p_tool_name == "add_spring_arm") {
		return _spawn(p_args, "SpringArm3D", "GapSpringArm");
	}
	if (p_tool_name == "add_global_illumination") {
		return _spawn(p_args, "VoxelGI", "GapVoxelGI");
	}
	if (p_tool_name == "create_material_preset") {
		return _material_preset(p_args);
	}
	if (p_tool_name == "set_material_property") {
		return _material_property(p_args);
	}
	if (p_tool_name == "set_csg_operation") {
		return _csg_operation(p_args);
	}
	if (p_tool_name == "gridmap_set_cell") {
		return _grid_cell(p_args, false);
	}
	if (p_tool_name == "paint_gridmap_region") {
		return _grid_cell(p_args, true);
	}
	if (p_tool_name == "create_terrain") {
		return _terrain_mesh(p_args);
	}
	if (p_tool_name == "scatter_multimesh") {
		return _scatter(p_args);
	}
	if (p_tool_name == "configure_tileset_layers") {
		return _tileset_layers(p_args);
	}
	if (p_tool_name == "configure_tileset_terrains") {
		return _tileset_terrains(p_args);
	}
	if (p_tool_name == "set_tileset_tile_data") {
		return _tileset_tile_data(p_args);
	}
	if (p_tool_name == "tilemap_paint_terrain") {
		return _paint_terrain(p_args);
	}
	if (p_tool_name == "tilemap_set_cells") {
		return _tile_cells(p_args);
	}
	if (p_tool_name == "assign_theme") {
		return _assign_theme(p_args);
	}
	if (p_tool_name == "merge_theme") {
		return _merge_theme(p_args);
	}
	if (p_tool_name == "configure_theme") {
		return _configure_theme(p_args);
	}
	if (p_tool_name == "add_shader_preset") {
		return _shader_preset(p_args);
	}
	if (p_tool_name == "create_visual_shader") {
		return _create_visual(p_args);
	}
	if (p_tool_name == "add_visual_shader_node") {
		return _add_visual_node(p_args);
	}
	if (p_tool_name == "connect_visual_shader_nodes") {
		return _connect_visual(p_args, false);
	}
	if (p_tool_name == "disconnect_visual_shader_nodes") {
		return _connect_visual(p_args, true);
	}
	if (p_tool_name == "remove_visual_shader_node") {
		return _remove_visual_node(p_args);
	}
	if (p_tool_name == "set_visual_shader_node_property") {
		return _visual_node_property(p_args);
	}
	if (p_tool_name == "get_visual_shader_info") {
		return _visual_info(p_args);
	}
	if (p_tool_name == "add_bone") {
		return _add_bone(p_args);
	}
	if (p_tool_name == "set_bone_rest") {
		return _bone_rest(p_args);
	}
	if (p_tool_name == "set_bone_pose") {
		return _bone_pose(p_args);
	}
	if (p_tool_name == "reset_bone_poses") {
		return _reset_poses(p_args);
	}
	if (p_tool_name == "get_skeleton_info") {
		return _skeleton_info(p_args);
	}
	if (p_tool_name == "set_blend_shape") {
		return _blend_shape(p_args);
	}
	if (p_tool_name == "get_mesh_info") {
		return _mesh_info(p_args);
	}
	if (p_tool_name == "list_model_animations") {
		return _model_anims(p_args);
	}
	if (p_tool_name == "create_skeleton_2d") {
		return _spawn(p_args, "Skeleton2D", "GapSkeleton2D");
	}
	if (p_tool_name == "create_bone_map") {
		return _bone_map(p_args, false);
	}
	if (p_tool_name == "auto_bone_map") {
		return _bone_map(p_args, true);
	}
	if (p_tool_name == "add_ik_chain") {
		return _spawn(p_args, "SkeletonIK3D", "GapIK");
	}
	if (p_tool_name == "add_look_at_modifier") {
		return _spawn(p_args, "LookAtModifier3D", "GapLookAt");
	}
	if (p_tool_name == "rename_animation") {
		return _rename_animation(p_args);
	}
	if (p_tool_name == "list_animation_tracks") {
		return _list_tracks(p_args);
	}
	if (p_tool_name == "configure_animation_track") {
		return _configure_track(p_args);
	}
	if (p_tool_name == "remove_animation_track") {
		return _remove_track(p_args);
	}
	if (p_tool_name == "set_animation_keyframes") {
		return _set_keys(p_args);
	}
	if (p_tool_name == "remove_animation_keyframe") {
		return _remove_key(p_args);
	}
	if (p_tool_name == "connect_animation_blend_nodes") {
		return _connect_blend(p_args, false);
	}
	if (p_tool_name == "remove_animation_blend_node") {
		return _connect_blend(p_args, true);
	}
	if (p_tool_name == "add_collision_polygon") {
		return _spawn(p_args, _is_3d(p_args) ? "CollisionPolygon3D" : "CollisionPolygon2D", "GapCollisionPolygon");
	}
	if (p_tool_name == "add_mesh_collision") {
		Dictionary spawned = _spawn(p_args, "CollisionShape3D", "GapMeshCollision");
		if (bool(spawned.get("ok", false))) {
			Node *node = JustAMCPEditorSceneAccess::find_node(JustAMCPEditorSceneAccess::get_edited_root(), spawned["node_path"]);
			if (node) {
				Ref<BoxShape3D> shape;
				shape.instantiate();
				JustAMCPEditorSceneAccess::set_property_with_undo(node, "shape", shape, "Set Collision Shape");
			}
		}
		return spawned;
	}
	if (p_tool_name == "add_shape_cast") {
		return _spawn(p_args, _is_3d(p_args) ? "ShapeCast3D" : "ShapeCast2D", "GapShapeCast");
	}
	if (p_tool_name == "set_physics_material") {
		return _physics_material(p_args);
	}
	if (p_tool_name == "set_collision_layers_by_name") {
		return _layers_by_name(p_args);
	}
	if (p_tool_name == "add_particle_collision") {
		return _spawn(p_args, "GPUParticlesCollisionBox3D", "GapParticleCollision");
	}
	if (p_tool_name == "set_particle_process") {
		return _particle_process(p_args);
	}
	if (p_tool_name == "create_audio_randomizer") {
		return _audio_randomizer(p_args);
	}
	if (p_tool_name == "save_bus_layout") {
		return _save_bus_layout(p_args);
	}
	if (p_tool_name == "add_navigation_obstacle") {
		return _spawn(p_args, _is_3d(p_args) ? "NavigationObstacle3D" : "NavigationObstacle2D", "GapNavObstacle");
	}
	if (p_tool_name == "add_navigation_link") {
		return _spawn(p_args, _is_3d(p_args) ? "NavigationLink3D" : "NavigationLink2D", "GapNavLink");
	}
	if (p_tool_name == "list_plugins") {
		return _list_plugins(p_args);
	}
	if (p_tool_name == "enable_plugin") {
		return _enable_plugin(p_args);
	}
	if (p_tool_name == "set_import_options") {
		return _import_options(p_args);
	}
	if (p_tool_name == "get_image_info") {
		return _schedule_gap_io("image_info", p_args);
	}
	if (p_tool_name == "diff_images") {
		return _schedule_gap_io("diff_images", p_args);
	}
	if (p_tool_name == "check_all_scripts") {
		return _schedule_gap_io("scripts", p_args);
	}
	if (p_tool_name == "project_health_report") {
		Dictionary scene = _scene_summary(false);
		if (scene.is_empty()) {
			scene["nodes"] = 0;
			scene["capped"] = false;
		}
		return _schedule_gap_io("health", p_args, scene);
	}
	if (p_tool_name == "describe_scene_visual") {
		return _describe_visual(p_args);
	}
	if (p_tool_name == "add_translation") {
		return _add_translation(p_args);
	}
	if (p_tool_name == "set_locale") {
		return _set_locale(p_args);
	}
	if (p_tool_name == "create_export_preset") {
		return _export_preset(p_args);
	}
	return _err("Unknown addon gap tool: " + p_tool_name);
}

#endif
