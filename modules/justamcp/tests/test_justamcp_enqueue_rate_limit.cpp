/**************************************************************************/
/*  test_justamcp_enqueue_rate_limit.cpp                                  */
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

#include "test_justamcp_enqueue_rate_limit.h"
#include "test_justamcp_fixture.h"

#if defined(MODULE_HTTPSERVER_ENABLED)

#include "../justamcp_server.h"
#include "../justamcp_session_manager.h"
#include "core/config/project_settings.h"
#include "tests/test_macros.h"

void test_justamcp_enqueue_rate_limit_rejects_burst() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPSessionManager *session_manager = server.test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	const int prev_limit = int(GLOBAL_GET("blazium/justamcp/max_enqueue_per_sec_per_session"));
	ProjectSettings::get_singleton()->set_setting("blazium/justamcp/max_enqueue_per_sec_per_session", 2);

	session_manager->bind_request_tool_route(1, "rate-limit-session", 1);
	session_manager->bind_request_tool_route(2, "rate-limit-session", 1);
	session_manager->bind_request_tool_route(3, "rate-limit-session", 1);

	Dictionary queue_full;
	CHECK(server.test_enqueue_tool_request(1, "blazium_logs_read", Dictionary(), queue_full) != nullptr);
	CHECK(server.test_enqueue_tool_request(2, "blazium_logs_read", Dictionary(), queue_full) != nullptr);
	MCPToolQueueEntry *third = server.test_enqueue_tool_request(3, "blazium_logs_read", Dictionary(), queue_full);
	CHECK(third == nullptr);
	CHECK(queue_full.has("error"));
	CHECK(int(queue_full["error"].operator Dictionary().get("code", 0)) == -32004);
	CHECK(String(queue_full["error"].operator Dictionary().get("message", "")).contains("rate limit"));

	ProjectSettings::get_singleton()->set_setting("blazium/justamcp/max_enqueue_per_sec_per_session", prev_limit);
	server.test_clear_tool_queue();
}

#else
void test_justamcp_enqueue_rate_limit_rejects_burst() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for enqueue rate limit test");
}
#endif

#endif
