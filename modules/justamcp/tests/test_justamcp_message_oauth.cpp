/**************************************************************************/
/*  test_justamcp_message_oauth.cpp                                       */
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

#include "test_justamcp_message_oauth.h"
#include "test_justamcp_fixture.h"

#ifdef TESTS_ENABLED

#if defined(MODULE_HTTPSERVER_ENABLED)

#include "../justamcp_server.h"

#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "modules/httpserver/http_request_context.h"
#include "modules/httpserver/http_response.h"
#include "tests/test_macros.h"

static String _message_oauth_basic_auth_header(const String &p_id, const String &p_secret) {
	CharString raw = (p_id + ":" + p_secret).utf8();
	String b64 = CryptoCore::b64_encode_str((const uint8_t *)raw.get_data(), raw.length());
	return "Basic " + b64;
}

void test_justamcp_message_oauth_rejects_without_auth() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();

	ProjectSettings *ps = ProjectSettings::get_singleton();
	const bool prev_enabled = bool(ps->get_setting("blazium/justamcp/oauth_enabled", false));
	const String prev_id = ps->get_setting("blazium/justamcp/client_id", "");
	const String prev_secret = ps->get_setting("blazium/justamcp/client_secret", "");
	ps->set_setting("blazium/justamcp/oauth_enabled", true);
	ps->set_setting("blazium/justamcp/client_id", "test-client");
	ps->set_setting("blazium/justamcp/client_secret", "test-secret");

	Ref<HTTPRequestContext> ctx;
	ctx.instantiate();
	ctx->set_method("POST");
	ctx->set_path("/message");
	Dictionary headers;
	ctx->set_headers(headers);
	ctx->set_body("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}");
	Ref<HTTPResponse> response;
	response.instantiate();
	ERR_PRINT_OFF;
	server.test_handle_message_post(ctx, response);
	ERR_PRINT_ON;
	CHECK(response->get_status() == 401);

	ps->set_setting("blazium/justamcp/oauth_enabled", prev_enabled);
	ps->set_setting("blazium/justamcp/client_id", prev_id);
	ps->set_setting("blazium/justamcp/client_secret", prev_secret);
}

void test_justamcp_message_oauth_accepts_basic() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();

	ProjectSettings *ps = ProjectSettings::get_singleton();
	const bool prev_enabled = bool(ps->get_setting("blazium/justamcp/oauth_enabled", false));
	const String prev_id = ps->get_setting("blazium/justamcp/client_id", "");
	const String prev_secret = ps->get_setting("blazium/justamcp/client_secret", "");
	ps->set_setting("blazium/justamcp/oauth_enabled", true);
	ps->set_setting("blazium/justamcp/client_id", "test-client");
	ps->set_setting("blazium/justamcp/client_secret", "test-secret");

	Ref<HTTPRequestContext> ctx;
	ctx.instantiate();
	ctx->set_method("POST");
	ctx->set_path("/message");
	Dictionary headers;
	headers["Authorization"] = _message_oauth_basic_auth_header("test-client", "test-secret");
	ctx->set_headers(headers);
	ctx->set_body("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}");
	Ref<HTTPResponse> response;
	response.instantiate();
	server.test_handle_message_post(ctx, response);
	CHECK(response->get_status() != 401);

	ps->set_setting("blazium/justamcp/oauth_enabled", prev_enabled);
	ps->set_setting("blazium/justamcp/client_id", prev_id);
	ps->set_setting("blazium/justamcp/client_secret", prev_secret);
}

#else
void test_justamcp_message_oauth_rejects_without_auth() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}
void test_justamcp_message_oauth_accepts_basic() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}
#endif

#endif
