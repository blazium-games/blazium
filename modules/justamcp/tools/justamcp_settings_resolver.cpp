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
#include "editor/editor_settings.h"

bool JustAMCPSettingsResolver::uses_project_override() {
	if (!ProjectSettings::get_singleton() || !ProjectSettings::get_singleton()->has_setting("blazium/justamcp/override_editor_settings")) {
		return false;
	}
	return GLOBAL_GET("blazium/justamcp/override_editor_settings");
}

bool JustAMCPSettingsResolver::resolve_category_enabled(const String &p_category, bool p_default) {
	const String setting_path = "blazium/justamcp/tools/" + p_category;
	if (uses_project_override() || !EditorSettings::get_singleton()) {
		if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting(setting_path)) {
			return GLOBAL_GET(setting_path);
		}
		return p_default;
	}
	if (EditorSettings::get_singleton()->has_setting(setting_path)) {
		return EDITOR_GET(setting_path);
	}
	return p_default;
}

bool JustAMCPSettingsResolver::resolve_tool_enabled(const String &p_category, const String &p_full_name, bool p_default) {
	const String setting_path = "blazium/justamcp/tools/" + p_category + "/" + p_full_name;
	if (uses_project_override() || !EditorSettings::get_singleton()) {
		if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting(setting_path)) {
			return GLOBAL_GET(setting_path);
		}
		return p_default;
	}
	if (EditorSettings::get_singleton()->has_setting(setting_path)) {
		return EDITOR_GET(setting_path);
	}
	return p_default;
}

bool JustAMCPSettingsResolver::resolve_toolset_enabled(const String &p_name, bool p_default) {
	const String setting_path = "blazium/justamcp/toolsets/" + p_name;
	if (uses_project_override() || !EditorSettings::get_singleton()) {
		if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting(setting_path)) {
			return GLOBAL_GET(setting_path);
		}
		return p_default;
	}
	if (EditorSettings::get_singleton()->has_setting(setting_path)) {
		return EDITOR_GET(setting_path);
	}
	return p_default;
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

#endif
