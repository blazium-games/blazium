/**************************************************************************/
/*  justamcp_session_manager_http_post.cpp                                */
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

#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "core/os/time.h"
#include "justamcp_json_rpc_transport.h"
#include "justamcp_server.h"
#include "justamcp_session_manager.h"
#include "modules/modules_enabled.gen.h"
#if defined(MODULE_HTTPSERVER_ENABLED)
#include "modules/httpserver/http_request_context.h"
#include "modules/httpserver/http_response.h"
#include "modules/httpserver/http_server.h"
#endif

#if defined(MODULE_HTTPSERVER_ENABLED)

void MCPSessionManager::_process_post_sse_opened(int p_connection_id, const String &p_session_id) {
	PendingPostSse pending;
	{
		MutexLock lock(mutex);
		if (!pending_post_sse_by_session.has(p_session_id)) {
			return;
		}
		pending = pending_post_sse_by_session[p_session_id];
		pending_post_sse_by_session.erase(p_session_id);
		post_sse_upgrade_sessions.erase(p_session_id);
	}

	Variant request_id;
	Dictionary payload;
	{
		Ref<JSON> json;
		json.instantiate();
		if (json->parse(pending.body) == OK && json->get_data().get_type() == Variant::DICTIONARY) {
			payload = json->get_data();
			if (payload.has("id")) {
				request_id = payload["id"];
			}
		}
	}

	{
		MutexLock lock(mutex);
		_expire_sessions();
		MCPSession *session = _get_session(p_session_id);
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
		if (request_id.get_type() != Variant::NIL) {
			bind_request_tool_route(request_id, p_session_id, p_connection_id);
		} else {
			set_session_active_tool_connection(p_session_id, p_connection_id);
		}
	}

	if (HTTPServer::get_singleton()) {
		_send_priming_event(p_connection_id);
	}

	if (!payload.is_empty()) {
		owner->call_deferred("_deferred_post_sse_json_rpc", p_connection_id, p_session_id, payload);
	} else {
		complete_post_sse_stream(p_connection_id);
	}
}

void MCPSessionManager::complete_post_sse_stream(int p_connection_id) {
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
}

bool MCPSessionManager::is_post_stream_connection(int p_connection_id) const {
	MutexLock lock(mutex);
	if (!connection_to_session.has(p_connection_id)) {
		return false;
	}
	const String session_id = connection_to_session[p_connection_id];
	const MCPSession *session = _get_session(session_id);
	if (!session || !session->connection_to_stream.has(p_connection_id)) {
		return false;
	}
	const String stream_id = session->connection_to_stream[p_connection_id];
	if (!session->streams.has(stream_id)) {
		return false;
	}
	return session->streams[stream_id].is_post_stream;
}

void MCPSessionManager::complete_post_sse_stream_if_needed(int p_connection_id) {
	if (p_connection_id < 0) {
		return;
	}
	bool should_complete = false;
	{
		MutexLock lock(mutex);
		MCPSSEStream *stream = _get_stream_for_connection(p_connection_id);
		if (stream && stream->is_post_stream && !stream->response_sent) {
			should_complete = true;
		}
	}
	if (should_complete) {
		complete_post_sse_stream(p_connection_id);
	}
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
		Dictionary err;
		err["jsonrpc"] = "2.0";
		Dictionary error_dict;
		error_dict["code"] = -32700;
		error_dict["message"] = "Invalid JSON or empty body";
		err["error"] = error_dict;
		p_response->set_json(err);
		return true;
	}
	const Variant parsed = json->get_data();
	if (parsed.get_type() != Variant::DICTIONARY) {
		p_response->set_status(400);
		Dictionary err;
		err["jsonrpc"] = "2.0";
		Dictionary error_dict;
		error_dict["code"] = -32600;
		error_dict["message"] = "Invalid Request: JSON-RPC payload must be an object";
		err["error"] = error_dict;
		p_response->set_json(err);
		return true;
	}
	const Dictionary payload = parsed;
	const String method = payload.has("method") ? String(payload["method"]) : String();
	const bool is_notification = !payload.has("id");
	const bool is_initialize = method == "initialize";

	String session_id = get_header(p_context, "MCP-Session-Id");

	const bool streamable_client = !session_id.is_empty() || is_initialize || accepts_json_and_sse(p_context);

	if (!streamable_client) {
		return false;
	}

	if (!session_id.is_empty()) {
		MutexLock lock(mutex);
		_expire_sessions();
		MCPSession *session = _get_session(session_id);
		if (!session) {
			p_response->set_status(404);
			p_response->set_body("Unknown or expired MCP session");
			return true;
		}
		_touch_session(*session);

		owner->transport_negotiated_protocol = session->negotiated_protocol;
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
						: latest_protocol_version());
		{
			MutexLock lock(mutex);
			sessions.insert(session.session_id, session);
			session_id = session.session_id;
		}
		owner->transport_negotiated_protocol = session.negotiated_protocol;
		apply_cors_headers(p_response, p_context);
		p_response->add_header("MCP-Session-Id", session_id);
	}

	const bool async_rpc = (method == "tools/call");
	const bool wants_sse = accepts_json_and_sse(p_context) && !is_notification && method != "initialize" && async_rpc;
	if (wants_sse) {
		{
			MutexLock lock(mutex);
			if (pending_post_sse_by_session.has(session_id)) {
				p_response->set_status(409);
				p_response->set_body("Concurrent streamable POST already pending for this MCP session");
				return true;
			}
			PendingPostSse pending;
			pending.body = body;
			pending.session_id = session_id;
			pending.wants_sse_response = true;
			pending.requires_json_and_sse_accept = true;
			pending.claim_armed = false;
			pending.created_usec = Time::get_singleton()->get_ticks_usec();
			pending_post_sse_by_session.insert(session_id, pending);
			post_sse_upgrade_sessions.insert(session_id);
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
		Variant request_id;
		if (payload.has("id")) {
			request_id = payload["id"];
		}
		if (request_id.get_type() != Variant::NIL) {
			bind_request_tool_route(request_id, session_id, -1);
		} else {
			set_session_active_tool_connection(session_id, -1);
		}
	}

	const int client_id = p_context->get_client_id();
	if (client_id >= 0 && HTTPServer::get_singleton()) {
		p_response->hold();
		owner->call_deferred("_deferred_held_json_rpc", client_id, body, session_id, p_response);
		return true;
	}

	Dictionary result = JustAMCPJsonRpcTransport::handle_json_rpc_parsed(owner, payload, p_response, session_id);
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
			p_response->set_json(JustAMCPJsonRpcTransport::sanitize_wire_rpc(result));
		}
	}
	return true;
}

#endif
