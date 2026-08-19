/**************************************************************************/
/*  justamcp_settings_resolver.h                                          */
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

#include "core/string/ustring.h"

class JustAMCPSettingsResolver {
public:
	static bool uses_project_override();
	static bool resolve_bool(const String &p_path, bool p_default);
	static void set_category_default(const String &p_category, bool p_is_core);
	static bool resolve_category_enabled(const String &p_category, bool p_default = true);
	static bool resolve_tool_enabled(const String &p_category, const String &p_full_name, bool p_default = true);
	static bool resolve_toolset_enabled(const String &p_name, bool p_default = true);
	static bool resolve_tool_enabled(const String &p_category, const String &p_full_name, bool p_ignore_settings, bool p_include_disabled_tools, bool &r_cat_enabled, bool &r_tool_enabled);
	static bool is_tool_listed(const String &p_category, const String &p_full_name);
	static bool is_tool_executable(const String &p_category, const String &p_full_name);
	static bool is_prompt_listed(const String &p_name);
	static bool is_resource_listed(const String &p_name);
	static bool resolve_allow_execute_tool_bypass();
};

#endif
