/**************************************************************************/
/*  test_justamcp_session_manager.cpp                                     */
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

#include "test_justamcp_session_manager.h"

#if defined(MODULE_HTTPSERVER_ENABLED)

#include "core/os/time.h"

#include "modules/justamcp/justamcp_session_manager.h"

void test_mcp_session_manager_active_tool_connection() {
	MCPSessionManager session_manager(nullptr);
	session_manager.test_register_stream_for_session("session-a", 42);
	session_manager.set_session_active_tool_connection("session-a", 42);
	CHECK(session_manager.test_get_active_tool_connection_id("session-a") == 42);
	session_manager.bind_request_tool_route(10, "session-a", 42);
	CHECK(session_manager.test_get_active_tool_connection_id("session-a") == 42);
}

void test_mcp_session_manager_clear_all() {
	MCPSessionManager session_manager(nullptr);
	session_manager.test_register_stream_for_session("session-b", 7);
	session_manager.set_session_active_tool_connection("session-b", 7);
	session_manager.bind_request_tool_route(1, "session-b", 7);
	session_manager.clear_all();
	CHECK(!session_manager.session_exists("session-b"));
	CHECK(session_manager.test_connection_map_size() == 0);
}

void test_mcp_session_manager_pending_post_sse_per_session() {
	MCPSessionManager session_manager(nullptr);
	session_manager.test_set_pending_post_sse("session-a", "{\"id\":1,\"method\":\"tools/call\"}");
	session_manager.test_set_pending_post_sse("session-b", "{\"id\":2,\"method\":\"tools/call\"}");
	CHECK(session_manager.test_has_pending_post_sse_for_session("session-a"));
	CHECK(session_manager.test_has_pending_post_sse_for_session("session-b"));
	CHECK(session_manager.test_peek_pending_post_sse_body("session-a").contains("\"id\":1"));
	CHECK(session_manager.test_peek_pending_post_sse_body("session-b").contains("\"id\":2"));
}

void test_mcp_session_manager_pending_post_sse_conflict() {
	MCPSessionManager session_manager(nullptr);
	CHECK(session_manager.test_try_set_pending_post_sse("session-conflict", "{\"id\":1}"));
	CHECK(!session_manager.test_try_set_pending_post_sse("session-conflict", "{\"id\":2}"));
	CHECK(session_manager.test_peek_pending_post_sse_body("session-conflict").contains("\"id\":1"));
}

void test_mcp_session_manager_pending_post_sse_timeout() {
	MCPSessionManager session_manager(nullptr);
	session_manager.test_set_pending_post_sse("session-timeout", "{\"id\":1}");
	const uint64_t expired_usec = Time::get_singleton()->get_ticks_usec() - (200ULL * 1000000ULL);
	session_manager.test_set_pending_post_sse_created_usec("session-timeout", expired_usec);
	session_manager.test_prune_expired_pending_post_sse();
	CHECK(!session_manager.test_has_pending_post_sse_for_session("session-timeout"));
}

void test_mcp_session_manager_json_post_route_fallback() {
	MCPSessionManager session_manager(nullptr);
	const Variant request_id = 99;
	session_manager.test_register_stream_for_session("session-json", 55);
	session_manager.bind_request_tool_route(request_id, "session-json", -1);
	CHECK(session_manager.test_get_active_tool_connection_id("session-json") == 55);
}

void test_mcp_session_manager_last_registered_connection_fallback() {
	MCPSessionManager session_manager(nullptr);
	session_manager.test_register_stream_for_session("session-last", 10);
	session_manager.test_register_stream_for_session("session-last", 20);
	CHECK(session_manager.test_get_last_registered_connection_id("session-last") == 20);
	session_manager.set_session_active_tool_connection("session-last", -1);
	CHECK(session_manager.test_get_active_tool_connection_id("session-last") == 20);
}

void test_mcp_session_manager_delete_clears_connection_map() {
	MCPSessionManager session_manager(nullptr);
	session_manager.test_register_stream_for_session("del-session", 100);
	session_manager.test_register_stream_for_session("del-session", 101);
	session_manager.bind_request_tool_route(500, "del-session", 100);
	CHECK(session_manager.test_connection_map_size() == 2);
	session_manager.test_simulate_session_delete("del-session");
	CHECK(session_manager.test_connection_map_size() == 0);
	CHECK(!session_manager.test_has_connection_mapping(100));
	CHECK(!session_manager.test_has_connection_mapping(101));
	String session_id;
	int connection_id = -1;
	CHECK(!session_manager.get_request_tool_route(500, session_id, connection_id));
	CHECK(!session_manager.session_exists("del-session"));
}

#endif
