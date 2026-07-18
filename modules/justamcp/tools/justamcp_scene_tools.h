/**************************************************************************/
/*  justamcp_scene_tools.h                                                */
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

class JustAMCPEditorPlugin;

class JustAMCPSceneTools : public Object {
	GDCLASS(JustAMCPSceneTools, Object);

private:
	JustAMCPEditorPlugin *editor_plugin = nullptr;

protected:
	static void _bind_methods();

public:
	void set_editor_plugin(JustAMCPEditorPlugin *p_plugin) { editor_plugin = p_plugin; }

	Dictionary create_scene(const Dictionary &p_args);
	Dictionary create_inherited_scene(const Dictionary &p_args);
	Dictionary list_scene_nodes(const Dictionary &p_args);
	Dictionary get_scene_file_content(const Dictionary &p_args);
	Dictionary delete_scene_file(const Dictionary &p_args);
	Dictionary get_scene_exports(const Dictionary &p_args);
	Dictionary get_current_scene(const Dictionary &p_args);
	Dictionary list_open_scenes(const Dictionary &p_args);
	Dictionary set_current_scene(const Dictionary &p_args);
	Dictionary reload_scene(const Dictionary &p_args);
	Dictionary duplicate_scene_file(const Dictionary &p_args);
	Dictionary close_scene(const Dictionary &p_args);
	Dictionary add_node(const Dictionary &p_args);
	Dictionary instance_scene(const Dictionary &p_args);
	Dictionary delete_node(const Dictionary &p_args);
	Dictionary duplicate_node(const Dictionary &p_args);
	Dictionary reparent_node(const Dictionary &p_args);
	Dictionary set_node_properties(const Dictionary &p_args);
	Dictionary get_node_properties(const Dictionary &p_args);
	Dictionary load_sprite(const Dictionary &p_args);
	Dictionary save_scene(const Dictionary &p_args);
	Dictionary connect_signal(const Dictionary &p_args);
	Dictionary disconnect_signal(const Dictionary &p_args);
	Dictionary list_connections(const Dictionary &p_args);
	Dictionary list_node_signals(const Dictionary &p_args);
	Dictionary has_signal_connection(const Dictionary &p_args);

	Dictionary create_area_2d(const Dictionary &p_args);
	Dictionary create_line_2d(const Dictionary &p_args);
	Dictionary create_polygon_2d(const Dictionary &p_args);
	Dictionary create_csg_shape(const Dictionary &p_args);

	Dictionary setup_camera_2d(const Dictionary &p_args);
	Dictionary setup_parallax_2d(const Dictionary &p_args);
	Dictionary create_multimesh(const Dictionary &p_args);
	Dictionary setup_skeleton(const Dictionary &p_args);
	Dictionary setup_occlusion(const Dictionary &p_args);

	Dictionary execute_tool(const String &p_tool_name, const Dictionary &p_args);

private:
	void _refresh_and_reload(const String &p_scene_path);
	void _refresh_filesystem(const String &p_changed_path = String());
	void _deferred_refresh_filesystem();
	void _reload_scene_in_editor(const String &p_scene_path);
	String _ensure_res_path(const String &p_path);
	String _to_scene_res_path(const String &p_project_path, const String &p_scene_path);

	Array _load_scene(const String &p_scene_path);
	Dictionary _save_scene(Node *p_scene_root, const String &p_scene_path);
	Dictionary _pack_and_save_scene(Node *p_scene_root, const String &p_scene_path, bool p_free_root);
	Node *_find_node(Node *p_root, const String &p_path);

	Variant _parse_value(const Variant &p_value);
	Variant _serialize_value(const Variant &p_value);
	void _set_node_properties(Node *p_node, const Dictionary &p_properties);
	Dictionary _parse_properties_arg(const Variant &p_raw_properties);
	void _ensure_parent_dir_for_scene(const String &p_scene_path);
	void _set_owner_recursive(Node *p_node, Node *p_scene_owner);
	Dictionary _build_node_tree(Node *p_node, bool p_include_properties, int p_depth, int p_current_depth, const String &p_node_path);
	void _collect_nodes_recursive(Node *p_node, const String &p_path, Array &r_out_nodes, int p_max_nodes = 4096);

public:
	JustAMCPSceneTools();
	~JustAMCPSceneTools();
};

#endif
