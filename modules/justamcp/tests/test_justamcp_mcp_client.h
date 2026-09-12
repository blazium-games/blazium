/**************************************************************************/
/*  test_justamcp_mcp_client.h                                            */
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

#ifdef TESTS_ENABLED

void test_justamcp_client_meta_and_headers();
void test_justamcp_client_allow_list_and_sse_parse();
void test_justamcp_client_oauth_helpers();
void test_justamcp_mcp_apps_helpers();
void test_justamcp_input_required_modern_tools_call();
void test_justamcp_discover_honest_and_tasks_update();
void test_justamcp_bridge_crud_and_redaction();
void test_justamcp_loopback_discover_then_list();
void test_justamcp_mcp_apps_http_host_and_proxy();

TEST_CASE("[Modules][JustAMCP] client _meta and Mcp-Method headers") {
	test_justamcp_client_meta_and_headers();
}

TEST_CASE("[Modules][JustAMCP] client allow-list and SSE parse") {
	test_justamcp_client_allow_list_and_sse_parse();
}

TEST_CASE("[Modules][JustAMCP] OAuth client metadata DCR issuer and CIMD") {
	test_justamcp_client_oauth_helpers();
}

TEST_CASE("[Modules][JustAMCP] MCP Apps ui CSP and path sandbox") {
	test_justamcp_mcp_apps_helpers();
}

TEST_CASE("[Modules][JustAMCP] InputRequiredResult on modern tools/call") {
	test_justamcp_input_required_modern_tools_call();
}

TEST_CASE("[Modules][JustAMCP] discover caps and tasks/update") {
	test_justamcp_discover_honest_and_tasks_update();
}

TEST_CASE("[Modules][JustAMCP] bridge CRUD redacts secrets") {
	test_justamcp_bridge_crud_and_redaction();
}

TEST_CASE("[Modules][JustAMCP] loopback discover then tools/list") {
	test_justamcp_loopback_discover_then_list();
}

TEST_CASE("[Modules][JustAMCP] MCP Apps host injects HTML and proxy sanitizes") {
	test_justamcp_mcp_apps_http_host_and_proxy();
}

#endif
