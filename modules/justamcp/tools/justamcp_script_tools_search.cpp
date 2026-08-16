/**************************************************************************/
/*  justamcp_script_tools_search.cpp                                      */
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

#include "../justamcp_mcp_tool_macros.h"
#include "../justamcp_read_limits.h"
#include "justamcp_script_tools.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h" // IWYU pragma: keep
#include "core/object/class_db.h"
#include "core/object/script_language.h"
#include "scene/resources/packed_scene.h" // IWYU pragma: keep

#include "modules/regex/regex.h"

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
		int64_t file_size = 0;
		if (!justamcp_file_within_read_limit(script_path, JUSTAMCP_MAX_SYNC_READ_BYTES, file_size)) {
			continue;
		}
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
		int64_t file_size = 0;
		if (!justamcp_file_within_read_limit(script_path, JUSTAMCP_MAX_SYNC_READ_BYTES, file_size)) {
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

	if (!(path.ends_with(".gd") || path.ends_with(".cs") || path.ends_with(".luau"))) {
		return MCP_INVALID_PARAMS("validate_script expects a script path (.gd/.cs/.luau), got: " + path);
	}

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

	String source_code = file->get_as_text();
	file->close();

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
	res["error_string"] = String::num_int64(err);
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
	meta["class_name"] = script_res->get_instance_base_type();
	meta["is_tool"] = script_res->is_tool();

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
	const int max_results = CLAMP(int(p_params.get("max_results", 200)), 1, 2000);
	_find_references_recursive("res://", path, references, max_results);

	Dictionary res;
	res["script_path"] = path;
	res["references"] = references;
	res["count"] = references.size();
	return MCP_SUCCESS(res);
}

void JustAMCPScriptTools::_find_references_recursive(const String &p_path, const String &p_target_script, Array &r_references, int p_max_results) {
	if (r_references.size() >= p_max_results) {
		return;
	}
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
			_find_references_recursive(full_path, p_target_script, r_references, p_max_results);
		} else if (file_name.ends_with(".tscn") || file_name.ends_with(".tres")) {
			int64_t file_size = 0;
			if (!justamcp_file_within_read_limit(full_path, JUSTAMCP_MAX_SYNC_READ_BYTES, file_size)) {
				file_name = dir->get_next();
				continue;
			}
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
