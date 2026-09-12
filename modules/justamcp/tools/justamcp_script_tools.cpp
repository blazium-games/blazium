/**************************************************************************/
/*  justamcp_script_tools.cpp                                             */
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

#include "justamcp_script_tools.h"
#include "../justamcp_editor_filesystem.h"
#include "../justamcp_editor_scene_access.h"
#include "../justamcp_read_limits.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_interface.h"
#include "editor/plugins/script_editor_plugin.h"
#include "scene/gui/control.h"
#endif

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/script_language.h"
#include "modules/regex/regex.h"
#include "scene/resources/packed_scene.h"

#include "../justamcp_mcp_tool_macros.h"

void JustAMCPScriptTools::_bind_methods() {}

JustAMCPScriptTools::JustAMCPScriptTools() {}
JustAMCPScriptTools::~JustAMCPScriptTools() {}

Node *JustAMCPScriptTools::_find_node_by_path(const String &p_path) {
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

Dictionary JustAMCPScriptTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "list_scripts") {
		return _list_scripts(p_args);
	}
	if (p_tool_name == "read_script") {
		return _read_script(p_args);
	}
	if (p_tool_name == "create_script") {
		return _create_script(p_args);
	}
	if (p_tool_name == "edit_script") {
		return _edit_script(p_args);
	}
	if (p_tool_name == "delete_script") {
		return _delete_script(p_args);
	}
	if (p_tool_name == "attach_script") {
		return _attach_script(p_args);
	}
	if (p_tool_name == "detach_script") {
		return _detach_script(p_args);
	}
	if (p_tool_name == "get_open_scripts") {
		return _get_open_scripts(p_args);
	}
	if (p_tool_name == "open_script_in_editor") {
		return _open_script_in_editor(p_args);
	}
	if (p_tool_name == "get_script_errors") {
		return _get_script_errors(p_args);
	}
	if (p_tool_name == "search_in_scripts") {
		return _search_in_scripts(p_args);
	}
	if (p_tool_name == "find_script_symbols") {
		return _find_script_symbols(p_args);
	}
	if (p_tool_name == "patch_script") {
		return _patch_script(p_args);
	}
	if (p_tool_name == "validate_script") {
		return _validate_script(p_args);
	}
	if (p_tool_name == "get_script_metadata") {
		return _get_script_metadata(p_args);
	}
	if (p_tool_name == "get_script_references") {
		return _get_script_references(p_args);
	}

	return Dictionary();
}

Dictionary JustAMCPScriptTools::_list_scripts(const Dictionary &p_params) {
	String path = p_params.has("path") ? String(p_params["path"]) : "res://";
	bool recursive = p_params.has("recursive") ? bool(p_params["recursive"]) : true;
	const int max_results = CLAMP(int(p_params.get("max_results", 2000)), 1, 10000);

	Array scripts;
	_find_scripts(path, recursive, scripts, max_results);
	const bool truncated = scripts.size() >= max_results;

	Dictionary res;
	res["scripts"] = scripts;
	res["count"] = scripts.size();
	res["truncated"] = truncated;
	return MCP_SUCCESS(res);
}

void JustAMCPScriptTools::_find_scripts(const String &p_path, bool p_recursive, Array &r_scripts, int p_max_results) {
	if (p_max_results > 0 && r_scripts.size() >= p_max_results) {
		return;
	}
	Ref<DirAccess> dir = DirAccess::open(p_path);
	if (dir.is_null()) {
		return;
	}

	dir->list_dir_begin();
	String file_name = dir->get_next();
	while (!file_name.is_empty()) {
		if (p_max_results > 0 && r_scripts.size() >= p_max_results) {
			break;
		}
		if (file_name.begins_with(".")) {
			file_name = dir->get_next();
			continue;
		}

		String full_path = p_path.path_join(file_name);
		if (dir->current_is_dir()) {
			if (p_recursive) {
				_find_scripts(full_path, p_recursive, r_scripts, p_max_results);
			}
		} else if (file_name.get_extension() == "gd" || file_name.get_extension() == "cs" || file_name.get_extension() == "gdshader") {
			Dictionary info;
			info["path"] = full_path;
			info["type"] = file_name.get_extension();

			Ref<FileAccess> file = FileAccess::open(full_path, FileAccess::READ);
			if (file.is_valid()) {
				info["size"] = file->get_length();
				String first_line = file->get_line().strip_edges();
				if (first_line.begins_with("class_name ")) {
					info["class_name"] = first_line.substr(11).strip_edges();
				} else if (first_line.begins_with("extends ")) {
					info["extends"] = first_line.substr(8).strip_edges();
				}
				file->close();
			}
			r_scripts.push_back(info);
		}
		file_name = dir->get_next();
	}
	dir->list_dir_end();
}

Dictionary JustAMCPScriptTools::_read_script(const Dictionary &p_params) {
	if (!p_params.has("path")) {
		return MCP_INVALID_PARAMS("Missing param: path");
	}
	String path = p_params["path"];

	if (!FileAccess::exists(path)) {
		return MCP_NOT_FOUND("Script '" + path + "'");
	}

	int64_t file_size = 0;
	if (!justamcp_file_within_read_limit(path, JUSTAMCP_MAX_SYNC_READ_BYTES, file_size)) {
		return justamcp_read_limit_error(path, file_size, JUSTAMCP_MAX_SYNC_READ_BYTES);
	}

	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
	if (file.is_null()) {
		return MCP_INTERNAL("Cannot read script");
	}

	String content = file->get_as_text();
	int line_count = content.get_slice_count("\n");
	file->close();

	Dictionary res;
	res["path"] = path;
	res["content"] = content;
	res["line_count"] = line_count;
	res["size"] = content.length();
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPScriptTools::_create_script(const Dictionary &p_params) {
	if (!p_params.has("path")) {
		return MCP_INVALID_PARAMS("Missing param: path");
	}
	String path = p_params["path"];

	String content = p_params.has("content") ? String(p_params["content"]) : "";
	String base_class = p_params.has("extends") ? String(p_params["extends"]) : "Node";
	String class_name_str = p_params.has("class_name") ? String(p_params["class_name"]) : "";

	if (content.is_empty()) {
		Vector<String> lines;
		if (!class_name_str.is_empty()) {
			lines.push_back("class_name " + class_name_str);
		}
		lines.push_back("extends " + base_class);
		lines.push_back("");
		lines.push_back("");
		lines.push_back("func _ready() -> void:");
		lines.push_back("\tpass");
		lines.push_back("");
		content = String("\n").join(lines);
	}

	String dir_path = path.get_base_dir();
	if (!DirAccess::dir_exists_absolute(dir_path)) {
		DirAccess::make_dir_recursive_absolute(dir_path);
	}

	Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
	if (file.is_null()) {
		return MCP_INTERNAL("Cannot create script");
	}

	file->store_string(content);
	file->close();

	_reload_script(path);

	Dictionary res;
	res["path"] = path;
	res["created"] = true;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPScriptTools::_edit_script(const Dictionary &p_params) {
	if (!p_params.has("path")) {
		return MCP_INVALID_PARAMS("Missing param: path");
	}
	String path = p_params["path"];

	if (!FileAccess::exists(path)) {
		return MCP_NOT_FOUND("Script '" + path + "'");
	}

	int64_t file_size = 0;
	if (!justamcp_file_within_read_limit(path, JUSTAMCP_MAX_SYNC_READ_BYTES, file_size)) {
		return justamcp_read_limit_error(path, file_size, JUSTAMCP_MAX_SYNC_READ_BYTES);
	}

	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
	if (file.is_null()) {
		return MCP_INTERNAL("Cannot read script");
	}

	String content = file->get_as_text();
	file->close();

	int changes_made = 0;

	if (p_params.has("replacements") && p_params["replacements"].get_type() == Variant::ARRAY) {
		Array replacements = p_params["replacements"];
		for (int i = 0; i < replacements.size(); i++) {
			if (replacements[i].get_type() == Variant::DICTIONARY) {
				Dictionary rep = replacements[i];
				String search = rep.has("search") ? String(rep["search"]) : "";
				String replace = rep.has("replace") ? String(rep["replace"]) : "";
				bool use_regex = rep.has("regex") ? bool(rep["regex"]) : false;

				if (!search.is_empty()) {
					if (use_regex) {
						Ref<RegEx> regex;
						regex.instantiate();
						if (regex->compile(search) == OK) {
							String new_content = regex->sub(content, replace, true);
							if (new_content != content) {
								content = new_content;
								changes_made++;
							}
						}
					} else {
						if (content.contains(search)) {
							content = content.replace(search, replace);
							changes_made++;
						}
					}
				}
			}
		}
	} else if (p_params.has("content")) {
		content = p_params["content"];
		changes_made++;
	} else if (p_params.has("insert_at_line") && p_params.has("text")) {
		int line_num = p_params["insert_at_line"];
		String text = p_params["text"];
		Vector<String> lines = content.split("\n");
		line_num = CLAMP(line_num, 0, lines.size());
		lines.insert(line_num, text);
		content = String("\n").join(lines);
		changes_made++;
	}

	if (changes_made == 0) {
		Dictionary res;
		res["path"] = path;
		res["changes_made"] = 0;
		res["message"] = "No changes applied";
		return MCP_SUCCESS(res);
	}

	file = FileAccess::open(path, FileAccess::WRITE);
	if (file.is_null()) {
		return MCP_INTERNAL("Cannot write script");
	}

	file->store_string(content);
	file->close();

	_reload_script(path);

	Dictionary res;
	res["path"] = path;
	res["changes_made"] = changes_made;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPScriptTools::_delete_script(const Dictionary &p_params) {
	if (!p_params.has("path")) {
		return MCP_INVALID_PARAMS("Missing param: path");
	}
	String path = p_params["path"];
	const String ext = path.get_extension().to_lower();
	if (ext != "gd" && ext != "cs") {
		return MCP_INVALID_PARAMS("delete_script path must be a script file (.gd, .cs)");
	}
	const String main_scene = ProjectSettings::get_singleton() ? String(ProjectSettings::get_singleton()->get_setting("application/run/main_scene", "")) : String();
	if (ProjectSettings::is_project_settings_file(path) || path == "res://export_presets.cfg" || path == "res://main.gd" || path == "res://main.tscn" || (!main_scene.is_empty() && (path == main_scene || path == main_scene.get_basename() + ".gd"))) {
		return MCP_ERROR(-32000, "Refusing to delete protected project path: " + path);
	}
	if (!FileAccess::exists(path)) {
		return MCP_NOT_FOUND("Script '" + path + "'");
	}
	Error err = DirAccess::remove_absolute(path);
	if (err != OK) {
		return MCP_INTERNAL("Failed to delete script: " + itos(err));
	}
#ifdef TOOLS_ENABLED
	JustAMCPEditorFilesystem::refresh_path(path);
#endif
	Dictionary res;
	res["path"] = path;
	res["deleted"] = true;
	return MCP_SUCCESS(res);
}

void JustAMCPScriptTools::_reload_script(const String &p_path) {
#ifdef TOOLS_ENABLED
	JustAMCPEditorFilesystem::refresh_path(p_path);
#endif
	if (ResourceLoader::exists(p_path)) {
		Ref<Script> loaded_script = ResourceLoader::load(p_path);
		if (loaded_script.is_valid()) {
			loaded_script->reload(true);
		}
	}
#ifdef TOOLS_ENABLED
	if (EditorInterface::get_singleton() && EditorInterface::get_singleton()->get_script_editor()) {
		EditorInterface::get_singleton()->get_script_editor()->notification(Control::NOTIFICATION_VISIBILITY_CHANGED);
	}
#endif
}

Dictionary JustAMCPScriptTools::_attach_script(const Dictionary &p_params) {
	String node_path = p_params.get("node_path", p_params.get("nodePath", ""));
	if (node_path.is_empty()) {
		return MCP_INVALID_PARAMS("Missing param: node_path");
	}
	String script_path = p_params.get("script_path", p_params.get("scriptPath", ""));
	if (script_path.is_empty()) {
		return MCP_INVALID_PARAMS("Missing param: script_path");
	}

	if (!script_path.begins_with("res://") && FileAccess::exists(script_path)) {
	} else if (!ResourceLoader::exists(script_path)) {
		return MCP_NOT_FOUND("Script '" + script_path + "'");
	}

	Ref<Script> loaded_script = ResourceLoader::load(script_path);
	if (loaded_script.is_null()) {
		return MCP_INTERNAL("Failed to load script: " + script_path);
	}

	String scene_path = p_params.get("scenePath", p_params.get("scene_path", ""));

	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (root && (scene_path.is_empty() || root->get_scene_file_path() == scene_path)) {
		Node *node = _find_node_by_path(node_path);
		if (!node) {
			return MCP_NOT_FOUND("Node '" + node_path + "'");
		}
		node->set_script(loaded_script);
		Dictionary res;
		res["node_path"] = JustAMCPEditorSceneAccess::safe_path_to(root, node);
		res["script_path"] = script_path;
		res["attached"] = true;
		return MCP_SUCCESS(res);
	}

	if (scene_path.is_empty()) {
		return MCP_ERROR(-32000, "No scene is currently open");
	}

	if (!scene_path.ends_with(".tscn")) {
		scene_path += ".tscn";
	}
	if (!ResourceLoader::exists(scene_path)) {
		return MCP_NOT_FOUND("Scene '" + scene_path + "'");
	}

	Ref<PackedScene> packed_scene = ResourceLoader::load(scene_path);
	if (packed_scene.is_null()) {
		return MCP_INTERNAL("Failed to load scene: " + scene_path);
	}

	Node *scene_root = packed_scene->instantiate();
	if (!scene_root) {
		return MCP_INTERNAL("Failed to instantiate scene: " + scene_path);
	}

	Node *node = scene_root;
	if (node_path != "." && !node_path.is_empty()) {
		if (scene_root->has_node(node_path)) {
			node = scene_root->get_node(node_path);
		} else {
			memdelete(scene_root);
			return MCP_NOT_FOUND("Node '" + node_path + "'");
		}
	}

	node->set_script(loaded_script);

	Ref<PackedScene> out_packed;
	out_packed.instantiate();
	if (out_packed->pack(scene_root) != OK) {
		memdelete(scene_root);
		return MCP_INTERNAL("Failed to pack scene after attach: " + scene_path);
	}
	if (ResourceSaver::save(out_packed, scene_path) != OK) {
		memdelete(scene_root);
		return MCP_INTERNAL("Failed to save scene after attach: " + scene_path);
	}
	memdelete(scene_root);

	Dictionary res;
	res["node_path"] = node_path;
	res["script_path"] = script_path;
	res["scene_path"] = scene_path;
	res["attached"] = true;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPScriptTools::_detach_script(const Dictionary &p_params) {
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
	node->set_script(Variant());
	Dictionary res;
	res["node_path"] = JustAMCPEditorSceneAccess::safe_path_to(root, node);
	res["detached"] = true;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPScriptTools::_get_open_scripts(const Dictionary &p_params) {
	Array open_scripts;
#ifdef TOOLS_ENABLED
	if (EditorInterface::get_singleton() && EditorInterface::get_singleton()->get_script_editor()) {
		Vector<Ref<Script>> scripts = EditorInterface::get_singleton()->get_script_editor()->get_open_scripts();
		for (int i = 0; i < scripts.size(); i++) {
			Ref<Script> s = scripts[i];
			if (s.is_valid()) {
				Dictionary info;
				info["path"] = s->get_path();
				info["type"] = s->get_class();
				open_scripts.push_back(info);
			}
		}
	}
#endif
	Dictionary res;
	res["scripts"] = open_scripts;
	res["count"] = open_scripts.size();
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPScriptTools::_open_script_in_editor(const Dictionary &p_params) {
	if (!p_params.has("path")) {
		return MCP_INVALID_PARAMS("Missing param: path");
	}
	String path = p_params["path"];
	int line = p_params.has("line") ? int(p_params["line"]) : -1;
	if (!FileAccess::exists(path)) {
		return MCP_NOT_FOUND("Script '" + path + "'");
	}
#ifdef TOOLS_ENABLED
	Ref<Script> loaded_script = ResourceLoader::load(path);
	if (loaded_script.is_valid() && EditorInterface::get_singleton()) {
		EditorInterface::get_singleton()->edit_script(loaded_script, line);
		Dictionary res;
		res["path"] = path;
		res["line"] = line;
		res["opened"] = true;
		return MCP_SUCCESS(res);
	}
#endif
	return MCP_INTERNAL("Script editor is unavailable or script failed to load.");
}

Dictionary JustAMCPScriptTools::_get_script_errors(const Dictionary &p_params) {
	Dictionary validation = _validate_script(p_params);
	Dictionary res;
	res["errors"] = Array();
	res["message"] = "Use validate_script for compile status and editor/LSP diagnostics for detailed errors.";
	if (validation.has("result")) {
		Dictionary result = validation["result"];
		if (result.has("valid") && !bool(result["valid"])) {
			Array errors;
			Dictionary error;
			error["path"] = result.get("path", p_params.get("path", ""));
			error["message"] = result.get("message", "Compilation failed.");
			error["error_code"] = result.get("error_code", 0);
			errors.push_back(error);
			res["errors"] = errors;
		}
	}
	return MCP_SUCCESS(res);
}
