/**************************************************************************/
/*  justamcp_analysis_tools.cpp                                           */
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

#include "justamcp_analysis_tools.h"
#include "../justamcp_editor_scene_access.h"
#include "../justamcp_mcp_tool_macros.h"
#include "../justamcp_read_limits.h"
#include "core/config/project_settings.h"
#include "core/error/error_macros.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/object/script_language.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "scene/resources/packed_scene.h"

JustAMCPAnalysisTools::JustAMCPAnalysisTools() {
}

JustAMCPAnalysisTools::~JustAMCPAnalysisTools() {
}

Dictionary JustAMCPAnalysisTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "find_unused_resources") {
		return find_unused_resources(p_args);
	}
	if (p_tool_name == "analyze_signal_flow") {
		return analyze_signal_flow(p_args);
	}
	if (p_tool_name == "analyze_scene_complexity") {
		return analyze_scene_complexity(p_args);
	}
	if (p_tool_name == "find_script_references") {
		return find_script_references(p_args);
	}
	if (p_tool_name == "detect_circular_dependencies") {
		return detect_circular_dependencies(p_args);
	}
	if (p_tool_name == "get_project_statistics") {
		return get_project_statistics(p_args);
	}

	return Dictionary();
}

void JustAMCPAnalysisTools::_collect_files_by_ext(const String &p_path, const Vector<String> &p_extensions, Array &r_out, bool p_include_addons, int p_max_results) {
	if (p_max_results > 0 && r_out.size() >= p_max_results) {
		return;
	}
	Ref<DirAccess> dir = DirAccess::open(p_path);
	if (dir.is_null()) {
		return;
	}

	dir->list_dir_begin();
	String file_name = dir->get_next();

	while (!file_name.is_empty()) {
		if (p_max_results > 0 && r_out.size() >= p_max_results) {
			break;
		}
		if (file_name.begins_with(".")) {
			file_name = dir->get_next();
			continue;
		}

		String full_path = p_path.path_join(file_name);

		if (dir->current_is_dir()) {
			if (file_name == "addons" && !p_include_addons) {
				file_name = dir->get_next();
				continue;
			}
			_collect_files_by_ext(full_path, p_extensions, r_out, p_include_addons, p_max_results);
		} else {
			String ext = file_name.get_extension().to_lower();
			if (p_extensions.has(ext)) {
				r_out.push_back(full_path);
			}
		}

		file_name = dir->get_next();
	}
	dir->list_dir_end();
}

String JustAMCPAnalysisTools::_read_file_text(const String &p_file_path) {
	String text;
	int64_t size = 0;
	Dictionary limit_err;
	if (!justamcp_read_utf8_within_limit(p_file_path, JUSTAMCP_MAX_SYNC_READ_BYTES, text, size, limit_err)) {
		return String();
	}
	return text;
}

Dictionary JustAMCPAnalysisTools::_read_file_text_checked(const String &p_file_path, String &r_text) {
	int64_t size = 0;
	Dictionary limit_err;
	if (!justamcp_read_utf8_within_limit(p_file_path, JUSTAMCP_MAX_SYNC_READ_BYTES, r_text, size, limit_err)) {
		return limit_err;
	}
	return Dictionary();
}

Dictionary JustAMCPAnalysisTools::find_unused_resources(const Dictionary &p_params) {
	String path = p_params.get("path", "res://");
	bool include_addons = p_params.get("include_addons", false);
	int max_results = CLAMP(int(p_params.get("max_results", 2000)), 1, 10000);

	Vector<String> resource_extensions;
	resource_extensions.push_back("tres");
	resource_extensions.push_back("tscn");
	resource_extensions.push_back("png");
	resource_extensions.push_back("jpg");
	resource_extensions.push_back("jpeg");
	resource_extensions.push_back("svg");
	resource_extensions.push_back("wav");
	resource_extensions.push_back("ogg");
	resource_extensions.push_back("mp3");
	resource_extensions.push_back("ttf");
	resource_extensions.push_back("otf");
	resource_extensions.push_back("gdshader");
	resource_extensions.push_back("material");
	resource_extensions.push_back("theme");
	resource_extensions.push_back("stylebox");
	resource_extensions.push_back("font");
	resource_extensions.push_back("anim");

	Array all_resources;
	_collect_files_by_ext(path, resource_extensions, all_resources, include_addons, max_results);

	Vector<String> ref_extensions;
	ref_extensions.push_back("tscn");
	ref_extensions.push_back("gd");
	ref_extensions.push_back("tres");
	ref_extensions.push_back("cfg");
	ref_extensions.push_back("godot");

	Array ref_files;
	_collect_files_by_ext(path, ref_extensions, ref_files, include_addons, max_results);

	Dictionary referenced;
	for (int i = 0; i < ref_files.size(); i++) {
		String ref_file = ref_files[i];
		String content;
		Dictionary read_err = _read_file_text_checked(ref_file, content);
		if (!read_err.is_empty()) {
			if (read_err.has("max_bytes")) {
				return read_err;
			}
			continue;
		}

		int idx = 0;
		while (idx < content.length()) {
			int found = content.find("res://", idx);
			if (found == -1) {
				break;
			}

			int end = found + 6;
			while (end < content.length()) {
				char32_t c = content[end];
				if (c == '"' || c == '\'' || c == ' ' || c == '\n' || c == '\r' || c == ')' || c == ']' || c == '}') {
					break;
				}
				end++;
			}
			String ref_path = content.substr(found, end - found);
			referenced[ref_path] = true;
			idx = end;
		}
	}

	Array unused;
	for (int i = 0; i < all_resources.size(); i++) {
		String p = all_resources[i];
		if (!referenced.has(p)) {
			unused.push_back(p);
		}
	}

	Dictionary res;
	res["unused_resources"] = unused;
	res["unused_count"] = unused.size();
	res["total_resources_scanned"] = all_resources.size();
	res["total_files_checked"] = ref_files.size();
	return MCP_SUCCESS(res);
}

void JustAMCPAnalysisTools::_collect_signal_data(Node *p_node, Node *p_root, Array &r_out, int p_max_nodes, bool &r_truncated) {
	if (!p_node || !p_root || r_truncated) {
		return;
	}
	if (p_max_nodes > 0 && r_out.size() >= p_max_nodes) {
		r_truncated = true;
		return;
	}
	String node_path = p_root->get_path_to(p_node);
	Array signals_emitted;
	Array signals_connected_to;

	List<MethodInfo> slist;
	p_node->get_signal_list(&slist);

	for (const MethodInfo &sig : slist) {
		String sig_name = sig.name;
		List<Object::Connection> signal_connections;
		p_node->get_signal_connection_list(sig_name, &signal_connections);

		if (signal_connections.size() > 0) {
			Array targets;
			for (const Object::Connection &conn : signal_connections) {
				Callable callable = conn.callable;
				Node *target_node = Object::cast_to<Node>(callable.get_object());
				String target_path = "";
				if (target_node) {
					if (target_node == p_root || p_root->is_ancestor_of(target_node)) {
						target_path = p_root->get_path_to(target_node);
					} else {
						target_path = target_node->get_name();
					}
				}

				Dictionary target;
				target["target_node"] = target_path;
				target["method"] = callable.get_method();
				targets.push_back(target);

				Dictionary sc;
				sc["from_node"] = node_path;
				sc["signal"] = sig_name;
				sc["method"] = callable.get_method();
				signals_connected_to.push_back(sc);
			}

			Dictionary se;
			se["signal"] = sig_name;
			se["targets"] = targets;
			signals_emitted.push_back(se);
		}
	}

	if (signals_emitted.size() > 0 || signals_connected_to.size() > 0) {
		if (p_max_nodes > 0 && r_out.size() >= p_max_nodes) {
			r_truncated = true;
			return;
		}
		Dictionary node_data;
		node_data["name"] = p_node->get_name();
		node_data["path"] = node_path;
		node_data["type"] = p_node->get_class();
		node_data["signals_emitted"] = signals_emitted;
		node_data["signals_connected_to"] = signals_connected_to;
		r_out.push_back(node_data);
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_collect_signal_data(p_node->get_child(i), p_root, r_out, p_max_nodes, r_truncated);
		if (r_truncated) {
			return;
		}
	}
}

Dictionary JustAMCPAnalysisTools::analyze_signal_flow(const Dictionary &p_params) {
	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No scene is currently open");
	}

	const int max_nodes = CLAMP(int(p_params.get("max_nodes", 2000)), 1, 10000);
	Array nodes_data;
	bool truncated = false;
	_collect_signal_data(root, root, nodes_data, max_nodes, truncated);

	Dictionary res;
	res["scene"] = root->get_scene_file_path();
	res["nodes"] = nodes_data;
	res["total_nodes"] = nodes_data.size();
	res["truncated"] = truncated;
	return MCP_SUCCESS(res);
}

void JustAMCPAnalysisTools::_analyze_node(Node *p_node, Node *p_root, int p_depth, int &r_total_nodes, int &r_max_depth, Dictionary &r_types, Array &r_scripts, Dictionary &r_resources, int p_max_nodes, bool &r_truncated) {
	if (!p_node || r_truncated) {
		return;
	}
	if (p_max_nodes > 0 && r_total_nodes >= p_max_nodes) {
		r_truncated = true;
		return;
	}
	r_total_nodes++;
	if (p_depth > r_max_depth) {
		r_max_depth = p_depth;
	}
	String type_name = p_node->get_class();
	int tcount = r_types.get(type_name, 0);
	r_types[type_name] = tcount + 1;

	Ref<Script> node_script = p_node->get_script();
	if (node_script.is_valid()) {
		String script_path = node_script->get_path();
		if (!script_path.is_empty()) {
			Dictionary sd;
			if (p_root && p_node) {
				sd["node"] = String(p_root->get_path_to(p_node));
			}
			sd["script"] = script_path;
			r_scripts.push_back(sd);
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_analyze_node(p_node->get_child(i), p_root, p_depth + 1, r_total_nodes, r_max_depth, r_types, r_scripts, r_resources, p_max_nodes, r_truncated);
		if (r_truncated) {
			return;
		}
	}
}

int JustAMCPAnalysisTools::_count_nodes_recursive(Node *p_node, int p_max_nodes, bool &r_truncated) {
	if (!p_node || r_truncated) {
		return 0;
	}
	int count = 1;
	if (p_max_nodes > 0 && count >= p_max_nodes) {
		r_truncated = true;
		return count;
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		count += _count_nodes_recursive(p_node->get_child(i), p_max_nodes > 0 ? p_max_nodes - count : 0, r_truncated);
		if (r_truncated || (p_max_nodes > 0 && count >= p_max_nodes)) {
			r_truncated = true;
			return count;
		}
	}
	return count;
}

int JustAMCPAnalysisTools::_get_max_depth(Node *p_node, int p_current_depth, int p_max_nodes, int &r_visited, bool &r_truncated) {
	if (!p_node || r_truncated) {
		return p_current_depth;
	}
	r_visited++;
	if (p_max_nodes > 0 && r_visited >= p_max_nodes) {
		r_truncated = true;
		return p_current_depth;
	}
	int max_d = p_current_depth;
	for (int i = 0; i < p_node->get_child_count(); i++) {
		int child_depth = _get_max_depth(p_node->get_child(i), p_current_depth + 1, p_max_nodes, r_visited, r_truncated);
		if (child_depth > max_d) {
			max_d = child_depth;
		}
		if (r_truncated) {
			return max_d;
		}
	}
	return max_d;
}

Dictionary JustAMCPAnalysisTools::analyze_scene_complexity(const Dictionary &p_params) {
	String scene_path = p_params.get("path", "");
	const int max_nodes = CLAMP(int(p_params.get("max_nodes", 2000)), 1, 10000);

	Node *root = nullptr;
	bool instantiated = false;

	if (scene_path.is_empty()) {
		root = JustAMCPEditorSceneAccess::get_edited_root();
		if (!root) {
			return MCP_ERROR(-32000, "No scene is currently open");
		}
		scene_path = root->get_scene_file_path();
	} else {
		if (!ResourceLoader::exists(scene_path)) {
			return MCP_ERROR(-32000, "Scene not found: " + scene_path);
		}
		Ref<PackedScene> packed = ResourceLoader::load(scene_path);
		if (packed.is_null()) {
			return MCP_ERROR(-32000, "Failed to load scene: " + scene_path);
		}
		root = packed->instantiate();
		instantiated = true;
	}

	int total_nodes = 0;
	int max_depth = 0;
	Dictionary types;
	Array scripts_attached;
	Dictionary resources_used;
	Array issues;
	bool truncated = false;

	_analyze_node(root, root, 0, total_nodes, max_depth, types, scripts_attached, resources_used, max_nodes, truncated);

	if (total_nodes > 1000) {
		Dictionary i;
		i["severity"] = "warning";
		i["message"] = vformat("Scene has %d nodes (>1000). Consider splitting into sub-scenes.", total_nodes);
		issues.push_back(i);
	} else if (total_nodes > 500) {
		Dictionary i;
		i["severity"] = "info";
		i["message"] = vformat("Scene has %d nodes (>500). Monitor performance.", total_nodes);
		issues.push_back(i);
	}

	if (max_depth > 15) {
		Dictionary i;
		i["severity"] = "warning";
		i["message"] = vformat("Max nesting depth is %d (>15). Deep hierarchies can be hard to maintain.", max_depth);
		issues.push_back(i);
	} else if (max_depth > 10) {
		Dictionary i;
		i["severity"] = "info";
		i["message"] = vformat("Max nesting depth is %d (>10).", max_depth);
		issues.push_back(i);
	}

	if (instantiated && root) {
		memdelete(root);
	}

	Dictionary res;
	res["scene_path"] = scene_path;
	res["total_nodes"] = total_nodes;
	res["max_depth"] = max_depth;
	res["nodes_by_type"] = types;
	res["scripts_attached"] = scripts_attached;
	res["unique_resource_count"] = resources_used.size();
	res["issues"] = issues;
	res["truncated"] = truncated;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPAnalysisTools::find_script_references(const Dictionary &p_params) {
	if (!p_params.has("query")) {
		return MCP_INVALID_PARAMS("Missing query");
	}
	String query = p_params["query"];

	String path = p_params.get("path", "res://");
	bool include_addons = p_params.get("include_addons", false);
	int max_results = CLAMP(int(p_params.get("max_results", 2000)), 1, 10000);

	Vector<String> search_extensions;
	search_extensions.push_back("tscn");
	search_extensions.push_back("gd");
	search_extensions.push_back("tres");
	search_extensions.push_back("cfg");
	search_extensions.push_back("godot");

	Array search_files;
	_collect_files_by_ext(path, search_extensions, search_files, include_addons, max_results);

	Array references;
	for (int i = 0; i < search_files.size(); i++) {
		String fp = search_files[i];
		String content;
		Dictionary read_err = _read_file_text_checked(fp, content);
		if (!read_err.is_empty()) {
			if (read_err.has("max_bytes")) {
				return read_err;
			}
			continue;
		}

		Vector<String> lines = content.split("\n");
		for (int l = 0; l < lines.size(); l++) {
			String line = lines[l];
			if (line.contains(query)) {
				Dictionary ref;
				ref["file"] = fp;
				ref["line"] = l + 1;
				ref["content"] = line.strip_edges();
				references.push_back(ref);
			}
		}
	}

	Dictionary res;
	res["query"] = query;
	res["references"] = references;
	res["reference_count"] = references.size();
	res["files_searched"] = search_files.size();
	return MCP_SUCCESS(res);
}

void JustAMCPAnalysisTools::_dfs_detect_cycle(const String &p_node, const Dictionary &p_graph, Dictionary &p_visited, Array &p_path_stack, Array &p_cycles) {
	p_visited[p_node] = "visiting";
	p_path_stack.push_back(p_node);

	if (p_graph.has(p_node)) {
		Array deps = p_graph[p_node];
		for (int i = 0; i < deps.size(); i++) {
			String d = deps[i];
			if (!p_visited.has(d)) {
				continue;
			}

			String vstatus = p_visited[d];
			if (vstatus == "visiting") {
				int cycle_start = p_path_stack.find(d);
				if (cycle_start != -1) {
					Array cycle = p_path_stack.slice(cycle_start);
					cycle.push_back(d);
					p_cycles.push_back(cycle);
				}
			} else if (vstatus == "unvisited") {
				_dfs_detect_cycle(d, p_graph, p_visited, p_path_stack, p_cycles);
			}
		}
	}

	p_path_stack.pop_back();
	p_visited[p_node] = "visited";
}

Dictionary JustAMCPAnalysisTools::detect_circular_dependencies(const Dictionary &p_params) {
	String path = p_params.get("path", "res://");
	bool include_addons = p_params.get("include_addons", false);
	int max_results = CLAMP(int(p_params.get("max_results", 2000)), 1, 10000);

	Vector<String> tscn_exts;
	tscn_exts.push_back("tscn");

	Array tscn_files;
	_collect_files_by_ext(path, tscn_exts, tscn_files, include_addons, max_results);

	Dictionary dep_graph;
	for (int i = 0; i < tscn_files.size(); i++) {
		String tp = tscn_files[i];
		String content;
		Dictionary read_err = _read_file_text_checked(tp, content);
		if (!read_err.is_empty()) {
			if (read_err.has("max_bytes")) {
				return read_err;
			}
			continue;
		}

		Array deps;
		Vector<String> lines = content.split("\n");
		for (int l = 0; l < lines.size(); l++) {
			String line = lines[l];
			if (line.begins_with("[ext_resource") && line.contains(".tscn")) {
				int path_start = line.find("path=\"");
				if (path_start == -1) {
					continue;
				}
				path_start += 6;
				int path_end = line.find("\"", path_start);
				if (path_end == -1) {
					continue;
				}

				String ref_path = line.substr(path_start, path_end - path_start);
				if (ref_path.ends_with(".tscn")) {
					deps.push_back(ref_path);
				}
			}
		}
		dep_graph[tp] = deps;
	}

	Array cycles;
	Dictionary visited;
	for (int i = 0; i < tscn_files.size(); i++) {
		visited[tscn_files[i]] = "unvisited";
	}

	for (int i = 0; i < tscn_files.size(); i++) {
		String scene = tscn_files[i];
		if (String(visited[scene]) == "unvisited") {
			Array path_stack;
			_dfs_detect_cycle(scene, dep_graph, visited, path_stack, cycles);
		}
	}

	Dictionary res;
	res["scenes_checked"] = tscn_files.size();
	res["circular_dependencies"] = cycles;
	res["has_circular"] = cycles.size() > 0;
	res["dependency_graph"] = dep_graph;
	return MCP_SUCCESS(res);
}

void JustAMCPAnalysisTools::_collect_statistics(const String &p_path, bool p_include_addons, Dictionary &r_file_counts, int p_max_files, bool &r_truncated) {
	if (r_truncated) {
		return;
	}
	Ref<DirAccess> dir = DirAccess::open(p_path);
	if (dir.is_null()) {
		return;
	}

	dir->list_dir_begin();
	String file_name = dir->get_next();

	while (!file_name.is_empty()) {
		if (p_max_files > 0 && int(r_file_counts.get("_total_files", 0)) >= p_max_files) {
			r_truncated = true;
			break;
		}
		if (file_name.begins_with(".")) {
			file_name = dir->get_next();
			continue;
		}

		String full_path = p_path.path_join(file_name);

		if (dir->current_is_dir()) {
			if (file_name == "addons" && !p_include_addons) {
				file_name = dir->get_next();
				continue;
			}
			_collect_statistics(full_path, p_include_addons, r_file_counts, p_max_files, r_truncated);
			if (r_truncated) {
				break;
			}
		} else {
			String ext = file_name.get_extension().to_lower();
			int current_ext_count = r_file_counts.get(ext, 0);
			r_file_counts[ext] = current_ext_count + 1;

			if (ext == "gd") {
				String content = _read_file_text(full_path);
				int line_count = content.is_empty() ? 0 : content.count("\n") + 1;
				int total_script_lines = r_file_counts.get("_total_script_lines", 0);
				r_file_counts["_total_script_lines"] = total_script_lines + line_count;
			}

			if (ext == "tscn") {
				int scene_count = r_file_counts.get("_scene_count", 0);
				r_file_counts["_scene_count"] = scene_count + 1;
			}

			if (ext == "tres" || ext == "material" || ext == "theme" || ext == "stylebox" || ext == "font") {
				int res_count = r_file_counts.get("_resource_count", 0);
				r_file_counts["_resource_count"] = res_count + 1;
			}

			int total_files = r_file_counts.get("_total_files", 0);
			r_file_counts["_total_files"] = total_files + 1;
		}

		file_name = dir->get_next();
	}
	dir->list_dir_end();
}

Dictionary JustAMCPAnalysisTools::get_project_statistics(const Dictionary &p_params) {
	String path = p_params.get("path", "res://");
	bool include_addons = p_params.get("include_addons", false);
	const int max_files = CLAMP(int(p_params.get("max_files", 2000)), 1, 10000);

	Dictionary file_counts;
	bool truncated = false;
	_collect_statistics(path, include_addons, file_counts, max_files, truncated);

	int total_script_lines = file_counts.get("_total_script_lines", 0);
	int scene_count = file_counts.get("_scene_count", 0);
	int resource_count = file_counts.get("_resource_count", 0);
	int total_files = file_counts.get("_total_files", 0);

	file_counts.erase("_total_script_lines");
	file_counts.erase("_scene_count");
	file_counts.erase("_resource_count");
	file_counts.erase("_total_files");

	Dictionary autoloads;
	List<PropertyInfo> props;
	ProjectSettings::get_singleton()->get_property_list(&props);
	for (const PropertyInfo &prop : props) {
		String prop_name = prop.name;
		if (prop_name.begins_with("autoload/")) {
			autoloads[prop_name.substr(9)] = String(Variant(ProjectSettings::get_singleton()->get(prop_name)));
		}
	}

	Array plugins;
	String plugin_cfg_path = "res://addons";
	PackedStringArray enabled_plugins = ProjectSettings::get_singleton()->get("editor_plugins/enabled");

	Ref<DirAccess> plugin_dir = DirAccess::open(plugin_cfg_path);
	if (plugin_dir.is_valid()) {
		plugin_dir->list_dir_begin();
		String dir_name = plugin_dir->get_next();
		while (!dir_name.is_empty()) {
			if (plugin_dir->current_is_dir() && !dir_name.begins_with(".")) {
				String cfg_path = plugin_cfg_path.path_join(dir_name).path_join("plugin.cfg");
				if (FileAccess::exists(cfg_path)) {
					String plugin_path = "res://addons/" + dir_name + "/plugin.cfg";
					Dictionary plug;
					plug["name"] = dir_name;
					plug["enabled"] = enabled_plugins.has(plugin_path);
					plugins.push_back(plug);
				}
			}
			dir_name = plugin_dir->get_next();
		}
		plugin_dir->list_dir_end();
	}

	Dictionary res;
	res["file_counts_by_extension"] = file_counts;
	res["total_files"] = total_files;
	res["total_script_lines"] = total_script_lines;
	res["scene_count"] = scene_count;
	res["resource_count"] = resource_count;
	res["autoloads"] = autoloads;
	res["plugins"] = plugins;
	res["truncated"] = truncated;
	return MCP_SUCCESS(res);
}

#endif
