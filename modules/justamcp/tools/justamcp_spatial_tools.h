/**************************************************************************/
/*  justamcp_spatial_tools.h                                              */
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
#include "scene/2d/node_2d.h"
#include "scene/3d/node_3d.h"
#include "scene/main/node.h"

class JustAMCPEditorPlugin;

class JustAMCPSpatialTools : public Object {
	GDCLASS(JustAMCPSpatialTools, Object);

	JustAMCPEditorPlugin *editor_plugin = nullptr;

	Node *_get_scene_root();
	Node *_get_node(const String &p_path);

	void _collect_spatial_nodes(Node *p_node, Array &p_list_2d, Array &p_list_3d, bool p_inc_2d, bool p_inc_3d);
	void _collect_node3d(Node *p_node, Vector<Node3D *> &p_list);

protected:
	static void _bind_methods();

public:
	void set_editor_plugin(JustAMCPEditorPlugin *p_plugin);

	Dictionary spatial_analyze_layout(const Dictionary &p_args);
	Dictionary spatial_suggest_placement(const Dictionary &p_args);
	Dictionary spatial_detect_overlaps(const Dictionary &p_args);
	Dictionary spatial_measure_distance(const Dictionary &p_args);
	Dictionary spatial_bake_navigation(const Dictionary &p_args);
	Dictionary navigation_set_layers(const Dictionary &p_args);
	Dictionary navigation_get_info(const Dictionary &p_args);

	Dictionary execute_tool(const String &p_tool_name, const Dictionary &p_args);

	JustAMCPSpatialTools() {}
	~JustAMCPSpatialTools() {}
};

#endif
