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
#include "justamcp_tool_executor.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_file_system.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/plugins/script_editor_plugin.h"
#endif

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/script_language.h"
#include "modules/regex/regex.h"
#include "scene/resources/packed_scene.h"

static inline Dictionary _MCP_SUCCESS(const Variant &data) {
	Dictionary r;
	r["ok"] = true;
	r["result"] = data;
	return r;
}
static inline Dictionary _MCP_ERROR_INTERNAL(int code, const String &msg) {
	Dictionary e, r;
	e["code"] = code;
	e["message"] = msg;
	r["ok"] = false;
	r["error"] = e;
	return r;
}
[[maybe_unused]] static inline Dictionary _MCP_ERROR_DATA(int code, const String &msg, const Variant &data) {
	Dictionary e, r;
	e["code"] = code;
	e["message"] = msg;
	e["data"] = data;
	r["ok"] = false;
	r["error"] = e;
	return r;
}
#undef MCP_SUCCESS
#undef MCP_ERROR
#undef MCP_ERROR_DATA
#undef MCP_INVALID_PARAMS
#undef MCP_NOT_FOUND
#undef MCP_INTERNAL
#define MCP_SUCCESS(data) _MCP_SUCCESS(data)
#define MCP_ERROR(code, msg) _MCP_ERROR_INTERNAL(code, msg)
#define MCP_ERROR_DATA(code, msg, data) _MCP_ERROR_DATA(code, msg, data)
#define MCP_INVALID_PARAMS(msg) _MCP_ERROR_INTERNAL(-32602, msg)
#define MCP_NOT_FOUND(msg) _MCP_ERROR_DATA(-32001, String(msg) + " not found", Dictionary())
#define MCP_INTERNAL(msg) _MCP_ERROR_INTERNAL(-32603, String("Internal error: ") + msg)

void JustAMCPScriptTools::_bind_methods() {}

JustAMCPScriptTools::JustAMCPScriptTools() {}
JustAMCPScriptTools::~JustAMCPScriptTools() {}

Node *JustAMCPScriptTools::_get_edited_root() {
#ifdef TOOLS_ENABLED
	if (JustAMCPToolExecutor::get_test_scene_root()) {
		return JustAMCPToolExecutor::get_test_scene_root();
	}
	if (EditorNode::get_singleton() && EditorInterface::get_singleton()) {
		return EditorInterface::get_singleton()->get_edited_scene_root();
	}
#endif
	return nullptr;
}

Node *JustAMCPScriptTools::_find_node_by_path(const String &p_path) {
	Node *root = _get_edited_root();
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

	Dictionary err;
	err["code"] = -32601;
	err["message"] = "Method not found: " + p_tool_name;
	Dictionary res;
	res["error"] = err;
	return res;
}

Dictionary JustAMCPScriptTools::_list_scripts(const Dictionary &p_params) {
	String path = p_params.has("path") ? String(p_params["path"]) : "res://";
	bool recursive = p_params.has("recursive") ? bool(p_params["recursive"]) : true;

	Array scripts;
	_find_scripts(path, recursive, scripts);

	Dictionary res;
	res["scripts"] = scripts;
	res["count"] = scripts.size();
	return MCP_SUCCESS(res);
}

void JustAMCPScriptTools::_find_scripts(const String &p_path, bool p_recursive, Array &r_scripts) {
	Ref<DirAccess> dir = DirAccess::open(p_path);
	if (dir.is_null()) {
		return;
	}

	dir->list_dir_begin();
	String file_name = dir->get_next();
	while (!file_name.is_empty()) {
		if (file_name.begins_with(".")) {
			file_name = dir->get_next();
			continue;
		}

		String full_path = p_path.path_join(file_name);
		if (dir->current_is_dir()) {
			if (p_recursive) {
				_find_scripts(full_path, p_recursive, r_scripts);
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
	if (!FileAccess::exists(path)) {
		return MCP_NOT_FOUND("Script '" + path + "'");
	}
	Error err = DirAccess::remove_absolute(path);
	if (err != OK) {
		return MCP_INTERNAL("Failed to delete script: " + itos(err));
	}
#ifdef TOOLS_ENABLED
	if (EditorFileSystem::get_singleton()) {
		EditorFileSystem::get_singleton()->scan();
	}
#endif
	Dictionary res;
	res["path"] = path;
	res["deleted"] = true;
	return MCP_SUCCESS(res);
}

void JustAMCPScriptTools::_reload_script(const String &p_path) {
#ifdef TOOLS_ENABLED
	if (EditorFileSystem::get_singleton()) {
		EditorFileSystem::get_singleton()->scan();
	}
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
		// Keep absolute paths as-is for existence check.
	} else if (!ResourceLoader::exists(script_path)) {
		return MCP_NOT_FOUND("Script '" + script_path + "'");
	}

	Ref<Script> loaded_script = ResourceLoader::load(script_path);
	if (loaded_script.is_null()) {
		return MCP_INTERNAL("Failed to load script: " + script_path);
	}

	String scene_path = p_params.get("scenePath", p_params.get("scene_path", ""));

	Node *root = _get_edited_root();
	if (root && (scene_path.is_empty() || root->get_scene_file_path() == scene_path)) {
		Node *node = _find_node_by_path(node_path);
		if (!node) {
			return MCP_NOT_FOUND("Node '" + node_path + "'");
		}
		node->set_script(loaded_script);
		Dictionary res;
		res["node_path"] = root->get_path_to(node);
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
	Node *root = _get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No scene is currently open");
	}
	Node *node = _find_node_by_path(node_path);
	if (!node) {
		return MCP_NOT_FOUND("Node '" + node_path + "'");
	}
	node->set_script(Variant());
	Dictionary res;
	res["node_path"] = root->get_path_to(node);
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

Dictionary JustAMCPScriptTools::_search_in_scripts(const Dictionary &p_params) {
	if (!p_params.has("pattern")) {
		return MCP_INVALID_PARAMS("Missing param: pattern");
	}
	String pattern = p_params["pattern"];
	String path = p_params.has("path") ? String(p_params["path"]) : "res://";
	Array scripts;
	_find_scripts(path, true, scripts);
	Array matches;
	for (int i = 0; i < scripts.size(); i++) {
		Dictionary info = scripts[i];
		String script_path = info["path"];
		Ref<FileAccess> file = FileAccess::open(script_path, FileAccess::READ);
		if (file.is_null()) {
			continue;
		}
		String content = file->get_as_text();
		file->close();
		Vector<String> lines = content.split("\n");
		for (int j = 0; j < lines.size(); j++) {
			if (lines[j].contains(pattern)) {
				Dictionary match;
				match["file"] = script_path;
				match["line"] = j + 1;
				match["text"] = lines[j].strip_edges();
				matches.push_back(match);
			}
		}
	}

	Dictionary res;
	res["pattern"] = pattern;
	res["matches"] = matches;
	res["count"] = matches.size();
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPScriptTools::_find_script_symbols(const Dictionary &p_params) {
	String path = p_params.has("path") ? String(p_params["path"]) : "";
	Array scripts;
	if (path.is_empty()) {
		_find_scripts("res://", true, scripts);
	} else {
		Dictionary info;
		info["path"] = path;
		scripts.push_back(info);
	}

	Array symbols;
	Ref<RegEx> regex;
	regex.instantiate();
	regex->compile("^\\s*(class_name|extends|signal|func|var|const|enum)\\s+([A-Za-z_][A-Za-z0-9_]*)?");
	for (int i = 0; i < scripts.size(); i++) {
		Dictionary script_info = scripts[i];
		String script_path = script_info.get("path", "");
		if (!FileAccess::exists(script_path)) {
			continue;
		}
		Ref<FileAccess> file = FileAccess::open(script_path, FileAccess::READ);
		if (file.is_null()) {
			continue;
		}
		Vector<String> lines = file->get_as_text().split("\n");
		file->close();
		for (int line_idx = 0; line_idx < lines.size(); line_idx++) {
			Ref<RegExMatch> match = regex->search(lines[line_idx]);
			if (match.is_valid()) {
				Dictionary symbol;
				symbol["file"] = script_path;
				symbol["line"] = line_idx + 1;
				symbol["kind"] = match->get_string(1);
				symbol["name"] = match->get_string(2);
				symbol["text"] = lines[line_idx].strip_edges();
				symbols.push_back(symbol);
			}
		}
	}

	Dictionary res;
	res["symbols"] = symbols;
	res["count"] = symbols.size();
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPScriptTools::_patch_script(const Dictionary &p_params) {
	if (!p_params.has("path")) {
		return MCP_INVALID_PARAMS("Missing param: path");
	}
	String path = p_params["path"];
	if (!FileAccess::exists(path)) {
		return MCP_NOT_FOUND("Script '" + path + "'");
	}

	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
	if (file.is_null()) {
		return MCP_INTERNAL("Cannot read script");
	}
	String content = file->get_as_text();
	file->close();

	String anchor = p_params.get("anchor", p_params.get("search", ""));
	String replacement = p_params.get("replacement", p_params.get("replace", ""));
	String insert_before = p_params.get("insert_before", "");
	String insert_after = p_params.get("insert_after", "");
	bool changed = false;

	if (!anchor.is_empty()) {
		if (content.contains(anchor)) {
			content = content.replace(anchor, replacement);
			changed = true;
		} else {
			return MCP_NOT_FOUND("Anchor");
		}
	} else if (!insert_before.is_empty()) {
		int pos = content.find(insert_before);
		if (pos < 0) {
			return MCP_NOT_FOUND("insert_before anchor");
		}
		content = content.substr(0, pos) + replacement + content.substr(pos);
		changed = true;
	} else if (!insert_after.is_empty()) {
		int pos = content.find(insert_after);
		if (pos < 0) {
			return MCP_NOT_FOUND("insert_after anchor");
		}
		pos += insert_after.length();
		content = content.substr(0, pos) + replacement + content.substr(pos);
		changed = true;
	} else {
		return MCP_INVALID_PARAMS("Provide anchor/search, insert_before, or insert_after.");
	}

	if (changed) {
		file = FileAccess::open(path, FileAccess::WRITE);
		if (file.is_null()) {
			return MCP_INTERNAL("Cannot write script");
		}
		file->store_string(content);
		file->close();
		_reload_script(path);
	}

	Dictionary res;
	res["path"] = path;
	res["patched"] = changed;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPScriptTools::_validate_script(const Dictionary &p_params) {
	if (!p_params.has("path")) {
		return MCP_INVALID_PARAMS("Missing param: path");
	}
	String path = p_params["path"];

	if (!FileAccess::exists(path)) {
		return MCP_NOT_FOUND("Script '" + path + "'");
	}

	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
	if (file.is_null()) {
		return MCP_INTERNAL("Cannot read script");
	}

	String source_code = file->get_as_text();
	file->close();

	// Create a new GDScript instance directly. Note: We use ClassDB::instantiate instead of GDScript::new()
	// because gdscript module header might not be universally available depending on SCons setup,
	// but ClassDB does not require header inclusion.
	Object *obj = ClassDB::instantiate("GDScript");
	if (!obj) {
		return MCP_INTERNAL("Godot Engine is not compiled with GDScript support");
	}
	Ref<Script> ref_script = Object::cast_to<Script>(obj);
	if (ref_script.is_null()) {
		if (obj) {
			memdelete(obj);
		}
		return MCP_INTERNAL("Godot Engine is not compiled with GDScript support or cast failed.");
	}

	ref_script->set_source_code(source_code);
	Error err = ref_script->reload();

	if (err == OK) {
		Dictionary res;
		res["path"] = path;
		res["valid"] = true;
		res["message"] = "Script compiles successfully";
		return MCP_SUCCESS(res);
	}

	Dictionary res;
	res["path"] = path;
	res["valid"] = false;
	res["error_code"] = err;
	res["error_string"] = String::num_int64(err); // simplified
	res["message"] = "Compilation failed.";
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPScriptTools::_get_script_metadata(const Dictionary &p_params) {
	if (!p_params.has("path")) {
		return MCP_INVALID_PARAMS("Missing param: path");
	}
	String path = p_params["path"];

	if (!ResourceLoader::exists(path)) {
		return MCP_NOT_FOUND("Script '" + path + "'");
	}

	Ref<Script> script_res = ResourceLoader::load(path);
	if (script_res.is_null()) {
		return MCP_INTERNAL("Failed to load script: " + path);
	}

	Dictionary meta;
	meta["path"] = path;
	meta["class_name"] = script_res->get_instance_base_type(); // fallback
	meta["is_tool"] = script_res->is_tool();

	// Methods
	Array methods;
	List<MethodInfo> m_list;
	script_res->get_script_method_list(&m_list);
	for (const MethodInfo &mi : m_list) {
		Dictionary m;
		m["name"] = mi.name;
		Array params;
		for (const PropertyInfo &pi : mi.arguments) {
			params.push_back(pi.name + ":" + Variant::get_type_name(pi.type));
		}
		m["parameters"] = params;
		m["return"] = Variant::get_type_name(mi.return_val.type);
		methods.push_back(m);
	}
	meta["methods"] = methods;

	// Properties
	Array properties;
	List<PropertyInfo> p_list;
	script_res->get_script_property_list(&p_list);
	for (const PropertyInfo &pi : p_list) {
		Dictionary prod;
		prod["name"] = pi.name;
		prod["type"] = Variant::get_type_name(pi.type);
		prod["usage"] = pi.usage;
		properties.push_back(prod);
	}
	meta["properties"] = properties;

	// Signals
	Array signals;
	List<MethodInfo> s_list;
	script_res->get_script_signal_list(&s_list);
	for (const MethodInfo &si : s_list) {
		Dictionary sig;
		sig["name"] = si.name;
		Array params;
		for (const PropertyInfo &pi : si.arguments) {
			params.push_back(pi.name + ":" + Variant::get_type_name(pi.type));
		}
		sig["parameters"] = params;
		signals.push_back(sig);
	}
	meta["signals"] = signals;

	return MCP_SUCCESS(meta);
}

Dictionary JustAMCPScriptTools::_get_script_references(const Dictionary &p_params) {
	if (!p_params.has("path")) {
		return MCP_INVALID_PARAMS("Missing param: path");
	}
	String path = p_params["path"];

	Array references;
	_find_references_recursive("res://", path, references);

	Dictionary res;
	res["script_path"] = path;
	res["references"] = references;
	res["count"] = references.size();
	return MCP_SUCCESS(res);
}

void JustAMCPScriptTools::_find_references_recursive(const String &p_path, const String &p_target_script, Array &r_references) {
	Ref<DirAccess> dir = DirAccess::open(p_path);
	if (dir.is_null()) {
		return;
	}

	dir->list_dir_begin();
	String file_name = dir->get_next();
	while (!file_name.is_empty()) {
		if (file_name.begins_with(".")) {
			file_name = dir->get_next();
			continue;
		}

		String full_path = p_path.path_join(file_name);
		if (dir->current_is_dir()) {
			_find_references_recursive(full_path, p_target_script, r_references);
		} else if (file_name.ends_with(".tscn") || file_name.ends_with(".tres")) {
			Ref<FileAccess> file = FileAccess::open(full_path, FileAccess::READ);
			if (file.is_valid()) {
				String content = file->get_as_text();
				if (content.contains(p_target_script)) {
					r_references.push_back(full_path);
				}
				file->close();
			}
		}
		file_name = dir->get_next();
	}
	dir->list_dir_end();
}
