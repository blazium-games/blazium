/**************************************************************************/
/*  test_justamcp_http_integration.cpp                                    */
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

#include "test_justamcp_http_integration.h"

#include "test_justamcp_fixture.h"

#ifdef TESTS_ENABLED

#if defined(MODULE_HTTPSERVER_ENABLED)

#include "../justamcp_server.h"
#include "../justamcp_session_manager.h"

#ifdef TOOLS_ENABLED
#include "../justamcp_editor_plugin.h"
#endif

#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "tests/test_macros.h"

#include "modules/httpserver/http_request_context.h"
#include "modules/httpserver/http_response.h"

static Ref<HTTPRequestContext> _make_test_context(const String &p_method, const Dictionary &p_headers, const String &p_body = String()) {
	Ref<HTTPRequestContext> context;
	context.instantiate();
	context->set_method(p_method);
	context->set_path("/mcp");
	context->set_headers(p_headers);
	context->set_body(p_body);
	return context;
}

static Dictionary _streamable_headers(const String &p_session_id = String()) {
	Dictionary headers;
	headers["Accept"] = "application/json, text/event-stream";
	headers["Content-Type"] = "application/json";
	if (!p_session_id.is_empty()) {
		headers["MCP-Session-Id"] = p_session_id;
	}
	return headers;
}

static String _get_response_header(const Ref<HTTPResponse> &p_response, const String &p_name) {
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

void test_justamcp_http_initialize_creates_session() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPSessionManager *session_manager = server.test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	const String body = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{},\"clientInfo\":{\"name\":\"test\",\"version\":\"1.0\"}}}";
	Ref<HTTPResponse> response;
	response.instantiate();
	const bool handled = session_manager->handle_mcp_post(_make_test_context("POST", _streamable_headers(), body), response);
	CHECK(handled);
	const int status = response->get_status();
	const bool acceptable_status = status == 200 ? true : status == 202;
	CHECK(acceptable_status);
	const String session_header = _get_response_header(response, "MCP-Session-Id");
	CHECK(!session_header.is_empty());
	CHECK(session_manager->session_exists(session_header));
}

void test_justamcp_http_initialize_json_only_accept_creates_session() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPSessionManager *session_manager = server.test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	Dictionary headers;
	headers["Accept"] = "application/json";
	headers["Content-Type"] = "application/json";

	const String body = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{},\"clientInfo\":{\"name\":\"json-only\",\"version\":\"1.0\"}}}";
	Ref<HTTPResponse> response;
	response.instantiate();
	const bool handled = session_manager->handle_mcp_post(_make_test_context("POST", headers, body), response);
	CHECK(handled);
	const String session_header = _get_response_header(response, "MCP-Session-Id");
	CHECK(!session_header.is_empty());
	CHECK(session_manager->session_exists(session_header));
}

void test_justamcp_http_post_sse_conflict_409() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPSessionManager *session_manager = server.test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	const String init_body = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{},\"clientInfo\":{\"name\":\"test\",\"version\":\"1.0\"}}}";
	Ref<HTTPResponse> init_response;
	init_response.instantiate();
	CHECK(session_manager->handle_mcp_post(_make_test_context("POST", _streamable_headers(), init_body), init_response));
	const String session_id = _get_response_header(init_response, "MCP-Session-Id");
	CHECK(!session_id.is_empty());

	const String tool_body = "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"blazium_logs_read\",\"arguments\":{}}}";
	Ref<HTTPResponse> first_sse;
	first_sse.instantiate();
	CHECK(session_manager->handle_mcp_post(_make_test_context("POST", _streamable_headers(session_id), tool_body), first_sse));
	CHECK(first_sse->get_status() != 409);

	Ref<HTTPResponse> second_sse;
	second_sse.instantiate();
	CHECK(session_manager->handle_mcp_post(_make_test_context("POST", _streamable_headers(session_id), tool_body), second_sse));
	CHECK(second_sse->get_status() == 409);
}

void test_justamcp_http_delete_teardown() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPSessionManager *session_manager = server.test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	session_manager->test_register_stream_for_session("delete-http-session", 901);
	CHECK(session_manager->session_exists("delete-http-session"));

	Dictionary headers;
	headers["MCP-Session-Id"] = "delete-http-session";
	Ref<HTTPResponse> response;
	response.instantiate();
	CHECK(session_manager->handle_mcp_delete(_make_test_context("DELETE", headers), response));
	CHECK(response->get_status() == 200);
	CHECK(!session_manager->session_exists("delete-http-session"));
}

void test_justamcp_http_cors_preflight() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPSessionManager *session_manager = server.test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	Dictionary headers;
	headers["Origin"] = "http://127.0.0.1";
	Ref<HTTPResponse> response;
	response.instantiate();
	session_manager->handle_cors_preflight(_make_test_context("OPTIONS", headers), response);
	CHECK(response->get_status() == 204);
}

void test_justamcp_http_stateless_empty_body_400() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	Dictionary headers;
	headers["Content-Type"] = "application/json";
	Ref<HTTPResponse> response;
	response.instantiate();
	server.test_handle_mcp_stateless_post(_make_test_context("POST", headers, String()), response);
	CHECK(response->get_status() == 400);
}

void test_justamcp_http_oauth_rejects_invalid_credentials() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	const bool prev_enabled = bool(GLOBAL_GET("blazium/justamcp/oauth_enabled"));
	const String prev_id = String(GLOBAL_GET("blazium/justamcp/client_id"));
	const String prev_secret = String(GLOBAL_GET("blazium/justamcp/client_secret"));
	ProjectSettings::get_singleton()->set_setting("blazium/justamcp/oauth_enabled", true);
	ProjectSettings::get_singleton()->set_setting("blazium/justamcp/client_id", "test-client");
	ProjectSettings::get_singleton()->set_setting("blazium/justamcp/client_secret", "test-secret");

	Dictionary headers;
	headers["authorization"] = "Bearer wrong-secret";
	headers["x-client-id"] = "test-client";
	Ref<HTTPResponse> response;
	response.instantiate();
	CHECK(!server.test_validate_mcp_oauth(_make_test_context("POST", headers), response));
	CHECK(response->get_status() == 401);

	ProjectSettings::get_singleton()->set_setting("blazium/justamcp/oauth_enabled", prev_enabled);
	ProjectSettings::get_singleton()->set_setting("blazium/justamcp/client_id", prev_id);
	ProjectSettings::get_singleton()->set_setting("blazium/justamcp/client_secret", prev_secret);
}

void test_justamcp_http_two_session_pending_isolation() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPSessionManager *session_manager = server.test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	CHECK(session_manager->test_try_set_pending_post_sse("iso-session-a", "{\"id\":1}"));
	CHECK(session_manager->test_try_set_pending_post_sse("iso-session-b", "{\"id\":2}"));
	CHECK(session_manager->test_peek_pending_post_sse_body("iso-session-a").contains("\"id\":1"));
	CHECK(session_manager->test_peek_pending_post_sse_body("iso-session-b").contains("\"id\":2"));
}

void test_justamcp_orphan_send_tool_result_route_fallback() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPSessionManager *session_manager = server.test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	session_manager->bind_request_tool_route(909, "orphan-session", 77);
	server.test_clear_last_send_tool_result();
	Dictionary ok;
	ok["ok"] = true;
	server.send_tool_result(909, true, ok, "");
	const Dictionary rpc_result = server.test_peek_last_send_tool_result();
	CHECK(rpc_result.has("result"));
}

void test_justamcp_legacy_sse_broadcast_tracking() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPSessionManager *session_manager = server.test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	server.test_set_current_sse_connection_id(44);
	session_manager->track_legacy_broadcast_connection(44);
	CHECK(server.test_count_notification_broadcast_targets() == 1);
	server.test_set_current_sse_connection_id(55);
	CHECK(server.test_count_notification_broadcast_targets() == 2);
	session_manager->untrack_legacy_broadcast_connection(44);
	CHECK(server.test_count_notification_broadcast_targets() == 1);
}

void test_justamcp_mcp_config_json_uses_streamable_mcp_path() {
#ifdef TOOLS_ENABLED
	const String cursor_cfg = JustAMCPEditorPlugin::get_mcp_config_json(JustAMCPEditorPlugin::MCP_CONFIG_CURSOR);
	const String ag_cfg = JustAMCPEditorPlugin::get_mcp_config_json(JustAMCPEditorPlugin::MCP_CONFIG_ANTIGRAVITY);
	const String opencode_cfg = JustAMCPEditorPlugin::get_mcp_config_json(JustAMCPEditorPlugin::MCP_CONFIG_OPENCODE);
	CHECK(cursor_cfg.contains("/mcp"));
	CHECK(!cursor_cfg.contains("/sse"));
	CHECK(!cursor_cfg.contains("\"MCP-Protocol-Version\""));
	CHECK(!cursor_cfg.contains("\"headers\""));
	CHECK(ag_cfg.contains("/mcp"));
	CHECK(!ag_cfg.contains("\"/sse\""));
	CHECK(opencode_cfg.contains("\"$schema\": \"https://opencode.ai/config.json\""));
	CHECK(opencode_cfg.contains("\"type\": \"remote\""));
	CHECK(opencode_cfg.contains("/mcp"));
	CHECK(opencode_cfg.contains("\"enabled\": true"));
#else
	SUCCEED();
#endif
}

void test_justamcp_http_catalogs_after_initialize() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPSessionManager *session_manager = server.test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	Dictionary init_headers;
	init_headers["Accept"] = "application/json";
	init_headers["Content-Type"] = "application/json";
	const String init_body = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{},\"clientInfo\":{\"name\":\"catalog\",\"version\":\"1.0\"}}}";
	Ref<HTTPResponse> init_response;
	init_response.instantiate();
	CHECK(session_manager->handle_mcp_post(_make_test_context("POST", init_headers, init_body), init_response));
	const String session_id = _get_response_header(init_response, "MCP-Session-Id");
	CHECK(!session_id.is_empty());

	Dictionary headers = init_headers;
	headers["MCP-Session-Id"] = session_id;

	const char *methods[] = { "tools/list", "prompts/list", "resources/list", "resources/templates/list", nullptr };
	const char *result_keys[] = { "tools", "prompts", "resources", "resourceTemplates", nullptr };
	for (int i = 0; methods[i]; i++) {
		const String body = "{\"jsonrpc\":\"2.0\",\"id\":" + itos(i + 2) + ",\"method\":\"" + String(methods[i]) + "\",\"params\":{}}";
		Ref<HTTPResponse> response;
		response.instantiate();
		CHECK(session_manager->handle_mcp_post(_make_test_context("POST", headers, body), response));
		const int status = response->get_status();
		const bool ok_status = status == 200 || status == 202;
		CHECK(ok_status);
		if (response->get_body().is_empty()) {
			continue;
		}
		Ref<JSON> json;
		json.instantiate();
		CHECK(json->parse(response->get_body()) == OK);
		CHECK(json->get_data().get_type() == Variant::DICTIONARY);
		Dictionary root = json->get_data();
		CHECK(!root.has("error"));
		CHECK(root.has("result"));
		CHECK(Dictionary(root["result"]).has(result_keys[i]));
	}
}

void test_justamcp_http_dual_accept_tools_list_returns_json() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPSessionManager *session_manager = server.test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	const String init_body = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{},\"clientInfo\":{\"name\":\"dual-list\",\"version\":\"1.0\"}}}";
	Ref<HTTPResponse> init_response;
	init_response.instantiate();
	CHECK(session_manager->handle_mcp_post(_make_test_context("POST", _streamable_headers(), init_body), init_response));
	const String session_id = _get_response_header(init_response, "MCP-Session-Id");
	CHECK(!session_id.is_empty());

	const String list_body = "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/list\",\"params\":{}}";
	Ref<HTTPResponse> response;
	response.instantiate();
	CHECK(session_manager->handle_mcp_post(_make_test_context("POST", _streamable_headers(session_id), list_body), response));
	CHECK(!response->is_sse_response());
	CHECK(response->get_status() == 200);
	CHECK(!response->get_body().is_empty());
	CHECK(session_manager->test_peek_pending_post_sse_body(session_id).is_empty());

	Ref<JSON> json;
	json.instantiate();
	CHECK(json->parse(response->get_body()) == OK);
	CHECK(json->get_data().get_type() == Variant::DICTIONARY);
	Dictionary root = json->get_data();
	CHECK(!root.has("error"));
	CHECK(!root.has("handled"));
	CHECK(root.has("jsonrpc"));
	CHECK(root.has("result"));
	Dictionary result = root["result"];
	CHECK(result.has("tools"));
	CHECK(Array(result["tools"]).size() > 0);
}

void test_justamcp_http_dual_accept_tools_call_uses_sse() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPSessionManager *session_manager = server.test_get_session_manager();
	TEST_FAIL_COND(session_manager == nullptr, "Session manager is required");

	const String init_body = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{},\"clientInfo\":{\"name\":\"dual-call\",\"version\":\"1.0\"}}}";
	Ref<HTTPResponse> init_response;
	init_response.instantiate();
	CHECK(session_manager->handle_mcp_post(_make_test_context("POST", _streamable_headers(), init_body), init_response));
	const String session_id = _get_response_header(init_response, "MCP-Session-Id");
	CHECK(!session_id.is_empty());

	const String tool_body = "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"blazium_list_toolsets\",\"arguments\":{}}}";
	Ref<HTTPResponse> sse;
	sse.instantiate();
	CHECK(session_manager->handle_mcp_post(_make_test_context("POST", _streamable_headers(session_id), tool_body), sse));
	CHECK(sse->is_sse_response());
	CHECK(sse->get_status() != 409);
	CHECK(session_manager->test_peek_pending_post_sse_body(session_id).contains("tools/call"));
}

void test_justamcp_mcp_config_client_field_shapes() {
#ifdef TOOLS_ENABLED
	const String cursor_cfg = JustAMCPEditorPlugin::get_mcp_config_json(JustAMCPEditorPlugin::MCP_CONFIG_CURSOR);
	const String ag_cfg = JustAMCPEditorPlugin::get_mcp_config_json(JustAMCPEditorPlugin::MCP_CONFIG_ANTIGRAVITY);
	const String opencode_cfg = JustAMCPEditorPlugin::get_mcp_config_json(JustAMCPEditorPlugin::MCP_CONFIG_OPENCODE);
	CHECK(cursor_cfg.contains("\"url\":"));
	CHECK(!cursor_cfg.contains("\"serverUrl\":"));
	CHECK(!cursor_cfg.contains("\"headers\""));
	CHECK(!cursor_cfg.contains("\"MCP-Protocol-Version\""));
	CHECK(!cursor_cfg.contains("\"oauth\""));
	CHECK(ag_cfg.contains("\"serverUrl\":"));
	CHECK(!ag_cfg.contains("\"url\": \"http://"));
	CHECK(opencode_cfg.contains("\"mcp\":"));
	CHECK(opencode_cfg.contains("\"blazium-mcp\":"));

	ProjectSettings *ps = ProjectSettings::get_singleton();
	if (ps) {
		const bool prev_override = bool(ps->get_setting("blazium/justamcp/override_editor_settings", false));
		const bool prev_oauth = bool(ps->get_setting("blazium/justamcp/oauth_enabled", false));
		const String prev_id = String(ps->get_setting("blazium/justamcp/client_id", ""));
		const String prev_secret = String(ps->get_setting("blazium/justamcp/client_secret", ""));
		ps->set_setting("blazium/justamcp/override_editor_settings", true);
		ps->set_setting("blazium/justamcp/oauth_enabled", true);
		ps->set_setting("blazium/justamcp/client_id", "cursor-client");
		ps->set_setting("blazium/justamcp/client_secret", "cursor-secret");
		const String cursor_oauth = JustAMCPEditorPlugin::get_mcp_config_json(JustAMCPEditorPlugin::MCP_CONFIG_CURSOR);
		const String ag_oauth = JustAMCPEditorPlugin::get_mcp_config_json(JustAMCPEditorPlugin::MCP_CONFIG_ANTIGRAVITY);
		CHECK(cursor_oauth.contains("\"auth\""));
		CHECK(cursor_oauth.contains("\"CLIENT_ID\""));
		CHECK(!cursor_oauth.contains("\"oauth\""));
		CHECK(ag_oauth.contains("\"oauth\""));
		CHECK(ag_oauth.contains("\"clientId\""));
		ps->set_setting("blazium/justamcp/override_editor_settings", prev_override);
		ps->set_setting("blazium/justamcp/oauth_enabled", prev_oauth);
		ps->set_setting("blazium/justamcp/client_id", prev_id);
		ps->set_setting("blazium/justamcp/client_secret", prev_secret);
	}
#else
	SUCCEED();
#endif
}

#else
void test_justamcp_http_initialize_creates_session() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for HTTP integration tests");
}
void test_justamcp_http_initialize_json_only_accept_creates_session() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for HTTP integration tests");
}
void test_justamcp_http_post_sse_conflict_409() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for HTTP integration tests");
}
void test_justamcp_http_delete_teardown() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for HTTP integration tests");
}
void test_justamcp_http_cors_preflight() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for HTTP integration tests");
}
void test_justamcp_http_stateless_empty_body_400() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for HTTP integration tests");
}
void test_justamcp_http_oauth_rejects_invalid_credentials() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for HTTP integration tests");
}
void test_justamcp_http_two_session_pending_isolation() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for HTTP integration tests");
}
void test_justamcp_orphan_send_tool_result_route_fallback() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for HTTP integration tests");
}
void test_justamcp_legacy_sse_broadcast_tracking() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for HTTP integration tests");
}
void test_justamcp_mcp_config_json_uses_streamable_mcp_path() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for HTTP integration tests");
}
void test_justamcp_http_catalogs_after_initialize() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for HTTP integration tests");
}
void test_justamcp_mcp_config_client_field_shapes() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for HTTP integration tests");
}
#endif

#endif
