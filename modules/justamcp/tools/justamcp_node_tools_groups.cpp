/**************************************************************************/
/*  justamcp_node_tools_groups.cpp                                        */
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

#include "../justamcp_editor_scene_access.h"
#include "justamcp_node_tools.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#endif

#include "../justamcp_mcp_tool_macros.h"

#include "core/config/project_settings.h" // IWYU pragma: keep
#include "core/io/resource_loader.h" // IWYU pragma: keep
#include "core/object/script_language.h" // IWYU pragma: keep
#include "scene/gui/control.h"

static String _justamcp_safe_path_to(Node *p_root, Node *p_node) {
	return JustAMCPEditorSceneAccess::safe_path_to(p_root, p_node);
}

Dictionary JustAMCPNodeTools::_set_anchor_preset(const Dictionary &p_params) {
	if (!p_params.has("node_path")) {
		return MCP_INVALID_PARAMS("Missing param: node_path");
	}
	if (!p_params.has("preset")) {
		return MCP_INVALID_PARAMS("Missing param: preset");
	}

	String node_path = p_params["node_path"];
	String preset_name = p_params["preset"];
	bool keep_offsets = p_params.has("keep_offsets") ? bool(p_params["keep_offsets"]) : false;

	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No scene is currently open");
	}

	Node *node = _find_node_by_path(node_path);
	if (!node || !Object::cast_to<Control>(node)) {
		return MCP_NOT_FOUND("Control node '" + node_path + "'");
	}
	Control *control = Object::cast_to<Control>(node);

	Control::LayoutPreset preset = Control::PRESET_TOP_LEFT;
	if (preset_name == "top_left") {
		preset = Control::PRESET_TOP_LEFT;
	} else if (preset_name == "top_right") {
		preset = Control::PRESET_TOP_RIGHT;
	} else if (preset_name == "bottom_left") {
		preset = Control::PRESET_BOTTOM_LEFT;
	} else if (preset_name == "bottom_right") {
		preset = Control::PRESET_BOTTOM_RIGHT;
	} else if (preset_name == "center_left") {
		preset = Control::PRESET_CENTER_LEFT;
	} else if (preset_name == "center_top") {
		preset = Control::PRESET_CENTER_TOP;
	} else if (preset_name == "center_right") {
		preset = Control::PRESET_CENTER_RIGHT;
	} else if (preset_name == "center_bottom") {
		preset = Control::PRESET_CENTER_BOTTOM;
	} else if (preset_name == "center") {
		preset = Control::PRESET_CENTER;
	} else if (preset_name == "left_wide") {
		preset = Control::PRESET_LEFT_WIDE;
	} else if (preset_name == "top_wide") {
		preset = Control::PRESET_TOP_WIDE;
	} else if (preset_name == "right_wide") {
		preset = Control::PRESET_RIGHT_WIDE;
	} else if (preset_name == "bottom_wide") {
		preset = Control::PRESET_BOTTOM_WIDE;
	} else if (preset_name == "vcenter_wide") {
		preset = Control::PRESET_VCENTER_WIDE;
	} else if (preset_name == "hcenter_wide") {
		preset = Control::PRESET_HCENTER_WIDE;
	} else if (preset_name == "full_rect") {
		preset = Control::PRESET_FULL_RECT;
	} else {
		return MCP_INVALID_PARAMS("Unknown preset");
	}

	Control::LayoutPresetMode mode = keep_offsets ? Control::PRESET_MODE_KEEP_SIZE : Control::PRESET_MODE_MINSIZE;

#ifdef TOOLS_ENABLED
	if (EditorUndoRedoManager::get_singleton()) {
		EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
		ur->create_action("MCP: Set anchor preset");

		ur->add_do_method(control, "set_anchors_and_offsets_preset", preset, mode);

		ur->add_undo_property(control, "anchor_left", control->get_anchor(SIDE_LEFT));
		ur->add_undo_property(control, "anchor_top", control->get_anchor(SIDE_TOP));
		ur->add_undo_property(control, "anchor_right", control->get_anchor(SIDE_RIGHT));
		ur->add_undo_property(control, "anchor_bottom", control->get_anchor(SIDE_BOTTOM));
		ur->add_undo_property(control, "offset_left", control->get_offset(SIDE_LEFT));
		ur->add_undo_property(control, "offset_top", control->get_offset(SIDE_TOP));
		ur->add_undo_property(control, "offset_right", control->get_offset(SIDE_RIGHT));
		ur->add_undo_property(control, "offset_bottom", control->get_offset(SIDE_BOTTOM));
		ur->commit_action();
	} else {
#endif
		control->set_anchors_and_offsets_preset(preset, mode);
#ifdef TOOLS_ENABLED
	}
#endif

	Dictionary res;
	res["node_path"] = _justamcp_safe_path_to(root, control);
	res["preset"] = preset_name;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPNodeTools::_rename_node(const Dictionary &p_params) {
	if (!p_params.has("node_path")) {
		return MCP_INVALID_PARAMS("Missing param: node_path");
	}
	if (!p_params.has("new_name")) {
		return MCP_INVALID_PARAMS("Missing param: new_name");
	}

	String node_path = p_params["node_path"];
	String new_name = p_params["new_name"];

	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No scene is currently open");
	}

	Node *node = _find_node_by_path(node_path);
	if (!node) {
		return MCP_NOT_FOUND("Node '" + node_path + "'");
	}

	String old_name = node->get_name();

#ifdef TOOLS_ENABLED
	if (EditorUndoRedoManager::get_singleton()) {
		EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
		ur->create_action("MCP: Rename " + old_name + " to " + new_name);
		ur->add_do_property(node, "name", new_name);
		ur->add_undo_property(node, "name", old_name);
		ur->commit_action();
	} else {
#endif
		node->set_name(new_name);
#ifdef TOOLS_ENABLED
	}
#endif

	Dictionary res;
	res["old_name"] = old_name;
	res["new_name"] = node->get_name();
	res["node_path"] = _justamcp_safe_path_to(root, node);
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPNodeTools::_connect_signal(const Dictionary &p_params) {
	if (!p_params.has("source_path")) {
		return MCP_INVALID_PARAMS("Missing source");
	}
	if (!p_params.has("signal_name")) {
		return MCP_INVALID_PARAMS("Missing signal");
	}
	if (!p_params.has("target_path")) {
		return MCP_INVALID_PARAMS("Missing target");
	}
	if (!p_params.has("method_name")) {
		return MCP_INVALID_PARAMS("Missing method");
	}

	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No scene is currently open");
	}

	Node *source = _find_node_by_path(p_params["source_path"]);
	if (!source) {
		return MCP_NOT_FOUND("Source node");
	}

	Node *target = _find_node_by_path(p_params["target_path"]);
	if (!target) {
		return MCP_NOT_FOUND("Target node");
	}

	String signal_name = p_params["signal_name"];
	String method_name = p_params["method_name"];

	if (!source->has_signal(signal_name)) {
		return MCP_INVALID_PARAMS("Signal not found");
	}

	Callable conn = Callable(target, StringName(method_name));
	if (source->is_connected(signal_name, conn)) {
		Dictionary res;
		res["already_connected"] = true;
		res["signal"] = signal_name;
		return MCP_SUCCESS(res);
	}

	source->connect(signal_name, conn);

	Dictionary res;
	res["source"] = _justamcp_safe_path_to(root, source);
	res["signal"] = signal_name;
	res["target"] = _justamcp_safe_path_to(root, target);
	res["method"] = method_name;
	res["connected"] = true;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPNodeTools::_disconnect_signal(const Dictionary &p_params) {
	if (!p_params.has("source_path")) {
		return MCP_INVALID_PARAMS("Missing source");
	}
	if (!p_params.has("signal_name")) {
		return MCP_INVALID_PARAMS("Missing signal");
	}
	if (!p_params.has("target_path")) {
		return MCP_INVALID_PARAMS("Missing target");
	}
	if (!p_params.has("method_name")) {
		return MCP_INVALID_PARAMS("Missing method");
	}

	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No scene is currently open");
	}

	Node *source = _find_node_by_path(p_params["source_path"]);
	if (!source) {
		return MCP_NOT_FOUND("Source node");
	}

	Node *target = _find_node_by_path(p_params["target_path"]);
	if (!target) {
		return MCP_NOT_FOUND("Target node");
	}

	String signal_name = p_params["signal_name"];
	String method_name = p_params["method_name"];

	Callable conn = Callable(target, StringName(method_name));
	if (!source->is_connected(signal_name, conn)) {
		Dictionary res;
		res["was_connected"] = false;
		return MCP_SUCCESS(res);
	}

	source->disconnect(signal_name, conn);

	Dictionary res;
	res["source"] = _justamcp_safe_path_to(root, source);
	res["signal"] = signal_name;
	res["target"] = _justamcp_safe_path_to(root, target);
	res["method"] = method_name;
	res["disconnected"] = true;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPNodeTools::_get_node_groups(const Dictionary &p_params) {
	if (!p_params.has("node_path")) {
		return MCP_INVALID_PARAMS("Missing param: node_path");
	}

	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No scene is currently open");
	}

	Node *node = _find_node_by_path(p_params["node_path"]);
	if (!node) {
		return MCP_NOT_FOUND("Node '" + String(p_params["node_path"]) + "'");
	}

	Array groups;
	List<Node::GroupInfo> gi;
	node->get_groups(&gi);
	for (const Node::GroupInfo &g : gi) {
		String gs = g.name;
		if (!gs.begins_with("_")) {
			groups.push_back(gs);
		}
	}

	Dictionary res;
	res["node_path"] = _justamcp_safe_path_to(root, node);
	res["groups"] = groups;
	res["count"] = groups.size();
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPNodeTools::_set_node_groups(const Dictionary &p_params) {
	if (!p_params.has("node_path")) {
		return MCP_INVALID_PARAMS("Missing param: node_path");
	}
	if (!p_params.has("groups")) {
		return MCP_INVALID_PARAMS("Missing param: groups");
	}

	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No scene is currently open");
	}

	Node *node = _find_node_by_path(p_params["node_path"]);
	if (!node) {
		return MCP_NOT_FOUND("Node '" + String(p_params["node_path"]) + "'");
	}

	Array desired_groups = p_params["groups"];
	Array current_groups;
	List<Node::GroupInfo> gi;
	node->get_groups(&gi);
	for (const Node::GroupInfo &g : gi) {
		String gs = g.name;
		if (!gs.begins_with("_")) {
			current_groups.push_back(gs);
		}
	}

	Array added;
	Array removed;

	for (int i = 0; i < current_groups.size(); i++) {
		if (!desired_groups.has(current_groups[i])) {
			node->remove_from_group(current_groups[i]);
			removed.push_back(current_groups[i]);
		}
	}

	for (int i = 0; i < desired_groups.size(); i++) {
		if (!current_groups.has(desired_groups[i])) {
			node->add_to_group(desired_groups[i], true);
			added.push_back(desired_groups[i]);
		}
	}

	Dictionary res;
	res["node_path"] = _justamcp_safe_path_to(root, node);
	res["groups"] = desired_groups;
	res["added"] = added;
	res["removed"] = removed;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPNodeTools::_find_nodes_in_group(const Dictionary &p_params) {
	if (!p_params.has("group")) {
		return MCP_INVALID_PARAMS("Missing param: group");
	}
	String group_name = p_params["group"];

	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No scene is currently open");
	}

	Array matches;
	_find_in_group_recursive(root, root, group_name, matches);

	Dictionary res;
	res["group"] = group_name;
	res["nodes"] = matches;
	res["count"] = matches.size();
	return MCP_SUCCESS(res);
}

void JustAMCPNodeTools::_find_in_group_recursive(Node *p_node, Node *p_root, const String &p_group_name, Array &r_matches) {
	if (p_node->is_in_group(p_group_name)) {
		Dictionary m;
		m["name"] = p_node->get_name();
		m["path"] = _justamcp_safe_path_to(p_root, p_node);
		m["type"] = p_node->get_class();
		r_matches.push_back(m);
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_find_in_group_recursive(p_node->get_child(i), p_root, p_group_name, r_matches);
	}
}
