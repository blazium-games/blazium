/**************************************************************************/
/*  test_justamcp_tool_schema_builder.cpp                                 */
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

#ifdef TESTS_ENABLED

#include "test_justamcp_tool_schema_builder.h"

#include "../tools/justamcp_tool_schema_builder.h"

#include "tests/test_macros.h"

void test_justamcp_tool_schema_builder() {
	bool cat_enabled = false;
	bool tool_enabled = false;
	CHECK(JustAMCPToolSchemaBuilder::resolve_tool_enabled("asset_tags_tools", "blazium_tags_list", true, false, cat_enabled, tool_enabled));
	Dictionary schema = JustAMCPToolSchemaBuilder::build_tool_schema(
			"blazium_tags_list", "List tags", "asset_tags_tools", true,
			Vector<String>{ "parent_tag", "string" }, Vector<String>{});
	CHECK(String(schema.get("name", "")) == "blazium_tags_list");
	CHECK(schema.has("inputSchema"));
}

#endif
