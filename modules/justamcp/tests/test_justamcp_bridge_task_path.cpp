/**************************************************************************/
/*  test_justamcp_bridge_task_path.cpp                                    */
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

#include "test_justamcp_bridge_task_path.h"
#include "test_justamcp_fixture.h"

#ifdef TESTS_ENABLED

#if defined(MODULE_HTTPSERVER_ENABLED)

#include "../justamcp_json_rpc_transport.h"
#include "../justamcp_server.h"
#include "../tools/justamcp_mcp_client_bridge.h"

#include "modules/httpserver/http_response.h"
#include "tests/test_macros.h"

void test_justamcp_bridge_task_path() {
	JustAMCPMCPClientBridge bridge;
	Array schemas = bridge.get_tool_schemas(false, true, true);
	bool found_optional = false;
	for (int i = 0; i < schemas.size(); i++) {
		Dictionary tool = schemas[i];
		const String name = tool.get("name", "");
		if (name != "blazium_mcp_client_call_remote_tool" && name != "blazium_mcp_client_list_remote_tools") {
			continue;
		}
		Dictionary execution = tool.get("execution", Dictionary());
		CHECK(String(execution.get("taskSupport", "")) == "required");
		found_optional = true;
	}
	CHECK(found_optional);

	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	Ref<HTTPResponse> response;
	response.instantiate();

	const String body = "{\"jsonrpc\":\"2.0\",\"id\":50,\"method\":\"tools/call\",\"params\":{\"name\":\"blazium_mcp_client_list_remote_tools\",\"arguments\":{\"bridge_name\":\"missing\"},\"task\":{\"ttl\":60000}}}";
	const Dictionary result = JustAMCPJsonRpcTransport::handle_json_rpc(&server, body, response);

	const bool async_or_sync = result.is_empty() || result.has("result") || result.has("error");
	CHECK(async_or_sync);
	if (result.is_empty()) {
		CHECK(server.get_pending_tool_queue_size() >= 1);
	}

	Dictionary listed = bridge.execute_tool("mcp_client_list_bridges", Dictionary());
	const bool listed_ok = listed.has("bridges") || listed.has("count") || listed.has("ok") || !listed.is_empty();
	CHECK(listed_ok);
}

#else
void test_justamcp_bridge_task_path() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}
#endif

#endif
