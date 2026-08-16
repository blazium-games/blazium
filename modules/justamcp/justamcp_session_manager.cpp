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
#include "modules/modules_enabled.gen.h"

#include "justamcp_pagination.h"
#include "justamcp_server.h"
#include "tools/justamcp_json_rpc_helpers.h"

#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "core/templates/list.h"

#ifdef TOOLS_ENABLED
#include "editor/settings/editor_settings.h"
#endif

#if defined(MODULE_HTTPSERVER_ENABLED)
#include "modules/httpserver/http_server.h"
#endif

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
}

static String _cli_protocol_version_override;
static bool _cli_protocol_version_parsed = false;

static const char *const _supported_protocol_versions[] = {
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
#ifdef TOOLS_ENABLED
	bool use_project = true;
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/justamcp/override_editor_settings")) {
		use_project = GLOBAL_GET("blazium/justamcp/override_editor_settings");
	}
	if (!use_project && EditorSettings::get_singleton() && EditorSettings::get_singleton()->has_setting("blazium/justamcp/protocol_version")) {
		return String(EditorSettings::get_singleton()->get_setting("blazium/justamcp/protocol_version"));
	}
#endif
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/justamcp/protocol_version")) {
		return String(GLOBAL_GET("blazium/justamcp/protocol_version"));
	}
	return String();
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

String MCPSessionManager::negotiate_protocol_version(const String &p_client_version) {
	if (is_supported_protocol_version(p_client_version)) {
		return p_client_version;
	}
	return latest_protocol_version();
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
	const String allowed = GLOBAL_GET("blazium/justamcp/streamable_http_allowed_origin");
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
	const bool strict = GLOBAL_GET("blazium/justamcp/streamable_http_strict_origin");
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
	const String header = get_header(p_context, "MCP-Protocol-Version");
	if (header.is_empty()) {
		return true;
	}
	static const char *supported[] = {
		"2025-11-25",
		"2025-06-18",
		"2025-03-26",
		"2024-11-05",
		nullptr
	};
	for (int i = 0; supported[i]; i++) {
		if (header == supported[i]) {
			return true;
		}
	}
	r_error = "Unsupported MCP-Protocol-Version: " + header;
	return false;
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
	p_response->add_header("Access-Control-Allow-Headers", "Content-Type, Authorization, MCP-Session-Id, MCP-Protocol-Version, Last-Event-ID, Accept, X-Client-Id, X-Client-Secret");
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
