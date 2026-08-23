/**************************************************************************/
/*  test_justamcp_mcp_client.cpp                                          */
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

#include "test_justamcp_mcp_client.h"

#ifdef TOOLS_ENABLED

#include "../justamcp_json_rpc_transport.h"
#include "../justamcp_mcp_apps.h"
#include "../justamcp_mcp_client.h"
#include "../justamcp_mcp_client_http.h"
#include "../justamcp_mcp_client_oauth.h"
#include "../justamcp_mcp_spec.h"
#include "../justamcp_server.h"
#include "../tools/justamcp_json_rpc_helpers.h"
#include "../tools/justamcp_json_rpc_router.h"
#include "../tools/justamcp_mcp_client_bridge.h"
#include "../tools/justamcp_task_manager.h"
#include "../tools/resources/justamcp_resource_project_file.h"
#include "../tools/resources/justamcp_resource_ui.h"
#include "test_justamcp_fixture.h"

#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "modules/modules_enabled.gen.h"
#include "tests/test_macros.h"

#if defined(MODULE_HTTPSERVER_ENABLED)
#include "modules/httpserver/http_request_context.h"
#include "modules/httpserver/http_response.h"
#endif

void test_justamcp_client_meta_and_headers() {
	Dictionary params;
	JustAMCPMCPClient::attach_client_meta(params, "2026-07-28", true);
	CHECK(params.has("_meta"));
	const Dictionary meta = params["_meta"];
	CHECK(String(meta.get("io.modelcontextprotocol/protocolVersion", "")) == "2026-07-28");
	CHECK(meta.has("io.modelcontextprotocol/clientCapabilities"));
	CHECK(meta.has("io.modelcontextprotocol/clientInfo"));
	CHECK(String(meta.get("io.modelcontextprotocol/logLevel", "")) == "debug");
	const Dictionary info = meta["io.modelcontextprotocol/clientInfo"];
	CHECK(String(info.get("name", "")) == "JustAMCP");
	const Dictionary caps = meta["io.modelcontextprotocol/clientCapabilities"];
	CHECK(caps.has("tools"));
	CHECK(caps.has("prompts"));
	CHECK(caps.has("resources"));
	CHECK(caps.has("elicitation"));
	CHECK(Dictionary(caps.get("extensions", Dictionary())).has("io.modelcontextprotocol/ui"));

	Dictionary extra;
	const Vector<String> headers = JustAMCPMCPClientHTTP::streamable_headers("tools/list", "2026-07-28", "tok", extra, true);
	bool saw_method = false;
	bool saw_accept = false;
	bool saw_name = false;
	for (int i = 0; i < headers.size(); i++) {
		if (headers[i].begins_with("Mcp-Method: tools/list")) {
			saw_method = true;
		}
		if (headers[i].contains("text/event-stream")) {
			saw_accept = true;
		}
		if (headers[i].begins_with("Mcp-Name:")) {
			saw_name = true;
		}
	}
	CHECK(saw_method);
	CHECK(saw_accept);
	CHECK(!saw_name);

	const Vector<String> call_headers = JustAMCPMCPClientHTTP::streamable_headers("tools/call", "2026-07-28", "", extra, true, "my_tool");
	bool saw_call_name = false;
	for (int i = 0; i < call_headers.size(); i++) {
		if (call_headers[i].begins_with("Mcp-Name: my_tool")) {
			saw_call_name = true;
		}
	}
	CHECK(saw_call_name);

	const Vector<String> legacy = JustAMCPMCPClientHTTP::streamable_headers("tools/list", "2025-11-25", "", extra, false);
	for (int i = 0; i < legacy.size(); i++) {
		CHECK(!legacy[i].begins_with("Mcp-Method:"));
	}

	Dictionary rpc = JustAMCPMCPClient::build_json_rpc("tools/list", params, 7);
	CHECK(String(rpc.get("method", "")) == "tools/list");
	CHECK(int(rpc.get("id", 0)) == 7);
}

void test_justamcp_client_allow_list_and_sse_parse() {
	String err;
	CHECK(JustAMCPMCPClientHTTP::url_allowed("http://127.0.0.1:6506/mcp", err));
	CHECK(!JustAMCPMCPClientHTTP::url_allowed("http://evil.example/mcp", err));
	CHECK(err.contains("allow-listed"));
	CHECK(err.contains("evil.example"));
	CHECK(err.contains("bridge_url_allow_hosts"));

	Dictionary parsed = JustAMCPMCPClientHTTP::parse_json_or_sse("event: message\ndata: {\"jsonrpc\":\"2.0\",\"result\":{\"ok\":true}}\n\n", "text/event-stream");
	CHECK(parsed.get("ok", false));
	CHECK(Dictionary(parsed.get("payload", Dictionary())).has("result"));
}

void test_justamcp_client_oauth_helpers() {
	CHECK(JustAMCPMCPClientOAuth::parse_www_authenticate_metadata_url("Bearer realm=\"mcp\", resource_metadata=\"https://as.example/.well-known/oauth-protected-resource\"") == "https://as.example/.well-known/oauth-protected-resource");

	Dictionary prm;
	Array servers;
	servers.push_back("https://issuer.example");
	prm["authorization_servers"] = servers;
	prm["resource"] = "https://mcp.example/mcp";
	Dictionary parsed_prm = JustAMCPMCPClientOAuth::parse_protected_resource_metadata(prm);
	CHECK(String(parsed_prm.get("resource", "")).ends_with("/mcp"));

	Dictionary as;
	as["issuer"] = "https://issuer.example";
	as["authorization_endpoint"] = "https://issuer.example/authorize";
	as["token_endpoint"] = "https://issuer.example/token";
	as["registration_endpoint"] = "https://issuer.example/register";
	as["client_id_metadata_document_supported"] = true;
	Dictionary parsed_as = JustAMCPMCPClientOAuth::parse_authorization_server_metadata(as);
	CHECK(bool(parsed_as.get("client_id_metadata_document_supported", false)));

	Dictionary dcr = JustAMCPMCPClientOAuth::build_dcr_request("http://127.0.0.1:6506/oauth/callback");
	CHECK(String(dcr.get("application_type", "")) == "native");
	CHECK(String(dcr.get("token_endpoint_auth_method", "")) == "none");
	CHECK(Array(dcr.get("redirect_uris", Array())).size() == 1);

	String cimd_error;
	CHECK(JustAMCPMCPClientOAuth::validate_cimd_url("https://blazium.app/oauth/client-metadata.json", cimd_error));
	CHECK(!JustAMCPMCPClientOAuth::validate_cimd_url("http://evil.example/client.json", cimd_error));

	String iss_error;
	CHECK(JustAMCPMCPClientOAuth::validate_iss("https://issuer.example", "https://issuer.example/", iss_error));
	CHECK(!JustAMCPMCPClientOAuth::validate_iss("https://issuer.example", "https://other.example", iss_error));

	const String key_a = JustAMCPMCPClientOAuth::issuer_storage_key("https://issuer.example");
	const String key_b = JustAMCPMCPClientOAuth::issuer_storage_key("https://other.example");
	CHECK(!key_a.is_empty());
	CHECK(key_a != key_b);

	const String verifier = "demo-verifier";
	const String challenge = JustAMCPMCPClientOAuth::pkce_challenge_s256(verifier);
	CHECK(!challenge.is_empty());
	CHECK(!challenge.contains("+"));
	CHECK(!challenge.contains("/"));
	CHECK(!challenge.contains("="));

	Dictionary blocked_http;
	blocked_http["www_authenticate"] = "Bearer resource_metadata=\"https://evil.example/.well-known/oauth-protected-resource\"";
	Dictionary blocked_cfg;
	blocked_cfg["oauth_mode"] = "auto";
	Dictionary blocked = JustAMCPMCPClientOAuth::resolve_bearer_token(blocked_cfg, blocked_http);
	CHECK(!blocked.get("ok", true));
	CHECK(String(blocked.get("error", "")).contains("blocked"));
	CHECK(String(blocked.get("error", "")).contains("evil.example"));
}

void test_justamcp_mcp_apps_helpers() {
	Dictionary tool;
	Dictionary meta;
	Dictionary ui;
	ui["resourceUri"] = "ui://.blazium/apps/demo.html";
	ui["csp"] = Dictionary();
	Dictionary perms;
	perms["camera"] = true;
	ui["permissions"] = perms;
	meta["ui"] = ui;
	tool["_meta"] = meta;
	Dictionary detected = JustAMCPMCPAppsHost::detect_ui_meta(tool);
	CHECK(bool(detected.get("present", false)));
	CHECK(String(detected.get("resourceUri", "")).begins_with("ui://"));

	Dictionary grants;
	CHECK(!JustAMCPMCPAppsHost::permissions_granted(perms, grants));
	grants["camera"] = true;
	CHECK(JustAMCPMCPAppsHost::permissions_granted(perms, grants));

	const String csp = JustAMCPMCPAppsHost::csp_header(Dictionary());
	CHECK(csp.contains("default-src 'none'"));
	CHECK(csp.contains("base-uri 'none'"));

	String path;
	String error;
	CHECK(JustAMCPMCPAppsHost::sanitize_ui_uri("ui://.blazium/apps/demo.html", path, error));
	CHECK(path == ".blazium/apps/demo.html");
	CHECK(JustAMCPMCPAppsHost::sanitize_ui_uri(JustAMCPMCPAppsHost::bundled_host_uri(), path, error));
	CHECK(!JustAMCPMCPAppsHost::sanitize_ui_uri("ui://res://apps/demo.html", path, error));
	CHECK(!JustAMCPMCPAppsHost::sanitize_ui_uri("ui://.blazium/apps/../secret.html", path, error));
	CHECK(!JustAMCPMCPAppsHost::sanitize_ui_uri("ui://../secret", path, error));
	CHECK(!JustAMCPMCPAppsHost::sanitize_ui_uri("file:///etc/passwd", path, error));

	const String host_page = JustAMCPMCPAppsHost::host_page_html("<div id=\"sterilize-marker\">ok</div>", JustAMCPMCPAppsHost::csp_header(Dictionary()));
	CHECK(host_page.contains("iframe"));
	CHECK(host_page.contains("sterilize-marker"));
	CHECK(host_page.contains("/mcp-apps/proxy"));
	CHECK(!host_page.contains("{{JUSTAMCP_APP_HTML}}"));

	JustAMCPResourceUI ui_resource;
	Dictionary bundled = ui_resource.read_resource(JustAMCPMCPAppsHost::bundled_host_uri());
	CHECK(bundled.get("ok", false));
	CHECK(String(Dictionary(Array(bundled.get("contents", Array()))[0]).get("text", "")).contains("{{JUSTAMCP_APP_HTML}}"));

	JustAMCPResourceProjectFile project_file;
	Dictionary escaped = project_file.read_resource("res://foo/../../outside");
	CHECK(!escaped.get("ok", true));

	JustAMCPMCPAppsHost host;
	Dictionary denied = host.proxy_tools_call("local", "echo", Dictionary(), false);
	CHECK(!denied.get("ok", true));
	CHECK(bool(denied.get("needs_consent", false)));
}

void test_justamcp_input_required_modern_tools_call() {
	Dictionary payload = justamcp_input_required_result("form", "Confirm?", justamcp_confirm_enum_schema());
	CHECK(String(payload.get("resultType", "")) == "input_required");
	CHECK(Dictionary(payload.get("elicitation", Dictionary())).has("requestedSchema"));

	Dictionary formatted = JustAMCPJsonRpcHelpers::format_tool_result(true, payload, "");
	CHECK(formatted.has("result"));
	CHECK(String(Dictionary(formatted["result"]).get("resultType", "")) == "input_required");
	CHECK(Dictionary(formatted["result"]).has("elicitation"));

	JustAMCPTestServerFixture fixture;
	fixture.get_server().test_set_transport_negotiated_protocol("2026-07-28");
	fixture.get_server().hold_tool_for_elicitation(42, "blazium_demo_tool", Dictionary(), justamcp_confirm_enum_schema(), "form");
	CHECK(fixture.get_server().has_pending_elicitation(42));
	Dictionary args;
	Dictionary responses;
	responses["action"] = "accept";
	Dictionary content;
	content["confirmed"] = "accept";
	responses["content"] = content;
	CHECK(fixture.get_server().apply_input_responses("blazium_demo_tool", args, responses));
	CHECK(bool(args.get("confirmed", false)));

	fixture.get_server().hold_tool_for_elicitation(43, "blazium_demo_tool", Dictionary(), justamcp_confirm_enum_schema(), "form");
	Dictionary bad_args;
	Dictionary bad_responses;
	bad_responses["action"] = "accept";
	Dictionary bad_content;
	bad_content["confirmed"] = "not-an-enum";
	bad_responses["content"] = bad_content;
	CHECK(!fixture.get_server().apply_input_responses("blazium_demo_tool", bad_args, bad_responses));

	Dictionary orphan_args;
	CHECK(!fixture.get_server().apply_input_responses("missing_tool", orphan_args, responses));
	CHECK(!orphan_args.has("confirmed"));
}

void test_justamcp_discover_honest_and_tasks_update() {
	JustAMCPTestServerFixture fixture;
	Dictionary routed = JustAMCPJsonRpcRouter::route_discover(&fixture.get_server(), 1);
	Dictionary result = routed["result"];
	Dictionary caps = result["capabilities"];
	CHECK(caps.has("tools"));
	CHECK(caps.has("prompts"));
	CHECK(caps.has("resources"));
	CHECK(caps.has("completions"));
	CHECK(caps.has("elicitation"));
	CHECK(!caps.has("sampling"));
	CHECK(!caps.has("roots"));
	CHECK(!caps.has("logging"));
	CHECK(Dictionary(Dictionary(caps.get("extensions", Dictionary())).get("io.modelcontextprotocol/tasks", Dictionary())).has("update"));

	JustAMCPTaskManager manager;
	const String task_id = manager.create_task(60000, 1000, "progress-update");
	Dictionary patch;
	patch["statusMessage"] = "halfway";
	patch["pollInterval"] = 250;
	Dictionary updated = manager.update_task(task_id, patch);
	CHECK(updated.get("ok", false));
	CHECK(String(updated.get("statusMessage", "")) == "halfway");
}

void test_justamcp_bridge_crud_and_redaction() {
	JustAMCPMCPClientBridge bridge;
	ProjectSettings::get_singleton()->set_setting("blazium/justamcp/mcp_clients", Array());
	Dictionary add_args;
	add_args["name"] = "crud-bridge";
	add_args["url"] = "http://127.0.0.1:9998/mcp";
	add_args["auth_token"] = "secret";
	add_args["client_secret"] = "oauth-secret";
	add_args["auto_connect"] = true;
	add_args["expose_remote_tools"] = false;
	CHECK(bridge.add_bridge(add_args).get("ok", false));

	Dictionary listed = bridge.list_bridges(Dictionary());
	Array bridges = listed.get("bridges", Array());
	CHECK(bridges.size() >= 1);
	Dictionary row = bridges[0];
	CHECK(!row.has("auth_token"));
	CHECK(!row.has("client_secret"));
	CHECK(bool(row.get("has_auth_token", false)));

	Dictionary update_args;
	update_args["name"] = "crud-bridge";
	update_args["timeout_ms"] = 1234;
	CHECK(bridge.update_bridge(update_args).get("ok", false));

	Dictionary status_args;
	status_args["name"] = "crud-bridge";
	Dictionary status = bridge.status_bridge(status_args);
	CHECK(status.get("ok", false));
	CHECK(String(status.get("name", "")) == "crud-bridge");

	Dictionary remove_args;
	remove_args["name"] = "crud-bridge";
	CHECK(bridge.remove_bridge(remove_args).get("ok", false));
	ProjectSettings::get_singleton()->set_setting("blazium/justamcp/mcp_clients", Array());
}

void test_justamcp_loopback_discover_then_list() {
	JustAMCPTestServerFixture fixture;
	Dictionary discover = JustAMCPJsonRpcRouter::route_discover(&fixture.get_server(), 1);
	CHECK(discover.get("handled", false));
	CHECK(Dictionary(discover.get("result", Dictionary())).has("supportedVersions"));

	Dictionary list = JustAMCPJsonRpcRouter::route_tools_list("", 2);
	CHECK(list.get("handled", false));
}

#if defined(MODULE_HTTPSERVER_ENABLED)
void test_justamcp_mcp_apps_http_host_and_proxy() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();

	Ref<HTTPRequestContext> unknown_ctx;
	unknown_ctx.instantiate();
	unknown_ctx->set_method("GET");
	unknown_ctx->set_path("/mcp-apps/host");
	Dictionary unknown_query;
	unknown_query["uri"] = "ui://unknown";
	unknown_ctx->set_query_params(unknown_query);
	Ref<HTTPResponse> unknown_resp;
	unknown_resp.instantiate();
	server.test_handle_mcp_apps_host(unknown_ctx, unknown_resp);
	CHECK(unknown_resp->get_status() == 404);

	JustAMCPMCPAppsHost *apps = JustAMCPMCPAppsHost::get_singleton();
	TEST_FAIL_COND(apps == nullptr, "MCP Apps host singleton is required");
	Dictionary ui;
	ui["resourceUri"] = "ui://.blazium/apps/sterilize-demo.html";
	Dictionary opened = apps->open_app("local", ui, "<div id=\"sterilize-marker\">ok</div>");
	CHECK(opened.get("ok", false));
	CHECK(!bool(opened.get("opened", true)));
	CHECK(String(opened.get("url", "")).contains("/mcp-apps/host"));

	Ref<HTTPRequestContext> host_ctx;
	host_ctx.instantiate();
	host_ctx->set_method("GET");
	host_ctx->set_path("/mcp-apps/host");
	Dictionary host_query;
	host_query["uri"] = "ui://.blazium/apps/sterilize-demo.html";
	host_ctx->set_query_params(host_query);
	Ref<HTTPResponse> host_resp;
	host_resp.instantiate();
	server.test_handle_mcp_apps_host(host_ctx, host_resp);
	CHECK(host_resp->get_status() == 200);
	CHECK(host_resp->get_body().contains("sterilize-marker"));
	CHECK(host_resp->get_body().contains("iframe"));

	Ref<HTTPRequestContext> empty_proxy;
	empty_proxy.instantiate();
	empty_proxy->set_method("POST");
	empty_proxy->set_path("/mcp-apps/proxy");
	empty_proxy->set_body("");
	Ref<HTTPResponse> empty_resp;
	empty_resp.instantiate();
	server.test_handle_mcp_apps_proxy(empty_proxy, empty_resp);
	CHECK(empty_resp->get_status() == 400);

	Ref<HTTPRequestContext> bad_name;
	bad_name.instantiate();
	bad_name->set_method("POST");
	bad_name->set_path("/mcp-apps/proxy");
	bad_name->set_body("{\"jsonrpc\":\"2.0\",\"id\":1,\"params\":{\"name\":\"Bad-Name\",\"bridge\":\"local\",\"arguments\":{}}}");
	Ref<HTTPResponse> bad_name_resp;
	bad_name_resp.instantiate();
	server.test_handle_mcp_apps_proxy(bad_name, bad_name_resp);
	CHECK(bad_name_resp->get_status() == 400);

	ProjectSettings *ps = ProjectSettings::get_singleton();
	const bool prev_override = bool(ps->get_setting("blazium/justamcp/override_editor_settings", false));
	const bool prev_strict = bool(ps->get_setting("blazium/justamcp/streamable_http_strict_origin", false));
	ps->set_setting("blazium/justamcp/override_editor_settings", true);
	ps->set_setting("blazium/justamcp/streamable_http_strict_origin", true);
	Ref<HTTPRequestContext> evil;
	evil.instantiate();
	evil->set_method("POST");
	evil->set_path("/mcp-apps/proxy");
	Dictionary evil_headers;
	evil_headers["Origin"] = "https://evil.example";
	evil->set_headers(evil_headers);
	evil->set_body("{\"jsonrpc\":\"2.0\",\"id\":1,\"params\":{\"name\":\"echo\",\"bridge\":\"local\",\"arguments\":{}}}");
	Ref<HTTPResponse> evil_resp;
	evil_resp.instantiate();
	server.test_handle_mcp_apps_proxy(evil, evil_resp);
	CHECK(evil_resp->get_status() == 403);
	ps->set_setting("blazium/justamcp/streamable_http_strict_origin", prev_strict);
	ps->set_setting("blazium/justamcp/override_editor_settings", prev_override);
}
#else
void test_justamcp_mcp_apps_http_host_and_proxy() {}
#endif

#else

void test_justamcp_client_meta_and_headers() {}
void test_justamcp_client_allow_list_and_sse_parse() {}
void test_justamcp_client_oauth_helpers() {}
void test_justamcp_mcp_apps_helpers() {}
void test_justamcp_input_required_modern_tools_call() {}
void test_justamcp_discover_honest_and_tasks_update() {}
void test_justamcp_bridge_crud_and_redaction() {}
void test_justamcp_loopback_discover_then_list() {}
void test_justamcp_mcp_apps_http_host_and_proxy() {}

#endif

#endif
