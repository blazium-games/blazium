/**************************************************************************/
/*  test_justamcp_protocol_matrix.cpp                                     */
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

#include "test_justamcp_fixture.h"

#include "modules/modules_enabled.gen.h"

#if defined(MODULE_HTTPSERVER_ENABLED)

#include "../justamcp_json_rpc_transport.h"
#include "../justamcp_mcp_spec.h"
#include "../justamcp_server.h"
#include "../tools/justamcp_json_rpc_helpers.h"
#include "../tools/justamcp_json_rpc_router.h"

#include "tests/test_macros.h"

#ifdef TOOLS_ENABLED
#include "../tools/justamcp_resource_manifest.h"
#endif

static Dictionary _initialize_for(JustAMCPServer &p_server, const String &p_version) {
	Dictionary payload;
	Dictionary params;
	params["protocolVersion"] = p_version;
	params["capabilities"] = Dictionary();
	payload["params"] = params;
	return JustAMCPJsonRpcRouter::route_initialize(&p_server, payload, 1);
}

void test_justamcp_protocol_matrix_initialize_capabilities() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();

	{
		Dictionary routed = _initialize_for(server, "2024-11-05");
		Dictionary caps = Dictionary(routed["result"])["capabilities"];
		CHECK(caps.has("tools"));
		CHECK(!caps.has("completions"));
		CHECK(!caps.has("tasks"));
		CHECK(!caps.has("elicitation"));
		CHECK(!Dictionary(Dictionary(routed["result"])["serverInfo"]).has("title"));
	}
	{
		Dictionary routed = _initialize_for(server, "2025-03-26");
		Dictionary caps = Dictionary(routed["result"])["capabilities"];
		CHECK(caps.has("completions"));
		CHECK(!caps.has("tasks"));
		CHECK(!caps.has("elicitation"));
		CHECK(!Dictionary(Dictionary(routed["result"])["serverInfo"]).has("title"));
	}
	{
		Dictionary routed = _initialize_for(server, "2025-06-18");
		Dictionary caps = Dictionary(routed["result"])["capabilities"];
		CHECK(caps.has("completions"));
		CHECK(!caps.has("tasks"));
		CHECK(caps.has("elicitation"));
		CHECK(Dictionary(caps["elicitation"]).has("form"));
		CHECK(!Dictionary(caps["elicitation"]).has("url"));
		CHECK(Dictionary(Dictionary(routed["result"])["serverInfo"]).has("title"));
	}
	{
		Dictionary routed = _initialize_for(server, "2025-11-25");
		Dictionary caps = Dictionary(routed["result"])["capabilities"];
		CHECK(caps.has("completions"));
		CHECK(caps.has("tasks"));
		CHECK(Dictionary(caps["tasks"]).has("update"));
		CHECK(caps.has("elicitation"));
		CHECK(Dictionary(caps["elicitation"]).has("form"));
		CHECK(Dictionary(caps["elicitation"]).has("url"));
		CHECK(Dictionary(Dictionary(routed["result"])["serverInfo"]).has("title"));
	}
}

void test_justamcp_protocol_matrix_batch_execute_2025_03_26() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	server.test_set_transport_negotiated_protocol("2025-03-26");
	Dictionary wrapped = JustAMCPJsonRpcTransport::handle_json_rpc(
			&server,
			"[{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"},{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"ping\"}]",
			Ref<HTTPResponse>());
	CHECK(wrapped.has("_justamcp_batch_results"));
	Array responses = wrapped["_justamcp_batch_results"];
	CHECK(responses.size() == 2);
	CHECK(Dictionary(responses[0]).has("result"));
	CHECK(Dictionary(responses[1]).has("result"));
	CHECK(int(Dictionary(responses[0]).get("id", 0)) == 1);
	CHECK(int(Dictionary(responses[1]).get("id", 0)) == 2);
}

void test_justamcp_protocol_matrix_icons_and_structured_content() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();

	Dictionary listed;
	Array tools;
	Dictionary tool;
	tool["name"] = "blazium_search_tools";
	justamcp_attach_icons(tool);
	tools.push_back(tool);
	listed["tools"] = tools;

	Dictionary old_list = listed.duplicate(true);
	justamcp_apply_protocol_to_list_result(old_list, "2025-06-18");
	CHECK(!Dictionary(Array(old_list["tools"])[0]).has("icons"));

	Dictionary new_list = listed.duplicate(true);
	justamcp_apply_protocol_to_list_result(new_list, "2025-11-25");
	CHECK(Dictionary(Array(new_list["tools"])[0]).has("icons"));

	server.test_set_transport_negotiated_protocol("2025-03-26");
	Dictionary payload;
	payload["ok"] = true;
	payload["message"] = "hi";
	Dictionary formatted = JustAMCPJsonRpcHelpers::format_tool_result(true, payload, "");
	CHECK(formatted.has("result"));
	CHECK(!Dictionary(formatted["result"]).has("structuredContent"));

	server.test_set_transport_negotiated_protocol("2025-06-18");
	Dictionary formatted_new = JustAMCPJsonRpcHelpers::format_tool_result(true, payload, "");
	CHECK(Dictionary(formatted_new["result"]).has("structuredContent"));

#ifdef TOOLS_ENABLED
	Array resources = JustAMCPResourceManifest::get_static_resource_schemas();
	REQUIRE(resources.size() > 0);
	CHECK(Dictionary(resources[0]).has("icons"));
#endif
}

#else

void test_justamcp_protocol_matrix_initialize_capabilities() {}
void test_justamcp_protocol_matrix_batch_execute_2025_03_26() {}
void test_justamcp_protocol_matrix_icons_and_structured_content() {}

#endif

#endif
