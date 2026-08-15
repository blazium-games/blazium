/**************************************************************************/
/*  justamcp_node_tools.h                                                 */
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

class JustAMCPNodeTools : public Object {
	GDCLASS(JustAMCPNodeTools, Object);

private:
	EditorPlugin *editor_plugin = nullptr;

	Node *_find_node_by_path(const String &p_path);
	void _set_owner_recursive(Node *p_node, Node *p_owner);

	Dictionary _add_node(const Dictionary &p_params);
	Dictionary _delete_node(const Dictionary &p_params);
	Dictionary _duplicate_node(const Dictionary &p_params);
	Dictionary _move_node(const Dictionary &p_params);
	Dictionary _update_property(const Dictionary &p_params);
	Dictionary _get_node_properties(const Dictionary &p_params);
	Dictionary _add_resource(const Dictionary &p_params);
	Dictionary _set_anchor_preset(const Dictionary &p_params);
	Dictionary _rename_node(const Dictionary &p_params);
	Dictionary _connect_signal(const Dictionary &p_params);
	Dictionary _disconnect_signal(const Dictionary &p_params);
	Dictionary _get_node_groups(const Dictionary &p_params);
	Dictionary _set_node_groups(const Dictionary &p_params);
	Dictionary _find_nodes_in_group(const Dictionary &p_params);

	void _find_in_group_recursive(Node *p_node, Node *p_root, const String &p_group_name, Array &r_matches);

protected:
	static void _bind_methods();

public:
	void set_editor_plugin(EditorPlugin *p_plugin) { editor_plugin = p_plugin; }
	Dictionary execute_tool(const String &p_tool_name, const Dictionary &p_args);

	JustAMCPNodeTools();
	~JustAMCPNodeTools();
};
