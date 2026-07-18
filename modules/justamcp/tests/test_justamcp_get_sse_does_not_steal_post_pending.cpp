/**************************************************************************/
/*  test_justamcp_get_sse_does_not_steal_post_pending.cpp                 */
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

#include "test_justamcp_get_sse_does_not_steal_post_pending.h"
#include "test_justamcp_fixture.h"

#ifdef TESTS_ENABLED

#if defined(MODULE_HTTPSERVER_ENABLED)

#include "../justamcp_server.h"
#include "../justamcp_session_manager.h"
#include "tests/test_macros.h"

void test_justamcp_get_sse_does_not_steal_post_pending() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPSessionManager *session_manager = server.test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	const String session_id = "get-steal-session";
	session_manager->test_register_stream_for_session(session_id, 9000);
	session_manager->test_set_pending_post_sse(session_id, "{\"jsonrpc\":\"2.0\",\"id\":9,\"method\":\"tools/call\",\"params\":{\"name\":\"blazium_logs_read\",\"arguments\":{}}}");
	CHECK(session_manager->test_has_pending_post_sse_for_session(session_id));

	Dictionary get_headers;
	get_headers["MCP-Session-Id"] = session_id;
	get_headers["Accept"] = "text/event-stream";
	session_manager->on_sse_connection_opened(9001, "/mcp", get_headers);

	CHECK(session_manager->test_has_pending_post_sse_for_session(session_id));
	CHECK(session_manager->test_peek_pending_post_sse_body(session_id).contains("tools/call"));

	Dictionary dual_get_headers;
	dual_get_headers["MCP-Session-Id"] = session_id;
	dual_get_headers["Accept"] = "application/json, text/event-stream";
	session_manager->on_sse_connection_opened(9002, "/mcp", dual_get_headers);
	CHECK(session_manager->test_has_pending_post_sse_for_session(session_id));

	session_manager->test_arm_pending_post_sse_claim(session_id);
	Dictionary post_headers;
	post_headers["MCP-Session-Id"] = session_id;
	post_headers["Accept"] = "application/json, text/event-stream";
	session_manager->on_sse_connection_opened(9003, "/mcp", post_headers);

	CHECK(!session_manager->test_has_pending_post_sse_for_session(session_id));
}

#else
void test_justamcp_get_sse_does_not_steal_post_pending() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}
#endif

#endif
