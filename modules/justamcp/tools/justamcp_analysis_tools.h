/**************************************************************************/
/*  justamcp_analysis_tools.h                                             */
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
class JustAMCPToolExecutor;

class JustAMCPAnalysisTools : public Object {
	GDCLASS(JustAMCPAnalysisTools, Object);

private:
	JustAMCPEditorPlugin *editor_plugin = nullptr;
	JustAMCPToolExecutor *owner = nullptr;

	void _collect_files_by_ext(const String &p_path, const Vector<String> &p_extensions, Array &r_out, bool p_include_addons, int p_max_results = 2000);
	String _read_file_text(const String &p_file_path);
	Dictionary _read_file_text_checked(const String &p_file_path, String &r_text);

	void _collect_signal_data(Node *p_node, Node *p_root, Array &r_out, int p_max_nodes, bool &r_truncated);
	void _analyze_node(Node *p_node, Node *p_root, int p_depth, int &r_total_nodes, int &r_max_depth, Dictionary &r_types, Array &r_scripts, Dictionary &r_resources, int p_max_nodes, bool &r_truncated);
	int _count_nodes_recursive(Node *p_node, int p_max_nodes, bool &r_truncated);
	int _get_max_depth(Node *p_node, int p_current_depth, int p_max_nodes, int &r_visited, bool &r_truncated);

	void _dfs_detect_cycle(const String &p_node, const Dictionary &p_graph, Dictionary &p_visited, Array &p_path_stack, Array &p_cycles);
	void _collect_statistics(const String &p_path, bool p_include_addons, Dictionary &r_file_counts, int p_max_files, bool &r_truncated);

public:
	void set_editor_plugin(JustAMCPEditorPlugin *p_plugin) { editor_plugin = p_plugin; }
	void set_owner(JustAMCPToolExecutor *p_owner) { owner = p_owner; }

	Dictionary execute_tool(const String &p_tool_name, const Dictionary &p_args);

	Dictionary find_unused_resources(const Dictionary &p_params);
	Dictionary analyze_signal_flow(const Dictionary &p_params);
	Dictionary analyze_scene_complexity(const Dictionary &p_params);
	Dictionary find_script_references(const Dictionary &p_params);
	Dictionary detect_circular_dependencies(const Dictionary &p_params);
	Dictionary get_project_statistics(const Dictionary &p_params);

	JustAMCPAnalysisTools();
	~JustAMCPAnalysisTools();
};

#endif
