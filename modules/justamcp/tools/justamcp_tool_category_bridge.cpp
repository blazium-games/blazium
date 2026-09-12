/**************************************************************************/
/*  justamcp_tool_category_bridge.cpp                                     */
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

#include "justamcp_tool_category_bridge.h"

#include "justamcp_tool_executor.h"

void JustAMCPToolCategoryBridge::_bind_methods() {
	ClassDB::bind_method(D_METHOD("provide_tool_schemas", "register_only", "ignore_settings", "include_disabled_tools"), &JustAMCPToolCategoryBridge::provide_tool_schemas, DEFVAL(false), DEFVAL(false), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("execute_tool", "tool_name", "arguments"), &JustAMCPToolCategoryBridge::execute_tool);
}

void JustAMCPToolCategoryBridge::setup(const String &p_toolset_name, const String &p_category, const String &p_description) {
	toolset_name = p_toolset_name;
	category = p_category;
	description = p_description;
}

Array JustAMCPToolCategoryBridge::provide_tool_schemas(bool p_register_only, bool p_ignore_settings, bool p_include_disabled_tools) {
	return JustAMCPToolExecutor::get_tool_schemas_for_category(category, p_register_only, p_ignore_settings, p_include_disabled_tools);
}

Dictionary JustAMCPToolCategoryBridge::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (JustAMCPToolExecutor *executor = JustAMCPToolExecutor::get_active_instance()) {
		return executor->execute_registry_category_tool(category, p_tool_name, p_args);
	}
	Dictionary result;
	result["ok"] = false;
	result["error"] = "JustAMCPToolExecutor unavailable";
	return result;
}

JustAMCPToolCategoryBridge::JustAMCPToolCategoryBridge() {}

JustAMCPToolCategoryBridge::~JustAMCPToolCategoryBridge() {}

#endif
