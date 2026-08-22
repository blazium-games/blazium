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
#include "../tools/justamcp_json_rpc_router.h"

#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "tests/test_macros.h"

#include "modules/httpserver/http_request_context.h"
#include "modules/httpserver/http_response.h"

static const char *const k_legacy_protocols[] = {
	"2025-11-25",
	"2025-06-18",
	"2025-03-26",
	"2024-11-05",
	nullptr
};

static const char *const k_modern_protocol = "2026-07-28";

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

static Dictionary _parse_json_body(const Ref<HTTPResponse> &p_response) {
	if (p_response.is_null() || p_response->get_body().is_empty()) {
		return Dictionary();
	}
	Ref<JSON> json;
	json.instantiate();
	if (json->parse(p_response->get_body()) != OK || json->get_data().get_type() != Variant::DICTIONARY) {
		return Dictionary();
	}
	return json->get_data();
}

static Dictionary _modern_headers(const String &p_method, const String &p_name = String()) {
	Dictionary headers;
	headers["Accept"] = "application/json";
	headers["Content-Type"] = "application/json";
	headers["MCP-Protocol-Version"] = k_modern_protocol;
	headers["Mcp-Method"] = p_method;
	if (!p_name.is_empty()) {
		headers["Mcp-Name"] = p_name;
	}
	return headers;
}

static String _modern_body(const String &p_method, int p_id = 1, const String &p_params = "{}") {
	return "{\"jsonrpc\":\"2.0\",\"id\":" + itos(p_id) + ",\"method\":\"" + p_method + "\",\"params\":" + p_params + "}";
}

void test_justamcp_negotiate_protocol_versions() {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	TEST_FAIL_COND(ps == nullptr, "ProjectSettings is required");
	const bool prev_override = bool(ps->get_setting("blazium/justamcp/override_editor_settings", false));
	const String prev_version = String(ps->get_setting("blazium/justamcp/protocol_version", k_modern_protocol));
	const String prev_accepted = String(ps->get_setting("blazium/justamcp/accepted_protocol_versions", ""));
	MCPSessionManager::clear_cli_protocol_version_override();
	ps->set_setting("blazium/justamcp/override_editor_settings", true);
	ps->set_setting("blazium/justamcp/protocol_version", k_modern_protocol);
	ps->set_setting("blazium/justamcp/accepted_protocol_versions", "");

	for (int i = 0; k_legacy_protocols[i]; i++) {
		const String version = k_legacy_protocols[i];
		CHECK(MCPSessionManager::is_legacy_protocol_version(version));
		CHECK(MCPSessionManager::negotiate_protocol_version(version) == version);
	}
	CHECK(MCPSessionManager::is_supported_protocol_version(k_modern_protocol));
	CHECK(MCPSessionManager::is_modern_protocol_version(k_modern_protocol));
	CHECK(MCPSessionManager::latest_protocol_version() == k_modern_protocol);
	CHECK(MCPSessionManager::negotiate_protocol_version(k_modern_protocol) == k_modern_protocol);
	CHECK(MCPSessionManager::negotiate_legacy_initialize(k_modern_protocol) == "2025-11-25");
	CHECK(MCPSessionManager::negotiate_legacy_initialize("2025-11-25") == "2025-11-25");
	CHECK(MCPSessionManager::negotiate_protocol_version("2099-01-01") == k_modern_protocol);
	CHECK(MCPSessionManager::negotiate_protocol_version("") == k_modern_protocol);

	ps->set_setting("blazium/justamcp/accepted_protocol_versions", prev_accepted);
	ps->set_setting("blazium/justamcp/protocol_version", prev_version);
	ps->set_setting("blazium/justamcp/override_editor_settings", prev_override);
}

void test_justamcp_protocol_version_setting_and_cli() {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	TEST_FAIL_COND(ps == nullptr, "ProjectSettings is required");

	const bool prev_override = bool(ps->get_setting("blazium/justamcp/override_editor_settings", false));
	const String prev_version = String(ps->get_setting("blazium/justamcp/protocol_version", "2025-11-25"));
	MCPSessionManager::clear_cli_protocol_version_override();
	ps->set_setting("blazium/justamcp/override_editor_settings", true);
	ps->set_setting("blazium/justamcp/protocol_version", "2024-11-05");
	CHECK(MCPSessionManager::latest_protocol_version() == "2024-11-05");
	CHECK(MCPSessionManager::negotiate_protocol_version("2099-01-01") == "2024-11-05");

	CHECK(MCPSessionManager::set_cli_protocol_version_override("2025-03-26"));
	CHECK(MCPSessionManager::latest_protocol_version() == "2025-03-26");
	CHECK(!MCPSessionManager::set_cli_protocol_version_override("not-a-version"));
	CHECK(MCPSessionManager::latest_protocol_version() == "2025-03-26");

	MCPSessionManager::clear_cli_protocol_version_override();
	ps->set_setting("blazium/justamcp/protocol_version", "bogus");
	ERR_PRINT_OFF;
	CHECK(MCPSessionManager::latest_protocol_version() == k_modern_protocol);
	ERR_PRINT_ON;

	ps->set_setting("blazium/justamcp/protocol_version", prev_version);
	ps->set_setting("blazium/justamcp/override_editor_settings", prev_override);
}

void test_justamcp_initialize_result_shape() {
	JustAMCPTestServerFixture fixture;
	Dictionary payload;
	Dictionary params;
	params["protocolVersion"] = "2025-11-25";
	params["capabilities"] = Dictionary();
	payload["params"] = params;

	Dictionary routed = JustAMCPJsonRpcRouter::route_initialize(&fixture.get_server(), payload, 1);
	CHECK(bool(routed.get("handled", false)));
	CHECK(routed.has("result"));
	Dictionary result = routed["result"];
	CHECK(result.has("instructions"));
	CHECK(result.has("capabilities"));
	CHECK(result.has("serverInfo"));
	Dictionary capabilities = result["capabilities"];
	CHECK(capabilities.has("completions"));
	CHECK(capabilities.has("tools"));
	CHECK(capabilities.has("elicitation"));
	Dictionary elicitation = capabilities["elicitation"];
	CHECK(elicitation.has("form"));
	CHECK(elicitation.has("url"));
	Dictionary server_info = result["serverInfo"];
	CHECK(!server_info.has("instructions"));
	CHECK(String(server_info.get("name", "")) == "blazium-mcp-server");
	CHECK(String(server_info.get("title", "")) == "Blazium MCP");
	CHECK(String(server_info.get("websiteUrl", "")) == "https://blazium.app");
}

void test_justamcp_validate_protocol_header_rejects_unknown() {
	Dictionary ok_headers = _json_headers();
	ok_headers["MCP-Protocol-Version"] = "2025-06-18";
	String err;
	CHECK(MCPSessionManager::validate_protocol_header(_make_ctx("POST", ok_headers), err));

	Dictionary modern_headers = _json_headers();
	modern_headers["MCP-Protocol-Version"] = k_modern_protocol;
	err = String();
	CHECK(MCPSessionManager::validate_protocol_header(_make_ctx("POST", modern_headers), err));

	Dictionary empty_headers = _json_headers();
	err = String();
	CHECK(MCPSessionManager::validate_protocol_header(_make_ctx("POST", empty_headers), err));

	Dictionary bad_headers = _json_headers();
	bad_headers["MCP-Protocol-Version"] = "1999-01-01";
	err = String();
	CHECK(!MCPSessionManager::validate_protocol_header(_make_ctx("POST", bad_headers), err));
	CHECK(err.contains("Unsupported MCP-Protocol-Version"));

	Dictionary combined_headers = _json_headers();
	combined_headers["MCP-Protocol-Version"] = "2025-11-25, 2025-11-25";
	err = String();
	CHECK(MCPSessionManager::validate_protocol_header(_make_ctx("POST", combined_headers), err));

	Dictionary mixed_headers = _json_headers();
	mixed_headers["MCP-Protocol-Version"] = "2025-11-25, 1999-01-01";
	err = String();
	CHECK(!MCPSessionManager::validate_protocol_header(_make_ctx("POST", mixed_headers), err));
	CHECK(err.contains("Unsupported MCP-Protocol-Version"));
}

void test_justamcp_http_initialize_all_protocol_versions() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPSessionManager *session_manager = server.test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	for (int i = 0; k_legacy_protocols[i]; i++) {
		const String session_id = _init_session(server, session_manager, k_legacy_protocols[i]);
		CHECK(!session_id.is_empty());
	}

	Dictionary combined = _json_headers();
	combined["MCP-Protocol-Version"] = "2025-11-25, 2025-11-25";
	Ref<HTTPResponse> combined_response;
	combined_response.instantiate();
	CHECK(session_manager->handle_mcp_post(_make_ctx("POST", combined, _initialize_body("2025-11-25", 99)), combined_response));
	CHECK(combined_response->get_status() == 200);
	CHECK(!_response_header(combined_response, "MCP-Session-Id").is_empty());
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
	CHECK(String(older["error"].operator Dictionary().get("message", "")).contains("batch"));
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

void test_justamcp_http_modern_discover_and_list() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPSessionManager *session_manager = server.test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	Ref<HTTPResponse> discover;
	discover.instantiate();
	CHECK(session_manager->handle_mcp_post(_make_ctx("POST", _modern_headers("server/discover"), _modern_body("server/discover")), discover));
	CHECK(discover->get_status() == 200);
	CHECK(_response_header(discover, "MCP-Session-Id").is_empty());
	Dictionary discover_root = _parse_json_body(discover);
	CHECK(discover_root.has("result"));
	Dictionary discover_result = discover_root["result"];
	CHECK(String(discover_result.get("resultType", "")) == "complete");
	CHECK(discover_result.has("supportedVersions"));
	CHECK(discover_result.has("ttlMs"));
	CHECK(String(discover_result.get("cacheScope", "")) == "public");
	Array supported = discover_result["supportedVersions"];
	bool saw_modern = false;
	bool saw_legacy = false;
	for (int i = 0; i < supported.size(); i++) {
		if (String(supported[i]) == k_modern_protocol) {
			saw_modern = true;
		}
		if (String(supported[i]) == "2025-11-25") {
			saw_legacy = true;
		}
	}
	CHECK(saw_modern);
	CHECK(saw_legacy);

	Ref<HTTPResponse> list;
	list.instantiate();
	CHECK(session_manager->handle_mcp_post(_make_ctx("POST", _modern_headers("tools/list"), _modern_body("tools/list", 2)), list));
	CHECK(list->get_status() == 200);
	CHECK(_response_header(list, "MCP-Session-Id").is_empty());
	Dictionary list_root = _parse_json_body(list);
	CHECK(list_root.has("result"));
	Dictionary list_result = list_root["result"];
	CHECK(String(list_result.get("resultType", "")) == "complete");
	CHECK(list_result.has("ttlMs"));
	CHECK(String(list_result.get("cacheScope", "")) == "public");
}

void test_justamcp_http_modern_header_mismatch_and_unsupported() {
	JustAMCPTestServerFixture fixture;
	MCPSessionManager *session_manager = fixture.get_server().test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	Ref<HTTPResponse> mismatch;
	mismatch.instantiate();
	CHECK(session_manager->handle_mcp_post(_make_ctx("POST", _modern_headers("tools/list"), _modern_body("ping")), mismatch));
	CHECK(mismatch->get_status() == 400);
	Dictionary mismatch_root = _parse_json_body(mismatch);
	CHECK(mismatch_root.has("error"));
	CHECK(int(Dictionary(mismatch_root["error"]).get("code", 0)) == -32020);

	Dictionary bad_headers = _json_headers();
	bad_headers["MCP-Protocol-Version"] = "2027-01-01";
	bad_headers["Mcp-Method"] = "server/discover";
	Ref<HTTPResponse> unsupported;
	unsupported.instantiate();
	CHECK(session_manager->handle_mcp_post(_make_ctx("POST", bad_headers, _modern_body("server/discover", 3)), unsupported));
	CHECK(unsupported->get_status() == 400);
	Dictionary unsupported_root = _parse_json_body(unsupported);
	CHECK(unsupported_root.has("error"));
	CHECK(int(Dictionary(unsupported_root["error"]).get("code", 0)) == -32022);
	Dictionary data = Dictionary(unsupported_root["error"]).get("data", Dictionary());
	CHECK(String(data.get("requested", "")) == "2027-01-01");
	CHECK(data.has("supported"));
}

void test_justamcp_http_initialize_modern_client_stays_legacy() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPSessionManager *session_manager = server.test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	Ref<HTTPResponse> response;
	response.instantiate();
	CHECK(session_manager->handle_mcp_post(_make_ctx("POST", _json_headers(), _initialize_body(k_modern_protocol)), response));
	CHECK(response->get_status() == 200);
	CHECK(!_response_header(response, "MCP-Session-Id").is_empty());
	CHECK(server.test_get_transport_negotiated_protocol() == "2025-11-25");
	Dictionary root = _parse_json_body(response);
	if (root.has("result")) {
		CHECK(String(Dictionary(root["result"]).get("protocolVersion", "")) == "2025-11-25");
	}
}

void test_justamcp_http_modern_get_delete_and_listen() {
	JustAMCPTestServerFixture fixture;
	MCPSessionManager *session_manager = fixture.get_server().test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	Ref<HTTPResponse> get_no_session;
	get_no_session.instantiate();
	Dictionary get_headers;
	get_headers["Accept"] = "text/event-stream";
	CHECK(session_manager->handle_mcp_get(_make_ctx("GET", get_headers), get_no_session));
	CHECK(get_no_session->get_status() == 400);

	Ref<HTTPResponse> get_modern;
	get_modern.instantiate();
	Dictionary modern_get;
	modern_get["Accept"] = "text/event-stream";
	modern_get["MCP-Protocol-Version"] = k_modern_protocol;
	CHECK(session_manager->handle_mcp_get(_make_ctx("GET", modern_get), get_modern));
	CHECK(get_modern->get_status() == 405);

	Ref<HTTPResponse> delete_modern;
	delete_modern.instantiate();
	Dictionary modern_delete;
	modern_delete["MCP-Protocol-Version"] = k_modern_protocol;
	CHECK(session_manager->handle_mcp_delete(_make_ctx("DELETE", modern_delete), delete_modern));
	CHECK(delete_modern->get_status() == 405);

	Ref<HTTPResponse> listen;
	listen.instantiate();
	CHECK(session_manager->handle_mcp_post(_make_ctx("POST", _modern_headers("subscriptions/listen"), _modern_body("subscriptions/listen", 8)), listen));
	CHECK(listen->is_sse_response());
	CHECK(_response_header(listen, "MCP-Session-Id").is_empty());
}

void test_justamcp_accepted_protocol_versions_pinning() {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	TEST_FAIL_COND(ps == nullptr, "ProjectSettings is required");
	const bool prev_override = bool(ps->get_setting("blazium/justamcp/override_editor_settings", false));
	const String prev_accepted = String(ps->get_setting("blazium/justamcp/accepted_protocol_versions", ""));
	MCPSessionManager::clear_cli_protocol_version_override();
	ps->set_setting("blazium/justamcp/override_editor_settings", true);
	ps->set_setting("blazium/justamcp/accepted_protocol_versions", "2025-11-25");

	JustAMCPTestServerFixture fixture;
	MCPSessionManager *session_manager = fixture.get_server().test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	CHECK(MCPSessionManager::is_accepted_protocol_version("2025-11-25"));
	CHECK(!MCPSessionManager::is_accepted_protocol_version(k_modern_protocol));

	Ref<HTTPResponse> response;
	response.instantiate();
	CHECK(session_manager->handle_mcp_post(_make_ctx("POST", _modern_headers("server/discover"), _modern_body("server/discover")), response));
	CHECK(response->get_status() == 400);
	Dictionary root = _parse_json_body(response);
	CHECK(root.has("error"));
	CHECK(int(Dictionary(root["error"]).get("code", 0)) == -32022);

	ps->set_setting("blazium/justamcp/accepted_protocol_versions", prev_accepted);
	ps->set_setting("blazium/justamcp/override_editor_settings", prev_override);
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
void test_justamcp_protocol_version_setting_and_cli() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}
void test_justamcp_initialize_result_shape() {
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
void test_justamcp_http_modern_discover_and_list() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}
void test_justamcp_http_modern_header_mismatch_and_unsupported() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}
void test_justamcp_http_initialize_modern_client_stays_legacy() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}
void test_justamcp_http_modern_get_delete_and_listen() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}
void test_justamcp_accepted_protocol_versions_pinning() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}

#endif

#endif
