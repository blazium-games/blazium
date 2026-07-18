/**************************************************************************/
/*  test_justamcp_delete_origin.cpp                                       */
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

#include "test_justamcp_delete_origin.h"
#include "test_justamcp_fixture.h"

#ifdef TESTS_ENABLED

#if defined(MODULE_HTTPSERVER_ENABLED)

#include "../justamcp_server.h"
#include "../justamcp_session_manager.h"

#include "core/config/project_settings.h"
#include "modules/httpserver/http_request_context.h"
#include "modules/httpserver/http_response.h"
#include "tests/test_macros.h"

void test_justamcp_delete_origin() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPSessionManager *session_manager = server.test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	ProjectSettings *ps = ProjectSettings::get_singleton();
	const bool prev_strict = bool(ps->get_setting("blazium/justamcp/streamable_http_strict_origin", true));
	const bool prev_allow_delete = bool(ps->get_setting("blazium/justamcp/session_allow_client_delete", true));
	ps->set_setting("blazium/justamcp/streamable_http_strict_origin", true);
	ps->set_setting("blazium/justamcp/session_allow_client_delete", true);

	session_manager->test_register_stream_for_session("delete-origin-session", 9101);

	Dictionary bad_headers;
	bad_headers["MCP-Session-Id"] = "delete-origin-session";
	bad_headers["Origin"] = "http://127.0.0.1.evil";
	Ref<HTTPRequestContext> bad_ctx;
	bad_ctx.instantiate();
	bad_ctx->set_method("DELETE");
	bad_ctx->set_path("/mcp");
	bad_ctx->set_headers(bad_headers);
	Ref<HTTPResponse> bad_response;
	bad_response.instantiate();
	CHECK(session_manager->handle_mcp_delete(bad_ctx, bad_response));
	CHECK(bad_response->get_status() == 403);
	CHECK(session_manager->session_exists("delete-origin-session"));

	Dictionary good_headers;
	good_headers["MCP-Session-Id"] = "delete-origin-session";
	good_headers["Origin"] = "http://127.0.0.1";
	Ref<HTTPRequestContext> good_ctx;
	good_ctx.instantiate();
	good_ctx->set_method("DELETE");
	good_ctx->set_path("/mcp");
	good_ctx->set_headers(good_headers);
	Ref<HTTPResponse> good_response;
	good_response.instantiate();
	CHECK(session_manager->handle_mcp_delete(good_ctx, good_response));
	CHECK(good_response->get_status() == 200);
	CHECK(!session_manager->session_exists("delete-origin-session"));

	ps->set_setting("blazium/justamcp/streamable_http_strict_origin", prev_strict);
	ps->set_setting("blazium/justamcp/session_allow_client_delete", prev_allow_delete);
}

#else
void test_justamcp_delete_origin() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}
#endif

#endif
