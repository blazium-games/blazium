/**************************************************************************/
/*  test_justamcp_oauth_discovery.cpp                                     */
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

#include "../justamcp_oauth_discovery.h"
#include "../justamcp_server.h"

#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "modules/httpserver/http_request_context.h"
#include "modules/httpserver/http_response.h"
#include "tests/test_macros.h"

static String _header_value(const Ref<HTTPResponse> &p_response, const String &p_name) {
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

void test_justamcp_oauth_401_www_authenticate() {
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
	ctx->set_path("/mcp");
	ctx->set_headers(Dictionary());
	Ref<HTTPResponse> response;
	response.instantiate();
	CHECK(!server.test_validate_mcp_oauth(ctx, response));
	CHECK(response->get_status() == 401);
	const String www = _header_value(response, "WWW-Authenticate");
	CHECK(www.contains("Bearer"));
	CHECK(www.contains("resource_metadata="));
	CHECK(www.contains("oauth-protected-resource"));

	ps->set_setting("blazium/justamcp/oauth_enabled", prev_enabled);
	ps->set_setting("blazium/justamcp/client_id", prev_id);
	ps->set_setting("blazium/justamcp/client_secret", prev_secret);
}

void test_justamcp_oauth_well_known_documents() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	ProjectSettings *ps = ProjectSettings::get_singleton();
	const bool prev_enabled = bool(ps->get_setting("blazium/justamcp/oauth_enabled", false));
	ps->set_setting("blazium/justamcp/oauth_enabled", true);

	Ref<HTTPRequestContext> ctx;
	ctx.instantiate();
	ctx->set_method("GET");
	ctx->set_path("/.well-known/oauth-protected-resource/mcp");
	Ref<HTTPResponse> prm;
	prm.instantiate();
	server.test_handle_oauth_protected_resource(ctx, prm);
	CHECK(prm->get_status() == 200);
	Ref<JSON> json;
	json.instantiate();
	CHECK(json->parse(prm->get_body()) == OK);
	Dictionary prm_body = json->get_data();
	CHECK(prm_body.has("resource"));
	CHECK(prm_body.has("authorization_servers"));
	CHECK(prm_body.has("scopes_supported"));
	CHECK(prm_body.has("bearer_methods_supported"));

	Ref<HTTPResponse> as;
	as.instantiate();
	server.test_handle_oauth_authorization_server(ctx, as);
	CHECK(as->get_status() == 200);
	CHECK(json->parse(as->get_body()) == OK);
	Dictionary as_body = json->get_data();
	CHECK(as_body.has("issuer"));
	CHECK(as_body.has("token_endpoint"));

	Dictionary meta = JustAMCPOauthDiscovery::protected_resource_metadata();
	CHECK(String(meta.get("resource", "")).ends_with("/mcp"));

	ps->set_setting("blazium/justamcp/oauth_enabled", false);
	Ref<HTTPResponse> missing;
	missing.instantiate();
	server.test_handle_oauth_protected_resource(ctx, missing);
	CHECK(missing->get_status() == 404);

	ps->set_setting("blazium/justamcp/oauth_enabled", prev_enabled);
}

void test_justamcp_oauth_cimd_client_id() {
	CHECK(JustAMCPOauthDiscovery::client_id_is_cimd("https://example.test/client.json"));
	CHECK(!JustAMCPOauthDiscovery::client_id_is_cimd("cursor-local"));

	ProjectSettings *ps = ProjectSettings::get_singleton();
	const String prev = ps->get_setting("blazium/justamcp/oauth_cimd_json", "");
	ps->set_setting("blazium/justamcp/oauth_cimd_json", "{\"client_id\":\"https://example.test/client.json\"}");
	String err;
	CHECK(JustAMCPOauthDiscovery::validate_cimd_client_id("https://example.test/client.json", err));
	CHECK(!JustAMCPOauthDiscovery::validate_cimd_client_id("https://other.test/client.json", err));
	ps->set_setting("blazium/justamcp/oauth_cimd_json", prev);
}

#else

void test_justamcp_oauth_401_www_authenticate() {}
void test_justamcp_oauth_well_known_documents() {}
void test_justamcp_oauth_cimd_client_id() {}

#endif

#endif
