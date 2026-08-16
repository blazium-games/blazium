/**************************************************************************/
/*  justamcp_session_manager_core.cpp                                     */
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

#include "justamcp_pagination.h"
#include "justamcp_server.h"
#include "justamcp_session_manager.h"
#include "tools/justamcp_json_rpc_helpers.h"

#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/io/json.h"
#include "core/os/time.h"

#include "modules/modules_enabled.gen.h"

#if defined(MODULE_HTTPSERVER_ENABLED)
#include "modules/httpserver/http_server.h"

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

void MCPSessionManager::_prune_expired_pending_post_sse() {
	int timeout_sec = 120;
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/justamcp/pending_post_sse_timeout_sec")) {
		timeout_sec = int(GLOBAL_GET("blazium/justamcp/pending_post_sse_timeout_sec"));
	}

	if (timeout_sec < 1) {
		timeout_sec = 1;
	}
	const uint64_t now = Time::get_singleton()->get_ticks_usec();
	const uint64_t timeout_usec = (uint64_t)timeout_sec * 1000000ULL;
	Vector<String> expired_pending;
	for (const KeyValue<String, PendingPostSse> &kv : pending_post_sse_by_session) {
		if (kv.value.created_usec > 0 && now - kv.value.created_usec > timeout_usec) {
			expired_pending.push_back(kv.key);
		}
	}
	for (int i = 0; i < expired_pending.size(); i++) {
		pending_post_sse_by_session.erase(expired_pending[i]);
		post_sse_upgrade_sessions.erase(expired_pending[i]);
	}
}

void MCPSessionManager::_expire_sessions() {
	_prune_expired_pending_post_sse();
	const int ttl_seconds = GLOBAL_GET("blazium/justamcp/session_ttl_seconds");
	if (ttl_seconds <= 0) {
		prune_request_routes();
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
			all_sse_connection_ids.erase(conn.key);
			if (HTTPServer::get_singleton()) {
				HTTPServer::get_singleton()->close_sse_connection(conn.key);
			}
		}
		pending_post_sse_by_session.erase(expired[i]);
		sessions.erase(expired[i]);
	}
	prune_request_routes();
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
	all_sse_connection_ids.insert(p_connection_id);
	p_session.last_registered_connection_id = p_connection_id;
	r_stream = p_session.streams.getptr(stream.stream_id);
	return r_stream != nullptr;
}

void MCPSessionManager::_unregister_connection(int p_connection_id) {
	if (!connection_to_session.has(p_connection_id)) {
		all_sse_connection_ids.erase(p_connection_id);
		return;
	}
	const String session_id = connection_to_session[p_connection_id];
	connection_to_session.erase(p_connection_id);
	all_sse_connection_ids.erase(p_connection_id);
	MCPSession *session = _get_session(session_id);
	if (!session) {
		return;
	}
	if (session->active_tool_connection_id == p_connection_id) {
		session->active_tool_connection_id = -1;
	}
	if (session->last_registered_connection_id == p_connection_id) {
		session->last_registered_connection_id = -1;
	}
	if (session->connection_to_stream.has(p_connection_id)) {
		const String stream_id = session->connection_to_stream[p_connection_id];
		session->streams.erase(stream_id);
		session->connection_to_stream.erase(p_connection_id);
	}
}

int MCPSessionManager::_resolve_active_tool_connection_for_session(const MCPSession &p_session) const {
	if (p_session.active_tool_connection_id >= 0 &&
			p_session.connection_to_stream.has(p_session.active_tool_connection_id)) {
		return p_session.active_tool_connection_id;
	}
	if (p_session.last_registered_connection_id >= 0 &&
			p_session.connection_to_stream.has(p_session.last_registered_connection_id)) {
		return p_session.last_registered_connection_id;
	}
	return -1;
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
#ifdef TESTS_ENABLED
	test_send_json_on_connection_calls++;
#endif
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
		target = _resolve_active_tool_connection_for_session(*session);
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

void MCPSessionManager::set_session_active_tool_connection(const String &p_session_id, int p_connection_id) {
	MutexLock lock(mutex);
	MCPSession *session = _get_session(p_session_id);
	if (!session) {
		return;
	}
	int active_connection = p_connection_id;
	if (active_connection < 0) {
		active_connection = _resolve_active_tool_connection_for_session(*session);
	}
	if (active_connection >= 0) {
		session->active_tool_connection_id = active_connection;
	}
}

void MCPSessionManager::bind_request_tool_route(const Variant &p_request_id, const String &p_session_id, int p_connection_id) {
	if (p_request_id.get_type() == Variant::NIL || p_session_id.is_empty()) {
		return;
	}
	request_router.bind(p_request_id, p_session_id, p_connection_id);
	set_session_active_tool_connection(p_session_id, p_connection_id);
}

bool MCPSessionManager::get_request_tool_route(const Variant &p_request_id, String &r_session_id, int &r_connection_id) const {
	return request_router.lookup(p_request_id, r_session_id, r_connection_id);
}

void MCPSessionManager::clear_request_tool_route(const Variant &p_request_id) {
	request_router.clear(p_request_id);
}

void MCPSessionManager::clear_request_routes_for_connection(int p_connection_id) {
	request_router.clear_for_connection(p_connection_id);
}

bool MCPSessionManager::session_exists(const String &p_session_id) const {
	MutexLock lock(mutex);
	return sessions.has(p_session_id);
}

void MCPSessionManager::mark_session_initialized(const String &p_session_id) {
	if (p_session_id.is_empty()) {
		return;
	}
	bool first_init = false;
	{
		MutexLock lock(mutex);
		MCPSession *session = _get_session(p_session_id);
		if (session) {
			first_init = !session->initialized;
			session->initialized = true;
		}
	}
	if (first_init) {
		request_session_roots(p_session_id);
	}
}

void MCPSessionManager::request_session_roots(const String &p_session_id) {
	if (p_session_id.is_empty()) {
		return;
	}
	const String req_id = "roots_list_" + p_session_id;
	{
		MutexLock lock(mutex);
		MCPSession *session = _get_session(p_session_id);
		if (!session) {
			return;
		}
		session->pending_roots_list_id = req_id;
	}
	Dictionary rpc;
	rpc["jsonrpc"] = "2.0";
	rpc["id"] = req_id;
	rpc["method"] = "roots/list";
	rpc["params"] = Dictionary();
	send_json_to_session(p_session_id, JSON::stringify(rpc));
}

void MCPSessionManager::set_session_roots(const String &p_session_id, const Array &p_roots) {
	MutexLock lock(mutex);
	MCPSession *session = _get_session(p_session_id);
	if (session) {
		session->roots = p_roots;
	}
}

Array MCPSessionManager::get_session_roots(const String &p_session_id) const {
	MutexLock lock(mutex);
	const MCPSession *session = _get_session(p_session_id);
	if (!session) {
		return Array();
	}
	return session->roots;
}

void MCPSessionManager::apply_roots_list_result(const String &p_session_id, const Variant &p_id, const Dictionary &p_result) {
	const String id_text = String(p_id);
	MutexLock lock(mutex);
	MCPSession *session = nullptr;
	if (!p_session_id.is_empty()) {
		session = _get_session(p_session_id);
	}
	if (!session) {
		for (KeyValue<String, MCPSession> &E : sessions) {
			if (E.value.pending_roots_list_id == id_text) {
				session = &E.value;
				break;
			}
		}
	}
	if (!session) {
		return;
	}
	if (p_result.has("roots") && p_result["roots"].get_type() == Variant::ARRAY) {
		session->roots = p_result["roots"];
	}
	if (session->pending_roots_list_id == id_text) {
		session->pending_roots_list_id = String();
	}
}

void MCPSessionManager::handle_roots_list_changed(const String &p_session_id, const Dictionary &p_params) {
	if (p_params.has("roots") && p_params["roots"].get_type() == Variant::ARRAY) {
		set_session_roots(p_session_id, p_params["roots"]);
	}
	request_session_roots(p_session_id);
}

void MCPSessionManager::deferred_replay_stream_events(int p_connection_id, const String &p_last_event_id) {
	MutexLock lock(mutex);
	MCPSSEStream *replay_stream = _get_stream_for_connection(p_connection_id);
	if (replay_stream) {
		_replay_stream_events(*replay_stream, p_last_event_id);
	}
}

Vector<int> MCPSessionManager::collect_session_connections(const String &p_session_id) const {
	Vector<int> connections;
	MutexLock lock(mutex);
	const MCPSession *session = _get_session(p_session_id);
	if (!session) {
		return connections;
	}
	for (const KeyValue<String, MCPSSEStream> &stream : session->streams) {
		if (stream.value.http_connection_id >= 0) {
			connections.push_back(stream.value.http_connection_id);
		}
	}
	return connections;
}

Vector<int> MCPSessionManager::collect_all_broadcast_connection_ids() const {
	Vector<int> connections;
	MutexLock lock(mutex);
	for (int connection_id : all_sse_connection_ids) {
		connections.push_back(connection_id);
	}
	return connections;
}

Vector<int> MCPSessionManager::snapshot_broadcast_targets() const {
	return collect_all_broadcast_connection_ids();
}

void MCPSessionManager::track_legacy_broadcast_connection(int p_connection_id) {
	if (p_connection_id < 0) {
		return;
	}
	MutexLock lock(mutex);
	all_sse_connection_ids.insert(p_connection_id);
}

void MCPSessionManager::untrack_legacy_broadcast_connection(int p_connection_id) {
	if (p_connection_id < 0) {
		return;
	}
	MutexLock lock(mutex);
	all_sse_connection_ids.erase(p_connection_id);
}

void MCPSessionManager::prune_request_routes() {
	int ttl_sec = 3600;
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/justamcp/request_route_ttl_sec")) {
		ttl_sec = int(GLOBAL_GET("blazium/justamcp/request_route_ttl_sec"));
	}
	if (ttl_sec <= 0) {
		return;
	}
	const uint64_t ttl_usec = (uint64_t)ttl_sec * 1000000ULL;
	request_router.prune_expired(ttl_usec);
}
#ifdef TESTS_ENABLED

bool MCPSessionManager::test_has_pending_post_sse_for_session(const String &p_session_id) const {
	MutexLock lock(mutex);
	return pending_post_sse_by_session.has(p_session_id);
}

void MCPSessionManager::test_set_pending_post_sse(const String &p_session_id, const String &p_body) {
	MutexLock lock(mutex);
	PendingPostSse pending;
	pending.body = p_body;
	pending.session_id = p_session_id;
	pending.wants_sse_response = true;
	pending.requires_json_and_sse_accept = true;
	pending.claim_armed = false;
	pending.created_usec = Time::get_singleton()->get_ticks_usec();
	pending_post_sse_by_session.insert(p_session_id, pending);
}

void MCPSessionManager::test_arm_pending_post_sse_claim(const String &p_session_id) {
	MutexLock lock(mutex);
	if (pending_post_sse_by_session.has(p_session_id)) {
		pending_post_sse_by_session[p_session_id].claim_armed = true;
		post_sse_upgrade_sessions.insert(p_session_id);
	}
}

void MCPSessionManager::test_set_pending_post_sse_created_usec(const String &p_session_id, uint64_t p_created_usec) {
	MutexLock lock(mutex);
	if (pending_post_sse_by_session.has(p_session_id)) {
		pending_post_sse_by_session[p_session_id].created_usec = p_created_usec;
	}
}

void MCPSessionManager::test_prune_expired_pending_post_sse() {
	MutexLock lock(mutex);
	_prune_expired_pending_post_sse();
}

String MCPSessionManager::test_peek_pending_post_sse_body(const String &p_session_id) const {
	MutexLock lock(mutex);
	if (!pending_post_sse_by_session.has(p_session_id)) {
		return String();
	}
	return pending_post_sse_by_session[p_session_id].body;
}

bool MCPSessionManager::test_register_stream_for_session(const String &p_session_id, int p_connection_id) {
	MutexLock lock(mutex);
	MCPSession *session = _get_session(p_session_id);
	if (!session) {
		MCPSession created;
		created.session_id = p_session_id;
		created.created_usec = Time::get_singleton()->get_ticks_usec();
		created.last_activity_usec = created.created_usec;
		sessions.insert(p_session_id, created);
		session = _get_session(p_session_id);
	}
	if (!session) {
		return false;
	}
	MCPSSEStream *stream = nullptr;
	return _register_stream(*session, p_connection_id, false, stream) && stream != nullptr;
}

int MCPSessionManager::test_get_active_tool_connection_id(const String &p_session_id) const {
	MutexLock lock(mutex);
	const MCPSession *session = _get_session(p_session_id);
	if (!session) {
		return -1;
	}
	return session->active_tool_connection_id;
}

int MCPSessionManager::test_get_last_registered_connection_id(const String &p_session_id) const {
	MutexLock lock(mutex);
	const MCPSession *session = _get_session(p_session_id);
	if (!session) {
		return -1;
	}
	return session->last_registered_connection_id;
}

int MCPSessionManager::test_connection_map_size() const {
	MutexLock lock(mutex);
	return connection_to_session.size();
}

bool MCPSessionManager::test_has_connection_mapping(int p_connection_id) const {
	MutexLock lock(mutex);
	return connection_to_session.has(p_connection_id);
}

int MCPSessionManager::test_count_send_json_on_connection_calls() const {
	return test_send_json_on_connection_calls;
}

void MCPSessionManager::test_simulate_session_delete(const String &p_session_id) {
	Vector<int> connection_ids;
	{
		MutexLock lock(mutex);
		MCPSession *session = _get_session(p_session_id);
		if (!session) {
			return;
		}
		for (const KeyValue<int, String> &conn : session->connection_to_stream) {
			connection_ids.push_back(conn.key);
			connection_to_session.erase(conn.key);
			all_sse_connection_ids.erase(conn.key);
		}
		pending_post_sse_by_session.erase(p_session_id);
		sessions.erase(p_session_id);
	}
	for (int i = 0; i < connection_ids.size(); i++) {
		clear_request_routes_for_connection(connection_ids[i]);
	}
}

bool MCPSessionManager::test_try_set_pending_post_sse(const String &p_session_id, const String &p_body) {
	MutexLock lock(mutex);
	if (pending_post_sse_by_session.has(p_session_id)) {
		return false;
	}
	PendingPostSse pending;
	pending.body = p_body;
	pending.session_id = p_session_id;
	pending.wants_sse_response = true;
	pending.requires_json_and_sse_accept = true;
	pending.claim_armed = false;
	pending.created_usec = Time::get_singleton()->get_ticks_usec();
	pending_post_sse_by_session.insert(p_session_id, pending);
	return true;
}

void MCPSessionManager::test_backdate_request_route(const Variant &p_request_id, uint64_t p_age_usec) {
	request_router.test_backdate_route(p_request_id, p_age_usec);
}

void MCPSessionManager::test_set_request_route_created_usec(const Variant &p_request_id, uint64_t p_created_usec) {
	request_router.test_set_route_created_usec(p_request_id, p_created_usec);
}

void MCPSessionManager::test_prune_request_routes_usec(uint64_t p_ttl_usec) {
	request_router.prune_expired(p_ttl_usec);
}

#endif

#endif
