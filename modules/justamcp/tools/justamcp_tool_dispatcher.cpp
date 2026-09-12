/**************************************************************************/
/*  justamcp_tool_dispatcher.cpp                                          */
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

#include "justamcp_tool_dispatcher.h"

#include "modules/modules_enabled.gen.h"

#include "justamcp_executor_dispatch_table.h"
#include "justamcp_settings_resolver.h"
#include "justamcp_tool_executor.h"
#include "justamcp_toolset_registry.h"

bool JustAMCPToolDispatcher::is_tool_enabled(const String &p_full_name, const String &p_category) {
	return JustAMCPSettingsResolver::is_tool_executable(p_category, p_full_name);
}

bool JustAMCPToolDispatcher::matches_prefix_route(const String &p_internal_name) {
	return JustAMCPExecutorDispatchTable::matches_dispatch_route(p_internal_name);
}

Dictionary JustAMCPToolDispatcher::dispatch_prefix_tools(JustAMCPToolExecutor *p_executor, const String &p_internal_name, const Dictionary &p_args) {
	Dictionary err;
	err["ok"] = false;
	err["error"] = "Tool dispatch unavailable";
	(void)p_executor;

	const char *toolset = JustAMCPExecutorDispatchTable::get_toolset_for_tool(p_internal_name);
	if (!toolset) {
		err["error"] = "Unknown prefixed tool: " + p_internal_name;
		return err;
	}

	if (JustAMCPToolsetRegistry::get_singleton()) {
		return JustAMCPToolsetRegistry::get_singleton()->call_toolset(toolset, p_internal_name, p_args);
	}
	err["error"] = vformat("%s toolset unavailable", toolset);
	return err;
}

#endif
