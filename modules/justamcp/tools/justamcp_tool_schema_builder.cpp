/**************************************************************************/
/*  justamcp_tool_schema_builder.cpp                                      */
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

#ifdef TOOLS_ENABLED

#include "justamcp_tool_schema_builder.h"

#include "justamcp_settings_resolver.h"

#include "core/config/project_settings.h"
#include "editor/editor_settings.h"

void JustAMCPToolSchemaBuilder::register_tool_settings(const String &p_category, const String &p_full_name, bool p_is_core) {
	const String cat_path = "blazium/justamcp/tools/" + p_category;
	const String tool_path = cat_path + "/" + p_full_name;
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::BOOL, cat_path), p_is_core);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::BOOL, tool_path), true);
	if (EditorSettings::get_singleton()) {
		EDITOR_DEF_BASIC(cat_path, p_is_core);
		EDITOR_DEF_BASIC(tool_path, true);
	}
}

bool JustAMCPToolSchemaBuilder::resolve_tool_enabled(const String &p_category, const String &p_full_name, bool p_ignore_settings, bool p_include_disabled_tools, bool &r_cat_enabled, bool &r_tool_enabled) {
	return JustAMCPSettingsResolver::resolve_tool_enabled(p_category, p_full_name, p_ignore_settings, p_include_disabled_tools, r_cat_enabled, r_tool_enabled);
}

Dictionary JustAMCPToolSchemaBuilder::build_tool_schema(const String &p_full_name, const String &p_desc, const String &p_category, bool p_enabled, const Vector<String> &p_props, const Vector<String> &p_req, const String &p_task_support, const String &p_thread_affinity) {
	Dictionary t;
	t["name"] = p_full_name;
	t["description"] = p_desc;
	Dictionary meta;
	meta["category"] = p_category;
	meta["enabled"] = p_enabled;
	if (!p_task_support.is_empty()) {
		meta["taskSupport"] = p_task_support;
	}
	if (!p_thread_affinity.is_empty()) {
		meta["threadAffinity"] = p_thread_affinity;
	}
	t["_meta"] = meta;
	Dictionary schema;
	schema["type"] = "object";
	Dictionary props;
	for (int i = 0; i < p_props.size(); i += 2) {
		Dictionary prop;
		prop["type"] = p_props[i + 1];
		props[p_props[i]] = prop;
	}
	schema["properties"] = props;
	if (!p_req.is_empty()) {
		Array req;
		for (int i = 0; i < p_req.size(); i++) {
			req.push_back(p_req[i]);
		}
		schema["required"] = req;
	}
	t["inputSchema"] = schema;
	if (p_task_support != "forbidden" || p_thread_affinity == "worker") {
		Dictionary execution;
		if (!p_task_support.is_empty() && p_task_support != "forbidden") {
			execution["taskSupport"] = p_task_support;
		}
		if (p_thread_affinity == "worker") {
			execution["threadAffinity"] = "worker";
		}
		if (!execution.is_empty()) {
			t["execution"] = execution;
		}
	}
	return t;
}

#endif
