/**************************************************************************/
/*  justamcp_session_manager_http_delete.cpp                              */
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

bool MCPSessionManager::handle_mcp_delete(const Ref<HTTPRequestContext> &p_context, Ref<HTTPResponse> p_response) {
	const bool allow_delete = GLOBAL_GET("blazium/justamcp/session_allow_client_delete");
	if (!allow_delete) {
		p_response->set_status(405);
		p_response->set_body("Session DELETE not enabled");
		return true;
	}

	if (!validate_origin(p_context)) {
		apply_cors_headers(p_response, p_context);
		p_response->set_status(403);
		p_response->set_body("Forbidden - Invalid Origin");
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
	Vector<int> connection_ids;
	for (const KeyValue<int, String> &conn : session->connection_to_stream) {
		connection_ids.push_back(conn.key);
		connection_to_session.erase(conn.key);
		all_sse_connection_ids.erase(conn.key);
	}
	pending_post_sse_by_session.erase(session_id);
	post_sse_upgrade_sessions.erase(session_id);
	sessions.erase(session_id);
	lock.temp_unlock();

	for (int i = 0; i < connection_ids.size(); i++) {
		clear_request_routes_for_connection(connection_ids[i]);
		if (HTTPServer::get_singleton()) {
			HTTPServer::get_singleton()->close_sse_connection(connection_ids[i]);
		}
	}

	apply_cors_headers(p_response, p_context);
	p_response->set_status(200);
	p_response->set_body("");
	return true;
}

#endif
