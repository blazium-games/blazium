/**************************************************************************/
/*  test_justamcp_routing_dispatch.cpp                                    */
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

#include "test_justamcp_routing_dispatch.h"
#include "test_justamcp_fixture.h"

#if defined(MODULE_HTTPSERVER_ENABLED)

#include "modules/justamcp/justamcp_request_router.h"
#include "modules/justamcp/justamcp_server.h"
#include "modules/justamcp/justamcp_session_manager.h"
#include "modules/justamcp/justamcp_tool_dispatch.h"
#include "modules/justamcp/tools/justamcp_tool_executor.h"

void test_mcp_session_manager_request_route_binding() {
	MCPSessionManager session_manager(nullptr);
	const Variant request_id = 42;
	session_manager.bind_request_tool_route(request_id, "session-a", 7);

	String session_id;
	int connection_id = -1;
	CHECK(session_manager.get_request_tool_route(request_id, session_id, connection_id));
	CHECK(session_id == "session-a");
	CHECK(connection_id == 7);

	session_manager.bind_request_tool_route(43, "session-b", 8);
	CHECK(session_manager.get_request_tool_route(43, session_id, connection_id));
	CHECK(session_id == "session-b");
	CHECK(connection_id == 8);

	CHECK(session_manager.get_request_tool_route(request_id, session_id, connection_id));
	CHECK(session_id == "session-a");

	session_manager.clear_request_tool_route(request_id);
	CHECK(!session_manager.get_request_tool_route(request_id, session_id, connection_id));

	session_manager.clear_request_routes_for_connection(8);
	CHECK(!session_manager.get_request_tool_route(43, session_id, connection_id));
}

void test_justamcp_request_router_ttl_and_cleanup() {
	JustAMCPRequestRouter router;
	router.bind(101, "session-ttl", 3);
	String session_id;
	int connection_id = -1;
	CHECK(router.lookup(101, session_id, connection_id));
	CHECK(session_id == "session-ttl");
	router.clear(101);
	CHECK(!router.lookup(101, session_id, connection_id));
	router.bind(102, "session-b", 4);
	router.clear_for_connection(4);
	CHECK(!router.lookup(102, session_id, connection_id));

	router.bind(103, "session-expired", 5);
	router.test_set_route_created_usec(103, 1);
	router.prune_expired(1);
	CHECK(!router.lookup(103, session_id, connection_id));
}

void test_mcp_session_manager_prune_request_routes() {
	MCPSessionManager session_manager(nullptr);
	session_manager.bind_request_tool_route(201, "session-prune", 9);
	session_manager.test_set_request_route_created_usec(201, 1);
	session_manager.test_prune_request_routes_usec(1);
	String session_id;
	int connection_id = -1;
	CHECK(!session_manager.get_request_tool_route(201, session_id, connection_id));
}

void test_mcp_session_manager_broadcast_connection_index() {
	MCPSessionManager session_manager(nullptr);
	CHECK(session_manager.test_register_stream_for_session("session-broadcast", 11));
	CHECK(session_manager.test_register_stream_for_session("session-broadcast", 12));
	const Vector<int> connections = session_manager.collect_all_broadcast_connection_ids();
	CHECK(connections.size() == 2);
	bool found11 = false;
	bool found12 = false;
	for (int i = 0; i < connections.size(); i++) {
		if (connections[i] == 11) {
			found11 = true;
		}
		if (connections[i] == 12) {
			found12 = true;
		}
	}
	CHECK(found11);
	CHECK(found12);
}

void test_justamcp_notification_bus_broadcast_dedup() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPSessionManager *session_manager = server.test_get_session_manager();
	CHECK(session_manager != nullptr);
	session_manager->test_register_stream_for_session("session-dedup", 11);
	server.test_set_current_sse_connection_id(11);
	CHECK(server.test_count_notification_broadcast_targets() == 1);
	server.test_set_current_sse_connection_id(12);
	CHECK(server.test_count_notification_broadcast_targets() == 2);
}

void test_justamcp_active_task_route() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	server.test_register_task_route("task-route-1", "session-task", 77);
	String session_id;
	int connection_id = -1;
	CHECK(server.test_get_active_task_route("task-route-1", session_id, connection_id));
	CHECK(session_id == "session-task");
	CHECK(connection_id == 77);
	CHECK(!server.test_get_active_task_route("missing-task", session_id, connection_id));
}

#endif

#ifdef TOOLS_ENABLED

void test_justamcp_tool_dispatch_cancel_payload() {
#if defined(MODULE_HTTPSERVER_ENABLED)
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	Dictionary queue_full;
	MCPToolQueueEntry *entry = server.test_enqueue_tool_request(777, "blazium_logs_read", Dictionary(), queue_full);
	CHECK(entry != nullptr);
	entry->cancel_requested = true;
	CHECK(server.is_tool_cancel_requested(777));
	JustAMCPToolExecutor executor;
	server.test_clear_last_send_tool_result();
	JustAMCPToolDispatch::execute_and_send(&server, &executor, 777, "blazium_logs_read", Dictionary());
	CHECK(server.get_pending_tool_queue_size() == 0);
	const Dictionary rpc_result = server.test_peek_last_send_tool_result();
	CHECK(rpc_result.has("result"));
	const Dictionary result_dict = rpc_result["result"];
	CHECK(result_dict.get("isError", false));
	const Array content = result_dict["content"];
	CHECK(content.size() > 0);
	const Dictionary content_item = content[0];
	CHECK(String(content_item.get("text", "")) == "cancelled");
#else
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for tool dispatch cancel tests");
#endif
}

#endif
