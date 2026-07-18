/**************************************************************************/
/*  test_justamcp_complete_current_respects_tombstone.cpp                 */
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

#include "test_justamcp_complete_current_respects_tombstone.h"
#include "test_justamcp_fixture.h"

#ifdef TESTS_ENABLED

#if defined(MODULE_HTTPSERVER_ENABLED)

#include "../justamcp_server.h"
#include "../justamcp_tool_context.h"
#include "../tools/justamcp_json_rpc_helpers.h"

#include "tests/test_macros.h"

void test_justamcp_complete_current_respects_tombstone() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();

	JustAMCPToolContextScope scope(7777);
	server.test_insert_tool_result_tombstone(JustAMCPJsonRpcHelpers::request_id_to_string(7777));
	server.test_clear_last_send_tool_result();

	Dictionary rpc;
	rpc["jsonrpc"] = "2.0";
	rpc["id"] = 7777;
	Dictionary result;
	result["ok"] = true;
	rpc["result"] = result;
	server.test_complete_current_tool_request(rpc);

	CHECK(server.test_peek_last_send_tool_result().is_empty());
}

#else
void test_justamcp_complete_current_respects_tombstone() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}
#endif

#endif
