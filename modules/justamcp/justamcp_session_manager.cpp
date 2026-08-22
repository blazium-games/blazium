/**************************************************************************/
/*  justamcp_session_manager.cpp                                          */
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

#include "justamcp_session_manager.h"

#include "justamcp_server.h"
#include "tools/justamcp_settings_resolver.h"

#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "core/templates/list.h"

#include "modules/modules_enabled.gen.h"

MCPSessionManager::MCPSessionManager(JustAMCPServer *p_owner) {
	owner = p_owner;
}

void MCPSessionManager::clear_all() {
	MutexLock lock(mutex);
	sessions.clear();
	connection_to_session.clear();
	pending_post_sse_by_session.clear();
	post_sse_upgrade_sessions.clear();
	all_sse_connection_ids.clear();
	request_router.clear_all();
#if defined(MODULE_HTTPSERVER_ENABLED)
	pending_modern_listen = 0;
#endif
}

static String _cli_protocol_version_override;
static bool _cli_protocol_version_parsed = false;

static const char *const _supported_protocol_versions[] = {
	"2026-07-28",
	"2025-11-25",
	"2025-06-18",
	"2025-03-26",
	"2024-11-05",
	nullptr
};

bool MCPSessionManager::is_supported_protocol_version(const String &p_version) {
	for (int i = 0; _supported_protocol_versions[i]; i++) {
		if (p_version == _supported_protocol_versions[i]) {
			return true;
		}
	}
	return false;
}

bool MCPSessionManager::is_modern_protocol_version(const String &p_version) {
	return p_version == "2026-07-28";
}

bool MCPSessionManager::is_legacy_protocol_version(const String &p_version) {
	return is_supported_protocol_version(p_version) && !is_modern_protocol_version(p_version);
}

static String _configured_accepted_protocol_versions() {
	return JustAMCPSettingsResolver::resolve_string("blazium/justamcp/accepted_protocol_versions");
}

Vector<String> MCPSessionManager::supported_protocol_versions() {
	Vector<String> versions;
	for (int i = 0; _supported_protocol_versions[i]; i++) {
		versions.push_back(_supported_protocol_versions[i]);
	}
	return versions;
}

Vector<String> MCPSessionManager::accepted_protocol_versions() {
	const String configured = _configured_accepted_protocol_versions().strip_edges();
	if (configured.is_empty()) {
		return supported_protocol_versions();
	}
	Vector<String> accepted;
	const Vector<String> parts = configured.split(",", false);
	for (int i = 0; i < parts.size(); i++) {
		const String version = parts[i].strip_edges();
		if (is_supported_protocol_version(version) && accepted.find(version) < 0) {
			accepted.push_back(version);
		}
	}
	if (accepted.is_empty()) {
		return supported_protocol_versions();
	}
	return accepted;
}

bool MCPSessionManager::is_accepted_protocol_version(const String &p_version) {
	if (!is_supported_protocol_version(p_version)) {
		return false;
	}
	const Vector<String> accepted = accepted_protocol_versions();
	return accepted.find(p_version) >= 0;
}

String MCPSessionManager::first_protocol_version_token(const String &p_header) {
	const Vector<String> parts = p_header.split(",", false);
	for (int i = 0; i < parts.size(); i++) {
		const String version = parts[i].strip_edges();
		if (!version.is_empty()) {
			return version;
		}
	}
	return String();
}

Dictionary MCPSessionManager::header_mismatch_error(const Variant &p_id, const String &p_message) {
	Dictionary err;
	err["jsonrpc"] = "2.0";
	if (p_id.get_type() != Variant::NIL) {
		err["id"] = p_id;
	}
	Dictionary error_dict;
	error_dict["code"] = -32020;
	error_dict["message"] = p_message;
	err["error"] = error_dict;
	return err;
}

Dictionary MCPSessionManager::mcp_server_info() {
	Dictionary info;
	info["name"] = "blazium-mcp-server";
	info["title"] = "Blazium MCP";
	info["version"] = "1.0.0";
	info["websiteUrl"] = "https://blazium.app";
	return info;
}

String MCPSessionManager::decode_mcp_header_value(const String &p_value) {
	const String value = p_value.strip_edges();
	if (value.begins_with("=?base64?") && value.ends_with("?=") && value.length() >= 11) {
		const String b64 = value.substr(9, value.length() - 11);
		const CharString cstr = b64.ascii();
		Vector<uint8_t> buf;
		buf.resize(b64.length() / 4 * 3 + 1);
		size_t decoded_len = 0;
		uint8_t *w = buf.ptrw();
		if (CryptoCore::b64_decode(&w[0], buf.size(), &decoded_len, (unsigned char *)cstr.get_data(), b64.length()) != OK) {
			return value;
		}
		buf.resize(decoded_len);
		return String::utf8((const char *)buf.ptr(), buf.size());
	}
	return value;
}

int MCPSessionManager::modern_http_status_for_rpc(const Dictionary &p_rpc) {
	if (!p_rpc.has("error") || p_rpc["error"].get_type() != Variant::DICTIONARY) {
		return 200;
	}
	const int code = int(Dictionary(p_rpc["error"]).get("code", 0));
	if (code == -32601) {
		return 404;
	}
	if (code == -32020 || code == -32022 || code == -32700 || code == -32600 || code == -32602) {
		return 400;
	}
	return 200;
}

String MCPSessionManager::protocol_version_from_payload(const Dictionary &p_payload) {
	if (!p_payload.has("params") || p_payload["params"].get_type() != Variant::DICTIONARY) {
		return String();
	}
	const Dictionary params = p_payload["params"];
	if (!params.has("_meta") || params["_meta"].get_type() != Variant::DICTIONARY) {
		return String();
	}
	const Dictionary meta = params["_meta"];
	if (meta.has("io.modelcontextprotocol/protocolVersion")) {
		return String(meta["io.modelcontextprotocol/protocolVersion"]);
	}
	return String();
}

void MCPSessionManager::decorate_modern_rpc(Dictionary &p_rpc, const String &p_method) {
	if (p_rpc.is_empty() || p_rpc.has("error") || !p_rpc.has("result") || p_rpc["result"].get_type() != Variant::DICTIONARY) {
		return;
	}
	Dictionary result = p_rpc["result"];
	if (!result.has("resultType")) {
		result["resultType"] = "complete";
	}
	Dictionary meta = result.has("_meta") && result["_meta"].get_type() == Variant::DICTIONARY ? Dictionary(result["_meta"]) : Dictionary();
	meta["io.modelcontextprotocol/serverInfo"] = mcp_server_info();
	result["_meta"] = meta;
	if (p_method == "server/discover" || p_method == "tools/list" || p_method == "prompts/list" || p_method == "resources/list" || p_method == "resources/read" || p_method == "resources/templates/list") {
		if (!result.has("ttlMs")) {
			result["ttlMs"] = 3600000;
		}
		if (!result.has("cacheScope")) {
			result["cacheScope"] = "public";
		}
	}
	p_rpc["result"] = result;
}

Dictionary MCPSessionManager::unsupported_protocol_version_error(const String &p_requested) {
	Dictionary err;
	err["jsonrpc"] = "2.0";
	Dictionary error_dict;
	error_dict["code"] = -32022;
	error_dict["message"] = "Unsupported protocol version";
	Dictionary data;
	Array supported;
	const Vector<String> accepted = accepted_protocol_versions();
	for (int i = 0; i < accepted.size(); i++) {
		supported.push_back(accepted[i]);
	}
	data["supported"] = supported;
	data["requested"] = p_requested;
	error_dict["data"] = data;
	err["error"] = error_dict;
	return err;
}

static void _ensure_cli_protocol_version_parsed() {
	if (_cli_protocol_version_parsed) {
		return;
	}
	_cli_protocol_version_parsed = true;
	if (!OS::get_singleton()) {
		return;
	}
	const List<String> &args = OS::get_singleton()->get_cmdline_args();
	for (const List<String>::Element *E = args.front(); E; E = E->next()) {
		if (E->get() == "--mcp-protocol-version" && E->next()) {
			const String version = E->next()->get();
			if (MCPSessionManager::is_supported_protocol_version(version)) {
				_cli_protocol_version_override = version;
			} else {
				ERR_PRINT("JustAMCP: --mcp-protocol-version '" + version + "' is not supported. Keeping setting/default.");
			}
			break;
		}
	}
}

bool MCPSessionManager::set_cli_protocol_version_override(const String &p_version) {
	_cli_protocol_version_parsed = true;
	if (!is_supported_protocol_version(p_version)) {
		return false;
	}
	_cli_protocol_version_override = p_version;
	return true;
}

void MCPSessionManager::clear_cli_protocol_version_override() {
	_cli_protocol_version_override = String();
	_cli_protocol_version_parsed = true;
}

static String _configured_protocol_version() {
	return JustAMCPSettingsResolver::resolve_string("blazium/justamcp/protocol_version");
}

String MCPSessionManager::latest_protocol_version() {
	_ensure_cli_protocol_version_parsed();
	if (!_cli_protocol_version_override.is_empty()) {
		return _cli_protocol_version_override;
	}
	const String configured = _configured_protocol_version();
	if (configured.is_empty()) {
		return String(hardcoded_latest_protocol_version());
	}
	if (is_supported_protocol_version(configured)) {
		return configured;
	}
	ERR_PRINT("JustAMCP: invalid protocol_version '" + configured + "', falling back to " + String(hardcoded_latest_protocol_version()) + ".");
	return String(hardcoded_latest_protocol_version());
}

String MCPSessionManager::latest_legacy_protocol_version() {
	const Vector<String> accepted = accepted_protocol_versions();
	for (int i = 0; i < accepted.size(); i++) {
		if (is_legacy_protocol_version(accepted[i])) {
			return accepted[i];
		}
	}
	return String(hardcoded_latest_legacy_protocol_version());
}

String MCPSessionManager::negotiate_protocol_version(const String &p_client_version) {
	if (is_accepted_protocol_version(p_client_version)) {
		return p_client_version;
	}
	return latest_protocol_version();
}

String MCPSessionManager::negotiate_legacy_initialize(const String &p_client_version) {
	if (is_legacy_protocol_version(p_client_version) && is_accepted_protocol_version(p_client_version)) {
		return p_client_version;
	}
	return latest_legacy_protocol_version();
}

#if defined(MODULE_HTTPSERVER_ENABLED)

static String _get_header_from_dict(const Dictionary &p_headers, const String &p_name) {
	if (p_headers.has(p_name)) {
		return String(p_headers[p_name]);
	}
	const String lower = p_name.to_lower();
	for (const Variant &key : p_headers.keys()) {
		if (String(key).to_lower() == lower) {
			return String(p_headers[key]);
		}
	}
	return String();
}

String MCPSessionManager::get_header(const Ref<HTTPRequestContext> &p_context, const String &p_name) {
	if (p_context.is_null()) {
		return String();
	}
	const Dictionary headers = p_context->get_headers();
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

bool MCPSessionManager::is_allowed_origin_string(const String &p_origin) {
	if (p_origin.is_empty()) {
		return false;
	}
	const String allowed = JustAMCPSettingsResolver::resolve_string("blazium/justamcp/streamable_http_allowed_origin");
	if (!allowed.is_empty()) {
		return p_origin == allowed;
	}

	String origin = p_origin.strip_edges();
	if (origin == "vscode-file://vscode-app" || origin == "https://cursor.sh" || origin == "https://www.cursor.com") {
		return true;
	}
	String scheme;
	if (origin.begins_with("https://")) {
		scheme = "https";
		origin = origin.substr(8);
	} else if (origin.begins_with("http://")) {
		scheme = "http";
		origin = origin.substr(7);
	} else {
		return false;
	}
	(void)scheme;

	if (origin.contains("/") || origin.contains("?") || origin.contains("#") || origin.contains("@")) {
		return false;
	}

	String host = origin;
	if (host.begins_with("[")) {
		const int close = host.find("]");
		if (close < 0) {
			return false;
		}
		const String ipv6 = host.substr(1, close - 1);
		if (ipv6 != "::1") {
			return false;
		}
		const String rest = host.substr(close + 1);
		if (!rest.is_empty() && !rest.begins_with(":")) {
			return false;
		}
		if (rest.begins_with(":")) {
			const String port = rest.substr(1);
			if (!port.is_valid_int() || port.to_int() <= 0 || port.to_int() > 65535) {
				return false;
			}
		}
		return true;
	}

	const int colon = host.find(":");
	String port;
	if (colon >= 0) {
		port = host.substr(colon + 1);
		host = host.substr(0, colon);
		if (!port.is_valid_int() || port.to_int() <= 0 || port.to_int() > 65535) {
			return false;
		}
	}
	return host == "127.0.0.1" || host == "localhost" || host == "::1";
}

bool MCPSessionManager::validate_origin(const Ref<HTTPRequestContext> &p_context) {
	const bool strict = JustAMCPSettingsResolver::resolve_bool("blazium/justamcp/streamable_http_strict_origin", false);
	if (!strict) {
		return true;
	}
	const String origin = get_header(p_context, "Origin");

	if (origin.is_empty()) {
		const String method = p_context.is_valid() ? p_context->get_method().to_upper() : String();
		return method != "OPTIONS";
	}
	return is_allowed_origin_string(origin);
}

bool MCPSessionManager::validate_protocol_header(const Ref<HTTPRequestContext> &p_context, String &r_error) {
	const String header = get_header(p_context, "MCP-Protocol-Version").strip_edges();
	if (header.is_empty()) {
		return true;
	}
	// RFC 9110 combines duplicate request headers with commas. Cursor also
	// concatenates mcp.json headers with the protocol version it already sends,
	// producing values such as "2025-11-25, 2025-11-25".
	const Vector<String> parts = header.split(",", false);
	bool saw_version = false;
	for (int i = 0; i < parts.size(); i++) {
		const String version = parts[i].strip_edges();
		if (version.is_empty()) {
			continue;
		}
		if (!is_accepted_protocol_version(version)) {
			r_error = "Unsupported MCP-Protocol-Version: " + header;
			return false;
		}
		saw_version = true;
	}
	if (!saw_version) {
		r_error = "Unsupported MCP-Protocol-Version: " + header;
		return false;
	}
	return true;
}

bool MCPSessionManager::accepts_json_and_sse_header(const String &p_accept) {
	if (p_accept.is_empty()) {
		return false;
	}
	return p_accept.contains("application/json") && p_accept.contains("text/event-stream");
}

bool MCPSessionManager::accepts_json_and_sse(const Ref<HTTPRequestContext> &p_context) {
	return accepts_json_and_sse_header(get_header(p_context, "Accept"));
}

bool MCPSessionManager::accepts_event_stream(const Ref<HTTPRequestContext> &p_context) {
	const String accept = get_header(p_context, "Accept");
	if (accept.is_empty()) {
		return false;
	}
	return accept.contains("text/event-stream") || accept.contains("*/*");
}

void MCPSessionManager::apply_cors_headers(Ref<HTTPResponse> p_response, const Ref<HTTPRequestContext> &p_context) const {
	const String origin = get_header(p_context, "Origin");

	if (!origin.is_empty() && is_allowed_origin_string(origin)) {
		p_response->add_header("Access-Control-Allow-Origin", origin);
	}
	p_response->add_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
	p_response->add_header("Access-Control-Allow-Headers", "Content-Type, Authorization, MCP-Session-Id, MCP-Protocol-Version, Last-Event-ID, Accept, X-Client-Id, X-Client-Secret, Mcp-Method, Mcp-Name, Mcp-Param-*");
}

void MCPSessionManager::handle_cors_preflight(const Ref<HTTPRequestContext> &p_context, Ref<HTTPResponse> p_response) const {
	if (!validate_origin(p_context)) {
		p_response->set_status(403);
		p_response->set_body("Forbidden - Invalid Origin");
		return;
	}
	apply_cors_headers(p_response, p_context);
	p_response->set_status(204);
	p_response->set_body("");
}

void MCPSessionManager::on_sse_connection_opened(int p_connection_id, const String &p_path, const Dictionary &p_headers) {
	if (p_path != "/mcp") {
		return;
	}

	bool claimed_modern_listen = false;
	{
		MutexLock lock(mutex);
		if (pending_modern_listen > 0) {
			pending_modern_listen--;
			all_sse_connection_ids.insert(p_connection_id);
			claimed_modern_listen = true;
		}
	}
	if (claimed_modern_listen) {
		if (HTTPServer::get_singleton()) {
			Dictionary ack;
			ack["jsonrpc"] = "2.0";
			ack["method"] = "notifications/subscriptions/acknowledged";
			ack["params"] = Dictionary();
			HTTPServer::get_singleton()->send_sse_event(p_connection_id, "message", JSON::stringify(ack));
			HTTPServer::get_singleton()->send_sse_comment(p_connection_id, "keepalive");
		}
		return;
	}

	const String session_id = _get_header_from_dict(p_headers, "MCP-Session-Id");
	const String accept = _get_header_from_dict(p_headers, "Accept");
	const bool streamable_accept = accepts_json_and_sse_header(accept);

	bool has_pending_post = false;
	{
		MutexLock lock(mutex);
		if (!session_id.is_empty() && pending_post_sse_by_session.has(session_id)) {
			if (post_sse_upgrade_sessions.has(session_id)) {
				pending_post_sse_by_session[session_id].claim_armed = true;
				post_sse_upgrade_sessions.erase(session_id);
			}
			const PendingPostSse &pending = pending_post_sse_by_session[session_id];

			if (pending.claim_armed && (!pending.requires_json_and_sse_accept || streamable_accept)) {
				has_pending_post = true;
			}
		}
	}

	if (has_pending_post) {
		_process_post_sse_opened(p_connection_id, session_id);
		return;
	}

	const String last_event_id = _get_header_from_dict(p_headers, "Last-Event-ID");
	if (!session_id.is_empty()) {
		_process_get_sse_opened(p_connection_id, session_id, last_event_id);
	}
}

void MCPSessionManager::on_sse_connection_closed(int p_connection_id) {
	MutexLock lock(mutex);
	_unregister_connection(p_connection_id);
	_prune_expired_pending_post_sse();
	lock.temp_unlock();
	clear_request_routes_for_connection(p_connection_id);
	prune_request_routes();
}

#endif
