/**************************************************************************/
/*  test_justamcp_export_task_support_required.cpp                        */
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

#include "test_justamcp_export_task_support_required.h"

#ifdef TESTS_ENABLED

#ifdef TOOLS_ENABLED

#include "../tools/justamcp_tool_schema_cache.h"

#include "tests/test_macros.h"

void test_justamcp_export_task_support_required() {
	JustAMCPToolSchemaCache::invalidate();
	const char *required_tools[] = {
		"blazium_list_android_devices",
		"blazium_deploy_to_android",
		"blazium_export_project",
		"blazium_rescan_filesystem",
		nullptr
	};
	for (int i = 0; required_tools[i]; i++) {
		Dictionary schema = JustAMCPToolSchemaCache::find_tool_schema(required_tools[i], true);
		CHECK(!schema.is_empty());
		Dictionary execution = schema.get("execution", Dictionary());
		CHECK(String(execution.get("taskSupport", "")) == "required");
	}
}

#else
void test_justamcp_export_task_support_required() {
	TEST_FAIL_COND(true, "TOOLS_ENABLED is required");
}
#endif

#endif
