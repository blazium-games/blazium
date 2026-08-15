/**************************************************************************/
/*  test_justamcp_cancel_deadline_ignores_late_complete.cpp               */
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

#include "test_justamcp_cancel_deadline_ignores_late_complete.h"
#include "test_justamcp_fixture.h"

#ifdef TESTS_ENABLED

#if defined(MODULE_HTTPSERVER_ENABLED)

#include "../justamcp_server.h"
#include "../mcp_tool_queue_entry.h"

#include "tests/test_macros.h"

void test_justamcp_cancel_deadline_ignores_late_complete() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();

	MCPToolQueueEntry *entry = memnew(MCPToolQueueEntry);
	entry->request_id = 4242;
	entry->tool_name = "blazium_logs_read";
	entry->cancel_requested = true;
	entry->cancel_requested_usec = 1;
	server.test_set_in_flight_entries(entry, nullptr);

	server.test_clear_last_send_tool_result();
	server.test_enforce_in_flight_cancel_deadline(4242);
	Dictionary first = server.test_peek_last_send_tool_result();
	CHECK(first.has("id"));

	server.test_clear_last_send_tool_result();
	Dictionary late_ok;
	late_ok["message"] = "should-be-ignored";
	server.send_tool_result(4242, true, late_ok, "");
	Dictionary second = server.test_peek_last_send_tool_result();
	CHECK(second.is_empty());
}

#else
void test_justamcp_cancel_deadline_ignores_late_complete() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}
#endif

#endif
