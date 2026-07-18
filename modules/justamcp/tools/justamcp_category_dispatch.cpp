/**************************************************************************/
/*  justamcp_category_dispatch.cpp                                        */
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

#include "justamcp_category_dispatch.h"

#include "justamcp_category_registry.h"
#include "justamcp_tool_executor.h"

static bool _module_tool_was_handled(const Dictionary &p_routed) {
	if (p_routed.is_empty()) {
		return false;
	}
	if (p_routed.has("handled")) {
		return bool(p_routed.get("handled", false));
	}

	if (p_routed.has("ok")) {
		return true;
	}
	return p_routed.has("result");
}

bool JustAMCPToolCategoryDispatch::is_registry_category(const String &p_category) {
	return JustAMCPCategoryRegistry::is_registered_category(p_category);
}

bool JustAMCPToolCategoryDispatch::has_module_executor(const String &p_category) {
	return is_registry_category(p_category);
}

Dictionary JustAMCPToolCategoryDispatch::dispatch_module_tools(JustAMCPToolExecutor *p_executor, const String &p_category, const String &p_internal_name, const Dictionary &p_args) {
	Dictionary result;
	result["handled"] = false;
	if (!p_executor || !has_module_executor(p_category)) {
		return result;
	}

	Dictionary routed = p_executor->execute_module_category_tool(p_category, p_internal_name, p_args);
	if (_module_tool_was_handled(routed)) {
		routed["handled"] = true;
		return routed;
	}
	return result;
}

#endif
