/**************************************************************************/
/*  justamcp_project_tools.cpp                                            */
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

#include "justamcp_project_tools.h"

#include "../justamcp_editor_plugin.h"
#include "../justamcp_read_limits.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/io/resource_uid.h"

#include "modules/regex/regex.h"

void JustAMCPProjectTools::_bind_methods() {}

Dictionary JustAMCPProjectTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	String tool_name = p_tool_name;
	if (tool_name == "project_map_project") {
		tool_name = "map_project";
	} else if (tool_name == "project_map_scenes") {
		tool_name = "map_scenes";
	} else if (tool_name == "project_list_settings" || tool_name == "get_project_settings") {
		tool_name = "list_settings";
	} else if (tool_name == "project_update_settings" || tool_name == "update_project_settings") {
		tool_name = "update_settings";
	} else if (tool_name == "project_manage_autoloads") {
		tool_name = "manage_autoloads";
	} else if (tool_name == "project_get_collision_layers") {
		tool_name = "get_collision_layers";
	} else if (tool_name == "get_input_map") {
		tool_name = "project_get_input_actions";
	} else if (tool_name == "configure_input_map") {
		tool_name = "project_set_input_action";
	}

	if (tool_name == "map_project") {
		return map_project(p_args);
	}
	if (tool_name == "map_scenes") {
		return map_scenes(p_args);
	}
	if (tool_name == "list_settings") {
		return list_settings(p_args);
	}
	if (tool_name == "update_settings") {
		return update_settings(p_args);
	}
	if (tool_name == "manage_autoloads") {
		return manage_autoloads(p_args);
	}
	if (tool_name == "get_collision_layers") {
		return get_collision_layers(p_args);
	}
	if (tool_name == "project_get_input_actions") {
		return get_input_actions(p_args);
	}
	if (tool_name == "project_set_input_action") {
		return set_input_action(p_args);
	}
	if (tool_name == "project_remove_input_action") {
		return remove_input_action(p_args);
	}
	if (tool_name == "get_project_info") {
		return get_project_info(p_args);
	}
	if (tool_name == "set_project_setting") {
		return set_project_setting(p_args);
	}
	if (tool_name == "get_filesystem_tree") {
		return get_filesystem_tree(p_args);
	}
	if (tool_name == "search_files") {
		return search_files(p_args);
	}
	if (tool_name == "search_in_files") {
		return search_in_files(p_args);
	}
	if (tool_name == "uid_to_project_path") {
		return uid_to_project_path(p_args);
	}
	if (tool_name == "project_path_to_uid") {
		return project_path_to_uid(p_args);
	}
	if (tool_name == "add_autoload") {
		return add_autoload(p_args);
	}
	if (tool_name == "remove_autoload") {
		return remove_autoload(p_args);
	}

	return Dictionary();
}

Dictionary JustAMCPProjectTools::map_project(const Dictionary &p_args) {
	String root_path = p_args.get("root", "res://");
	bool include_addons = p_args.get("include_addons", false);
	int lod = p_args.get("lod", 1);
	int max_results = CLAMP(int(p_args.get("max_results", 2000)), 1, 10000);

	if (!root_path.begins_with("res://")) {
		root_path = "res://" + root_path;
	}

	Array script_paths;
	_collect_scripts(root_path, script_paths, include_addons, max_results);
	const bool truncated = script_paths.size() >= max_results;

	Array nodes;
	Dictionary class_map;

	for (int i = 0; i < script_paths.size(); i++) {
		String path = script_paths[i];
		Dictionary info = _parse_script(path, lod);
		nodes.push_back(info);
		if (info.has("class_name") && !String(info["class_name"]).is_empty()) {
			class_map[info["class_name"]] = path;
		}
	}

	Dictionary result;
	result["ok"] = true;
	result["nodes"] = nodes;
	result["total_scripts"] = nodes.size();
	result["truncated"] = truncated;
	return result;
}

void JustAMCPProjectTools::_collect_scripts(const String &p_path, Array &r_results, bool p_include_addons, int p_max_results) {
	if (p_max_results > 0 && r_results.size() >= p_max_results) {
		return;
	}
	Ref<DirAccess> dir = DirAccess::open(p_path);
	if (dir.is_null()) {
		return;
	}

	dir->list_dir_begin();
	String name = dir->get_next();
	while (!name.is_empty()) {
		if (p_max_results > 0 && r_results.size() >= p_max_results) {
			break;
		}
		if (name.begins_with(".")) {
			name = dir->get_next();
			continue;
		}
		String full_path = p_path.path_join(name);
		if (dir->current_is_dir()) {
			if (name == "addons" && !p_include_addons) {
				name = dir->get_next();
				continue;
			}
			_collect_scripts(full_path, r_results, p_include_addons, p_max_results);
		} else if (name.ends_with(".gd")) {
			r_results.push_back(full_path);
		}
		name = dir->get_next();
	}
	dir->list_dir_end();
}

Dictionary JustAMCPProjectTools::_parse_script(const String &p_path, int p_lod) {
	Dictionary info;
	info["path"] = p_path;
	if (p_lod == 0) {
		return info;
	}

	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);
	if (f.is_null()) {
		return info;
	}

	int64_t file_size = 0;
	if (!justamcp_file_within_read_limit(p_path, JUSTAMCP_MAX_SYNC_READ_BYTES, file_size)) {
		return info;
	}

	String content = f->get_as_text();
	f->close();

	if (p_lod >= 1) {
		Ref<RegEx> re_class;
		re_class.instantiate();
		re_class->compile("^class_name\\s+(\\w+)");
		Ref<RegEx> re_extends;
		re_extends.instantiate();
		re_extends->compile("^extends\\s+(\\w+)");

		Ref<RegExMatch> m_class = re_class->search(content);
		if (m_class.is_valid()) {
			info["class_name"] = m_class->get_string(1);
		}

		Ref<RegExMatch> m_ext = re_extends->search(content);
		if (m_ext.is_valid()) {
			info["extends"] = m_ext->get_string(1);
		}
	}

	if (p_lod >= 2) {
		Array methods;
		Ref<RegEx> re_func;
		re_func.instantiate();
		re_func->compile("^func\\s+(\\w+)");
		TypedArray<RegExMatch> matches = re_func->search_all(content);
		for (int i = 0; i < matches.size(); i++) {
			Ref<RegExMatch> m = matches[i];
			methods.push_back(m->get_string(1));
		}
		info["methods"] = methods;
	}

	return info;
}

Dictionary JustAMCPProjectTools::map_scenes(const Dictionary &p_args) {
	String root_path = p_args.get("root", "res://");
	bool include_addons = p_args.get("include_addons", false);
	int max_results = CLAMP(int(p_args.get("max_results", 2000)), 1, 10000);

	if (!root_path.begins_with("res://")) {
		root_path = "res://" + root_path;
	}

	Array scene_paths;
	_collect_scenes(root_path, scene_paths, include_addons, max_results);
	const bool truncated = scene_paths.size() >= max_results;

	Array scenes;
	for (int i = 0; i < scene_paths.size(); i++) {
		scenes.push_back(_parse_scene(scene_paths[i]));
	}

	Dictionary result;
	result["ok"] = true;
	result["scenes"] = scenes;
	result["total_scenes"] = scenes.size();
	result["truncated"] = truncated;
	Array resource_links;
	Dictionary root_link;
	root_link["uri"] = root_path;
	root_link["name"] = "project-scenes";
	resource_links.push_back(root_link);
	result["resourceLinks"] = resource_links;
	return result;
}

void JustAMCPProjectTools::_collect_scenes(const String &p_path, Array &r_results, bool p_include_addons, int p_max_results) {
	if (p_max_results > 0 && r_results.size() >= p_max_results) {
		return;
	}
	Ref<DirAccess> dir = DirAccess::open(p_path);
	if (dir.is_null()) {
		return;
	}

	dir->list_dir_begin();
	String name = dir->get_next();
	while (!name.is_empty()) {
		if (p_max_results > 0 && r_results.size() >= p_max_results) {
			break;
		}
		if (name.begins_with(".")) {
			name = dir->get_next();
			continue;
		}
		String full_path = p_path.path_join(name);
		if (dir->current_is_dir()) {
			if (name == "addons" && !p_include_addons) {
				name = dir->get_next();
				continue;
			}
			_collect_scenes(full_path, r_results, p_include_addons, p_max_results);
		} else if (name.ends_with(".tscn") || name.ends_with(".scn")) {
			r_results.push_back(full_path);
		}
		name = dir->get_next();
	}
	dir->list_dir_end();
}

Dictionary JustAMCPProjectTools::_parse_scene(const String &p_path) {
	Dictionary info;
	info["path"] = p_path;
	info["filename"] = p_path.get_file();

	return info;
}
