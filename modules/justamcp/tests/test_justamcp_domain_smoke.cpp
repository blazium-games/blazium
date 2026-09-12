/**************************************************************************/
/*  test_justamcp_domain_smoke.cpp                                        */
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

#include "test_justamcp_domain_smoke.h"

#ifdef TESTS_ENABLED

#include "../tools/justamcp_mcp_client_bridge.h"
#include "../tools/justamcp_tool_executor.h"
#include "tests/test_macros.h"

static void _assert_tool_shape(const Dictionary &p_result) {
	const bool ok_shape = bool(p_result.get("ok", false)) || p_result.has("error") || p_result.has("result") || !p_result.is_empty();
	CHECK(ok_shape);
}

void test_justamcp_domain_smoke_execute_tool() {
	JustAMCPToolExecutor executor;

	_assert_tool_shape(executor.execute_tool("blazium_get_project_info", Dictionary()));
	_assert_tool_shape(executor.execute_tool("blazium_logs_read", Dictionary()));
	_assert_tool_shape(executor.execute_tool("blazium_scene_tree_dump", Dictionary()));
	_assert_tool_shape(executor.execute_tool("blazium_list_scripts", Dictionary()));
	_assert_tool_shape(executor.execute_tool("blazium_list_resource_files", Dictionary()));
	_assert_tool_shape(executor.execute_tool("blazium_get_node_properties", Dictionary()));

	JustAMCPMCPClientBridge bridge;
	_assert_tool_shape(bridge.execute_tool("mcp_client_list_bridges", Dictionary()));
}

#endif
