/**************************************************************************/
/*  justamcp_toolset_registry.cpp                                         */
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

#include "justamcp_toolset_registry.h"

#include "justamcp_tool_dispatcher.h"

#include "justamcp_settings_resolver.h"

#include "core/config/project_settings.h"
#include "editor/editor_settings.h"

JustAMCPToolsetRegistry *JustAMCPToolsetRegistry::singleton = nullptr;

static bool _is_toolset_enabled_live(const String &p_name) {
	return JustAMCPSettingsResolver::resolve_toolset_enabled(p_name, true);
}

void JustAMCPToolsetRegistry::_bind_methods() {
	ClassDB::bind_method(D_METHOD("register_toolset", "name", "description", "get_schemas", "execute_tool"), &JustAMCPToolsetRegistry::register_toolset);
	ClassDB::bind_method(D_METHOD("unregister_toolset", "name"), &JustAMCPToolsetRegistry::unregister_toolset);
	ClassDB::bind_method(D_METHOD("is_discovery_enabled"), &JustAMCPToolsetRegistry::is_discovery_enabled);
	ClassDB::bind_method(D_METHOD("list_toolset_names"), &JustAMCPToolsetRegistry::list_toolset_names);
	ClassDB::bind_method(D_METHOD("list_toolsets"), &JustAMCPToolsetRegistry::list_toolsets);
	ClassDB::bind_method(D_METHOD("describe_toolset", "name"), &JustAMCPToolsetRegistry::describe_toolset);
	ClassDB::bind_method(D_METHOD("call_toolset", "toolset_name", "tool_name", "arguments"), &JustAMCPToolsetRegistry::call_toolset);
}

JustAMCPToolsetRegistry *JustAMCPToolsetRegistry::get_singleton() {
	return singleton;
}

void JustAMCPToolsetRegistry::register_toolset(const String &p_name, const String &p_description, const Callable &p_get_schemas, const Callable &p_execute_tool) {
	register_toolset_with_owner(p_name, p_description, p_get_schemas, p_execute_tool, nullptr, String());
}

void JustAMCPToolsetRegistry::register_toolset_with_owner(const String &p_name, const String &p_description, const Callable &p_get_schemas, const Callable &p_execute_tool, Object *p_owned_provider, const String &p_category) {
#ifndef TESTS_ENABLED
	ERR_FAIL_COND_MSG(this != singleton, "JustAMCPToolsetRegistry: only the module singleton may register toolsets.");
#endif
	const String toolset_path = "blazium/justamcp/toolsets/" + p_name;
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::BOOL, toolset_path), true);
#ifdef TOOLS_ENABLED
	if (EditorSettings::get_singleton()) {
		EDITOR_DEF_BASIC(toolset_path, true);
	}
#endif

	ToolsetEntry entry;
	entry.name = p_name;
	entry.description = p_description;
	entry.category = p_category;
	entry.get_schemas = p_get_schemas;
	entry.execute_tool = p_execute_tool;
	entry.owned_provider = p_owned_provider;
	entry.enabled = true;
	toolsets[p_name] = entry;
	if (p_owned_provider) {
		owned_providers.push_back(p_owned_provider);
	}
}

void JustAMCPToolsetRegistry::unregister_toolset(const String &p_name) {
#ifndef TESTS_ENABLED
	ERR_FAIL_COND_MSG(this != singleton, "JustAMCPToolsetRegistry: only the module singleton may unregister toolsets.");
#endif
	if (!toolsets.has(p_name)) {
		return;
	}
	Object *owned = toolsets[p_name].owned_provider;
	toolsets.erase(p_name);
	if (owned) {
		for (int i = 0; i < owned_providers.size(); i++) {
			if (owned_providers[i] == owned) {
				owned_providers.remove_at(i);
				break;
			}
		}
		memdelete(owned);
	}
}

bool JustAMCPToolsetRegistry::is_discovery_enabled() const {
	return JustAMCPSettingsResolver::resolve_bool("blazium/justamcp/enable_toolset_discovery", false);
}

Array JustAMCPToolsetRegistry::list_toolset_names() const {
	Array names;
	for (const KeyValue<String, ToolsetEntry> &kv : toolsets) {
		names.push_back(kv.key);
	}
	names.sort();
	return names;
}

Dictionary JustAMCPToolsetRegistry::list_toolsets() const {
	Dictionary result;
	result["ok"] = true;
	Array toolset_list;
	for (const KeyValue<String, ToolsetEntry> &kv : toolsets) {
		Dictionary item;
		item["name"] = kv.key;
		item["description"] = kv.value.description;
		item["enabled"] = _is_toolset_enabled_live(kv.key);
		if (!kv.value.category.is_empty()) {
			item["category"] = kv.value.category;
		}
		toolset_list.push_back(item);
	}
	toolset_list.sort();
	result["toolsets"] = toolset_list;
	result["count"] = toolsets.size();
	return result;
}

Dictionary JustAMCPToolsetRegistry::describe_toolset(const String &p_name) const {
	Dictionary result;
	if (!toolsets.has(p_name)) {
		result["ok"] = false;
		result["error"] = "Unknown toolset: " + p_name;
		return result;
	}
	const ToolsetEntry &entry = toolsets[p_name];
	const bool enabled = _is_toolset_enabled_live(p_name);
	if (!entry.get_schemas.is_valid()) {
		result["ok"] = false;
		result["error"] = "Toolset has no schema provider: " + p_name;
		return result;
	}
	Variant schemas = entry.get_schemas.call(false, true, true);
	result["ok"] = true;
	result["toolset_name"] = p_name;
	result["description"] = entry.description;
	result["enabled"] = enabled;
	result["tools"] = schemas;
	if (schemas.get_type() == Variant::ARRAY) {
		result["tool_count"] = Array(schemas).size();
	}
	return result;
}

Array JustAMCPToolsetRegistry::collect_tool_schemas(const String &p_name, bool p_register_only, bool p_ignore_settings, bool p_include_disabled_tools) const {
	if (!toolsets.has(p_name)) {
		return Array();
	}
	const ToolsetEntry &entry = toolsets[p_name];
	if (!entry.get_schemas.is_valid()) {
		return Array();
	}
	if (!p_register_only && !p_ignore_settings && !_is_toolset_enabled_live(p_name)) {
		return Array();
	}
	const Variant schemas = entry.get_schemas.call(p_register_only, p_ignore_settings, p_include_disabled_tools);
	if (schemas.get_type() == Variant::ARRAY) {
		return schemas;
	}
	return Array();
}

Dictionary JustAMCPToolsetRegistry::call_toolset(const String &p_toolset_name, const String &p_tool_name, const Dictionary &p_args) const {
	Dictionary result;
	if (!toolsets.has(p_toolset_name)) {
		result["ok"] = false;
		result["error"] = "Unknown toolset: " + p_toolset_name;
		return result;
	}
	const ToolsetEntry &entry = toolsets[p_toolset_name];
	if (!entry.execute_tool.is_valid()) {
		result["ok"] = false;
		result["error"] = "Toolset has no executor: " + p_toolset_name;
		return result;
	}
	String full_tool_name = p_tool_name;
	if (!full_tool_name.begins_with("blazium_")) {
		full_tool_name = "blazium_" + full_tool_name;
	}
	if (!entry.category.is_empty() && !JustAMCPSettingsResolver::is_tool_executable(entry.category, full_tool_name)) {
		result["ok"] = false;
		result["error"] = "Tool is disabled: " + full_tool_name;
		return result;
	}
	return entry.execute_tool.call(full_tool_name, p_args);
}

JustAMCPToolsetRegistry::JustAMCPToolsetRegistry() {
	if (!singleton) {
		singleton = this;
	}
}

JustAMCPToolsetRegistry::~JustAMCPToolsetRegistry() {
	for (int i = 0; i < owned_providers.size(); i++) {
		memdelete(owned_providers[i]);
	}
	owned_providers.clear();
	if (singleton == this) {
		singleton = nullptr;
	}
}

#endif
