/**************************************************************************/
/*  justamcp_resource_tools.h                                             */
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

#ifdef TOOLS_ENABLED

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "scene/main/node.h"
#include "scene/resources/theme.h"

class JustAMCPEditorPlugin;

class JustAMCPResourceTools : public Object {
	GDCLASS(JustAMCPResourceTools, Object);

private:
	JustAMCPEditorPlugin *editor_plugin = nullptr;

protected:
	static void _bind_methods();

public:
	void set_editor_plugin(JustAMCPEditorPlugin *p_plugin) { editor_plugin = p_plugin; }

	Dictionary create_resource(const Dictionary &p_args);
	Dictionary modify_resource(const Dictionary &p_args);
	Dictionary create_material(const Dictionary &p_args);
	Dictionary create_shader(const Dictionary &p_args);
	Dictionary create_tileset(const Dictionary &p_args);
	Dictionary set_tilemap_cells(const Dictionary &p_args);
	Dictionary get_resource_info(const Dictionary &p_args);
	Dictionary read_resource_file(const Dictionary &p_args);
	Dictionary edit_resource_file(const Dictionary &p_args);
	Dictionary get_resource_preview(const Dictionary &p_args);
	Dictionary list_resource_files(const Dictionary &p_args);
	Dictionary save_resource_as(const Dictionary &p_args);
	Dictionary get_resource_dependencies(const Dictionary &p_args);
	Dictionary import_asset_copy(const Dictionary &p_args);
	Dictionary manage_resource_autoloads(const Dictionary &p_args);
	Dictionary set_theme_color(const Dictionary &p_args);
	Dictionary set_theme_font_size(const Dictionary &p_args);
	Dictionary apply_theme_shader(const Dictionary &p_args);
	Dictionary resource_import_asset(const Dictionary &p_args);
	Dictionary execute_tool(const String &p_tool_name, const Dictionary &p_args);

private:
	String _ensure_res_path(const String &p_path);
	void _refresh_filesystem(const String &p_path = String());
	Variant _parse_value(const Variant &p_value);
	void _set_resource_properties(Ref<Resource> p_resource, const Variant &p_properties);
	Dictionary _parse_properties_dict(const Variant &p_raw);
	Ref<Theme> _load_theme(const String &p_theme_path);
	Error _save_scene_root(Node *p_root, const String &p_scene_path);
	String _get_theme_shader_code(const String &p_theme, const String &p_effect);
	void _list_resources_recursive(const String &p_path, const String &p_type_filter, Array &r_results, int p_max_results = 500);

public:
	JustAMCPResourceTools();
	~JustAMCPResourceTools();
};

#endif
