/**************************************************************************/
/*  test_justamcp_schema_dirty_per_cache_key.cpp                          */
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

#include "test_justamcp_schema_dirty_per_cache_key.h"

#ifdef TESTS_ENABLED

#ifdef TOOLS_ENABLED

#include "../tools/justamcp_tool_schema_cache.h"

#include "tests/test_macros.h"

void test_justamcp_schema_dirty_per_cache_key() {
	JustAMCPToolSchemaCache::invalidate();
	const Array full = JustAMCPToolSchemaCache::get_schemas(false, false, false, true);
	const Array discovery = JustAMCPToolSchemaCache::get_schemas(false, false, true, true);
	CHECK(discovery.size() <= full.size());

	JustAMCPToolSchemaCache::invalidate_category("editor_tools");

	const Array full_after = JustAMCPToolSchemaCache::get_schemas(false, false, false, true);
	const Array discovery_after = JustAMCPToolSchemaCache::get_schemas(false, false, true, true);
	CHECK(full_after.size() == full.size());
	CHECK(discovery_after.size() == discovery.size());
	CHECK(discovery_after.size() <= full_after.size());
}

#else
void test_justamcp_schema_dirty_per_cache_key() {
	TEST_FAIL_COND(true, "TOOLS_ENABLED is required");
}
#endif

#endif
