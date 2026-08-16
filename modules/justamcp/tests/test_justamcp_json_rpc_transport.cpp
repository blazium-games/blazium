/**************************************************************************/
/*  test_justamcp_json_rpc_transport.cpp                                  */
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

#include "test_justamcp_json_rpc_transport.h"

#include "test_justamcp_fixture.h"

#include "modules/modules_enabled.gen.h"

#if defined(MODULE_HTTPSERVER_ENABLED)

#include "../justamcp_json_rpc_transport.h"
#include "../justamcp_server.h"
#include "../tools/justamcp_json_rpc_helpers.h"

#include "core/config/project_settings.h"
#include "tests/test_macros.h"

void test_justamcp_json_rpc_transport_invalid_json() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	Dictionary invalid = JustAMCPJsonRpcTransport::handle_json_rpc(&server, "{not-json", Ref<HTTPResponse>());
	CHECK(invalid.has("error"));
	CHECK(int(invalid["error"].operator Dictionary().get("code", 0)) == -32700);

	Dictionary null_server = JustAMCPJsonRpcTransport::handle_json_rpc(nullptr, "{}", Ref<HTTPResponse>());
	CHECK(null_server.is_empty());
}

void test_justamcp_json_rpc_request_ids_equal() {
	CHECK(JustAMCPJsonRpcHelpers::request_ids_equal(1, 1));
	CHECK(JustAMCPJsonRpcHelpers::request_ids_equal(1.0, 1));
	CHECK(JustAMCPJsonRpcHelpers::request_ids_equal("42", "42"));
	CHECK(!JustAMCPJsonRpcHelpers::request_ids_equal(1, 2));
}

void test_justamcp_tool_queue_full() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	const int prev_rate = int(GLOBAL_GET("blazium/justamcp/max_enqueue_per_sec_per_session"));
	ProjectSettings::get_singleton()->set_setting("blazium/justamcp/max_enqueue_per_sec_per_session", 0);
	Dictionary queue_full;
	for (int i = 0; i < JustAMCPServer::test_tool_queue_max(); i++) {
		MCPToolQueueEntry *entry = server.test_enqueue_tool_request(i, "blazium_logs_read", Dictionary(), queue_full);
		CHECK(entry != nullptr);
	}
	CHECK(server.get_pending_tool_queue_size() == JustAMCPServer::test_tool_queue_max());
	MCPToolQueueEntry *overflow = server.test_enqueue_tool_request(JustAMCPServer::test_tool_queue_max(), "blazium_logs_read", Dictionary(), queue_full);
	CHECK(overflow == nullptr);
	CHECK(queue_full.has("error"));
	CHECK(int(queue_full["error"].operator Dictionary().get("code", 0)) == -32003);
	ProjectSettings::get_singleton()->set_setting("blazium/justamcp/max_enqueue_per_sec_per_session", prev_rate);
}

void test_justamcp_in_flight_cancel_flag_only() {
	MCPToolQueueEntry entry;
	entry.cancel_requested = false;
	entry.is_task_augmented = false;
	entry.has_stateless_response = true;
	entry.request_id = 77;
	entry.cancel_requested = true;
	CHECK(entry.cancel_requested);
	CHECK(entry.rpc_result.is_empty());
}

void test_justamcp_json_rpc_tools_call_e2e() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	const bool prev_blocking = bool(GLOBAL_GET("blazium/justamcp/stateless_tool_blocking"));
	ProjectSettings::get_singleton()->set_setting("blazium/justamcp/stateless_tool_blocking", false);
	const String body = "{\"jsonrpc\":\"2.0\",\"id\":77,\"method\":\"tools/call\",\"params\":{\"name\":\"blazium_get_project_info\",\"arguments\":{}}}";
	Dictionary result = JustAMCPJsonRpcTransport::handle_json_rpc(&server, body, Ref<HTTPResponse>());
	ProjectSettings::get_singleton()->set_setting("blazium/justamcp/stateless_tool_blocking", prev_blocking);
	CHECK(result.is_empty());
	CHECK(server.get_pending_tool_queue_size() == 1);
	server.test_process_pending_tools();
	CHECK(server.get_pending_tool_queue_size() == 0);
}

void test_justamcp_json_rpc_tools_list_strips_handled() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	const String body = "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/list\",\"params\":{}}";
	Dictionary result = JustAMCPJsonRpcTransport::handle_json_rpc(&server, body, Ref<HTTPResponse>());
	CHECK(result.has("result"));
	CHECK(result.has("jsonrpc"));
	CHECK(result.has("id"));
	CHECK(!result.has("handled"));
	CHECK(!result.has("subscription_uri"));
	CHECK(!result.has("subscription_action"));
	Dictionary sanitized = JustAMCPJsonRpcTransport::sanitize_wire_rpc(result);
	CHECK(!sanitized.has("handled"));
	CHECK(sanitized.has("result"));

	Dictionary with_internal;
	with_internal["handled"] = true;
	with_internal["subscription_uri"] = "blazium://x";
	with_internal["subscription_action"] = "subscribe";
	with_internal["jsonrpc"] = "2.0";
	with_internal["id"] = 1;
	with_internal["result"] = Dictionary();
	Dictionary cleaned = JustAMCPJsonRpcTransport::sanitize_wire_rpc(with_internal);
	CHECK(!cleaned.has("handled"));
	CHECK(!cleaned.has("subscription_uri"));
	CHECK(!cleaned.has("subscription_action"));
	CHECK(cleaned.has("result"));
}

#endif

#endif
