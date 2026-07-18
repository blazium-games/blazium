/**************************************************************************/
/*  justamcp_node_tools.cpp                                               */
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

#include "justamcp_node_tools.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_undo_redo_manager.h"
#endif

#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "core/object/script_language.h"
#include "scene/gui/control.h"

#include "../justamcp_editor_scene_access.h"
#include "../justamcp_mcp_tool_macros.h"

void JustAMCPNodeTools::_bind_methods() {}

JustAMCPNodeTools::JustAMCPNodeTools() {}
JustAMCPNodeTools::~JustAMCPNodeTools() {}

Node *JustAMCPNodeTools::_find_node_by_path(const String &p_path) {
	return JustAMCPEditorSceneAccess::find_node_in_edited_scene(p_path);
}

void JustAMCPNodeTools::_set_owner_recursive(Node *p_node, Node *p_owner) {
	p_node->set_owner(p_owner);
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_set_owner_recursive(p_node->get_child(i), p_owner);
	}
}

Dictionary JustAMCPNodeTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "node_add") {
		return _add_node(p_args);
	}
	if (p_tool_name == "node_delete") {
		return _delete_node(p_args);
	}
	if (p_tool_name == "node_duplicate") {
		return _duplicate_node(p_args);
	}
	if (p_tool_name == "node_move") {
		return _move_node(p_args);
	}
	if (p_tool_name == "node_update_property") {
		return _update_property(p_args);
	}
	if (p_tool_name == "node_get_properties") {
		return _get_node_properties(p_args);
	}
	if (p_tool_name == "node_add_resource") {
		return _add_resource(p_args);
	}
	if (p_tool_name == "node_set_anchor_preset") {
		return _set_anchor_preset(p_args);
	}
	if (p_tool_name == "node_rename") {
		return _rename_node(p_args);
	}
	if (p_tool_name == "node_connect_signal") {
		return _connect_signal(p_args);
	}
	if (p_tool_name == "node_disconnect_signal") {
		return _disconnect_signal(p_args);
	}
	if (p_tool_name == "node_get_groups") {
		return _get_node_groups(p_args);
	}
	if (p_tool_name == "node_set_groups") {
		return _set_node_groups(p_args);
	}
	if (p_tool_name == "node_find_in_group") {
		return _find_nodes_in_group(p_args);
	}

	Dictionary err;
	err["code"] = -32601;
	err["message"] = "Method not found: " + p_tool_name;
	Dictionary res;
	res["error"] = err;
	return Dictionary();
}

Dictionary JustAMCPNodeTools::_add_node(const Dictionary &p_params) {
	if (!p_params.has("type")) {
		return MCP_INVALID_PARAMS("Missing param: type");
	}
	String type = p_params["type"];
	String parent_path = p_params.has("parent_path") ? String(p_params["parent_path"]) : ".";
	String node_name = p_params.has("name") ? String(p_params["name"]) : "";
	Dictionary properties = p_params.has("properties") ? Dictionary(p_params["properties"]) : Dictionary();

	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No scene is currently open");
	}

	Node *parent = _find_node_by_path(parent_path);
	if (!parent) {
		return MCP_NOT_FOUND("Parent node '" + parent_path + "'");
	}

	Node *node = nullptr;
	Ref<Script> custom_script;

	if (ClassDB::class_exists(type)) {
		Object *_node_obj = ClassDB::instantiate(type);
		node = Object::cast_to<Node>(_node_obj);
		if (!node && _node_obj) {
			memdelete(_node_obj);
		}
	} else {
		TypedArray<Dictionary> global_classes = ProjectSettings::get_singleton()->get_global_class_list();
		String script_path = "";
		for (int i = 0; i < global_classes.size(); i++) {
			Dictionary entry = global_classes[i];
			if (String(entry.get("class", "")) == type) {
				script_path = entry.get("path", "");
				break;
			}
		}
		if (script_path.is_empty()) {
			return MCP_INVALID_PARAMS("Unknown node type: '" + type + "'");
		}
		custom_script = ResourceLoader::load(script_path);
		if (custom_script.is_null()) {
			return MCP_INVALID_PARAMS("Unknown node type: loading script failed");
		}
		String base_type = custom_script->get_instance_base_type();
		if (!ClassDB::class_exists(base_type)) {
			return MCP_INVALID_PARAMS("Script extends invalid type");
		}
		Object *_base_node_obj = ClassDB::instantiate(base_type);
		node = Object::cast_to<Node>(_base_node_obj);
		if (!node && _base_node_obj) {
			memdelete(_base_node_obj);
		}
		node->set_script(custom_script);
	}

	if (!node_name.is_empty()) {
		node->set_name(node_name);
	} else {
		node->set_name(type);
	}

	Array keys = properties.keys();
	for (int i = 0; i < keys.size(); i++) {
		String prop_name = keys[i];
		bool valid = false;
		Variant val = properties[prop_name];
		node->set(prop_name, val, &valid);
	}

#ifdef TOOLS_ENABLED
	if (EditorUndoRedoManager::get_singleton()) {
		EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
		ur->create_action("MCP: Add " + type);
		ur->add_do_method(parent, "add_child", node);
		ur->add_do_method(node, "set_owner", root);
		ur->add_do_reference(node);
		ur->add_undo_method(parent, "remove_child", node);
		ur->commit_action();
	} else {
#endif
		parent->add_child(node);
		node->set_owner(root);
#ifdef TOOLS_ENABLED
	}
#endif

	Dictionary res;
	res["node_path"] = root->get_path_to(node);
	res["type"] = type;
	res["name"] = node->get_name();
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPNodeTools::_delete_node(const Dictionary &p_params) {
	if (!p_params.has("node_path")) {
		return MCP_INVALID_PARAMS("Missing param: node_path");
	}
	String node_path = p_params["node_path"];

	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No scene is currently open");
	}

	Node *node = _find_node_by_path(node_path);
	if (!node) {
		return MCP_NOT_FOUND("Node '" + node_path + "'");
	}

	if (node == root) {
		return MCP_INVALID_PARAMS("Cannot delete the root node");
	}

	Node *parent = node->get_parent();
	String node_name = node->get_name();

#ifdef TOOLS_ENABLED
	if (EditorUndoRedoManager::get_singleton()) {
		EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
		ur->create_action("MCP: Delete " + node_name);
		ur->add_do_method(parent, "remove_child", node);
		ur->add_undo_method(parent, "add_child", node);
		ur->add_undo_method(node, "set_owner", root);
		ur->add_undo_reference(node);
		ur->commit_action();
	} else {
#endif
		parent->remove_child(node);
		node->queue_free();
#ifdef TOOLS_ENABLED
	}
#endif

	Dictionary res;
	res["deleted"] = node_name;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPNodeTools::_duplicate_node(const Dictionary &p_params) {
	if (!p_params.has("node_path")) {
		return MCP_INVALID_PARAMS("Missing param: node_path");
	}
	String node_path = p_params["node_path"];
	String new_name = p_params.has("name") ? String(p_params["name"]) : "";

	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No scene is currently open");
	}

	Node *node = _find_node_by_path(node_path);
	if (!node) {
		return MCP_NOT_FOUND("Node '" + node_path + "'");
	}

	if (new_name.is_empty()) {
		new_name = String(node->get_name()) + "_copy";
	}

	Node *dup = node->duplicate();
	dup->set_name(new_name);
	Node *parent = node->get_parent();

#ifdef TOOLS_ENABLED
	if (EditorUndoRedoManager::get_singleton()) {
		EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
		ur->create_action("MCP: Duplicate " + node->get_name());
		ur->add_do_method(parent, "add_child", dup);
		ur->add_do_method(dup, "set_owner", root);
		ur->add_do_reference(dup);
		ur->add_undo_method(parent, "remove_child", dup);
		ur->commit_action();
	} else {
#endif
		parent->add_child(dup);
		dup->set_owner(root);
#ifdef TOOLS_ENABLED
	}
#endif

	_set_owner_recursive(dup, root);

	Dictionary res;
	res["original"] = root->get_path_to(node);
	res["duplicate"] = root->get_path_to(dup);
	res["name"] = dup->get_name();
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPNodeTools::_move_node(const Dictionary &p_params) {
	if (!p_params.has("node_path")) {
		return MCP_INVALID_PARAMS("Missing param: node_path");
	}
	if (!p_params.has("new_parent_path")) {
		return MCP_INVALID_PARAMS("Missing param: new_parent_path");
	}
	String node_path = p_params["node_path"];
	String new_parent_path = p_params["new_parent_path"];

	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No scene is currently open");
	}

	Node *node = _find_node_by_path(node_path);
	if (!node) {
		return MCP_NOT_FOUND("Node '" + node_path + "'");
	}
	if (node == root) {
		return MCP_INVALID_PARAMS("Cannot move the root node");
	}

	Node *new_parent = _find_node_by_path(new_parent_path);
	if (!new_parent) {
		return MCP_NOT_FOUND("Target parent '" + new_parent_path + "'");
	}

	if (new_parent == node || node->is_ancestor_of(new_parent)) {
		return MCP_INVALID_PARAMS("Cannot move a node into its own subtree");
	}

	Node *old_parent = node->get_parent();

#ifdef TOOLS_ENABLED
	if (EditorUndoRedoManager::get_singleton()) {
		EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
		ur->create_action("MCP: Move " + node->get_name());
		ur->add_do_method(old_parent, "remove_child", node);
		ur->add_do_method(new_parent, "add_child", node);
		ur->add_do_method(node, "set_owner", root);
		ur->add_undo_method(new_parent, "remove_child", node);
		ur->add_undo_method(old_parent, "add_child", node);
		ur->add_undo_method(node, "set_owner", root);
		ur->commit_action();
	} else {
#endif
		old_parent->remove_child(node);
		new_parent->add_child(node);
		node->set_owner(root);
#ifdef TOOLS_ENABLED
	}
#endif

	_set_owner_recursive(node, root);

	Dictionary res;
	res["node"] = node->get_name();
	res["old_parent"] = root->get_path_to(old_parent);
	res["new_parent"] = root->get_path_to(new_parent);
	res["new_path"] = root->get_path_to(node);
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPNodeTools::_update_property(const Dictionary &p_params) {
	if (!p_params.has("node_path")) {
		return MCP_INVALID_PARAMS("Missing param: node_path");
	}
	if (!p_params.has("property")) {
		return MCP_INVALID_PARAMS("Missing param: property");
	}
	if (!p_params.has("value")) {
		return MCP_INVALID_PARAMS("Missing param: value");
	}

	String node_path = p_params["node_path"];
	String property = p_params["property"];
	Variant value = p_params["value"];

	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No scene is currently open");
	}

	Node *node = _find_node_by_path(node_path);
	if (!node) {
		return MCP_NOT_FOUND("Node '" + node_path + "'");
	}

	bool valid = false;
	Variant old_value = node->get(property, &valid);
	if (!valid) {
		return MCP_NOT_FOUND("Property '" + property + "'");
	}

#ifdef TOOLS_ENABLED
	if (EditorUndoRedoManager::get_singleton()) {
		EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
		ur->create_action("MCP: Set " + String(node->get_name()) + "." + property);
		ur->add_do_property(node, property, value);
		ur->add_undo_property(node, property, old_value);
		ur->commit_action();
	} else {
#endif
		node->set(property, value);
#ifdef TOOLS_ENABLED
	}
#endif

	Dictionary res;
	res["node"] = root->get_path_to(node);
	res["property"] = property;
	res["old_value"] = old_value;
	res["new_value"] = node->get(property);
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPNodeTools::_get_node_properties(const Dictionary &p_params) {
	if (!p_params.has("node_path")) {
		return MCP_INVALID_PARAMS("Missing param: node_path");
	}
	String node_path = p_params["node_path"];
	String category = p_params.has("category") ? String(p_params["category"]) : "";

	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No scene is currently open");
	}

	Node *node = _find_node_by_path(node_path);
	if (!node) {
		return MCP_NOT_FOUND("Node '" + node_path + "'");
	}

	Dictionary props;
	List<PropertyInfo> prop_list;
	node->get_property_list(&prop_list);

	for (const PropertyInfo &pi : prop_list) {
		if (pi.usage & PROPERTY_USAGE_EDITOR) {
			if (!category.is_empty() && !String(pi.name).begins_with(category)) {
				continue;
			}
			props[pi.name] = node->get(pi.name);
		}
	}

	Dictionary res;
	res["node_path"] = root->get_path_to(node);
	res["type"] = node->get_class();
	res["properties"] = props;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPNodeTools::_add_resource(const Dictionary &p_params) {
	if (!p_params.has("node_path")) {
		return MCP_INVALID_PARAMS("Missing param: node_path");
	}
	if (!p_params.has("property")) {
		return MCP_INVALID_PARAMS("Missing param: property");
	}
	if (!p_params.has("resource_type")) {
		return MCP_INVALID_PARAMS("Missing param: resource_type");
	}

	String node_path = p_params["node_path"];
	String property = p_params["property"];
	String resource_type = p_params["resource_type"];

	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No scene is currently open");
	}

	Node *node = _find_node_by_path(node_path);
	if (!node) {
		return MCP_NOT_FOUND("Node '" + node_path + "'");
	}

	if (!ClassDB::class_exists(resource_type) || !ClassDB::is_parent_class(resource_type, "Resource")) {
		return MCP_INVALID_PARAMS("Unknown resource type: " + resource_type);
	}

	Object *_resource_obj = ClassDB::instantiate(resource_type);
	Ref<Resource> resource = Ref<Resource>(Object::cast_to<Resource>(_resource_obj));
	if (resource.is_null()) {
		if (_resource_obj) {
			memdelete(_resource_obj);
		}
		return MCP_INTERNAL("Failed to create resource");
	}

	if (p_params.has("resource_properties")) {
		Dictionary rp = p_params["resource_properties"];
		Array keys = rp.keys();
		for (int i = 0; i < keys.size(); i++) {
			String k = keys[i];
			resource->set(k, rp[k]);
		}
	}

	Variant old_value = node->get(property);

#ifdef TOOLS_ENABLED
	if (EditorUndoRedoManager::get_singleton()) {
		EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
		ur->create_action("MCP: Add " + resource_type + " to " + String(node->get_name()));
		ur->add_do_property(node, property, resource);
		ur->add_undo_property(node, property, old_value);
		ur->commit_action();
	} else {
#endif
		node->set(property, resource);
#ifdef TOOLS_ENABLED
	}
#endif

	Dictionary res;
	res["node_path"] = root->get_path_to(node);
	res["property"] = property;
	res["resource_type"] = resource_type;
	return MCP_SUCCESS(res);
}
