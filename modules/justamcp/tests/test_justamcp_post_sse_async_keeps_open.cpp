/**************************************************************************/
/*  test_justamcp_post_sse_async_keeps_open.cpp                           */
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

#include "test_justamcp_post_sse_async_keeps_open.h"

#include "test_justamcp_fixture.h"

#ifdef TESTS_ENABLED

#if defined(MODULE_HTTPSERVER_ENABLED)

#include "../justamcp_json_rpc_transport.h"
#include "../justamcp_server.h"
#include "../justamcp_session_manager.h"

#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "core/object/message_queue.h"
#include "tests/test_macros.h"

#include "modules/httpserver/http_request_context.h"
#include "modules/httpserver/http_response.h"

static Ref<HTTPRequestContext> _post_sse_async_make_ctx(const String &p_method, const Dictionary &p_headers, const String &p_body = String()) {
	Ref<HTTPRequestContext> context;
	context.instantiate();
	context->set_method(p_method);
	context->set_path("/mcp");
	context->set_headers(p_headers);
	context->set_body(p_body);
	return context;
}

static Dictionary _post_sse_async_headers(const String &p_session_id = String()) {
	Dictionary headers;
	headers["Accept"] = "application/json, text/event-stream";
	headers["Content-Type"] = "application/json";
	if (!p_session_id.is_empty()) {
		headers["MCP-Session-Id"] = p_session_id;
	}
	return headers;
}

static String _post_sse_async_header(const Ref<HTTPResponse> &p_response, const String &p_name) {
	const Dictionary headers = p_response->get_headers();
	if (headers.has(p_name)) {
		return String(headers[p_name]);
	}
	const String lower = p_name.to_lower();
	for (const Variant &key : headers.keys()) {
		if (String(key).to_lower() == lower) {
			return String(headers[key]);
		}
	}
	return String();
}

void test_justamcp_post_sse_async_keeps_open() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPSessionManager *session_manager = server.test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	const int prev_rate = int(GLOBAL_GET("blazium/justamcp/max_enqueue_per_sec_per_session"));
	ProjectSettings::get_singleton()->set_setting("blazium/justamcp/max_enqueue_per_sec_per_session", 0);

	const String init_body = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{},\"clientInfo\":{\"name\":\"test\",\"version\":\"1.0\"}}}";
	Ref<HTTPResponse> init_response;
	init_response.instantiate();
	CHECK(session_manager->handle_mcp_post(_post_sse_async_make_ctx("POST", _post_sse_async_headers(), init_body), init_response));
	const String session_id = _post_sse_async_header(init_response, "MCP-Session-Id");
	CHECK(!session_id.is_empty());

	const String tool_body = "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"blazium_logs_read\",\"arguments\":{}}}";
	Ref<HTTPResponse> sse;
	sse.instantiate();
	CHECK(session_manager->handle_mcp_post(_post_sse_async_make_ctx("POST", _post_sse_async_headers(session_id), tool_body), sse));
	CHECK(sse->is_sse_response());
	CHECK(sse->get_status() != 409);

	Dictionary open_headers;
	open_headers["MCP-Session-Id"] = session_id;
	open_headers["Accept"] = "application/json, text/event-stream";
	const int conn_id = 7101;
	session_manager->on_sse_connection_opened(conn_id, "/mcp", open_headers);

	CHECK(session_manager->is_post_stream_connection(conn_id));
	CHECK(session_manager->session_exists(session_id));

	MessageQueue::get_singleton()->flush();
	CHECK(session_manager->session_exists(session_id));

	const Dictionary async_result = JustAMCPJsonRpcTransport::handle_json_rpc_parsed(&server, JSON::parse_string(tool_body), Ref<HTTPResponse>());
	CHECK(async_result.is_empty());
	CHECK(server.get_pending_tool_queue_size() >= 1);

	ProjectSettings::get_singleton()->set_setting("blazium/justamcp/max_enqueue_per_sec_per_session", prev_rate);
}

#else
void test_justamcp_post_sse_async_keeps_open() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}
#endif

#endif
