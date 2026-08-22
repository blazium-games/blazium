/**************************************************************************/
/*  justamcp_batch_tools.cpp                                              */
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

#include "justamcp_batch_tools.h"
#include "../justamcp_editor_filesystem.h"
#include "../justamcp_editor_scene_access.h"
#include "../justamcp_read_limits.h"
#include "../justamcp_tool_context.h"
#include "justamcp_tool_executor.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#endif

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/math/expression.h"
#include "modules/regex/regex.h"
#include "scene/resources/packed_scene.h"

#include "../justamcp_mcp_tool_macros.h"

void JustAMCPBatchTools::_bind_methods() {}

JustAMCPBatchTools::JustAMCPBatchTools() {}
JustAMCPBatchTools::~JustAMCPBatchTools() {}

Node *JustAMCPBatchTools::_find_node_by_path(const String &p_path) {
	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return nullptr;
	}

	if (p_path == "." || p_path == root->get_name()) {
		return root;
	}
	if (root->has_node(p_path)) {
		return root->get_node(p_path);
	}

	if (p_path.begins_with(String(root->get_name()) + "/")) {
		String rel = p_path.substr(String(root->get_name()).length() + 1);
		if (root->has_node(rel)) {
			return root->get_node(rel);
		}
	}
	return nullptr;
}

Dictionary JustAMCPBatchTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "find_nodes_by_type") {
		return _find_nodes_by_type(p_args);
	}
	if (p_tool_name == "find_signal_connections") {
		return _find_signal_connections(p_args);
	}
	if (p_tool_name == "batch_set_property") {
		return _batch_set_property(p_args);
	}
	if (p_tool_name == "batch_add_nodes") {
		return _batch_add_nodes(p_args);
	}
	if (p_tool_name == "find_node_references") {
		return _find_node_references(p_args);
	}
	if (p_tool_name == "cross_scene_set_property") {
		return _cross_scene_set_property(p_args);
	}
	if (p_tool_name == "get_scene_dependencies") {
		return _get_scene_dependencies(p_args);
	}
	if (p_tool_name == "batch_execute") {
		if (!owner) {
			Dictionary err;
			err["ok"] = false;
			err["error"] = "batch_execute is unavailable without a tool executor.";
			return err;
		}
		Array steps = p_args.get("steps", Array());
		bool stop_on_error = p_args.get("stop_on_error", true);
		bool undo_on_error = p_args.get("undo_on_error", false);
		Array results;
		int completed = 0;
		const int total_steps = steps.size();
		justamcp_report_progress(0, total_steps > 0 ? total_steps : 1, "Starting batch_execute");
		for (int i = 0; i < steps.size(); i++) {
			if (justamcp_is_cancel_requested()) {
				Dictionary err;
				err["ok"] = false;
				err["error"] = "cancelled";
				results.push_back(err);
				break;
			}
			justamcp_report_progress(i, total_steps > 0 ? total_steps : 1, vformat("batch step %d", i + 1));
			if (steps[i].get_type() != Variant::DICTIONARY) {
				Dictionary step_error;
				step_error["ok"] = false;
				step_error["error"] = "Step is not an object.";
				results.push_back(step_error);
				if (stop_on_error) {
					break;
				}
				continue;
			}
			Dictionary step = steps[i];
			String tool_name = step.get("tool_name", step.get("tool", ""));
			if (tool_name == "batch_execute" || tool_name == "blazium_batch_execute") {
				Dictionary step_error;
				step_error["ok"] = false;
				step_error["error"] = "Nested batch_execute is not allowed.";
				results.push_back(step_error);
				if (stop_on_error) {
					break;
				}
				continue;
			}
			if (tool_name == "execute_tool" || tool_name == "blazium_execute_tool") {
				Dictionary step_error;
				step_error["ok"] = false;
				step_error["error"] = "execute_tool is not allowed in batch_execute.";
				results.push_back(step_error);
				if (stop_on_error) {
					break;
				}
				continue;
			}
			Dictionary args = step.get("arguments", step.get("args", Dictionary()));
			Dictionary step_result = owner->execute_tool(tool_name, args);
			results.push_back(step_result);
			bool ok = step_result.get("ok", !step_result.has("error"));
			if (!ok) {
				if (undo_on_error && owner->editor_tools) {
					for (int undo_idx = 0; undo_idx < completed; undo_idx++) {
						owner->editor_tools->editor_undo(Dictionary());
					}
				}
				if (stop_on_error) {
					break;
				}
			} else {
				completed++;
			}
		}
		justamcp_report_progress(total_steps > 0 ? total_steps : 1, total_steps > 0 ? total_steps : 1, "batch_execute finished");
		Dictionary ret;
		ret["ok"] = true;
		ret["results"] = results;
		ret["completed"] = completed;
		ret["count"] = results.size();
		return ret;
	}

	Dictionary err;
	err["code"] = -32601;
	err["message"] = "Method not found: " + p_tool_name;
	Dictionary res;
	res["error"] = err;
	return Dictionary();
}

Dictionary JustAMCPBatchTools::_find_nodes_by_type(const Dictionary &p_params) {
	if (!p_params.has("type") || String(p_params["type"]).is_empty()) {
		return MCP_INVALID_PARAMS("Missing param: type");
	}
	String type_name = p_params["type"];

	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No scene is currently open");
	}

	bool recursive = p_params.has("recursive") ? bool(p_params["recursive"]) : true;
	Array matches;
	_search_by_type(root, type_name, recursive, matches);

	Dictionary res;
	res["type"] = type_name;
	res["matches"] = matches;
	res["count"] = matches.size();
	return MCP_SUCCESS(res);
}

void JustAMCPBatchTools::_search_by_type(Node *p_node, const String &p_type_name, bool p_recursive, Array &r_matches) {
	if (p_node->is_class(p_type_name) || String(p_node->get_class()) == p_type_name) {
		Node *root = JustAMCPEditorSceneAccess::get_edited_root();
		if (root) {
			Dictionary m;
			m["name"] = p_node->get_name();
			m["path"] = JustAMCPEditorSceneAccess::safe_path_to(root, p_node);
			m["type"] = p_node->get_class();
			r_matches.push_back(m);
		}
	}
	if (p_recursive) {
		for (int i = 0; i < p_node->get_child_count(); i++) {
			_search_by_type(p_node->get_child(i), p_type_name, p_recursive, r_matches);
		}
	}
}

Dictionary JustAMCPBatchTools::_find_signal_connections(const Dictionary &p_params) {
	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No scene is currently open");
	}

	String signal_filter = p_params.has("signal_name") ? String(p_params["signal_name"]) : "";
	String node_filter = p_params.has("node_path") ? String(p_params["node_path"]) : "";

	Array collected_connections;
	_collect_signals(root, root, signal_filter, node_filter, collected_connections);

	Dictionary res;
	res["connections"] = collected_connections;
	res["count"] = collected_connections.size();
	return MCP_SUCCESS(res);
}

void JustAMCPBatchTools::_collect_signals(Node *p_node, Node *p_root, const String &p_signal_filter, const String &p_node_filter, Array &r_connections) {
	String node_path = JustAMCPEditorSceneAccess::safe_path_to(p_root, p_node);

	if (p_node_filter.is_empty() || node_path.contains(p_node_filter)) {
		List<MethodInfo> signals;
		p_node->get_signal_list(&signals);
		for (const MethodInfo &sig : signals) {
			String sig_name = sig.name;
			if (!p_signal_filter.is_empty() && !sig_name.contains(p_signal_filter)) {
				continue;
			}

			List<Connection> conns;
			p_node->get_signal_connection_list(sig_name, &conns);
			for (const Connection &c : conns) {
				Dictionary d;
				d["source"] = node_path;
				d["signal"] = sig_name;

				Object *target_obj = c.callable.get_object();
				Node *target_node = Object::cast_to<Node>(target_obj);
				if (target_node) {
					if (target_node == p_root || p_root->is_ancestor_of(target_node)) {
						d["target"] = JustAMCPEditorSceneAccess::safe_path_to(p_root, target_node);
					} else {
						d["target"] = target_node->get_name();
					}
				} else {
					d["target"] = String::num_uint64(target_obj->get_instance_id());
				}
				d["method"] = c.callable.get_method();
				r_connections.push_back(d);
			}
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_collect_signals(p_node->get_child(i), p_root, p_signal_filter, p_node_filter, r_connections);
	}
}

Dictionary JustAMCPBatchTools::_batch_set_property(const Dictionary &p_params) {
	if (!p_params.has("type")) {
		return MCP_INVALID_PARAMS("Missing param: type");
	}
	if (!p_params.has("property")) {
		return MCP_INVALID_PARAMS("Missing param: property");
	}
	if (!p_params.has("value")) {
		return MCP_INVALID_PARAMS("Missing param: value");
	}

	String type_name = p_params["type"];
	String property = p_params["property"];
	Variant value = p_params["value"];

	if (value.get_type() == Variant::STRING) {
		String s = value;
		Expression expr;
		if (expr.parse(s) == OK) {
			Variant parsed = expr.execute(Array(), nullptr, false, true);
			if (parsed.get_type() != Variant::NIL) {
				value = parsed;
			}
		}
	}

	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No scene is currently open");
	}

	Array affected;
	_batch_set_recursive(root, root, type_name, property, value, affected);

	Dictionary res;
	res["property"] = property;
	res["affected"] = affected;
	res["count"] = affected.size();
	return MCP_SUCCESS(res);
}

void JustAMCPBatchTools::_batch_set_recursive(Node *p_node, Node *p_root, const String &p_type_name, const String &p_property, const Variant &p_value, Array &r_affected) {
	if (p_node->is_class(p_type_name) || String(p_node->get_class()) == p_type_name) {
		bool valid = false;
		p_node->set(p_property, p_value, &valid);
		if (valid) {
			r_affected.push_back(JustAMCPEditorSceneAccess::safe_path_to(p_root, p_node));
		}
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_batch_set_recursive(p_node->get_child(i), p_root, p_type_name, p_property, p_value, r_affected);
	}
}

Dictionary JustAMCPBatchTools::_batch_add_nodes(const Dictionary &p_params) {
	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No scene is currently open");
	}

	Array node_defs = p_params.get("nodes", Array());
	if (node_defs.is_empty()) {
		return MCP_INVALID_PARAMS("Missing param: nodes");
	}

	Array created;
	for (int i = 0; i < node_defs.size(); i++) {
		if (node_defs[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary def = node_defs[i];
		String type_name = def.get("type", def.get("node_type", "Node"));
		String parent_path = def.get("parent_path", def.get("parentPath", "."));
		Node *parent = parent_path == "." ? root : _find_node_by_path(parent_path);
		if (!parent) {
			continue;
		}
		Object *obj = ClassDB::instantiate(StringName(type_name));
		Node *node = Object::cast_to<Node>(obj);
		if (!node) {
			if (obj) {
				memdelete(obj);
			}
			continue;
		}
		node->set_name(def.get("name", type_name));
		parent->add_child(node);
		if (root == node || root->is_ancestor_of(node)) {
			node->set_owner(root);
		}
		Dictionary properties = def.get("properties", Dictionary());
		Array keys = properties.keys();
		for (int j = 0; j < keys.size(); j++) {
			node->set(keys[j], properties[keys[j]]);
		}

		Dictionary info;
		info["name"] = node->get_name();
		info["type"] = node->get_class();
		info["path"] = JustAMCPEditorSceneAccess::safe_path_to(root, node);
		created.push_back(info);
	}

#ifdef TOOLS_ENABLED
	if (!created.is_empty() && EditorNode::get_singleton() && EditorInterface::get_singleton()) {
		EditorInterface::get_singleton()->mark_scene_as_unsaved();
	}
#endif

	Dictionary res;
	res["created"] = created;
	res["count"] = created.size();
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPBatchTools::_find_node_references(const Dictionary &p_params) {
	if (!p_params.has("pattern")) {
		return MCP_INVALID_PARAMS("Missing param: pattern");
	}
	String pattern = p_params["pattern"];

	Array matches;
	_search_files_for_pattern("res://", pattern, matches, 100);
	Dictionary res;
	res["pattern"] = pattern;
	res["matches"] = matches;
	res["count"] = matches.size();
	return MCP_SUCCESS(res);
}

void JustAMCPBatchTools::_search_files_for_pattern(const String &p_path, const String &p_pattern, Array &r_matches, int p_max_results) {
	if (r_matches.size() >= p_max_results) {
		return;
	}

	Ref<DirAccess> dir = DirAccess::open(p_path);
	if (dir.is_null()) {
		return;
	}

	dir->list_dir_begin();
	String file_name = dir->get_next();
	while (!file_name.is_empty() && r_matches.size() < p_max_results) {
		if (file_name.begins_with(".")) {
			file_name = dir->get_next();
			continue;
		}

		String full_path = p_path.path_join(file_name);
		if (dir->current_is_dir()) {
			_search_files_for_pattern(full_path, p_pattern, r_matches, p_max_results);
		} else if (file_name.get_extension() == "tscn" || file_name.get_extension() == "gd" || file_name.get_extension() == "tres" || file_name.get_extension() == "gdshader") {
			int64_t file_size = 0;
			if (!justamcp_file_within_read_limit(full_path, JUSTAMCP_MAX_SYNC_READ_BYTES, file_size)) {
				file_name = dir->get_next();
				continue;
			}
			Ref<FileAccess> file = FileAccess::open(full_path, FileAccess::READ);
			if (file.is_valid()) {
				String content = file->get_as_text();
				file->close();
				if (content.contains(p_pattern)) {
					Vector<String> lines = content.split("\n");
					Array line_matches;
					for (int i = 0; i < lines.size(); i++) {
						if (lines[i].contains(p_pattern)) {
							line_matches.push_back(i + 1);
							if (line_matches.size() >= 5) {
								break;
							}
						}
					}
					Dictionary d;
					d["file"] = full_path;
					d["lines"] = line_matches;
					r_matches.push_back(d);
				}
			}
		}
		file_name = dir->get_next();
	}
	dir->list_dir_end();
}

Dictionary JustAMCPBatchTools::_cross_scene_set_property(const Dictionary &p_params) {
	if (!p_params.has("type")) {
		return MCP_INVALID_PARAMS("Missing param: type");
	}
	if (!p_params.has("property")) {
		return MCP_INVALID_PARAMS("Missing param: property");
	}
	if (!p_params.has("value")) {
		return MCP_INVALID_PARAMS("Missing param: value");
	}

	String type_name = p_params["type"];
	String property = p_params["property"];
	Variant value = p_params["value"];

	if (value.get_type() == Variant::STRING) {
		Expression expr;
		if (expr.parse(String(value)) == OK) {
			Variant parsed = expr.execute(Array(), nullptr, false, true);
			if (parsed.get_type() != Variant::NIL) {
				value = parsed;
			}
		}
	}

	String path_filter = p_params.has("path_filter") ? String(p_params["path_filter"]) : "res://";
	bool exclude_addons = p_params.has("exclude_addons") ? bool(p_params["exclude_addons"]) : true;
	const int max_results = CLAMP(int(p_params.get("max_results", 200)), 1, 2000);

	if (path_filter == "res://" || path_filter.is_empty()) {
		return MCP_ERROR(-32602, "cross_scene_set_property requires a narrower path_filter than res:// (pass a subdirectory).");
	}

	Array scenes_affected;
	int total_nodes = 0;
	Array scene_files;
	_collect_scene_files(path_filter, scene_files, exclude_addons, max_results);
	const bool truncated = scene_files.size() >= max_results;

	for (int i = 0; i < scene_files.size(); i++) {
		String scene_path = scene_files[i];
		Ref<PackedScene> packed = ResourceLoader::load(scene_path);
		if (packed.is_null()) {
			continue;
		}

		Node *instance = packed->instantiate();
		if (!instance) {
			continue;
		}

		Array affected_nodes;
		_cross_scene_set_recursive(instance, instance, type_name, property, value, affected_nodes);

		if (!affected_nodes.is_empty()) {
			Ref<PackedScene> new_packed;
			new_packed.instantiate();
			new_packed->pack(instance);
			ResourceSaver::save(new_packed, scene_path);

			Dictionary d;
			d["scene"] = scene_path;
			d["nodes"] = affected_nodes;
			d["count"] = affected_nodes.size();
			scenes_affected.push_back(d);
			total_nodes += affected_nodes.size();
		}

		memdelete(instance);
	}

#ifdef TOOLS_ENABLED
	if (!scenes_affected.is_empty()) {
		for (int i = 0; i < scenes_affected.size(); i++) {
			Dictionary d = scenes_affected[i];
			String scene_path = d.get("scene", "");
			if (!scene_path.is_empty()) {
				JustAMCPEditorFilesystem::refresh_path(scene_path);
			}
		}
	}
#endif

	Dictionary res;
	res["type"] = type_name;
	res["property"] = property;
	res["scenes_affected"] = scenes_affected;
	res["total_scenes"] = scenes_affected.size();
	res["total_nodes"] = total_nodes;
	res["truncated"] = truncated;
	return MCP_SUCCESS(res);
}

void JustAMCPBatchTools::_collect_scene_files(const String &p_path, Array &r_files, bool p_exclude_addons, int p_max_results) {
	if (p_max_results > 0 && r_files.size() >= p_max_results) {
		return;
	}
	Ref<DirAccess> dir = DirAccess::open(p_path);
	if (dir.is_null()) {
		return;
	}

	dir->list_dir_begin();
	String file_name = dir->get_next();
	while (!file_name.is_empty()) {
		if (p_max_results > 0 && r_files.size() >= p_max_results) {
			break;
		}
		if (file_name.begins_with(".")) {
			file_name = dir->get_next();
			continue;
		}
		String full_path = p_path.path_join(file_name);
		if (dir->current_is_dir()) {
			if (p_exclude_addons && file_name == "addons") {
				file_name = dir->get_next();
				continue;
			}
			_collect_scene_files(full_path, r_files, p_exclude_addons, p_max_results);
		} else if (file_name.get_extension() == "tscn") {
			r_files.push_back(full_path);
		}
		file_name = dir->get_next();
	}
	dir->list_dir_end();
}

void JustAMCPBatchTools::_cross_scene_set_recursive(Node *p_node, Node *p_root, const String &p_type_name, const String &p_property, const Variant &p_value, Array &r_affected) {
	if (p_node->is_class(p_type_name) || String(p_node->get_class()) == p_type_name) {
		bool valid = false;
		p_node->set(p_property, p_value, &valid);
		if (valid) {
			r_affected.push_back(JustAMCPEditorSceneAccess::safe_path_to(p_root, p_node));
		}
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_cross_scene_set_recursive(p_node->get_child(i), p_root, p_type_name, p_property, p_value, r_affected);
	}
}

Dictionary JustAMCPBatchTools::_get_scene_dependencies(const Dictionary &p_params) {
	if (!p_params.has("path")) {
		return MCP_INVALID_PARAMS("Missing param: path");
	}
	String path = p_params["path"];

	if (!FileAccess::exists(path)) {
		return MCP_NOT_FOUND("File '" + path + "'");
	}

	List<String> deps;
	ResourceLoader::get_dependencies(path, &deps);

	Array dependencies;
	for (const String &dep : deps) {
		Vector<String> parts = dep.split("::");
		Dictionary d;
		d["path"] = parts.size() > 0 ? parts[0] : dep;
		d["type"] = parts.size() > 2 ? parts[2] : "";
		dependencies.push_back(d);
	}
	Dictionary res;
	res["path"] = path;
	res["dependencies"] = dependencies;
	res["count"] = dependencies.size();
	return MCP_SUCCESS(res);
}
