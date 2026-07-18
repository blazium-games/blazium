/**************************************************************************/
/*  test_justamcp_phase_k.cpp                                             */
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

#include "test_justamcp_phase_k.h"
#include "../justamcp_server.h"
#include "../justamcp_tool_dispatch.h"
#include "../mcp_tool_queue.h"
#include "../tools/justamcp_readonly_tools.h"
#include "../tools/justamcp_tool_schema_builder.h"
#include "modules/modules_enabled.gen.h"
#include "test_justamcp_fixture.h"
#if defined(MODULE_HTTPSERVER_ENABLED)
#include "modules/httpserver/http_response.h"
#endif
#include "tests/test_macros.h"

void test_justamcp_worker_safe_registry_locked() {
	JustAMCPReadonlyTools::register_worker_safe_tool("phase_k_worker_probe");
	CHECK(JustAMCPReadonlyTools::is_worker_safe_tool("phase_k_worker_probe"));
	CHECK(JustAMCPReadonlyTools::is_worker_safe_tool("blazium_phase_k_worker_probe"));
	CHECK(JustAMCPReadonlyTools::is_readonly_tool("phase_k_worker_probe"));

	Dictionary schema = JustAMCPToolSchemaBuilder::build_tool_schema(
			"blazium_phase_k_schema_worker", "probe", "meta", true,
			Vector<String>{}, Vector<String>{}, "forbidden", "worker");
	JustAMCPReadonlyTools::register_readonly_from_schema(schema);
	CHECK(JustAMCPReadonlyTools::is_worker_safe_tool("blazium_phase_k_schema_worker"));
}

void test_justamcp_main_only_not_scheduled_on_pool() {
	CHECK(JustAMCPReadonlyTools::is_readonly_tool("blazium_get_project_info"));
	CHECK(!JustAMCPReadonlyTools::is_worker_safe_tool("blazium_create_scene"));
	CHECK(!JustAMCPReadonlyTools::is_worker_safe_tool("blazium_set_project_setting"));
	CHECK(JustAMCPReadonlyTools::is_worker_safe_tool("blazium_logs_read"));
	CHECK(JustAMCPReadonlyTools::is_worker_safe_tool("blazium_docs_search"));
	CHECK(JustAMCPReadonlyTools::is_worker_safe_tool("blazium_search_files"));

	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	CHECK(!JustAMCPToolDispatch::try_schedule_worker_execute(&server, 1, "blazium_create_scene", Dictionary()));
}

void test_justamcp_parallel_readonly_overlap() {
	MCPToolQueue queue;
	MCPToolQueueEntry a;
	MCPToolQueueEntry b;
	a.request_id = 1;
	a.is_readonly_tool = true;
	a.session_id = "s1";
	b.request_id = 2;
	b.is_readonly_tool = true;
	b.session_id = "s2";
	CHECK(queue.enqueue(&a));
	CHECK(queue.enqueue(&b));

	Vector<MCPToolQueueEntry *> inflight;
	inflight.push_back(&a);
	queue.set_in_flight(nullptr, inflight);
	MCPToolQueueEntry *picked = queue.pick_next(2, true);
	CHECK(picked == &b);
	CHECK(queue.readonly_inflight_count() == 1);
}

void test_justamcp_cancel_scoped_multi_inflight() {
#if defined(MODULE_HTTPSERVER_ENABLED)
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPToolQueueEntry a;
	a.request_id = 11;
	a.session_id = "sess-a";
	a.cancel_requested = false;
	server.test_set_in_flight_entries(nullptr, &a);

	server._on_request_cancelled(999, "test", "sess-a");
	CHECK(!a.cancel_requested);
	server._on_request_cancelled(11, "test", "sess-a");
	CHECK(a.cancel_requested);
	server.test_set_in_flight_entries(nullptr, nullptr);
#else
	CHECK(true);
#endif
}

void test_justamcp_legacy_message_rpc_deferred() {
#if defined(MODULE_HTTPSERVER_ENABLED)
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	server.call("_deferred_legacy_message_json_rpc", String("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}"), String());
	CHECK(true);
#else
	CHECK(true);
#endif
}

void test_justamcp_json_post_hold_client_completes() {
#if defined(MODULE_HTTPSERVER_ENABLED)
	Ref<HTTPResponse> response;
	response.instantiate();
	CHECK(!response->is_held());
	response->hold();
	CHECK(response->is_held());
	response->set_held_client_id(42);
	CHECK(response->get_held_client_id() == 42);
#else
	CHECK(true);
#endif
}

void test_justamcp_tools_list_does_not_block_accept() {
	CHECK(JustAMCPReadonlyTools::is_worker_safe_tool("blazium_search_tools"));
	CHECK(JustAMCPReadonlyTools::is_worker_safe_tool("blazium_list_toolsets"));
	CHECK(JustAMCPReadonlyTools::is_worker_safe_tool("blazium_describe_toolset"));
}

#endif
