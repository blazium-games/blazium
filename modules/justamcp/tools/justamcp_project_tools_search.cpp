/**************************************************************************/
/*  justamcp_project_tools_search.cpp                                     */
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

#include "../justamcp_editor_plugin.h"
#include "../justamcp_read_limits.h"
#include "justamcp_project_tools.h"

#include "core/config/project_settings.h"
#include "core/input/input_event.h"
#include "core/input/input_map.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/io/resource_uid.h"
#include "modules/regex/regex.h"

Dictionary JustAMCPProjectTools::_build_filesystem_tree(const String &p_path, const String &p_filter, int p_max_depth, int p_depth) {
	Dictionary node;
	node["path"] = p_path;
	node["name"] = p_path == "res://" ? "res://" : p_path.get_file();

	Ref<DirAccess> dir = DirAccess::open(p_path);
	if (dir.is_null()) {
		node["type"] = "missing";
		return node;
	}

	if (p_max_depth >= 0 && p_depth >= p_max_depth) {
		node["type"] = "dir";
		return node;
	}

	node["type"] = "dir";
	Array children;
	dir->list_dir_begin();
	String name = dir->get_next();
	while (!name.is_empty()) {
		if (name.begins_with(".")) {
			name = dir->get_next();
			continue;
		}
		String full_path = p_path.path_join(name);
		if (dir->current_is_dir()) {
			children.push_back(_build_filesystem_tree(full_path, p_filter, p_max_depth, p_depth + 1));
		} else {
			if (!p_filter.is_empty() && !name.ends_with(p_filter) && !name.contains(p_filter)) {
				name = dir->get_next();
				continue;
			}
			Dictionary file_node;
			file_node["path"] = full_path;
			file_node["name"] = name;
			file_node["type"] = "file";
			children.push_back(file_node);
		}
		name = dir->get_next();
	}
	dir->list_dir_end();
	node["children"] = children;
	return node;
}

Dictionary JustAMCPProjectTools::get_filesystem_tree(const Dictionary &p_args) {
	String path = p_args.get("path", "res://");
	String filter = p_args.get("filter", "");
	int max_depth = p_args.has("max_depth") ? int(p_args.get("max_depth", -1)) : 8;

	if (!path.begins_with("res://")) {
		path = "res://" + path;
	}

	Dictionary result;
	result["ok"] = true;
	result["root"] = _build_filesystem_tree(path, filter, max_depth, 0);
	return result;
}

String JustAMCPProjectTools::_type_to_string(int p_type_id) {
	return Variant::get_type_name(Variant::Type(p_type_id));
}

Variant JustAMCPProjectTools::_serialize_value(const Variant &p_value) {
	return p_value;
}

JustAMCPProjectTools::JustAMCPProjectTools() {}
JustAMCPProjectTools::~JustAMCPProjectTools() {}

void JustAMCPProjectTools::_collect_matching_files(const String &p_path, const String &p_query, const String &p_file_type, Array &r_results, int p_max_results) {
	if (r_results.size() >= p_max_results) {
		return;
	}
	Ref<DirAccess> dir = DirAccess::open(p_path);
	if (dir.is_null()) {
		return;
	}
	dir->list_dir_begin();
	String name = dir->get_next();
	while (!name.is_empty()) {
		if (name.begins_with(".")) {
			name = dir->get_next();
			continue;
		}
		const String full_path = p_path.path_join(name);
		if (dir->current_is_dir()) {
			_collect_matching_files(full_path, p_query, p_file_type, r_results, p_max_results);
		} else if (r_results.size() < p_max_results) {
			bool type_match = p_file_type.is_empty() || name.ends_with(p_file_type) || name.contains(p_file_type);
			bool query_match = p_query.is_empty() || name.containsn(p_query) || full_path.containsn(p_query);
			if (type_match && query_match) {
				Dictionary item;
				item["path"] = full_path;
				item["name"] = name;
				r_results.push_back(item);
			}
		}
		name = dir->get_next();
	}
	dir->list_dir_end();
}

void JustAMCPProjectTools::_search_file_contents(const String &p_path, const String &p_query, bool p_regex, const String &p_file_type, Array &r_results, int p_max_results) {
	if (r_results.size() >= p_max_results) {
		return;
	}
	Ref<DirAccess> dir = DirAccess::open(p_path);
	if (dir.is_null()) {
		return;
	}
	Ref<RegEx> regex;
	if (p_regex) {
		regex.instantiate();
		if (regex->compile(p_query) != OK) {
			return;
		}
	}
	dir->list_dir_begin();
	String name = dir->get_next();
	while (!name.is_empty()) {
		if (name.begins_with(".")) {
			name = dir->get_next();
			continue;
		}
		const String full_path = p_path.path_join(name);
		if (dir->current_is_dir()) {
			_search_file_contents(full_path, p_query, p_regex, p_file_type, r_results, p_max_results);
		} else if (r_results.size() < p_max_results) {
			if (!p_file_type.is_empty() && !name.ends_with(p_file_type) && !name.contains(p_file_type)) {
				name = dir->get_next();
				continue;
			}
			Ref<FileAccess> file = FileAccess::open(full_path, FileAccess::READ);
			if (file.is_valid()) {
				int64_t file_size = 0;
				if (!justamcp_file_within_read_limit(full_path, JUSTAMCP_MAX_SYNC_READ_BYTES, file_size)) {
					name = dir->get_next();
					continue;
				}
				const String text = file->get_as_text();
				bool matched = false;
				if (p_regex && regex.is_valid()) {
					matched = regex->search(text).is_valid();
				} else if (!p_query.is_empty()) {
					matched = text.containsn(p_query);
				}
				if (matched) {
					Dictionary item;
					item["path"] = full_path;
					item["name"] = name;
					r_results.push_back(item);
				}
			}
		}
		name = dir->get_next();
	}
	dir->list_dir_end();
}

Dictionary JustAMCPProjectTools::search_files(const Dictionary &p_args) {
	String query = String(p_args.get("query", "")).strip_edges();
	String path = p_args.get("path", "res://");
	String file_type = p_args.get("file_type", "");
	int max_results = p_args.has("max_results") ? MAX(int(p_args.get("max_results", 50)), 1) : 50;
	if (!path.begins_with("res://")) {
		path = "res://" + path;
	}
	Array matches;
	_collect_matching_files(path, query, file_type, matches, max_results);
	Dictionary result;
	result["ok"] = true;
	result["query"] = query;
	result["path"] = path;
	result["matches"] = matches;
	result["count"] = matches.size();
	return result;
}

Dictionary JustAMCPProjectTools::search_in_files(const Dictionary &p_args) {
	String query = String(p_args.get("query", "")).strip_edges();
	if (query.is_empty()) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "query is required.";
		return err;
	}
	String path = p_args.get("path", "res://");
	String file_type = p_args.get("file_type", "");
	bool use_regex = p_args.get("regex", false);
	int max_results = p_args.has("max_results") ? MAX(int(p_args.get("max_results", 50)), 1) : 50;
	if (!path.begins_with("res://")) {
		path = "res://" + path;
	}
	Array matches;
	_search_file_contents(path, query, use_regex, file_type, matches, max_results);
	Dictionary result;
	result["ok"] = true;
	result["query"] = query;
	result["path"] = path;
	result["regex"] = use_regex;
	result["matches"] = matches;
	result["count"] = matches.size();
	return result;
}

Dictionary JustAMCPProjectTools::uid_to_project_path(const Dictionary &p_args) {
	String uid_text = String(p_args.get("uid", "")).strip_edges();
	if (uid_text.is_empty()) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "uid is required.";
		return err;
	}
	ResourceUID *uid_api = ResourceUID::get_singleton();
	if (!uid_api) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "ResourceUID singleton unavailable.";
		return err;
	}
	const ResourceUID::ID uid = uid_api->text_to_id(uid_text);
	if (uid == ResourceUID::INVALID_ID || !uid_api->has_id(uid)) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "Unknown UID: " + uid_text;
		return err;
	}
	Dictionary result;
	result["ok"] = true;
	result["uid"] = uid_text;
	result["path"] = uid_api->get_id_path(uid);
	return result;
}

Dictionary JustAMCPProjectTools::project_path_to_uid(const Dictionary &p_args) {
	String path = String(p_args.get("path", "")).strip_edges();
	if (path.is_empty()) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "path is required.";
		return err;
	}
	if (!path.begins_with("res://")) {
		path = "res://" + path;
	}
	ResourceUID *uid_api = ResourceUID::get_singleton();
	if (!uid_api) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "ResourceUID singleton unavailable.";
		return err;
	}
	ResourceUID::ID uid = ResourceLoader::get_resource_uid(path);
	if (uid == ResourceUID::INVALID_ID) {
		uid = ResourceSaver::get_resource_id_for_path(path, true);
	}
	if (uid == ResourceUID::INVALID_ID) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "No UID registered for path: " + path;
		return err;
	}
	Dictionary result;
	result["ok"] = true;
	result["path"] = path;
	result["uid"] = uid_api->id_to_text(uid);
	return result;
}

Dictionary JustAMCPProjectTools::add_autoload(const Dictionary &p_args) {
	Dictionary args = p_args;
	args["operation"] = "add";
	return manage_autoloads(args);
}

Dictionary JustAMCPProjectTools::remove_autoload(const Dictionary &p_args) {
	Dictionary args = p_args;
	args["operation"] = "remove";
	return manage_autoloads(args);
}
