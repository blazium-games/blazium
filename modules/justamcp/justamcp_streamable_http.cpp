/**************************************************************************/
/*  justamcp_streamable_http.cpp                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#include "modules/modules_enabled.gen.h"
#include "justamcp_streamable_http.h"

#include "justamcp_pagination.h"
#include "justamcp_server.h"

#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/io/json.h"
#include "core/os/time.h"

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
	has_pending_post_sse = false;
	tool_route_session_id = "";
	tool_route_connection_id = -1;
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

bool MCPSessionManager::validate_origin(const Ref<HTTPRequestContext> &p_context) {
	const bool bind_localhost = GLOBAL_GET("blazium/justamcp/bind_to_localhost_only");
	if (bind_localhost) {
		return true;
	}
	const bool strict = GLOBAL_GET("blazium/justamcp/streamable_http_strict_origin");
	if (!strict) {
		return true;
	}
	const String origin = get_header(p_context, "Origin");
	if (origin.is_empty()) {
		return true;
	}
	const String allowed = GLOBAL_GET("blazium/justamcp/streamable_http_allowed_origin");
	if (allowed.is_empty()) {
		return origin.begins_with("http://127.0.0.1") || origin.begins_with("http://localhost");
	}
	return origin == allowed;
}

bool MCPSessionManager::validate_protocol_header(const Ref<HTTPRequestContext> &p_context, String &r_error) {
	const String header = get_header(p_context, "MCP-Protocol-Version");
	if (header.is_empty()) {
		return true;
	}
	static const char *supported[] = {
		"2025-11-25",
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

bool MCPSessionManager::accepts_json_and_sse(const Ref<HTTPRequestContext> &p_context) {
	const String accept = get_header(p_context, "Accept");
	if (accept.is_empty()) {
		return false;
	}
	return accept.contains("application/json") && accept.contains("text/event-stream");
}

bool MCPSessionManager::accepts_event_stream(const Ref<HTTPRequestContext> &p_context) {
	const String accept = get_header(p_context, "Accept");
	if (accept.is_empty()) {
		return false;
	}
	return accept.contains("text/event-stream") || accept.contains("*/*");
}

String MCPSessionManager::negotiate_protocol_version(const String &p_client_version) {
	static const char *preferred[] = {
		"2025-11-25",
		"2025-03-26",
		"2024-11-05",
		nullptr
	};
	for (int i = 0; preferred[i]; i++) {
		if (p_client_version == preferred[i]) {
			return String(preferred[i]);
		}
	}
	return String("2024-11-05");
}

void MCPSessionManager::apply_cors_headers(Ref<HTTPResponse> p_response, const Ref<HTTPRequestContext> &p_context) const {
	const String origin = get_header(p_context, "Origin");
	if (!origin.is_empty()) {
		p_response->add_header("Access-Control-Allow-Origin", origin);
	} else if (HTTPServer::get_singleton()) {
		p_response->add_header("Access-Control-Allow-Origin", HTTPServer::get_singleton()->get_cors_origin());
	}
	p_response->add_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
	p_response->add_header("Access-Control-Allow-Headers", "Content-Type, Authorization, MCP-Session-Id, MCP-Protocol-Version, Last-Event-ID, Accept");
}

void MCPSessionManager::handle_cors_preflight(const Ref<HTTPRequestContext> &p_context, Ref<HTTPResponse> p_response) const {
	apply_cors_headers(p_response, p_context);
	p_response->set_status(204);
	p_response->set_body("");
}

String MCPSessionManager::_generate_session_id() const {
	CryptoCore::RandomGenerator rng;
	ERR_FAIL_COND_V(rng.init() != OK, String());
	uint8_t bytes[16];
	ERR_FAIL_COND_V(rng.get_random_bytes(bytes, sizeof(bytes)) != OK, String());
	String id;
	static const char *hex = "0123456789abcdef";
	for (int i = 0; i < 16; i++) {
		const uint8_t b = bytes[i];
		id += String::chr(hex[(b >> 4) & 0xf]);
		id += String::chr(hex[b & 0xf]);
		if (i == 3 || i == 5 || i == 7 || i == 9) {
			id += "-";
		}
	}
	return id;
}

String MCPSessionManager::_generate_stream_id() {
	return "stream-" + itos(next_stream_counter++);
}

MCPSession *MCPSessionManager::_get_session(const String &p_session_id) {
	if (!sessions.has(p_session_id)) {
		return nullptr;
	}
	return &sessions[p_session_id];
}

const MCPSession *MCPSessionManager::_get_session(const String &p_session_id) const {
	if (!sessions.has(p_session_id)) {
		return nullptr;
	}
	return &sessions[p_session_id];
}

MCPSSEStream *MCPSessionManager::_get_stream_for_connection(int p_connection_id) {
	if (!connection_to_session.has(p_connection_id)) {
		return nullptr;
	}
	const String session_id = connection_to_session[p_connection_id];
	MCPSession *session = _get_session(session_id);
	if (!session) {
		return nullptr;
	}
	if (!session->connection_to_stream.has(p_connection_id)) {
		return nullptr;
	}
	const String stream_id = session->connection_to_stream[p_connection_id];
	return session->streams.getptr(stream_id);
}

void MCPSessionManager::_touch_session(MCPSession &p_session) {
	p_session.last_activity_usec = Time::get_singleton()->get_ticks_usec();
}

void MCPSessionManager::_expire_sessions() {
	const int ttl_seconds = GLOBAL_GET("blazium/justamcp/session_ttl_seconds");
	if (ttl_seconds <= 0) {
		return;
	}
	const uint64_t now = Time::get_singleton()->get_ticks_usec();
	const uint64_t ttl_usec = (uint64_t)ttl_seconds * 1000000ULL;
	Vector<String> expired;
	for (const KeyValue<String, MCPSession> &E : sessions) {
		if (now - E.value.last_activity_usec > ttl_usec) {
			expired.push_back(E.key);
		}
	}
	for (int i = 0; i < expired.size(); i++) {
		const MCPSession &session = sessions[expired[i]];
		for (const KeyValue<int, String> &conn : session.connection_to_stream) {
			connection_to_session.erase(conn.key);
			if (HTTPServer::get_singleton()) {
				HTTPServer::get_singleton()->close_sse_connection(conn.key);
			}
		}
		sessions.erase(expired[i]);
	}
}

bool MCPSessionManager::_register_stream(MCPSession &p_session, int p_connection_id, bool p_is_post_stream, MCPSSEStream *&r_stream) {
	MCPSSEStream stream;
	stream.stream_id = _generate_stream_id();
	stream.session_id = p_session.session_id;
	stream.http_connection_id = p_connection_id;
	stream.is_post_stream = p_is_post_stream;
	stream.event_store.configure(p_session.session_id, stream.stream_id, justamcp_mcp_log_buffer_size());
	p_session.streams.insert(stream.stream_id, stream);
	p_session.connection_to_stream.insert(p_connection_id, stream.stream_id);
	connection_to_session.insert(p_connection_id, p_session.session_id);
	r_stream = p_session.streams.getptr(stream.stream_id);
	return r_stream != nullptr;
}

void MCPSessionManager::_unregister_connection(int p_connection_id) {
	if (!connection_to_session.has(p_connection_id)) {
		return;
	}
	const String session_id = connection_to_session[p_connection_id];
	connection_to_session.erase(p_connection_id);
	MCPSession *session = _get_session(session_id);
	if (!session) {
		return;
	}
	if (session->connection_to_stream.has(p_connection_id)) {
		const String stream_id = session->connection_to_stream[p_connection_id];
		session->streams.erase(stream_id);
		session->connection_to_stream.erase(p_connection_id);
	}
	if (session->streams.is_empty()) {
		sessions.erase(session_id);
	}
}

void MCPSessionManager::_replay_stream_events(MCPSSEStream &p_stream, const String &p_last_event_id) {
	if (!HTTPServer::get_singleton() || p_last_event_id.is_empty()) {
		return;
	}
	const Vector<MCPEventRecord> replay = p_stream.event_store.events_after(p_last_event_id);
	for (int i = 0; i < replay.size(); i++) {
		HTTPServer::get_singleton()->send_sse_event(p_stream.http_connection_id, replay[i].event_type, replay[i].data, replay[i].id);
	}
}

void MCPSessionManager::_send_priming_event(int p_connection_id) {
	send_json_on_connection(p_connection_id, "", "");
}

bool MCPSessionManager::send_json_on_connection(int p_connection_id, const String &p_json, const String &p_event_type) {
	if (!HTTPServer::get_singleton()) {
		return false;
	}
	MutexLock lock(mutex);
	MCPSSEStream *stream = _get_stream_for_connection(p_connection_id);
	String event_id;
	if (stream) {
		event_id = stream->event_store.append_event(p_event_type, p_json);
	}
	lock.temp_unlock();
	const Error err = HTTPServer::get_singleton()->send_sse_event(p_connection_id, p_event_type, p_json, event_id);
	return err == OK;
}

bool MCPSessionManager::send_json_to_session(const String &p_session_id, const String &p_json, int p_preferred_connection_id) {
	MutexLock lock(mutex);
	const MCPSession *session = _get_session(p_session_id);
	if (!session) {
		return false;
	}
	int target = p_preferred_connection_id;
	if (target < 0) {
		target = session->active_tool_connection_id;
	}
	if (target < 0 && !session->connection_to_stream.is_empty()) {
		target = session->connection_to_stream.begin()->key;
	}
	lock.temp_unlock();
	if (target < 0) {
		return false;
	}
	return send_json_on_connection(target, p_json);
}

bool MCPSessionManager::send_json_legacy_connection(int p_connection_id, const String &p_json) {
	if (!HTTPServer::get_singleton()) {
		return false;
	}
	return HTTPServer::get_singleton()->send_sse_event(p_connection_id, "message", p_json) == OK;
}

void MCPSessionManager::bind_tool_route(const String &p_session_id, int p_connection_id) {
	MutexLock lock(mutex);
	tool_route_session_id = p_session_id;
	tool_route_connection_id = p_connection_id;
	MCPSession *session = _get_session(p_session_id);
	if (session) {
		session->active_tool_connection_id = p_connection_id;
	}
}

void MCPSessionManager::clear_tool_route() {
	MutexLock lock(mutex);
	tool_route_session_id = "";
	tool_route_connection_id = -1;
}

String MCPSessionManager::get_tool_route_session_id() const {
	MutexLock lock(mutex);
	return tool_route_session_id;
}

int MCPSessionManager::get_tool_route_connection_id() const {
	MutexLock lock(mutex);
	return tool_route_connection_id;
}

bool MCPSessionManager::session_exists(const String &p_session_id) const {
	MutexLock lock(mutex);
	return sessions.has(p_session_id);
}

void MCPSessionManager::broadcast_json(const String &p_json) {
	Vector<int> connections;
	{
		MutexLock lock(mutex);
		for (const KeyValue<String, MCPSession> &sess : sessions) {
			for (const KeyValue<String, MCPSSEStream> &stream : sess.value.streams) {
				connections.push_back(stream.value.http_connection_id);
			}
		}
	}
	for (int i = 0; i < connections.size(); i++) {
		send_json_on_connection(connections[i], p_json);
	}
}

void MCPSessionManager::_process_post_sse_opened(int p_connection_id) {
	PendingPostSse pending;
	{
		MutexLock lock(mutex);
		if (!has_pending_post_sse) {
			return;
		}
		pending = pending_post_sse;
		has_pending_post_sse = false;
	}

	String session_id = pending.session_id;
	{
		MutexLock lock(mutex);
		_expire_sessions();
		MCPSession *session = _get_session(session_id);
		if (!session) {
			if (HTTPServer::get_singleton()) {
				HTTPServer::get_singleton()->close_sse_connection(p_connection_id);
			}
			return;
		}
		MCPSSEStream *stream = nullptr;
		if (!_register_stream(*session, p_connection_id, true, stream) || stream == nullptr) {
			return;
		}
		_touch_session(*session);
		bind_tool_route(session_id, p_connection_id);
	}

	if (HTTPServer::get_singleton()) {
		_send_priming_event(p_connection_id);
	}

	Dictionary result = owner->_transport_handle_json_rpc(pending.body, Ref<HTTPResponse>());
	if (!result.is_empty()) {
		send_json_on_connection(p_connection_id, JSON::stringify(result));
	}

	MutexLock lock(mutex);
	MCPSSEStream *stream = _get_stream_for_connection(p_connection_id);
	if (stream) {
		stream->response_sent = true;
	}
	lock.temp_unlock();

	if (HTTPServer::get_singleton()) {
		HTTPServer::get_singleton()->send_sse_retry(p_connection_id, 3000);
		HTTPServer::get_singleton()->close_sse_connection(p_connection_id);
	}
	clear_tool_route();
}

void MCPSessionManager::_process_get_sse_opened(int p_connection_id, const String &p_session_id, const String &p_last_event_id) {
	MutexLock lock(mutex);
	_expire_sessions();
	MCPSession *session = _get_session(p_session_id);
	if (!session) {
		lock.temp_unlock();
		if (HTTPServer::get_singleton()) {
			HTTPServer::get_singleton()->close_sse_connection(p_connection_id);
		}
		return;
	}
	MCPSSEStream *stream = nullptr;
	if (!_register_stream(*session, p_connection_id, false, stream) || stream == nullptr) {
		return;
	}
	_touch_session(*session);
	const String replay_from = p_last_event_id;
	if (stream->event_store.find_index_after(p_last_event_id) == -2 && !p_last_event_id.is_empty()) {
		lock.temp_unlock();
		if (HTTPServer::get_singleton()) {
			HTTPServer::get_singleton()->close_sse_connection(p_connection_id);
		}
		return;
	}
	lock.temp_unlock();

	_send_priming_event(p_connection_id);
	if (!replay_from.is_empty()) {
		MutexLock replay_lock(mutex);
		MCPSSEStream *replay_stream = _get_stream_for_connection(p_connection_id);
		if (replay_stream) {
			_replay_stream_events(*replay_stream, replay_from);
		}
	}
}

void MCPSessionManager::on_sse_connection_opened(int p_connection_id, const String &p_path, const Dictionary &p_headers) {
	if (p_path != "/mcp") {
		return;
	}

	MutexLock lock(mutex);
	const bool post_pending = has_pending_post_sse;
	lock.temp_unlock();

	if (post_pending) {
		_process_post_sse_opened(p_connection_id);
		return;
	}

	String get_session_id = _get_header_from_dict(p_headers, "MCP-Session-Id");
	const String last_event_id = _get_header_from_dict(p_headers, "Last-Event-ID");
	if (!get_session_id.is_empty()) {
		_process_get_sse_opened(p_connection_id, get_session_id, last_event_id);
	}
}

void MCPSessionManager::on_sse_connection_closed(int p_connection_id) {
	MutexLock lock(mutex);
	_unregister_connection(p_connection_id);
	if (tool_route_connection_id == p_connection_id) {
		tool_route_session_id = "";
		tool_route_connection_id = -1;
	}
}

bool MCPSessionManager::handle_mcp_get(const Ref<HTTPRequestContext> &p_context, Ref<HTTPResponse> p_response) {
	String protocol_error;
	if (!validate_protocol_header(p_context, protocol_error)) {
		p_response->set_status(400);
		p_response->set_body(protocol_error);
		return true;
	}
	if (!validate_origin(p_context)) {
		p_response->set_status(403);
		return true;
	}
	if (!accepts_event_stream(p_context)) {
		p_response->set_status(406);
		p_response->set_body("Accept must include text/event-stream");
		return true;
	}

	const String session_id = get_header(p_context, "MCP-Session-Id");
	if (session_id.is_empty()) {
		p_response->set_status(400);
		p_response->set_body("Missing MCP-Session-Id");
		return true;
	}

	MutexLock lock(mutex);
	_expire_sessions();
	if (!_get_session(session_id)) {
		p_response->set_status(404);
		p_response->set_body("Unknown or expired MCP session");
		return true;
	}
	_touch_session(*_get_session(session_id));
	lock.temp_unlock();

	apply_cors_headers(p_response, p_context);
	p_response->start_sse();
	return true;
}

bool MCPSessionManager::handle_mcp_post(const Ref<HTTPRequestContext> &p_context, Ref<HTTPResponse> p_response) {
	const String body = p_context->get_body();
	if (body.is_empty()) {
		p_response->set_status(400);
		Dictionary err;
		err["jsonrpc"] = "2.0";
		Dictionary error_dict;
		error_dict["code"] = -32700;
		error_dict["message"] = "Invalid JSON or empty body";
		err["error"] = error_dict;
		p_response->set_json(err);
		return true;
	}

	String protocol_error;
	if (!validate_protocol_header(p_context, protocol_error)) {
		p_response->set_status(400);
		p_response->set_body(protocol_error);
		return true;
	}
	if (!validate_origin(p_context)) {
		p_response->set_status(403);
		return true;
	}

	Ref<JSON> json;
	json.instantiate();
	if (json->parse(body) != OK) {
		p_response->set_status(400);
		return true;
	}
	const Dictionary payload = json->get_data();
	const String method = payload.has("method") ? String(payload["method"]) : String();
	const bool is_notification = !payload.has("id");
	const bool is_initialize = method == "initialize";

	String session_id = get_header(p_context, "MCP-Session-Id");
	const bool streamable_client = !session_id.is_empty() || (is_initialize && accepts_json_and_sse(p_context));

	if (!streamable_client) {
		return false;
	}

	if (!session_id.is_empty()) {
		MutexLock lock(mutex);
		_expire_sessions();
		if (!_get_session(session_id)) {
			p_response->set_status(404);
			p_response->set_body("Unknown or expired MCP session");
			return true;
		}
		_touch_session(*_get_session(session_id));
	} else if (!is_initialize) {
		p_response->set_status(400);
		p_response->set_body("Missing MCP-Session-Id");
		return true;
	}

	if (is_initialize && session_id.is_empty()) {
		MCPSession session;
		session.session_id = _generate_session_id();
		session.created_usec = Time::get_singleton()->get_ticks_usec();
		session.last_activity_usec = session.created_usec;
		session.negotiated_protocol = negotiate_protocol_version(
				payload.has("params") && Dictionary(payload["params"]).has("protocolVersion")
						? String(Dictionary(payload["params"])["protocolVersion"])
						: String("2024-11-05"));
		{
			MutexLock lock(mutex);
			sessions.insert(session.session_id, session);
			session_id = session.session_id;
		}
		owner->transport_negotiated_protocol = session.negotiated_protocol;
		apply_cors_headers(p_response, p_context);
		p_response->add_header("MCP-Session-Id", session_id);
	}

	const bool wants_sse = accepts_json_and_sse(p_context) && !is_notification && method != "initialize";
	if (wants_sse) {
		{
			MutexLock lock(mutex);
			pending_post_sse.body = body;
			pending_post_sse.session_id = session_id;
			pending_post_sse.wants_sse_response = true;
			has_pending_post_sse = true;
		}
		apply_cors_headers(p_response, p_context);
		p_response->add_header("MCP-Session-Id", session_id);
		p_response->start_sse();
		return true;
	}

	apply_cors_headers(p_response, p_context);
	if (!session_id.is_empty()) {
		p_response->add_header("MCP-Session-Id", session_id);
	}

	if (!is_notification && !session_id.is_empty()) {
		bind_tool_route(session_id, -1);
	}

	Dictionary result = owner->_transport_handle_json_rpc(body, p_response);
	if (method == "notifications/initialized" && !session_id.is_empty()) {
		MutexLock lock(mutex);
		MCPSession *session = _get_session(session_id);
		if (session) {
			session->initialized = true;
		}
	}

	if (!p_response->is_sent()) {
		if (result.is_empty()) {
			p_response->set_status(202);
			p_response->set_body("");
		} else {
			p_response->set_status(200);
			p_response->set_json(result);
		}
	}
	return true;
}

bool MCPSessionManager::handle_mcp_delete(const Ref<HTTPRequestContext> &p_context, Ref<HTTPResponse> p_response) {
	const bool allow_delete = GLOBAL_GET("blazium/justamcp/session_allow_client_delete");
	if (!allow_delete) {
		p_response->set_status(405);
		p_response->set_body("Session DELETE not enabled");
		return true;
	}

	const String session_id = get_header(p_context, "MCP-Session-Id");
	if (session_id.is_empty()) {
		p_response->set_status(400);
		p_response->set_body("Missing MCP-Session-Id");
		return true;
	}

	MutexLock lock(mutex);
	MCPSession *session = _get_session(session_id);
	if (!session) {
		p_response->set_status(404);
		p_response->set_body("Unknown session");
		return true;
	}
	for (const KeyValue<int, String> &conn : session->connection_to_stream) {
		if (HTTPServer::get_singleton()) {
			HTTPServer::get_singleton()->close_sse_connection(conn.key);
		}
	}
	sessions.erase(session_id);
	apply_cors_headers(p_response, p_context);
	p_response->set_status(200);
	p_response->set_body("");
	return true;
}

#endif // MODULE_HTTPSERVER_ENABLED
