/**************************************************************************/
/*  justamcp_readonly_tools.cpp                                           */
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

#include "justamcp_readonly_tools.h"

#include "core/os/mutex.h"
#include "core/templates/hash_set.h"
#include "core/variant/variant.h"

static Mutex g_readonly_tools_mutex;
static HashSet<String> g_registered_readonly_tools;
static HashSet<String> g_registered_worker_safe_tools;

static void _insert_name_aliases(HashSet<String> &r_set, const String &p_tool_name) {
	if (p_tool_name.is_empty()) {
		return;
	}
	r_set.insert(p_tool_name);
	if (p_tool_name.begins_with("blazium_")) {
		r_set.insert(p_tool_name.substr(8));
	} else {
		r_set.insert("blazium_" + p_tool_name);
	}
}

static String _internal_name(const String &p_tool_name) {
	if (p_tool_name.begins_with("blazium_")) {
		return p_tool_name.substr(8);
	}
	return p_tool_name;
}

void JustAMCPReadonlyTools::register_readonly_tool(const String &p_tool_name) {
	MutexLock lock(g_readonly_tools_mutex);
	_insert_name_aliases(g_registered_readonly_tools, p_tool_name);
}

void JustAMCPReadonlyTools::register_readonly_from_schema(const Dictionary &p_schema) {
	if (bool(p_schema.get("readonly", false))) {
		register_readonly_tool(String(p_schema.get("name", "")));
		return;
	}
	if (p_schema.has("_meta")) {
		const Dictionary meta = p_schema.get("_meta", Dictionary());
		if (bool(meta.get("readonly", false))) {
			register_readonly_tool(String(p_schema.get("name", "")));
		}
	}
	register_worker_safe_from_schema(p_schema);
}

void JustAMCPReadonlyTools::register_worker_safe_tool(const String &p_tool_name) {
	MutexLock lock(g_readonly_tools_mutex);
	_insert_name_aliases(g_registered_worker_safe_tools, p_tool_name);
	_insert_name_aliases(g_registered_readonly_tools, p_tool_name);
}

void JustAMCPReadonlyTools::register_worker_safe_from_schema(const Dictionary &p_schema) {
	String affinity;
	if (p_schema.has("execution") && p_schema["execution"].get_type() == Variant::DICTIONARY) {
		affinity = String(Dictionary(p_schema["execution"]).get("threadAffinity", ""));
	}
	if (affinity.is_empty() && p_schema.has("_meta")) {
		affinity = String(Dictionary(p_schema["_meta"]).get("threadAffinity", ""));
	}
	if (affinity == "worker") {
		register_worker_safe_tool(String(p_schema.get("name", "")));
	}
}

bool JustAMCPReadonlyTools::is_readonly_tool(const String &p_tool_name) {
	const String internal_name = _internal_name(p_tool_name);
	{
		MutexLock lock(g_readonly_tools_mutex);
		if (g_registered_readonly_tools.has(p_tool_name) || g_registered_readonly_tools.has(internal_name)) {
			return true;
		}
	}
	static const char *k_readonly_tools[] = {
		"logs_read",
		"editor_get_output_log",
		"editor_get_errors",
		"docs_list_classes",
		"docs_search",
		"docs_get_class",
		"docs_get_member",
		"classdb_query",
		"get_project_info",
		"get_filesystem_tree",
		"search_files",
		"search_in_files",
		"read_directory",
		"uid_to_project_path",
		"project_path_to_uid",
		"project_get_input_actions",
		"get_performance_monitors",
		"get_editor_performance",
		"list_export_presets",
		"get_export_info",
		"semantic_search",
		"semantic_find_similar",
		"semantic_index_stats",
		"semantic_search_enqueue",
		"semantic_search_poll",
		"semantic_search_cancel",
		"search_tools",
		"get_guide",
		"list_toolsets",
		"describe_toolset",
		"wait",
		nullptr,
	};
	for (int i = 0; k_readonly_tools[i] != nullptr; i++) {
		if (internal_name == k_readonly_tools[i]) {
			return true;
		}
	}
	return is_worker_safe_tool(p_tool_name);
}

bool JustAMCPReadonlyTools::is_worker_safe_tool(const String &p_tool_name) {
	const String internal_name = _internal_name(p_tool_name);
	{
		MutexLock lock(g_readonly_tools_mutex);
		if (g_registered_worker_safe_tools.has(p_tool_name) || g_registered_worker_safe_tools.has(internal_name)) {
			return true;
		}
	}

	static const char *k_worker_safe_tools[] = {
		"logs_read",
		"editor_get_output_log",
		"editor_get_errors",
		"docs_list_classes",
		"docs_search",
		"docs_get_class",
		"docs_get_member",
		"classdb_query",
		"search_tools",
		"get_guide",
		"list_toolsets",
		"describe_toolset",
		"get_filesystem_tree",
		"search_files",
		"search_in_files",
		"read_directory",
		"uid_to_project_path",
		"project_path_to_uid",
		"wait",
		nullptr,
	};
	for (int i = 0; k_worker_safe_tools[i] != nullptr; i++) {
		if (internal_name == k_worker_safe_tools[i]) {
			return true;
		}
	}
	return false;
}
