/**************************************************************************/
/*  test_justamcp_tool_dispatcher.cpp                                     */
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

#include "test_justamcp_tool_dispatcher.h"
#include "../tools/justamcp_executor_dispatch_table.h"
#include "../tools/justamcp_tool_dispatcher.h"
#include "tests/test_macros.h"

void test_justamcp_tool_dispatcher() {
	CHECK(JustAMCPToolDispatcher::matches_prefix_route("tags_search_assets"));
	CHECK(JustAMCPToolDispatcher::matches_prefix_route("tags_set_on_asset"));
	CHECK(JustAMCPToolDispatcher::matches_prefix_route("semantic_search"));
	CHECK(JustAMCPToolDispatcher::matches_prefix_route("mcp_client_list_servers"));

	CHECK(!JustAMCPToolDispatcher::matches_prefix_route("multiuser_get_status"));
	CHECK(JustAMCPToolDispatcher::matches_prefix_route("mcp_client_list_bridges"));
	CHECK(!JustAMCPToolDispatcher::matches_prefix_route("autowork_list_tasks"));
	CHECK(!JustAMCPToolDispatcher::matches_prefix_route("editor_play_scene"));
	CHECK(!JustAMCPToolDispatcher::matches_prefix_route("scene_get_tree"));
	CHECK(!JustAMCPToolDispatcher::matches_prefix_route("unknown_tool"));

	CHECK(JustAMCPExecutorDispatchTable::get_toolset_for_tool("tags_list") == String("AssetTags"));
	CHECK(JustAMCPExecutorDispatchTable::get_toolset_for_tool("semantic_find_similar") == String("SemanticSearch"));
	CHECK(JustAMCPExecutorDispatchTable::get_toolset_for_tool("multiuser_get_status") == nullptr);
}

#endif
