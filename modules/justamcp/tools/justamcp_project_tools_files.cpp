/**************************************************************************/
/*  justamcp_project_tools_files.cpp                                      */
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

#include "../justamcp_editor_filesystem.h"
#include "../justamcp_mcp_tool_macros.h"
#include "../justamcp_read_limits.h"
#include "justamcp_agent_helpers.h"
#include "justamcp_project_tools.h"
#include "justamcp_scene_file_io.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#ifdef TOOLS_ENABLED
#include "editor/plugins/script_editor_plugin.h"
#endif

static Dictionary _sandbox_path(const String &p_raw, String &r_path) {
	String sandbox_error;
	if (!justamcp_canonical_sandbox_path(String(p_raw), r_path, sandbox_error)) {
		return MCP_INVALID_PARAMS(sandbox_error);
	}
	return Dictionary();
}

static void _reload_if_script(const String &p_path) {
	if (!p_path.get_extension().to_lower().ends_with("gd") && p_path.get_extension().to_lower() != "gd" && p_path.get_extension().to_lower() != "cs") {
		return;
	}
#ifdef TOOLS_ENABLED
	if (ScriptEditor::get_singleton()) {
		ScriptEditor::get_singleton()->reload_scripts(true);
	}
#endif
}

Dictionary JustAMCPProjectTools::read_directory(const Dictionary &p_args) {
	String path;
	Dictionary err = _sandbox_path(p_args.get("file_path", p_args.get("path", "")), path);
	if (!err.is_empty()) {
		return err;
	}
	Ref<DirAccess> dir = DirAccess::open(path);
	if (dir.is_null()) {
		return MCP_ERROR(-32000, "Directory not found: " + path);
	}
	const int offset = MAX(0, int(p_args.get("offset", 0)));
	const int limit = CLAMP(int(p_args.get("limit", 200)), 1, 500);
	dir->list_dir_begin();
	Array entries;
	int skipped = 0;
	String name = dir->get_next();
	while (!name.is_empty()) {
		if (name != "." && name != "..") {
			if (skipped < offset) {
				skipped++;
			} else if (entries.size() < limit) {
				Dictionary entry;
				entry["name"] = name;
				entry["path"] = path.path_join(name);
				entry["type"] = dir->current_is_dir() ? "directory" : "file";
				entries.push_back(entry);
			}
		}
		name = dir->get_next();
	}
	dir->list_dir_end();
	Dictionary ret;
	ret["ok"] = true;
	ret["path"] = path;
	ret["type"] = "directory";
	ret["entries"] = entries;
	ret["offset"] = offset;
	ret["limit"] = limit;
	return ret;
}

Dictionary JustAMCPProjectTools::read_file(const Dictionary &p_args) {
	String path;
	Dictionary err = _sandbox_path(p_args.get("file_path", p_args.get("path", "")), path);
	if (!err.is_empty()) {
		return err;
	}
	if (DirAccess::exists(path)) {
		return read_directory(p_args);
	}
	if (p_args.has("start_line") && int(p_args.get("start_line", 1)) < 1) {
		return MCP_INVALID_PARAMS("start_line must be >= 1.");
	}
	String text;
	int64_t size = 0;
	if (!justamcp_read_utf8_within_limit(path, JUSTAMCP_MAX_SYNC_READ_BYTES, text, size, err)) {
		return err;
	}
	PackedStringArray lines = text.split("\n", true);
	const int start = MAX(0, int(p_args.get("start_line", 1)) - 1);
	const int max_lines = CLAMP(int(p_args.get("max_lines", lines.size())), 1, 20000);
	Array numbered;
	for (int i = start; i < lines.size() && numbered.size() < max_lines; i++) {
		numbered.push_back(vformat("%d|%s", i + 1, lines[i]));
	}
	Dictionary ret;
	ret["ok"] = true;
	ret["path"] = path;
	ret["content"] = numbered;
	ret["line_count"] = lines.size();
	ret["size"] = size;
	return ret;
}

Dictionary JustAMCPProjectTools::create_file(const Dictionary &p_args) {
	String path;
	Dictionary err = _sandbox_path(p_args.get("file_path", p_args.get("path", "")), path);
	if (!err.is_empty()) {
		return err;
	}
	if (!p_args.has("content")) {
		return MCP_INVALID_PARAMS("content is required.");
	}
	if (FileAccess::exists(path) && !bool(p_args.get("overwrite", false))) {
		return MCP_ERROR(-32000, "File already exists: " + path);
	}
	const String parent = path.get_base_dir();
	Ref<DirAccess> dir = DirAccess::create_for_path(parent);
	if (dir.is_valid() && !dir->dir_exists(parent)) {
		dir->make_dir_recursive(parent);
	}
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
	if (file.is_null()) {
		return MCP_ERROR(-32000, "Failed to write: " + path);
	}
	file->store_string(String(p_args.get("content", "")));
	file->close();
	JustAMCPEditorFilesystem::refresh_path(path);
	_reload_if_script(path);
	Dictionary ret;
	ret["ok"] = true;
	ret["path"] = path;
	ret["message"] = "File created";
	return ret;
}

Dictionary JustAMCPProjectTools::edit_file(const Dictionary &p_args) {
	String path;
	Dictionary err = _sandbox_path(p_args.get("file_path", p_args.get("path", "")), path);
	if (!err.is_empty()) {
		return err;
	}
	const String search = p_args.get("search_text", p_args.get("search", ""));
	const String replace = p_args.get("replace_text", p_args.get("replace", ""));
	if (search.is_empty()) {
		return MCP_INVALID_PARAMS("search_text is required.");
	}
	String text;
	int64_t size = 0;
	if (!justamcp_read_utf8_within_limit(path, JUSTAMCP_MAX_SYNC_READ_BYTES, text, size, err)) {
		return err;
	}
	const int matches = text.count(search);
	if (matches != 1) {
		return MCP_ERROR(-32000, vformat("search_text must match exactly once (found %d).", matches));
	}
	text = text.replace(search, replace);
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
	if (file.is_null()) {
		return MCP_ERROR(-32000, "Failed to write: " + path);
	}
	file->store_string(text);
	file->close();
	JustAMCPEditorFilesystem::refresh_path(path);
	_reload_if_script(path);
	Dictionary ret;
	ret["ok"] = true;
	ret["path"] = path;
	ret["message"] = "File edited.";
	return ret;
}

Dictionary JustAMCPProjectTools::move_file(const Dictionary &p_args) {
	String from;
	String to;
	Dictionary err = _sandbox_path(p_args.get("from", p_args.get("file_path", "")), from);
	if (!err.is_empty()) {
		return err;
	}
	err = _sandbox_path(p_args.get("to", p_args.get("destination", "")), to);
	if (!err.is_empty()) {
		return err;
	}
	if (FileAccess::exists(to) && !bool(p_args.get("overwrite", false))) {
		return MCP_ERROR(-32000, "Destination exists. Pass overwrite=true to replace: " + to);
	}
	Ref<DirAccess> dir = DirAccess::create_for_path(from.get_base_dir());
	if (dir.is_null() || dir->rename(from, to) != OK) {
		return MCP_ERROR(-32000, "Failed to move file.");
	}
	JustAMCPEditorFilesystem::refresh_path(from);
	JustAMCPEditorFilesystem::refresh_path(to);
	Dictionary ret;
	ret["ok"] = true;
	ret["from"] = from;
	ret["to"] = to;
	ret["message"] = "File moved.";
	return ret;
}

Dictionary JustAMCPProjectTools::copy_file(const Dictionary &p_args) {
	String from;
	String to;
	Dictionary err = _sandbox_path(p_args.get("from", p_args.get("file_path", "")), from);
	if (!err.is_empty()) {
		return err;
	}
	err = _sandbox_path(p_args.get("to", p_args.get("destination", "")), to);
	if (!err.is_empty()) {
		return err;
	}
	if (FileAccess::exists(to) && !bool(p_args.get("overwrite", false))) {
		return MCP_ERROR(-32000, "Destination exists. Pass overwrite=true to replace: " + to);
	}
	const String parent = to.get_base_dir();
	Ref<DirAccess> parent_dir = DirAccess::create_for_path(parent);
	if (parent_dir.is_valid() && !parent_dir->dir_exists(parent)) {
		parent_dir->make_dir_recursive(parent);
	}
	Ref<DirAccess> dir = DirAccess::create_for_path(from.get_base_dir());
	if (dir.is_null() || dir->copy(from, to) != OK) {
		return MCP_ERROR(-32000, "Failed to copy file.");
	}
	JustAMCPEditorFilesystem::refresh_path(to);
	Dictionary ret;
	ret["ok"] = true;
	ret["from"] = from;
	ret["to"] = to;
	ret["message"] = "File copied.";
	return ret;
}

Dictionary JustAMCPProjectTools::delete_file(const Dictionary &p_args) {
	String path;
	Dictionary err = _sandbox_path(p_args.get("file_path", p_args.get("path", "")), path);
	if (!err.is_empty()) {
		return err;
	}
	Ref<DirAccess> dir = DirAccess::create_for_path(path.get_base_dir());
	if (dir.is_null() || dir->remove(path) != OK) {
		return MCP_ERROR(-32000, "Failed to delete: " + path);
	}
	JustAMCPEditorFilesystem::refresh_path(path);
	Dictionary ret;
	ret["ok"] = true;
	ret["path"] = path;
	ret["message"] = "File deleted.";
	return ret;
}
