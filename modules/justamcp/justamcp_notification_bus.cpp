/**************************************************************************/
/*  justamcp_notification_bus.cpp                                         */
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

#include "justamcp_notification_bus.h"

#include "justamcp_server.h"
#include "justamcp_session_manager.h"

#include "modules/modules_enabled.gen.h"

#if defined(MODULE_HTTPSERVER_ENABLED)
#include "core/templates/hash_set.h"

#include "modules/httpserver/http_server.h"
#endif

void JustAMCPNotificationBus::set_owner(JustAMCPServer *p_owner) {
	owner = p_owner;
}

void JustAMCPNotificationBus::set_session_manager(MCPSessionManager *p_manager) {
	session_manager = p_manager;
}

void JustAMCPNotificationBus::send_routed_json(const String &p_json_string, const String &p_session_id, int p_connection_id) const {
#if defined(MODULE_HTTPSERVER_ENABLED)
	if (!HTTPServer::get_singleton()) {
		return;
	}
	if (session_manager) {
		if (!p_session_id.is_empty() && session_manager->send_json_to_session(p_session_id, p_json_string, p_connection_id)) {
			return;
		}
		if (p_connection_id >= 0 && session_manager->send_json_on_connection(p_connection_id, p_json_string)) {
			return;
		}
	}
	if (owner && p_session_id.is_empty() && p_connection_id < 0 && owner->current_sse_connection_id >= 0) {
		HTTPServer::get_singleton()->send_sse_event(owner->current_sse_connection_id, "message", p_json_string);
	}
#endif
}

void JustAMCPNotificationBus::broadcast_json(const String &p_json_string) const {
#if defined(MODULE_HTTPSERVER_ENABLED)
	if (!HTTPServer::get_singleton()) {
		return;
	}
	HashSet<int> targets;
	if (session_manager) {
		const Vector<int> connections = session_manager->snapshot_broadcast_targets();
		for (int i = 0; i < connections.size(); i++) {
			targets.insert(connections[i]);
		}
	}
	if (owner && owner->current_sse_connection_id >= 0) {
		targets.insert(owner->current_sse_connection_id);
	}
	for (int connection_id : targets) {
		if (session_manager) {
			session_manager->send_json_on_connection(connection_id, p_json_string);
		} else if (owner) {
			HTTPServer::get_singleton()->send_sse_event(connection_id, "message", p_json_string);
		}
	}
#endif
}

Vector<int> JustAMCPNotificationBus::collect_session_connections(const String &p_session_id) const {
#if defined(MODULE_HTTPSERVER_ENABLED)
	if (session_manager) {
		return session_manager->collect_session_connections(p_session_id);
	}
#endif
	return Vector<int>();
}

#ifdef TESTS_ENABLED
int JustAMCPNotificationBus::test_count_broadcast_targets(const String &p_json_string) const {
#if defined(MODULE_HTTPSERVER_ENABLED)
	if (!HTTPServer::get_singleton()) {
		return 0;
	}
	HashSet<int> targets;
	if (session_manager) {
		const Vector<int> connections = session_manager->snapshot_broadcast_targets();
		for (int i = 0; i < connections.size(); i++) {
			targets.insert(connections[i]);
		}
	}
	if (owner && owner->current_sse_connection_id >= 0) {
		targets.insert(owner->current_sse_connection_id);
	}
	(void)p_json_string;
	return targets.size();
#else
	(void)p_json_string;
	return 0;
#endif
}
#endif
