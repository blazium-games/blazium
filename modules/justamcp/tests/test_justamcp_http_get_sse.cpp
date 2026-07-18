/**************************************************************************/
/*  test_justamcp_http_get_sse.cpp                                        */
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

#include "test_justamcp_http_get_sse.h"
#include "test_justamcp_fixture.h"

#ifdef TESTS_ENABLED

#if defined(MODULE_HTTPSERVER_ENABLED)

#include "../justamcp_server.h"
#include "../justamcp_session_manager.h"
#include "modules/httpserver/http_request_context.h"
#include "modules/httpserver/http_response.h"
#include "tests/test_macros.h"

void test_justamcp_http_get_sse_registers_stream() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPSessionManager *session_manager = server.test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	session_manager->test_register_stream_for_session("get-sse-session", 501);
	CHECK(session_manager->session_exists("get-sse-session"));

	Ref<HTTPRequestContext> context;
	context.instantiate();
	context->set_method("GET");
	context->set_path("/mcp");
	Dictionary headers;
	headers["MCP-Session-Id"] = "get-sse-session";
	headers["MCP-Protocol-Version"] = "2024-11-05";
	headers["Accept"] = "text/event-stream";
	context->set_headers(headers);

	Ref<HTTPResponse> response;
	response.instantiate();
	CHECK(session_manager->handle_mcp_get(context, response));
	CHECK(response->is_sse_response());
}

void test_justamcp_http_get_sse_rejects_missing_session() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPSessionManager *session_manager = server.test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	Ref<HTTPRequestContext> context;
	context.instantiate();
	context->set_method("GET");
	context->set_path("/mcp");
	Dictionary headers;
	headers["MCP-Protocol-Version"] = "2024-11-05";
	headers["Accept"] = "text/event-stream";
	context->set_headers(headers);

	Ref<HTTPResponse> response;
	response.instantiate();
	CHECK(session_manager->handle_mcp_get(context, response));
	CHECK(response->get_status() == 400);
}

#else
void test_justamcp_http_get_sse_registers_stream() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for HTTP GET SSE tests");
}
void test_justamcp_http_get_sse_rejects_missing_session() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for HTTP GET SSE tests");
}
#endif

#endif
