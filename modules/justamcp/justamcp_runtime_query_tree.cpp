/**************************************************************************/
/*  justamcp_runtime_query_tree.cpp                                       */
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

#include "justamcp_runtime.h"

#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/input/input.h"
#include "core/input/input_event.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/json.h"
#include "core/math/expression.h"
#include "core/object/message_queue.h"
#include "core/object/script_language.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "main/performance.h"
#include "scene/gui/base_button.h"
#include "scene/gui/control.h"
#include "scene/main/multiplayer_api.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "servers/audio_server.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_settings.h"
#endif

void JustAMCPRuntime::_find_nodes_recursive(Node *p_node, const String &p_name, const String &p_type, const String &p_group, int p_limit, Array &r_results) {
	if (r_results.size() >= p_limit) {
		return;
	}
	bool hit = true;
	if (!p_name.is_empty() && p_name != "*") {
		hit = hit && String(p_node->get_name()).containsn(p_name);
	}
	if (!p_type.is_empty()) {
		hit = hit && p_node->is_class(p_type);
	}
	if (!p_group.is_empty()) {
		hit = hit && p_node->is_in_group(p_group);
	}
	if (hit && (!p_name.is_empty() || !p_type.is_empty() || !p_group.is_empty())) {
		Dictionary entry;
		entry["name"] = String(p_node->get_name());
		entry["type"] = p_node->get_class();
		entry["path"] = String(p_node->get_path());
		Array groups;
		List<Node::GroupInfo> glist;
		p_node->get_groups(&glist);
		for (const Node::GroupInfo &g : glist) {
			groups.push_back(String(g.name));
		}
		entry["groups"] = groups;
		r_results.push_back(entry);
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_find_nodes_recursive(p_node->get_child(i), p_name, p_type, p_group, p_limit, r_results);
	}
}

void JustAMCPRuntime::_find_nodes_by_script_recursive(Node *p_node, const String &p_script_path, const String &p_class_name, int p_limit, Array &r_results) {
	if (r_results.size() >= p_limit) {
		return;
	}

	Ref<Script> node_script = p_node->get_script();
	bool hit = false;
	if (node_script.is_valid()) {
		if (!p_script_path.is_empty() && node_script->get_path() == p_script_path) {
			hit = true;
		}
		if (!p_class_name.is_empty()) {
			String global_name = node_script->get_global_name();
			hit = hit || global_name == p_class_name || node_script->get_instance_base_type() == p_class_name;
		}
	}

	if (hit) {
		Dictionary entry;
		entry["name"] = String(p_node->get_name());
		entry["type"] = p_node->get_class();
		entry["path"] = String(p_node->get_path());
		entry["script_path"] = node_script->get_path();
		entry["script_class"] = node_script->get_global_name();
		r_results.push_back(entry);
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_find_nodes_by_script_recursive(p_node->get_child(i), p_script_path, p_class_name, p_limit, r_results);
	}
}

Dictionary JustAMCPRuntime::_cmd_get_tree(const Dictionary &p_params) {
	String root_path = p_params.get("root", "/root");
	int max_depth = p_params.get("depth", 3);
	bool include_properties = p_params.get("include_properties", false);

	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!tree) {
		Dictionary err;
		err["type"] = "error";
		err["message"] = "No SceneTree available";
		return err;
	}

	Node *root = tree->get_root()->get_node_or_null(root_path);
	if (!root) {
		Dictionary err;
		err["type"] = "error";
		err["message"] = "Node not found: " + root_path;
		return err;
	}

	Dictionary ret;
	ret["type"] = "tree";
	ret["root"] = _serialize_node_tree(root, 0, max_depth, include_properties);
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_get_node(const Dictionary &p_params) {
	String node_path = p_params.get("path", "");
	if (node_path.is_empty()) {
		Dictionary err;
		err["type"] = "error";
		err["message"] = "Node path required";
		return err;
	}

	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!tree) {
		Dictionary err;
		err["type"] = "error";
		err["message"] = "No SceneTree available";
		return err;
	}

	Node *node = tree->get_root()->get_node_or_null(node_path);
	if (!node) {
		Dictionary err;
		err["type"] = "error";
		err["message"] = "Node not found: " + node_path;
		return err;
	}

	Dictionary ret;
	ret["type"] = "node";
	ret["data"] = _serialize_node(node, true);
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_set_property(const Dictionary &p_params) {
	String node_path = p_params.get("path", "");
	String property = p_params.get("property", "");
	Variant value = p_params.get("value", Variant());

	if (node_path.is_empty() || property.is_empty()) {
		Dictionary err;
		err["type"] = "error";
		err["message"] = "Node path and property required";
		return err;
	}

	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!tree) {
		Dictionary err;
		err["type"] = "error";
		err["message"] = "No SceneTree available";
		return err;
	}

	Node *node = tree->get_root()->get_node_or_null(node_path);
	if (!node) {
		Dictionary err;
		err["type"] = "error";
		err["message"] = "Node not found: " + node_path;
		return err;
	}

	Variant old_value = node->get(property);
	node->set(property, _deserialize_value(value));

	Dictionary ret;
	ret["type"] = "property_set";
	ret["path"] = node_path;
	ret["property"] = property;
	ret["old_value"] = _serialize_value(old_value);
	ret["new_value"] = _serialize_value(node->get(property));
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_call_method(const Dictionary &p_params) {
	String node_path = p_params.get("path", "");
	String method = p_params.get("method", "");
	Array args = p_params.get("args", Array());

	if (node_path.is_empty() || method.is_empty()) {
		Dictionary err;
		err["type"] = "error";
		err["message"] = "Node path and method required";
		return err;
	}

	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!tree) {
		Dictionary err;
		err["type"] = "error";
		err["message"] = "No SceneTree available";
		return err;
	}

	Node *node = tree->get_root()->get_node_or_null(node_path);
	if (!node) {
		Dictionary err;
		err["type"] = "error";
		err["message"] = "Node not found: " + node_path;
		return err;
	}

	if (!node->has_method(method)) {
		Dictionary err;
		err["type"] = "error";
		err["message"] = "Method not found: " + method;
		return err;
	}

	Vector<Variant> variant_args;
	for (int i = 0; i < args.size(); i++) {
		variant_args.push_back(_deserialize_value(args[i]));
	}

	const Variant **argptrs = nullptr;
	if (variant_args.size() > 0) {
		argptrs = (const Variant **)alloca(sizeof(Variant *) * variant_args.size());
		for (int i = 0; i < variant_args.size(); i++) {
			argptrs[i] = &variant_args[i];
		}
	}

	Callable::CallError ce;
	Variant result_val = node->callp(method, argptrs, variant_args.size(), ce);

	Dictionary ret;
	ret["type"] = "method_result";
	ret["path"] = node_path;
	ret["method"] = method;
	ret["result"] = _serialize_value(result_val);
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_find_nodes(const Dictionary &p_params) {
	String name_filter = p_params.get("name", "");
	String type_filter = p_params.get("type", "");
	String group_filter = p_params.get("group", "");
	int limit = p_params.get("limit", 50);

	if (name_filter.is_empty() && type_filter.is_empty() && group_filter.is_empty()) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Requires at least one of: 'name', 'type', 'group'";
		return ret;
	}

	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!tree) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "No SceneTree available";
		return ret;
	}

	Array results;
	_find_nodes_recursive(tree->get_root(), name_filter, type_filter, group_filter, limit, results);

	Dictionary ret;
	ret["type"] = "find_nodes_result";
	ret["matches"] = results;
	ret["count"] = results.size();
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_get_node_property(const Dictionary &p_params) {
	String node_path = p_params.get("node", "");
	String property = p_params.get("property", "");

	if (node_path.is_empty() || property.is_empty()) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Requires 'node' (NodePath) and 'property'";
		return ret;
	}

	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!tree) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "No SceneTree available";
		return ret;
	}

	Node *node = tree->get_root()->get_node_or_null(node_path);
	if (!node) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Node not found: " + node_path;
		return ret;
	}

	Dictionary ret;
	ret["type"] = "node_property";
	ret["node"] = node_path;
	ret["property"] = property;
	ret["value"] = _serialize_value(node->get(property));
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_call_node_method(const Dictionary &p_params) {
	String node_path = p_params.get("node", "");
	String method = p_params.get("method", "");
	Array args = p_params.get("args", Array());

	if (node_path.is_empty() || method.is_empty()) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Requires 'node' and 'method'";
		return ret;
	}

	String method_lower = method.to_lower();
	for (int i = 0; _BLOCKED_METHODS[i] != nullptr; i++) {
		if (method_lower == String(_BLOCKED_METHODS[i]).to_lower()) {
			Dictionary ret;
			ret["type"] = "error";
			ret["message"] = "Method not allowed: " + method;
			return ret;
		}
	}

	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!tree) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "No SceneTree available";
		return ret;
	}

	Node *node = tree->get_root()->get_node_or_null(node_path);
	if (!node) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Node not found: " + node_path;
		return ret;
	}

	if (!node->has_method(method)) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Method not found: " + method;
		return ret;
	}

	Vector<Variant> variant_args;
	for (int i = 0; i < args.size(); i++) {
		variant_args.push_back(_deserialize_value(args[i]));
	}

	const Variant **argptrs = nullptr;
	if (variant_args.size() > 0) {
		argptrs = (const Variant **)alloca(sizeof(Variant *) * variant_args.size());
		for (int i = 0; i < variant_args.size(); i++) {
			argptrs[i] = &variant_args[i];
		}
	}

	Callable::CallError ce;
	Variant result_variant = node->callp(method, argptrs, variant_args.size(), ce);

	Dictionary ret;
	ret["type"] = "method_result";
	ret["node"] = node_path;
	ret["method"] = method;
	ret["result"] = _serialize_value(result_variant);
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_find_nodes_by_script(const Dictionary &p_params) {
	String script_path = p_params.get("script_path", p_params.get("path", ""));
	String class_name = p_params.get("class_name", "");
	int limit = p_params.get("limit", 100);
	if (script_path.is_empty() && class_name.is_empty()) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "script_path or class_name is required";
		return ret;
	}

	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!tree || !tree->get_root()) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "No SceneTree available";
		return ret;
	}

	Array matches;
	_find_nodes_by_script_recursive(tree->get_root(), script_path, class_name, limit, matches);
	Dictionary ret;
	ret["type"] = "nodes_by_script";
	ret["matches"] = matches;
	ret["count"] = matches.size();
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_batch_get_properties(const Dictionary &p_params) {
	Array node_paths = p_params.get("nodes", p_params.get("node_paths", Array()));
	Array properties = p_params.get("properties", Array());
	if (node_paths.is_empty() || properties.is_empty()) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "nodes/node_paths and properties are required";
		return ret;
	}

	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!tree || !tree->get_root()) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "No SceneTree available";
		return ret;
	}

	Array rows;
	for (int i = 0; i < node_paths.size(); i++) {
		String node_path = node_paths[i];
		Node *node = tree->get_root()->get_node_or_null(NodePath(node_path));
		Dictionary row;
		row["path"] = node_path;
		row["found"] = node != nullptr;
		Dictionary values;
		if (node) {
			for (int j = 0; j < properties.size(); j++) {
				String property = properties[j];
				values[property] = _serialize_value(node->get(property));
			}
		}
		row["properties"] = values;
		rows.push_back(row);
	}

	Dictionary ret;
	ret["type"] = "batch_properties";
	ret["nodes"] = rows;
	ret["count"] = rows.size();
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_move_node(const Dictionary &p_params) {
	String node_path = p_params.get("node", p_params.get("path", ""));
	Variant position = p_params.get("position", p_params.get("target", Variant()));
	if (node_path.is_empty()) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Missing node/path";
		return ret;
	}
	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	Node *node = tree && tree->get_root() ? tree->get_root()->get_node_or_null(NodePath(node_path)) : nullptr;
	if (!node) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Node not found: " + node_path;
		return ret;
	}
	node->set(p_params.get("property", "position"), _deserialize_value(position));
	Dictionary ret;
	ret["type"] = "node_moved";
	ret["node"] = node_path;
	ret["position"] = _serialize_value(node->get(p_params.get("property", "position")));
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_monitor_properties(const Dictionary &p_params) {
	String node_path = p_params.get("node", p_params.get("path", ""));
	Array properties = p_params.get("properties", Array());
	if (node_path.is_empty() || properties.is_empty()) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "node/path and properties are required";
		return ret;
	}
	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	Node *node = tree && tree->get_root() ? tree->get_root()->get_node_or_null(NodePath(node_path)) : nullptr;
	if (!node) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Node not found: " + node_path;
		return ret;
	}

	Dictionary values;
	for (int i = 0; i < properties.size(); i++) {
		String property = properties[i];
		values[property] = _serialize_value(node->get(property));
	}
	Dictionary ret;
	ret["type"] = "property_snapshot";
	ret["node"] = node_path;
	ret["properties"] = values;
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_wait_for_property(const Dictionary &p_params) {
	String node_path = p_params.get("node", "");
	String property = p_params.get("property", "");
	Variant expected = p_params.get("value", Variant());

	if (node_path.is_empty() || property.is_empty()) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Requires 'node', 'property', 'value'";
		return ret;
	}

	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!tree) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "No SceneTree available";
		return ret;
	}

	Node *node = tree->get_root()->get_node_or_null(node_path);
	if (!node) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Node not found: " + node_path;
		return ret;
	}

	Variant current = node->get(property);
	bool matched = (String(_serialize_value(current)) == String(_serialize_value(expected)));

	Dictionary ret;
	ret["type"] = "wait_for_property_result";
	ret["node"] = node_path;
	ret["property"] = property;
	ret["matched"] = matched;
	ret["current_value"] = _serialize_value(current);
	ret["expected_value"] = _serialize_value(expected);
	return ret;
}
