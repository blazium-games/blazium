/**************************************************************************/
/*  test_justamcp_http_integration.h                                      */
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

void test_justamcp_http_initialize_creates_session();
void test_justamcp_http_initialize_json_only_accept_creates_session();
void test_justamcp_http_post_sse_conflict_409();
void test_justamcp_http_delete_teardown();
void test_justamcp_http_cors_preflight();
void test_justamcp_http_stateless_empty_body_400();
void test_justamcp_http_oauth_rejects_invalid_credentials();
void test_justamcp_http_two_session_pending_isolation();
void test_justamcp_orphan_send_tool_result_route_fallback();
void test_justamcp_legacy_sse_broadcast_tracking();
void test_justamcp_mcp_config_json_uses_streamable_mcp_path();
void test_justamcp_http_catalogs_after_initialize();
void test_justamcp_http_dual_accept_tools_list_returns_json();
void test_justamcp_http_dual_accept_tools_call_uses_sse();
void test_justamcp_mcp_config_client_field_shapes();

TEST_CASE("[Modules][JustAMCP] http initialize creates session") {
	test_justamcp_http_initialize_creates_session();
}

TEST_CASE("[Modules][JustAMCP] http initialize json-only Accept creates session") {
	test_justamcp_http_initialize_json_only_accept_creates_session();
}

TEST_CASE("[Modules][JustAMCP] http post sse conflict 409") {
	test_justamcp_http_post_sse_conflict_409();
}

TEST_CASE("[Modules][JustAMCP] http delete teardown") {
	test_justamcp_http_delete_teardown();
}

TEST_CASE("[Modules][JustAMCP] http cors preflight") {
	test_justamcp_http_cors_preflight();
}

TEST_CASE("[Modules][JustAMCP] http stateless empty body 400") {
	test_justamcp_http_stateless_empty_body_400();
}

TEST_CASE("[Modules][JustAMCP] http oauth rejects invalid credentials") {
	test_justamcp_http_oauth_rejects_invalid_credentials();
}

TEST_CASE("[Modules][JustAMCP] http two session pending isolation") {
	test_justamcp_http_two_session_pending_isolation();
}

TEST_CASE("[Modules][JustAMCP] orphan send tool result route fallback") {
	test_justamcp_orphan_send_tool_result_route_fallback();
}

TEST_CASE("[Modules][JustAMCP] legacy sse broadcast tracking") {
	test_justamcp_legacy_sse_broadcast_tracking();
}

TEST_CASE("[Modules][JustAMCP] mcp config json uses streamable /mcp path") {
	test_justamcp_mcp_config_json_uses_streamable_mcp_path();
}

TEST_CASE("[Modules][JustAMCP] http catalogs after initialize") {
	test_justamcp_http_catalogs_after_initialize();
}

TEST_CASE("[Modules][JustAMCP] http dual-Accept tools/list returns JSON") {
	test_justamcp_http_dual_accept_tools_list_returns_json();
}

TEST_CASE("[Modules][JustAMCP] http dual-Accept tools/call uses SSE") {
	test_justamcp_http_dual_accept_tools_call_uses_sse();
}

TEST_CASE("[Modules][JustAMCP] mcp config client field shapes") {
	test_justamcp_mcp_config_client_field_shapes();
}
