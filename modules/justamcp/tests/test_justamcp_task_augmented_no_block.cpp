/**************************************************************************/
/*  test_justamcp_task_augmented_no_block.cpp                             */
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

#include "test_justamcp_task_augmented_no_block.h"

#include "test_justamcp_fixture.h"

#if defined(MODULE_HTTPSERVER_ENABLED)

#include "../justamcp_json_rpc_transport.h"
#include "../justamcp_server.h"

#include "core/os/os.h"
#include "tests/test_macros.h"

#include "modules/httpserver/http_response.h"

void test_justamcp_task_augmented_http_returns_immediately() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	Ref<HTTPResponse> response;
	response.instantiate();
	const String body = "{\"jsonrpc\":\"2.0\",\"id\":901,\"method\":\"tools/call\",\"params\":{\"name\":\"blazium_editor_reload_project\",\"arguments\":{},\"task\":{\"ttl\":60000}}}";
	const uint64_t start_msec = OS::get_singleton()->get_ticks_msec();
	const Dictionary result = JustAMCPJsonRpcTransport::handle_json_rpc(&server, body, response);
	const uint64_t elapsed_msec = OS::get_singleton()->get_ticks_msec() - start_msec;
	CHECK(result.is_empty());

	CHECK(elapsed_msec < 500);
	CHECK(server.get_pending_tool_queue_size() >= 1);
	server.test_clear_tool_queue();
}

#else
void test_justamcp_task_augmented_http_returns_immediately() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for task augmented no-block test");
}
#endif

#endif
