/**************************************************************************/
/*  test_justamcp_tool_schema_cache.cpp                                   */
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

#include "test_justamcp_tool_schema_cache.h"

#include "../tools/justamcp_tool_schema_cache.h"

#include "tests/test_macros.h"

void test_justamcp_tool_schema_cache() {
	JustAMCPToolSchemaCache::invalidate();
	const Array full_catalog = JustAMCPToolSchemaCache::get_schemas(false, false, false, true);
	const Array cached_catalog = JustAMCPToolSchemaCache::get_schemas(false, false, false, true);
	CHECK(full_catalog.size() == cached_catalog.size());

	const Array discovery_catalog = JustAMCPToolSchemaCache::get_schemas(false, false, true, true);
	CHECK(discovery_catalog.size() <= full_catalog.size());

	if (!full_catalog.is_empty()) {
		Dictionary first = full_catalog[0];
		const String tool_name = first.get("name", "");
		Dictionary found = JustAMCPToolSchemaCache::find_tool_schema(tool_name, true);
		CHECK(!found.is_empty());
		CHECK(String(found.get("name", "")) == tool_name);
	}

	Dictionary execute_schema = JustAMCPToolSchemaCache::find_tool_schema("blazium_execute_tool", true);
	if (!execute_schema.is_empty() && execute_schema.has("inputSchema")) {
		Dictionary input_schema = execute_schema["inputSchema"];
		if (input_schema.has("properties")) {
			Dictionary props = input_schema["properties"];
			if (props.has("arguments")) {
				Dictionary arguments = props["arguments"];
				CHECK(String(arguments.get("type", "")) == "object");
			}
		}
	}
}

void test_justamcp_tool_schema_cache_invalidate_category() {
	JustAMCPToolSchemaCache::invalidate();
	const Array before_editor = JustAMCPToolSchemaCache::get_category_schemas("editor_tools", false, false, true);
	const Array before_scene = JustAMCPToolSchemaCache::get_category_schemas("scene_tools", false, false, true);
	JustAMCPToolSchemaCache::invalidate_category("editor_tools");
	const Array after_editor = JustAMCPToolSchemaCache::get_category_schemas("editor_tools", false, false, true);
	const Array after_scene = JustAMCPToolSchemaCache::get_category_schemas("scene_tools", false, false, true);
	CHECK(after_editor.size() == before_editor.size());
	CHECK(after_scene.size() == before_scene.size());
	if (!before_scene.is_empty() && !after_scene.is_empty()) {
		CHECK(String(before_scene[0].operator Dictionary().get("name", "")) == String(after_scene[0].operator Dictionary().get("name", "")));
	}
}

#endif
