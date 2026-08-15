/**************************************************************************/
/*  justamcp_batch_tools.h                                                */
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

#include "core/object/object.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/main/node.h"

class JustAMCPBatchTools : public Object {
	GDCLASS(JustAMCPBatchTools, Object);

private:
	EditorPlugin *editor_plugin = nullptr;

	Node *_find_node_by_path(const String &p_path);

	Dictionary _find_nodes_by_type(const Dictionary &p_params);
	void _search_by_type(Node *p_node, const String &p_type_name, bool p_recursive, Array &r_matches);

	Dictionary _find_signal_connections(const Dictionary &p_params);
	void _collect_signals(Node *p_node, Node *p_root, const String &p_signal_filter, const String &p_node_filter, Array &r_connections);

	Dictionary _batch_set_property(const Dictionary &p_params);
	void _batch_set_recursive(Node *p_node, Node *p_root, const String &p_type_name, const String &p_property, const Variant &p_value, Array &r_affected);
	Dictionary _batch_add_nodes(const Dictionary &p_params);

	Dictionary _find_node_references(const Dictionary &p_params);
	void _search_files_for_pattern(const String &p_path, const String &p_pattern, Array &r_matches, int p_max_results);

	Dictionary _cross_scene_set_property(const Dictionary &p_params);
	void _collect_scene_files(const String &p_path, Array &r_files, bool p_exclude_addons, int p_max_results);
	void _cross_scene_set_recursive(Node *p_node, Node *p_root, const String &p_type_name, const String &p_property, const Variant &p_value, Array &r_affected);

	Dictionary _get_scene_dependencies(const Dictionary &p_params);

protected:
	static void _bind_methods();

public:
	void set_editor_plugin(EditorPlugin *p_plugin) { editor_plugin = p_plugin; }
	Dictionary execute_tool(const String &p_tool_name, const Dictionary &p_args);

	JustAMCPBatchTools();
	~JustAMCPBatchTools();
};
