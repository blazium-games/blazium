/**************************************************************************/
/*  justamcp_project_tools.h                                              */
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

#pragma once

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "scene/main/node.h"

class JustAMCPEditorPlugin;

class JustAMCPProjectTools : public Object {
	GDCLASS(JustAMCPProjectTools, Object);

	JustAMCPEditorPlugin *editor_plugin = nullptr;

protected:
	static void _bind_methods();

public:
	void set_editor_plugin(JustAMCPEditorPlugin *p_plugin) { editor_plugin = p_plugin; }
	Dictionary execute_tool(const String &p_tool_name, const Dictionary &p_args);

	Dictionary map_project(const Dictionary &p_args);
	Dictionary map_scenes(const Dictionary &p_args);
	Dictionary list_settings(const Dictionary &p_args);
	Dictionary update_settings(const Dictionary &p_args);
	Dictionary manage_autoloads(const Dictionary &p_args);
	Dictionary get_collision_layers(const Dictionary &p_args);
	Dictionary get_input_actions(const Dictionary &p_args);
	Dictionary set_input_action(const Dictionary &p_args);
	Dictionary remove_input_action(const Dictionary &p_args);
	Dictionary get_project_info(const Dictionary &p_args);
	Dictionary set_project_setting(const Dictionary &p_args);
	Dictionary get_filesystem_tree(const Dictionary &p_args);
	Dictionary search_files(const Dictionary &p_args);
	Dictionary search_in_files(const Dictionary &p_args);
	Dictionary uid_to_project_path(const Dictionary &p_args);
	Dictionary project_path_to_uid(const Dictionary &p_args);
	Dictionary add_autoload(const Dictionary &p_args);
	Dictionary remove_autoload(const Dictionary &p_args);

private:
	void _collect_matching_files(const String &p_path, const String &p_query, const String &p_file_type, Array &r_results, int p_max_results);
	void _search_file_contents(const String &p_path, const String &p_query, bool p_regex, const String &p_file_type, Array &r_results, int p_max_results);
	void _collect_scripts(const String &p_path, Array &r_results, bool p_include_addons, int p_max_results = 2000);
	Dictionary _parse_script(const String &p_path, int p_lod);
	void _collect_scenes(const String &p_path, Array &r_results, bool p_include_addons, int p_max_results = 2000);
	Dictionary _parse_scene(const String &p_path);
	String _type_to_string(int p_type_id);
	Variant _serialize_value(const Variant &p_value);
	Dictionary _build_filesystem_tree(const String &p_path, const String &p_filter, int p_max_depth, int p_depth);

public:
	JustAMCPProjectTools();
	~JustAMCPProjectTools();
};
