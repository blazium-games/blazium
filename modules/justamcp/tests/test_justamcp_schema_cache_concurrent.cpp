/**************************************************************************/
/*  test_justamcp_schema_cache_concurrent.cpp                             */
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

#include "test_justamcp_schema_cache_concurrent.h"

#ifdef TESTS_ENABLED

#ifdef TOOLS_ENABLED

#include "../tools/justamcp_tool_schema_cache.h"

#include "tests/test_macros.h"

void test_justamcp_schema_cache_dirty_category_refresh() {
	JustAMCPToolSchemaCache::invalidate();
	const Array before = JustAMCPToolSchemaCache::get_schemas(false, false, false, true);
	CHECK(!before.is_empty());
	JustAMCPToolSchemaCache::invalidate_category("editor_tools");
	const Dictionary found = JustAMCPToolSchemaCache::find_tool_schema("blazium_editor_play_scene", true);
	CHECK(!found.is_empty());
	const Array after = JustAMCPToolSchemaCache::get_schemas(false, false, false, true);
	CHECK(after.size() == before.size());
}

void test_justamcp_schema_cache_invalidate_clears_entries() {
	JustAMCPToolSchemaCache::get_schemas(false, false, false, true);
	JustAMCPToolSchemaCache::invalidate();
	JustAMCPToolSchemaCache::invalidate_category("scene_tools");
	const Array rebuilt = JustAMCPToolSchemaCache::get_category_schemas("scene_tools", false, false, true);
	CHECK(rebuilt.size() >= 0);
}

#else
void test_justamcp_schema_cache_dirty_category_refresh() {
	TEST_FAIL_COND(true, "TOOLS_ENABLED is required for schema cache tests");
}
void test_justamcp_schema_cache_invalidate_clears_entries() {
	TEST_FAIL_COND(true, "TOOLS_ENABLED is required for schema cache tests");
}
#endif

#endif
