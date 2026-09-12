/**************************************************************************/
/*  test_justamcp_session_manager.h                                       */
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

#pragma once

#include "tests/test_macros.h"

void test_mcp_session_manager_active_tool_connection();
void test_mcp_session_manager_clear_all();
void test_mcp_session_manager_pending_post_sse_per_session();
void test_mcp_session_manager_pending_post_sse_conflict();
void test_mcp_session_manager_pending_post_sse_timeout();
void test_mcp_session_manager_json_post_route_fallback();
void test_mcp_session_manager_last_registered_connection_fallback();
void test_mcp_session_manager_delete_clears_connection_map();

TEST_CASE("[Modules][JustAMCP] session manager active tool connection") {
	test_mcp_session_manager_active_tool_connection();
}

TEST_CASE("[Modules][JustAMCP] session manager clear all") {
	test_mcp_session_manager_clear_all();
}

TEST_CASE("[Modules][JustAMCP] session manager pending post sse per session") {
	test_mcp_session_manager_pending_post_sse_per_session();
}

TEST_CASE("[Modules][JustAMCP] session manager pending post sse conflict") {
	test_mcp_session_manager_pending_post_sse_conflict();
}

TEST_CASE("[Modules][JustAMCP] session manager pending post sse timeout") {
	test_mcp_session_manager_pending_post_sse_timeout();
}

TEST_CASE("[Modules][JustAMCP] session manager json post route fallback") {
	test_mcp_session_manager_json_post_route_fallback();
}

TEST_CASE("[Modules][JustAMCP] session manager last registered connection fallback") {
	test_mcp_session_manager_last_registered_connection_fallback();
}

TEST_CASE("[Modules][JustAMCP] session manager delete clears connection map") {
	test_mcp_session_manager_delete_clears_connection_map();
}
