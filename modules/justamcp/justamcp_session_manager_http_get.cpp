/**************************************************************************/
/*  justamcp_session_manager_http_get.cpp                                 */
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
#include "justamcp_server.h"
#include "justamcp_session_manager.h"
#include "modules/modules_enabled.gen.h"
#if defined(MODULE_HTTPSERVER_ENABLED)
#include "modules/httpserver/http_request_context.h"
#include "modules/httpserver/http_response.h"
#include "modules/httpserver/http_server.h"
#endif

#if defined(MODULE_HTTPSERVER_ENABLED)

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
		owner->call_deferred("_deferred_sse_replay", p_connection_id, replay_from);
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
	const String protocol_header = get_header(p_context, "MCP-Protocol-Version");
	if (!protocol_header_allows_legacy_sse(protocol_header)) {
		apply_cors_headers(p_response, p_context);
		p_response->set_status(405);
		p_response->set_body("GET is not supported for protocol " + first_protocol_version_token(protocol_header));
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
	MCPSession *session = _get_session(session_id);
	if (!session) {
		p_response->set_status(404);
		p_response->set_body("Unknown or expired MCP session");
		return true;
	}
	if (is_modern_protocol_version(session->negotiated_protocol)) {
		lock.temp_unlock();
		apply_cors_headers(p_response, p_context);
		p_response->set_status(405);
		p_response->set_body("GET is not supported for protocol " + session->negotiated_protocol);
		return true;
	}
	_touch_session(*session);

	owner->transport_negotiated_protocol = session->negotiated_protocol;
	lock.temp_unlock();

	apply_cors_headers(p_response, p_context);
	p_response->start_sse();
	return true;
}

#endif
