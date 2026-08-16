/**************************************************************************/
/*  test_justamcp_legacy_message.cpp                                      */
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

#include "test_justamcp_legacy_message.h"

#include "test_justamcp_fixture.h"

#ifdef TESTS_ENABLED

#if defined(MODULE_HTTPSERVER_ENABLED)

#include "../justamcp_json_rpc_transport.h"
#include "../justamcp_server.h"

#include "core/io/json.h"
#include "tests/test_macros.h"

#include "modules/httpserver/http_request_context.h"
#include "modules/httpserver/http_response.h"

void test_justamcp_legacy_message_routes_result() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	server.test_set_current_sse_connection_id(77);
	server.test_clear_last_send_tool_result();

	Ref<HTTPRequestContext> context;
	context.instantiate();
	context->set_method("POST");
	context->set_path("/message");
	Dictionary query;
	query["sessionId"] = "77";
	context->set_query_params(query);
	const String body = "{\"jsonrpc\":\"2.0\",\"id\":9,\"method\":\"ping\"}";
	context->set_body(body);

	Ref<HTTPResponse> response;
	response.instantiate();
	server.test_handle_message_post(context, response);
	CHECK(response->get_status() == 202);
}

void test_justamcp_tasks_result_rejects_wait_on_http() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	const String body = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tasks/result\",\"params\":{\"taskId\":\"task-1\",\"wait\":true}}";
	Ref<HTTPResponse> response;
	response.instantiate();
	const Dictionary result = JustAMCPJsonRpcTransport::handle_json_rpc(&server, body, response);
	CHECK(result.has("error"));
	const Dictionary error_dict = result["error"];
	CHECK(int(error_dict.get("code", 0)) == -32602);
}

#else
void test_justamcp_legacy_message_routes_result() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for legacy message tests");
}
void test_justamcp_tasks_result_rejects_wait_on_http() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for legacy message tests");
}
#endif

#endif
