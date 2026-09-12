/**************************************************************************/
/*  test_justamcp_session_survives_stream_close.cpp                       */
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

#include "test_justamcp_session_survives_stream_close.h"
#include "test_justamcp_fixture.h"

#ifdef TESTS_ENABLED

#if defined(MODULE_HTTPSERVER_ENABLED)

#include "../justamcp_server.h"
#include "../justamcp_session_manager.h"

#include "modules/httpserver/http_request_context.h"
#include "modules/httpserver/http_response.h"
#include "tests/test_macros.h"

void test_justamcp_session_survives_stream_close() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPSessionManager *session_manager = server.test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	session_manager->test_register_stream_for_session("survive-session", 801);
	CHECK(session_manager->session_exists("survive-session"));
	session_manager->on_sse_connection_closed(801);
	CHECK(session_manager->session_exists("survive-session"));

	Dictionary headers;
	headers["MCP-Session-Id"] = "survive-session";
	Ref<HTTPRequestContext> context;
	context.instantiate();
	context->set_method("DELETE");
	context->set_path("/mcp");
	context->set_headers(headers);
	Ref<HTTPResponse> del;
	del.instantiate();
	CHECK(session_manager->handle_mcp_delete(context, del));
	CHECK(!session_manager->session_exists("survive-session"));
}

#else
void test_justamcp_session_survives_stream_close() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}
#endif

#endif
