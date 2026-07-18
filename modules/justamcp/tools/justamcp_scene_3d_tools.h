/**************************************************************************/
/*  justamcp_scene_3d_tools.h                                             */
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

class JustAMCPScene3DTools : public Object {
	GDCLASS(JustAMCPScene3DTools, Object);

private:
	JustAMCPEditorPlugin *editor_plugin = nullptr;

	Node *_find_node_by_path(const String &p_path);
	void _add_child_with_undo(Node *p_node, Node *p_parent, Node *p_root, const String &p_action_name);

	Color _parse_color(const Variant &p_val, const Color &p_default);
	Vector3 _parse_vector3(const Variant &p_val, const Vector3 &p_default);
	float _optional_float(const Dictionary &p_params, const String &p_key, float p_default);

public:
	void set_editor_plugin(JustAMCPEditorPlugin *p_plugin) { editor_plugin = p_plugin; }

	Dictionary execute_tool(const String &p_tool_name, const Dictionary &p_args);

	Dictionary add_mesh_instance(const Dictionary &p_params);
	Dictionary setup_lighting(const Dictionary &p_params);
	Dictionary set_material_3d(const Dictionary &p_params);
	Dictionary setup_environment(const Dictionary &p_params);
	Dictionary setup_camera_3d(const Dictionary &p_params);
	Dictionary add_gridmap(const Dictionary &p_params);

	JustAMCPScene3DTools();
	~JustAMCPScene3DTools();
};

#endif
