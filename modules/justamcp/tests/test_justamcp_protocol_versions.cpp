/**************************************************************************/
/*  test_justamcp_protocol_versions.cpp                                   */
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

#include "test_justamcp_protocol_versions.h"

#include "test_justamcp_fixture.h"

#include "modules/modules_enabled.gen.h"

#if defined(MODULE_HTTPSERVER_ENABLED)

#include "../justamcp_json_rpc_transport.h"
#include "../justamcp_server.h"
#include "../justamcp_session_manager.h"

#include "core/io/json.h"
#include "tests/test_macros.h"

#include "modules/httpserver/http_request_context.h"
#include "modules/httpserver/http_response.h"

static const char *const k_supported_protocols[] = {
	"2025-11-25",
	"2025-06-18",
	"2025-03-26",
	"2024-11-05",
	nullptr
};

static Ref<HTTPRequestContext> _make_ctx(const String &p_method, const Dictionary &p_headers, const String &p_body = String()) {
	Ref<HTTPRequestContext> context;
	context.instantiate();
	context->set_method(p_method);
	context->set_path("/mcp");
	context->set_headers(p_headers);
	context->set_body(p_body);
	return context;
}

static String _response_header(const Ref<HTTPResponse> &p_response, const String &p_name) {
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

static Dictionary _json_headers(const String &p_session_id = String(), const String &p_protocol = String()) {
	Dictionary headers;
	headers["Accept"] = "application/json";
	headers["Content-Type"] = "application/json";
	if (!p_session_id.is_empty()) {
		headers["MCP-Session-Id"] = p_session_id;
	}
	if (!p_protocol.is_empty()) {
		headers["MCP-Protocol-Version"] = p_protocol;
	}
	return headers;
}

static String _initialize_body(const String &p_protocol, int p_id = 1) {
	return "{\"jsonrpc\":\"2.0\",\"id\":" + itos(p_id) + ",\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"" + p_protocol + "\",\"capabilities\":{},\"clientInfo\":{\"name\":\"protocol-test\",\"version\":\"1.0\"}}}";
}

static String _init_session(JustAMCPServer &p_server, MCPSessionManager *p_sm, const String &p_protocol) {
	Ref<HTTPResponse> response;
	response.instantiate();
	const bool handled = p_sm->handle_mcp_post(_make_ctx("POST", _json_headers(), _initialize_body(p_protocol)), response);
	CHECK(handled);
	const String session_id = _response_header(response, "MCP-Session-Id");
	CHECK(!session_id.is_empty());
	CHECK(p_sm->session_exists(session_id));
	CHECK(p_server.test_get_transport_negotiated_protocol() == p_protocol);
	if (!response->get_body().is_empty()) {
		Ref<JSON> json;
		json.instantiate();
		if (json->parse(response->get_body()) == OK && json->get_data().get_type() == Variant::DICTIONARY) {
			Dictionary root = json->get_data();
			if (root.has("result")) {
				Dictionary result = root["result"];
				CHECK(String(result.get("protocolVersion", "")) == p_protocol);
			}
		}
	}
	return session_id;
}

void test_justamcp_negotiate_protocol_versions() {
	for (int i = 0; k_supported_protocols[i]; i++) {
		const String version = k_supported_protocols[i];
		CHECK(MCPSessionManager::negotiate_protocol_version(version) == version);
	}
	CHECK(MCPSessionManager::latest_protocol_version() == "2025-11-25");
	CHECK(MCPSessionManager::negotiate_protocol_version("2099-01-01") == "2025-11-25");
	CHECK(MCPSessionManager::negotiate_protocol_version("") == "2025-11-25");
}

void test_justamcp_validate_protocol_header_rejects_unknown() {
	Dictionary ok_headers = _json_headers();
	ok_headers["MCP-Protocol-Version"] = "2025-06-18";
	String err;
	CHECK(MCPSessionManager::validate_protocol_header(_make_ctx("POST", ok_headers), err));

	Dictionary empty_headers = _json_headers();
	err = String();
	CHECK(MCPSessionManager::validate_protocol_header(_make_ctx("POST", empty_headers), err));

	Dictionary bad_headers = _json_headers();
	bad_headers["MCP-Protocol-Version"] = "1999-01-01";
	err = String();
	CHECK(!MCPSessionManager::validate_protocol_header(_make_ctx("POST", bad_headers), err));
	CHECK(err.contains("Unsupported MCP-Protocol-Version"));
}

void test_justamcp_http_initialize_all_protocol_versions() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPSessionManager *session_manager = server.test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	for (int i = 0; k_supported_protocols[i]; i++) {
		const String session_id = _init_session(server, session_manager, k_supported_protocols[i]);
		CHECK(!session_id.is_empty());
	}
}

void test_justamcp_http_protocol_header_falls_back_to_session() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPSessionManager *session_manager = server.test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	static const char *const strict[] = { "2025-06-18", "2025-11-25", nullptr };
	for (int i = 0; strict[i]; i++) {
		const String protocol = strict[i];
		const String session_id = _init_session(server, session_manager, protocol);

		const String list_body = "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/list\",\"params\":{}}";

		Ref<HTTPResponse> missing;
		missing.instantiate();
		CHECK(session_manager->handle_mcp_post(_make_ctx("POST", _json_headers(session_id), list_body), missing));
		CHECK(missing->get_status() != 400);
		CHECK(!String(missing->get_body()).contains("Missing MCP-Protocol-Version"));

		Ref<HTTPResponse> ok;
		ok.instantiate();
		CHECK(session_manager->handle_mcp_post(_make_ctx("POST", _json_headers(session_id, protocol), list_body), ok));
		CHECK(ok->get_status() != 400);
	}
}

void test_justamcp_http_protocol_header_optional_for_older_versions() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPSessionManager *session_manager = server.test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	static const char *const optional[] = { "2024-11-05", "2025-03-26", nullptr };
	for (int i = 0; optional[i]; i++) {
		const String protocol = optional[i];
		const String session_id = _init_session(server, session_manager, protocol);

		const String list_body = "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/list\",\"params\":{}}";
		Ref<HTTPResponse> response;
		response.instantiate();
		CHECK(session_manager->handle_mcp_post(_make_ctx("POST", _json_headers(session_id), list_body), response));
		CHECK(response->get_status() != 400);
		CHECK(!String(response->get_body()).contains("Missing MCP-Protocol-Version"));
	}
}

void test_justamcp_batch_rejected_for_newer_protocols() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();

	static const char *const strict[] = { "2025-06-18", "2025-11-25", nullptr };
	for (int i = 0; strict[i]; i++) {
		server.test_set_transport_negotiated_protocol(strict[i]);
		Dictionary result = JustAMCPJsonRpcTransport::handle_json_rpc(&server, "[{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}]", Ref<HTTPResponse>());
		CHECK(result.has("error"));
		CHECK(String(result["error"].operator Dictionary().get("message", "")).contains("batch"));
	}

	server.test_set_transport_negotiated_protocol("2024-11-05");
	Dictionary older = JustAMCPJsonRpcTransport::handle_json_rpc(&server, "[{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}]", Ref<HTTPResponse>());
	CHECK(older.has("error"));
	CHECK(!String(older["error"].operator Dictionary().get("message", "")).contains("batch requests are not supported"));
}

void test_justamcp_http_list_toolsets_smoke_per_strict_protocol() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPSessionManager *session_manager = server.test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	static const char *const strict[] = { "2025-06-18", "2025-11-25", nullptr };
	for (int i = 0; strict[i]; i++) {
		const String protocol = strict[i];
		const String session_id = _init_session(server, session_manager, protocol);
		const String call_body = "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\",\"params\":{\"name\":\"blazium_list_toolsets\",\"arguments\":{}}}";
		Ref<HTTPResponse> response;
		response.instantiate();
		CHECK(session_manager->handle_mcp_post(_make_ctx("POST", _json_headers(session_id, protocol), call_body), response));
		CHECK(response->get_status() != 400);
		CHECK(response->get_status() != 404);
	}
}

void test_justamcp_json_rpc_rejects_null_id() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();

	Dictionary null_id = JustAMCPJsonRpcTransport::handle_json_rpc(
			&server,
			"{\"jsonrpc\":\"2.0\",\"id\":null,\"method\":\"tools/list\",\"params\":{}}",
			Ref<HTTPResponse>());
	CHECK(null_id.has("error"));
	CHECK(int(Dictionary(null_id["error"]).get("code", 0)) == -32600);
	CHECK(String(Dictionary(null_id["error"]).get("message", "")).contains("must not be null"));

	Dictionary bad_id = JustAMCPJsonRpcTransport::handle_json_rpc(
			&server,
			"{\"jsonrpc\":\"2.0\",\"id\":true,\"method\":\"tools/list\",\"params\":{}}",
			Ref<HTTPResponse>());
	CHECK(bad_id.has("error"));
	CHECK(int(Dictionary(bad_id["error"]).get("code", 0)) == -32600);
	CHECK(String(Dictionary(bad_id["error"]).get("message", "")).contains("string or integer"));
}

#else

void test_justamcp_negotiate_protocol_versions() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}
void test_justamcp_validate_protocol_header_rejects_unknown() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}
void test_justamcp_http_initialize_all_protocol_versions() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}
void test_justamcp_http_protocol_header_falls_back_to_session() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}
void test_justamcp_http_protocol_header_optional_for_older_versions() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}
void test_justamcp_batch_rejected_for_newer_protocols() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}
void test_justamcp_http_list_toolsets_smoke_per_strict_protocol() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}
void test_justamcp_json_rpc_rejects_null_id() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}

#endif

#endif
