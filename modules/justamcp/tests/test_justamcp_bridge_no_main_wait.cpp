/**************************************************************************/
/*  test_justamcp_bridge_no_main_wait.cpp                                 */
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

#include "test_justamcp_bridge_no_main_wait.h"
#include "test_justamcp_fixture.h"

#ifdef TESTS_ENABLED

#if defined(MODULE_HTTPSERVER_ENABLED)

#include "../justamcp_json_rpc_transport.h"
#include "../justamcp_server.h"
#include "../tools/justamcp_mcp_client_bridge.h"

#include "core/os/os.h"
#include "modules/httpserver/http_response.h"
#include "tests/test_macros.h"

void test_justamcp_bridge_no_main_wait() {
	JustAMCPMCPClientBridge bridge;
	Array schemas = bridge.get_tool_schemas(false, true, true);
	bool found_required = false;
	for (int i = 0; i < schemas.size(); i++) {
		Dictionary tool = schemas[i];
		const String name = tool.get("name", "");
		if (name != "blazium_mcp_client_call_remote_tool" &&
				name != "blazium_mcp_client_list_remote_tools" &&
				name != "blazium_mcp_client_read_remote_resource") {
			continue;
		}
		Dictionary execution = tool.get("execution", Dictionary());
		CHECK(String(execution.get("taskSupport", "")) == "required");
		found_required = true;
	}
	CHECK(found_required);

	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	Ref<HTTPResponse> response;
	response.instantiate();

	const uint64_t start_ms = OS::get_singleton()->get_ticks_msec();
	const String body = "{\"jsonrpc\":\"2.0\",\"id\":77,\"method\":\"tools/call\",\"params\":{\"name\":\"blazium_mcp_client_list_remote_tools\",\"arguments\":{\"bridge_name\":\"missing\"},\"task\":{\"ttl\":60000}}}";
	const Dictionary result = JustAMCPJsonRpcTransport::handle_json_rpc(&server, body, response);
	const uint64_t elapsed_ms = OS::get_singleton()->get_ticks_msec() - start_ms;
	CHECK(elapsed_ms < justamcp_nonblocking_call_budget_ms());

	const bool async_or_task = result.is_empty() || result.has("result") || result.has("error");
	CHECK(async_or_task);
	if (result.is_empty()) {
		CHECK(server.get_pending_tool_queue_size() >= 1);
	} else if (result.has("result")) {
		Dictionary res = result["result"];
		const bool has_task = res.has("task") || res.has("taskId");
		CHECK(has_task);
	}
}

#else
void test_justamcp_bridge_no_main_wait() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}
#endif

#endif
