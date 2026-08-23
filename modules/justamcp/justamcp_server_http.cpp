/**************************************************************************/
/*  justamcp_server_http.cpp                                              */
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

#include "justamcp_server.h"

#include "justamcp_json_rpc_transport.h"
#include "justamcp_mcp_apps.h"
#include "justamcp_mcp_client_oauth.h"
#include "justamcp_mcp_spec.h"
#include "justamcp_oauth_discovery.h"
#include "justamcp_session_manager.h"
#include "tools/justamcp_settings_resolver.h"

#include "core/config/project_settings.h"
#include "core/crypto/crypto.h"
#include "core/crypto/crypto_core.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "modules/httpserver/http_server.h"

#if defined(MODULE_HTTPSERVER_ENABLED)

static bool _justamcp_secure_string_equal(const String &p_a, const String &p_b, bool &r_crypto_available) {
	Ref<Crypto> crypto = Ref<Crypto>(Crypto::create());
	if (crypto.is_null()) {
		r_crypto_available = false;
		return false;
	}
	r_crypto_available = true;
	return crypto->constant_time_compare(p_a.to_utf8_buffer(), p_b.to_utf8_buffer());
}

static String _justamcp_redact_header_value(const String &p_key, const String &p_value) {
	const String key = p_key.to_lower();
	if (key == "authorization" || key == "x-client-secret") {
		return "[redacted]";
	}
	return p_value;
}

static String _justamcp_redact_debug_body(const String &p_body) {
	const bool oauth_enabled = JustAMCPSettingsResolver::resolve_bool("blazium/justamcp/oauth_enabled", false);
	if (oauth_enabled) {
		return "[body redacted: oauth enabled]";
	}
	const int max_chars = 512;
	if (p_body.length() <= max_chars) {
		return p_body;
	}
	return p_body.substr(0, max_chars) + vformat("... [truncated, %d chars total]", p_body.length());
}

void JustAMCPServer::_handle_cors_preflight(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	if (session_manager) {
		session_manager->handle_cors_preflight(p_context, p_response);
	} else {
		p_response->set_status(204);
		p_response->set_body("");
	}
}

bool JustAMCPServer::_validate_mcp_oauth(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
#ifdef TOOLS_ENABLED
	const bool oauth_enabled = JustAMCPSettingsResolver::resolve_bool("blazium/justamcp/oauth_enabled", false);
	const String required_client_id = JustAMCPSettingsResolver::resolve_string("blazium/justamcp/client_id");
	const String required_client_secret = JustAMCPSettingsResolver::resolve_string("blazium/justamcp/client_secret");

	if (oauth_enabled) {
		if (required_client_id.is_empty() || required_client_secret.is_empty()) {
			ERR_PRINT("JustAMCP: oauth_enabled requires both client_id and client_secret; rejecting request.");
			p_response->set_status(401);
			_apply_oauth_www_authenticate(p_response);
			p_response->set_body("Unauthorized - OAuth credentials not configured");
			return false;
		}

		String cimd_error;
		if (!JustAMCPOauthDiscovery::validate_cimd_client_id(required_client_id, cimd_error)) {
			ERR_PRINT("JustAMCP: " + cimd_error);
			p_response->set_status(401);
			_apply_oauth_www_authenticate(p_response);
			p_response->set_body("Unauthorized - " + cimd_error);
			return false;
		}

		bool crypto_available = true;
		_justamcp_secure_string_equal(String(), String(), crypto_available);
		if (!crypto_available) {
			ERR_PRINT("JustAMCP: oauth_enabled but Crypto is unavailable; rejecting request.");
			p_response->set_status(503);
			p_response->set_body("Service Unavailable - Crypto required for OAuth");
			return false;
		}

		Dictionary headers = p_context->get_headers();
		String authorization = headers.get("authorization", headers.get("Authorization", ""));
		String client_id_header = headers.get("x-client-id", headers.get("X-Client-Id", ""));
		String client_secret_header = headers.get("x-client-secret", headers.get("X-Client-Secret", ""));
		const String bearer_prefix = "Bearer ";
		const String basic_prefix = "Basic ";
		const String expected_pair = required_client_id + ":" + required_client_secret;
		bool authorized = false;

		if (authorization.begins_with(basic_prefix)) {
			const String encoded = authorization.substr(basic_prefix.length()).strip_edges();
			PackedByteArray decoded;
			decoded.resize(((encoded.length() * 3) / 4) + 4);
			size_t decoded_len = 0;
			const CharString encoded_utf8 = encoded.utf8();
			if (CryptoCore::b64_decode(decoded.ptrw(), decoded.size(), &decoded_len, (const uint8_t *)encoded_utf8.get_data(), encoded_utf8.length()) == OK) {
				decoded.resize(decoded_len);
				const String decoded_pair = String::utf8((const char *)decoded.ptr(), decoded.size());
				authorized = _justamcp_secure_string_equal(decoded_pair, expected_pair, crypto_available);
			}
		} else {
			authorized = _justamcp_secure_string_equal(authorization, bearer_prefix + required_client_secret, crypto_available) ||
					_justamcp_secure_string_equal(authorization, bearer_prefix + expected_pair, crypto_available) ||
					_justamcp_secure_string_equal(client_secret_header, required_client_secret, crypto_available);
			if (authorized) {
				authorized = _justamcp_secure_string_equal(client_id_header, required_client_id, crypto_available) ||
						_justamcp_secure_string_equal(authorization, bearer_prefix + expected_pair, crypto_available);
			}
		}

		if (!authorized) {
			ERR_PRINT("JustAMCP: Unauthorized connection attempt.");
			p_response->set_status(401);
			_apply_oauth_www_authenticate(p_response);
			p_response->set_body("Unauthorized - Invalid OAuth credentials");
			return false;
		}
	}
#else
	(void)p_context;
	(void)p_response;
#endif
	return true;
}

void JustAMCPServer::_handle_legacy_sse_connect(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	if (JustAMCPSettingsResolver::resolve_bool("blazium/justamcp/enable_debug_logging", false)) {
		_mcp_debug_log("Incoming " + p_context->get_method() + " /sse connection attempt...");
	}
	if (!_validate_mcp_oauth(p_context, p_response)) {
		return;
	}
	if (session_manager && !MCPSessionManager::validate_origin(p_context)) {
		p_response->set_status(403);
		p_response->set_body("Forbidden - Invalid Origin");
		return;
	}
	p_response->start_sse();
}

void JustAMCPServer::_handle_mcp_get(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	if (!_validate_mcp_oauth(p_context, p_response)) {
		return;
	}
	if (session_manager && session_manager->handle_mcp_get(p_context, p_response)) {
		return;
	}
	p_response->set_status(405);
	p_response->set_body("Method not allowed");
}

void JustAMCPServer::_handle_mcp_post(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	if (!_validate_mcp_oauth(p_context, p_response)) {
		return;
	}
	if (session_manager && session_manager->handle_mcp_post(p_context, p_response)) {
		return;
	}
	_handle_mcp_stateless_post(p_context, p_response);
}

void JustAMCPServer::_handle_mcp_delete(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	if (!_validate_mcp_oauth(p_context, p_response)) {
		return;
	}
	if (session_manager) {
		session_manager->handle_mcp_delete(p_context, p_response);
	} else {
		p_response->set_status(405);
		p_response->set_body("Session DELETE not enabled");
	}
}

Dictionary JustAMCPServer::_transport_handle_json_rpc(const String &p_body, Ref<HTTPResponse> p_response) {
	return _handle_json_rpc(p_body, p_response);
}

void JustAMCPServer::_on_sse_connection_opened(int p_connection_id, const String &p_path, const Dictionary &p_headers) {
	if (p_path == "/mcp" && session_manager) {
		session_manager->on_sse_connection_opened(p_connection_id, p_path, p_headers);
		return;
	}
	if (p_path == "/sse") {
		if (JustAMCPSettingsResolver::resolve_bool("blazium/justamcp/enable_debug_logging", false)) {
			_mcp_debug_log("New SSE connection opened on " + p_path + " (ID: " + itos(p_connection_id) + ")");
		}
		current_sse_connection_id = p_connection_id;
		if (session_manager) {
			session_manager->track_legacy_broadcast_connection(p_connection_id);
		}

		int port = get_listening_port() > 0 ? get_listening_port() : JustAMCPSettingsResolver::resolve_server_port();
		const bool bind_to_localhost = JustAMCPSettingsResolver::resolve_bool("blazium/justamcp/bind_to_localhost_only", true);

		String host_authority;
		if (bind_to_localhost) {
			host_authority = "127.0.0.1:" + itos(port);
		} else {
			const String host_header = p_headers.has("Host") ? String(p_headers["Host"]).strip_edges() : String();
			if (!host_header.is_empty()) {
				host_authority = host_header.contains(":") ? host_header : (host_header + ":" + itos(port));
			} else {
				host_authority = "127.0.0.1:" + itos(port);
			}
		}
		String endpoint_url = "http://" + host_authority + "/message?sessionId=" + itos(p_connection_id);
		HTTPServer::get_singleton()->send_sse_event(p_connection_id, "endpoint", endpoint_url);
	}
}

void JustAMCPServer::_on_sse_connection_closed(int p_connection_id) {
	if (session_manager) {
		session_manager->on_sse_connection_closed(p_connection_id);
	}
	if (current_sse_connection_id == p_connection_id) {
		current_sse_connection_id = -1;
	}
	if (session_manager) {
		session_manager->untrack_legacy_broadcast_connection(p_connection_id);
	}
}

void JustAMCPServer::_deferred_post_sse_json_rpc(int p_connection_id, const String &p_session_id, const Dictionary &p_payload) {
	if (!session_manager) {
		return;
	}
	const Dictionary result = JustAMCPJsonRpcTransport::sanitize_wire_rpc(
			JustAMCPJsonRpcTransport::handle_json_rpc_parsed(this, p_payload, Ref<HTTPResponse>(), p_session_id));
	if (!result.is_empty()) {
		session_manager->send_json_on_connection(p_connection_id, JSON::stringify(result));
		session_manager->complete_post_sse_stream(p_connection_id);
		return;
	}
}

void JustAMCPServer::_deferred_legacy_message_json_rpc(const String &p_body, const String &p_session_id_param) {
	Dictionary result = JustAMCPJsonRpcTransport::sanitize_wire_rpc(_handle_json_rpc(p_body, Ref<HTTPResponse>()));
	if (result.is_empty()) {
		return;
	}
	if (!p_session_id_param.is_empty() && p_session_id_param.is_valid_int()) {
		const int conn_id = p_session_id_param.to_int();
		_send_sse_routed(JSON::stringify(result), String(), conn_id);
	} else {
		_send_sse_message(JSON::stringify(result));
	}
}

void JustAMCPServer::_deferred_held_json_rpc(int p_client_id, const String &p_body, const String &p_session_id, const Ref<HTTPResponse> &p_response) {
	Ref<HTTPResponse> response = p_response;
	if (response.is_null()) {
		response.instantiate();
	}
	if (!p_session_id.is_empty()) {
		response->add_header("MCP-Session-Id", p_session_id);
	}
	Dictionary result = JustAMCPJsonRpcTransport::handle_json_rpc(this, p_body, response, p_session_id);
	if (!p_session_id.is_empty() && session_manager) {
		Ref<JSON> json;
		json.instantiate();
		if (json->parse(p_body) == OK && json->get_data().get_type() == Variant::DICTIONARY) {
			const Dictionary payload = json->get_data();
			if (String(payload.get("method", "")) == "notifications/initialized") {
				session_manager->mark_session_initialized(p_session_id);
			}
		}
	}
	if (!response->is_sent()) {
		if (result.is_empty()) {
			response->set_status(202);
			response->set_body("");
		} else {
			if (result.has("_justamcp_batch_results") && result["_justamcp_batch_results"].get_type() == Variant::ARRAY) {
				response->set_status(200);
				response->set_content_type("application/json");
				response->set_body(JSON::stringify(result["_justamcp_batch_results"]));
			} else {
				const int status = MCPSessionManager::is_modern_protocol_version(transport_negotiated_protocol)
						? MCPSessionManager::modern_http_status_for_rpc(result)
						: 200;
				response->set_status(status);
				response->set_json(JustAMCPJsonRpcTransport::sanitize_wire_rpc(result));
			}
		}
	}
	if (HTTPServer::get_singleton()) {
		HTTPServer::get_singleton()->complete_response(p_client_id, response);
	}
}

void JustAMCPServer::_deferred_sse_replay(int p_connection_id, const String &p_last_event_id) {
	if (session_manager) {
		session_manager->deferred_replay_stream_events(p_connection_id, p_last_event_id);
	}
}

void JustAMCPServer::_handle_message_post(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	if (!_validate_mcp_oauth(p_context, p_response)) {
		return;
	}
	if (session_manager && !MCPSessionManager::validate_origin(p_context)) {
		p_response->set_status(403);
		p_response->set_body("Forbidden - Invalid Origin");
		return;
	}

	String body = p_context->get_body();
	if (body.is_empty()) {
		ERR_PRINT("JustAMCP: Received empty message POST body.");
		p_response->set_status(400);
		p_response->set_body("Empty request body");
		return;
	}

	if (JustAMCPSettingsResolver::resolve_bool("blazium/justamcp/enable_debug_logging", false)) {
		Array keys = p_context->get_headers().keys();
		String header_dump;
		for (int i = 0; i < keys.size(); i++) {
			const String key = String(keys[i]);
			header_dump += "  - " + key + ": " + _justamcp_redact_header_value(key, String(p_context->get_headers()[keys[i]])) + "\n";
		}
		_mcp_debug_log("Message POST Payload Headers:\n" + header_dump);
		_mcp_debug_log("Message POST Received Body: " + _justamcp_redact_debug_body(body));
	}

	p_response->set_status(202);
	p_response->set_body("Accepted");

	const String session_id_param = p_context->get_query_param("sessionId");
	call_deferred(SNAME("_deferred_legacy_message_json_rpc"), body, session_id_param);
}

void JustAMCPServer::_handle_mcp_stateless_post(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	String body = p_context->get_body();
	if (body.is_empty()) {
		ERR_PRINT("JustAMCP: Received empty message POST body on /mcp.");
		p_response->set_status(400);
		Dictionary err;
		err["jsonrpc"] = "2.0";
		Dictionary error_dict;
		error_dict["code"] = -32700;
		error_dict["message"] = "Invalid JSON or empty body";
		err["error"] = error_dict;
		p_response->set_json(err);
		return;
	}

	const bool debug_logging = JustAMCPSettingsResolver::resolve_bool("blazium/justamcp/enable_debug_logging", false);
	if (debug_logging) {
		Array keys = p_context->get_headers().keys();
		String header_dump;
		for (int i = 0; i < keys.size(); i++) {
			const String key = String(keys[i]);
			header_dump += "  - " + key + ": " + _justamcp_redact_header_value(key, String(p_context->get_headers()[keys[i]])) + "\n";
		}
		_mcp_debug_log("Stateless POST Headers:\n" + header_dump);
		_mcp_debug_log("Stateless POST Received Body: " + _justamcp_redact_debug_body(body));
	}

	const int client_id = p_context->get_client_id();
	if (client_id >= 0 && HTTPServer::get_singleton()) {
		p_response->hold();
		call_deferred(SNAME("_deferred_held_json_rpc"), client_id, body, String(), p_response);
		return;
	}

	Dictionary result = _handle_json_rpc(body, p_response);
	if (!p_response->is_sent()) {
		if (result.is_empty()) {
			if (debug_logging) {
				_mcp_debug_log("Stateless POST Replying HTTP 202 (Empty result object)");
			}
			p_response->set_status(202);
			p_response->set_body("");
		} else {
			if (debug_logging) {
				_mcp_debug_log("Stateless POST Replying HTTP 200 with Result JSON payload: " + JSON::stringify(result));
			}
			p_response->set_status(200);
			p_response->set_json(JustAMCPJsonRpcTransport::sanitize_wire_rpc(result));
		}
	}
}

Dictionary JustAMCPServer::_handle_json_rpc(const String &p_body, Ref<HTTPResponse> p_response) {
	return JustAMCPJsonRpcTransport::handle_json_rpc(this, p_body, p_response);
}

void JustAMCPServer::_send_sse_routed(const String &p_json_string, const String &p_session_id, int p_connection_id) {
	notification_bus.send_routed_json(p_json_string, p_session_id, p_connection_id);
	if (!session_manager) {
		return;
	}

	if (p_connection_id >= 0) {
		session_manager->complete_post_sse_stream_if_needed(p_connection_id);
		return;
	}
	if (p_session_id.is_empty()) {
		return;
	}
	const Vector<int> session_connections = session_manager->collect_session_connections(p_session_id);
	for (int i = 0; i < session_connections.size(); i++) {
		if (session_manager->is_post_stream_connection(session_connections[i])) {
			session_manager->complete_post_sse_stream_if_needed(session_connections[i]);
			break;
		}
	}
}

void JustAMCPServer::_send_sse_message(const String &p_json_string) {
	notification_bus.send_routed_json(p_json_string, String(), -1);
}

void JustAMCPServer::_apply_oauth_www_authenticate(Ref<HTTPResponse> p_response) {
	if (p_response.is_valid()) {
		p_response->add_header("WWW-Authenticate", JustAMCPOauthDiscovery::www_authenticate_header());
	}
}

void JustAMCPServer::_handle_oauth_protected_resource(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	(void)p_context;
	if (!JustAMCPOauthDiscovery::oauth_enabled()) {
		p_response->set_status(404);
		p_response->set_body("Not Found");
		return;
	}
	p_response->set_status(200);
	p_response->set_json(JustAMCPOauthDiscovery::protected_resource_metadata());
}

void JustAMCPServer::_handle_oauth_authorization_server(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	(void)p_context;
	if (!JustAMCPOauthDiscovery::oauth_enabled()) {
		p_response->set_status(404);
		p_response->set_body("Not Found");
		return;
	}
	p_response->set_status(200);
	p_response->set_json(JustAMCPOauthDiscovery::authorization_server_metadata());
}

#ifdef TOOLS_ENABLED
void JustAMCPServer::_handle_oauth_callback(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	Dictionary query = p_context.is_valid() ? p_context->get_query_params() : Dictionary();
	Dictionary result = JustAMCPMCPClientOAuth::handle_loopback_callback(query);
	p_response->set_status(result.get("ok", false) ? 200 : 400);
	p_response->set_content_type("text/html; charset=utf-8");
	if (result.get("ok", false)) {
		p_response->set_body("<!DOCTYPE html><html><body><p>JustAMCP OAuth completed. You can close this window.</p></body></html>");
	} else {
		p_response->set_body("<!DOCTYPE html><html><body><p>JustAMCP OAuth failed: " + String(result.get("error", "unknown")).xml_escape() + "</p></body></html>");
	}
}

void JustAMCPServer::_handle_oauth_client_metadata(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	(void)p_context;
	p_response->set_status(200);
	p_response->set_json(JustAMCPMCPClientOAuth::client_metadata_document(get_listening_port(), String()));
}

void JustAMCPServer::_handle_mcp_apps_host(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	if (session_manager && !MCPSessionManager::validate_origin(p_context)) {
		p_response->set_status(403);
		p_response->set_body("Forbidden");
		return;
	}
	if (!JustAMCPMCPAppsHost::get_singleton() || p_context.is_null()) {
		p_response->set_status(404);
		p_response->set_body("Not found");
		return;
	}
	const String uri = p_context->get_query_param("uri");
	const Dictionary app = JustAMCPMCPAppsHost::get_singleton()->get_open_app(uri);
	const String stored_html = String(app.get("html", ""));
	if (app.is_empty() || stored_html.is_empty()) {
		p_response->set_status(404);
		p_response->set_body("Not found");
		return;
	}
	const String csp = String(app.get("csp", JustAMCPMCPAppsHost::csp_header(Dictionary())));
	p_response->set_status(200);
	p_response->set_content_type("text/html; charset=utf-8");
	p_response->add_header("Content-Security-Policy", csp);
	p_response->set_body(JustAMCPMCPAppsHost::host_page_html(stored_html, csp));
}

void JustAMCPServer::_handle_mcp_apps_proxy(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	if (session_manager && !MCPSessionManager::validate_origin(p_context)) {
		p_response->set_status(403);
		p_response->set_body("Forbidden");
		return;
	}
	if (p_context.is_null() || p_context->get_body().strip_edges().is_empty()) {
		p_response->set_status(400);
		p_response->set_body("JSON object required");
		return;
	}
	Ref<JSON> json;
	json.instantiate();
	if (json->parse(p_context->get_body()) != OK || json->get_data().get_type() != Variant::DICTIONARY) {
		p_response->set_status(400);
		p_response->set_body("JSON object required");
		return;
	}
	const Dictionary msg = json->get_data();
	const Dictionary params = msg.has("params") && msg["params"].get_type() == Variant::DICTIONARY ? Dictionary(msg["params"]) : Dictionary();
	const String tool_name = String(params.get("name", ""));
	if (!justamcp_is_valid_mcp_tool_name(tool_name)) {
		p_response->set_status(400);
		p_response->set_body(justamcp_invalid_mcp_tool_name_message(tool_name));
		return;
	}
	const String bridge_name = String(params.get("bridge", params.get("bridge_name", "")));
	const Dictionary arguments = params.has("arguments") && params["arguments"].get_type() == Variant::DICTIONARY ? Dictionary(params["arguments"]) : Dictionary();
	Dictionary result;
	if (JustAMCPMCPAppsHost::get_singleton()) {
		result = JustAMCPMCPAppsHost::get_singleton()->proxy_tools_call(bridge_name, tool_name, arguments, false);
	} else {
		result["ok"] = false;
		result["error"] = "MCP Apps host is unavailable.";
	}
	Dictionary rpc;
	rpc["jsonrpc"] = "2.0";
	rpc["id"] = msg.get("id", Variant());
	if (result.get("ok", false)) {
		rpc["result"] = result.get("result", result);
	} else {
		Dictionary error;
		error["message"] = result.get("error", "Proxy failed.");
		rpc["error"] = error;
	}
	p_response->set_status(200);
	p_response->set_json(JustAMCPJsonRpcTransport::sanitize_wire_rpc(rpc));
}
#endif

#endif
