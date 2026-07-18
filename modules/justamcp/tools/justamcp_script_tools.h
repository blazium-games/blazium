/**************************************************************************/
/*  justamcp_script_tools.h                                               */
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
#include "editor/plugins/editor_plugin.h"
#include "scene/main/node.h"

class JustAMCPScriptTools : public Object {
	GDCLASS(JustAMCPScriptTools, Object);

private:
	EditorPlugin *editor_plugin = nullptr;

	Node *_find_node_by_path(const String &p_path);

	Dictionary _list_scripts(const Dictionary &p_params);
	void _find_scripts(const String &p_path, bool p_recursive, Array &r_scripts, int p_max_results = 2000);

	Dictionary _read_script(const Dictionary &p_params);
	Dictionary _create_script(const Dictionary &p_params);
	Dictionary _edit_script(const Dictionary &p_params);
	Dictionary _delete_script(const Dictionary &p_params);
	Dictionary _validate_script(const Dictionary &p_params);
	Dictionary _attach_script(const Dictionary &p_params);
	Dictionary _detach_script(const Dictionary &p_params);
	Dictionary _get_open_scripts(const Dictionary &p_params);
	Dictionary _open_script_in_editor(const Dictionary &p_params);
	Dictionary _get_script_errors(const Dictionary &p_params);
	Dictionary _search_in_scripts(const Dictionary &p_params);
	Dictionary _find_script_symbols(const Dictionary &p_params);
	Dictionary _patch_script(const Dictionary &p_params);
	Dictionary _get_script_metadata(const Dictionary &p_params);
	Dictionary _get_script_references(const Dictionary &p_params);

	void _reload_script(const String &p_path);
	void _find_references_recursive(const String &p_path, const String &p_script_path, Array &r_references, int p_max_results = 200);

protected:
	static void _bind_methods();

public:
	void set_editor_plugin(EditorPlugin *p_plugin) { editor_plugin = p_plugin; }
	Dictionary execute_tool(const String &p_tool_name, const Dictionary &p_args);

	JustAMCPScriptTools();
	~JustAMCPScriptTools();
};
