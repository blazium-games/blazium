/**************************************************************************/
/*  test_justamcp_routing_dispatch.h                                      */
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

void test_mcp_session_manager_request_route_binding();
void test_justamcp_request_router_ttl_and_cleanup();
void test_mcp_session_manager_prune_request_routes();
void test_mcp_session_manager_broadcast_connection_index();
void test_justamcp_notification_bus_broadcast_dedup();
void test_justamcp_active_task_route();
void test_justamcp_tool_dispatch_cancel_payload();

TEST_CASE("[Modules][JustAMCP] session manager per-request route binding") {
	test_mcp_session_manager_request_route_binding();
}

TEST_CASE("[Modules][JustAMCP] request router ttl and cleanup") {
	test_justamcp_request_router_ttl_and_cleanup();
}

TEST_CASE("[Modules][JustAMCP] session manager prune request routes") {
	test_mcp_session_manager_prune_request_routes();
}

TEST_CASE("[Modules][JustAMCP] session manager broadcast connection index") {
	test_mcp_session_manager_broadcast_connection_index();
}

TEST_CASE("[Modules][JustAMCP] notification bus broadcast dedup") {
	test_justamcp_notification_bus_broadcast_dedup();
}

TEST_CASE("[Modules][JustAMCP] active task route") {
	test_justamcp_active_task_route();
}

TEST_CASE("[Modules][JustAMCP] tool dispatch cancelled payload") {
	test_justamcp_tool_dispatch_cancel_payload();
}
