/**************************************************************************/
/*  justamcp_settings_resolver.cpp                                        */
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

#include "justamcp_settings_resolver.h"

#include "core/config/project_settings.h"
#include "core/templates/hash_map.h"
#include "editor/editor_settings.h"

static HashMap<String, bool> &justamcp_category_defaults() {
	static HashMap<String, bool> defaults;
	return defaults;
}

static bool _justamcp_variant_as_bool(const Variant &p_value, bool p_default) {
	if (p_value.get_type() == Variant::BOOL) {
		return p_value;
	}
	if (p_value.get_type() == Variant::NIL) {
		return p_default;
	}
	if (p_value.get_type() == Variant::STRING) {
		return p_default;
	}
	return p_value.booleanize();
}

bool JustAMCPSettingsResolver::uses_project_override() {
	if (!ProjectSettings::get_singleton() || !ProjectSettings::get_singleton()->has_setting("blazium/justamcp/override_editor_settings")) {
		return false;
	}
	return GLOBAL_GET("blazium/justamcp/override_editor_settings");
}

bool JustAMCPSettingsResolver::resolve_bool(const String &p_path, bool p_default) {
	if (uses_project_override() || !EditorSettings::get_singleton()) {
		if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting(p_path)) {
			return _justamcp_variant_as_bool(ProjectSettings::get_singleton()->get_setting(p_path), p_default);
		}
		return p_default;
	}
	if (EditorSettings::get_singleton()->has_setting(p_path)) {
		return _justamcp_variant_as_bool(EditorSettings::get_singleton()->get_setting(p_path), p_default);
	}
	return p_default;
}

void JustAMCPSettingsResolver::set_category_default(const String &p_category, bool p_is_core) {
	if (p_category.is_empty()) {
		return;
	}
	justamcp_category_defaults()[p_category] = p_is_core;
}

bool JustAMCPSettingsResolver::resolve_category_enabled(const String &p_category, bool p_default) {
	bool def = p_default;
	if (justamcp_category_defaults().has(p_category)) {
		def = justamcp_category_defaults()[p_category];
	}
	return resolve_bool("blazium/justamcp/tools/" + p_category, def);
}

bool JustAMCPSettingsResolver::resolve_tool_enabled(const String &p_category, const String &p_full_name, bool p_default) {
	return resolve_bool("blazium/justamcp/tools/" + p_category + "/" + p_full_name, p_default);
}

bool JustAMCPSettingsResolver::resolve_toolset_enabled(const String &p_name, bool p_default) {
	return resolve_bool("blazium/justamcp/toolsets/" + p_name, p_default);
}

bool JustAMCPSettingsResolver::resolve_tool_enabled(const String &p_category, const String &p_full_name, bool p_ignore_settings, bool p_include_disabled_tools, bool &r_cat_enabled, bool &r_tool_enabled) {
	r_cat_enabled = true;
	r_tool_enabled = true;
	if (p_ignore_settings) {
		return true;
	}
	r_cat_enabled = resolve_category_enabled(p_category, true);
	r_tool_enabled = resolve_tool_enabled(p_category, p_full_name, true);
	if (!p_include_disabled_tools && (!r_cat_enabled || !r_tool_enabled)) {
		return false;
	}
	return true;
}

bool JustAMCPSettingsResolver::is_tool_listed(const String &p_category, const String &p_full_name) {
	if (p_category.is_empty()) {
		return true;
	}
	return resolve_category_enabled(p_category, true) && resolve_tool_enabled(p_category, p_full_name, true);
}

bool JustAMCPSettingsResolver::is_tool_executable(const String &p_category, const String &p_full_name) {
	if (p_full_name.is_empty()) {
		return false;
	}
	if (p_category.is_empty()) {
		return true;
	}
	return resolve_tool_enabled(p_category, p_full_name, true);
}

bool JustAMCPSettingsResolver::is_prompt_listed(const String &p_name) {
	if (p_name.is_empty()) {
		return false;
	}
	return resolve_bool("blazium/justamcp/prompts", true) && resolve_bool("blazium/justamcp/prompts/" + p_name, true);
}

bool JustAMCPSettingsResolver::is_resource_listed(const String &p_name) {
	if (p_name.is_empty()) {
		return false;
	}
	return resolve_bool("blazium/justamcp/resources", true) && resolve_bool("blazium/justamcp/resources/" + p_name, true);
}

bool JustAMCPSettingsResolver::resolve_allow_execute_tool_bypass() {
	return resolve_bool("blazium/justamcp/allow_execute_tool_bypass", false);
}

#endif
