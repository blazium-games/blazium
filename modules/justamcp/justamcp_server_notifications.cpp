/**************************************************************************/
/*  justamcp_server_notifications.cpp                                     */
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

#include "justamcp_log_levels.h"
#include "justamcp_notification_bus.h"
#include "justamcp_pagination.h"
#include "justamcp_server.h"
#include "tools/justamcp_task_manager.h"

#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "core/os/time.h"

#include "modules/httpserver/http_server.h"

void JustAMCPServer::broadcast_prompts_list_changed() {
#if defined(MODULE_HTTPSERVER_ENABLED)
	Dictionary notification;
	notification["jsonrpc"] = "2.0";
	notification["method"] = "notifications/prompts/list_changed";
	notification_bus.broadcast_json(JSON::stringify(notification));
#endif
}

void JustAMCPServer::broadcast_tools_list_changed() {
#if defined(MODULE_HTTPSERVER_ENABLED)
	Dictionary notification;
	notification["jsonrpc"] = "2.0";
	notification["method"] = "notifications/tools/list_changed";
	notification_bus.broadcast_json(JSON::stringify(notification));
#endif
}

void JustAMCPServer::_append_mcp_notification_log(const String &p_level, const String &p_logger, const Dictionary &p_data) {
	Dictionary entry;
	entry["level"] = p_level;
	if (!p_logger.is_empty()) {
		entry["logger"] = p_logger;
	}
	entry["data"] = p_data;
	entry["timestamp_usec"] = Time::get_singleton()->get_ticks_usec();

	MutexLock lock(mcp_notification_log_mutex);
	mcp_notification_log.push_back(entry);
	const int max_entries = justamcp_mcp_log_buffer_size();
	while (mcp_notification_log.size() > max_entries) {
		mcp_notification_log.remove_at(0);
	}
}

Dictionary JustAMCPServer::get_mcp_notification_log_page(const String &p_cursor) {
	int offset = 0;
	if (!justamcp_pagination_decode_cursor(p_cursor, offset)) {
		Dictionary err;
		err["ok"] = false;
		err["error_code"] = -32602;
		err["error"] = "Invalid pagination cursor.";
		return err;
	}
	Array page;
	int total = 0;
	{
		MutexLock lock(mcp_notification_log_mutex);
		total = mcp_notification_log.size();
		if (offset < 0) {
			offset = 0;
		}
		const int page_size = justamcp_pagination_page_size();
		for (int i = offset; i < total && page.size() < page_size; i++) {
			page.push_back(mcp_notification_log[i]);
		}
	}
	Dictionary result;
	result["ok"] = true;
	result["notifications"] = page;
	if (offset + page.size() < total) {
		result["nextCursor"] = justamcp_pagination_encode_cursor(offset + page.size());
	}
	return result;
}

void JustAMCPServer::_mcp_debug_log(const String &p_message) {
	if (!GLOBAL_GET("blazium/justamcp/enable_debug_logging")) {
		return;
	}
	Dictionary log_data;
	log_data["message"] = p_message;
	send_log_message("debug", "justamcp", log_data);
}

bool JustAMCPServer::_should_emit_log(const String &p_level) {
	if (!server_started) {
		return false;
	}
	if (!justamcp_log_level_is_valid(p_level)) {
		return false;
	}
	String min_level;
	{
		MutexLock lock(minimum_log_level_mutex);
		min_level = minimum_log_level;
	}
	return justamcp_log_level_passes(p_level, min_level);
}

bool JustAMCPServer::_try_consume_log_rate_limit() {
	MutexLock lock(log_rate_mutex);
	const uint64_t now_usec = Time::get_singleton()->get_ticks_usec();
	const uint64_t window_usec = 1000000;
	if (log_rate_window_start_usec == 0 || now_usec - log_rate_window_start_usec >= window_usec) {
		log_rate_window_start_usec = now_usec;
		log_rate_count = 0;
	}
	if (log_rate_count >= LOG_RATE_LIMIT_PER_SEC) {
		return false;
	}
	log_rate_count++;
	return true;
}

void JustAMCPServer::send_log_message(const String &p_level, const String &p_logger, const Variant &p_data) {
	const String level = justamcp_log_level_canonical(p_level);
	if (!justamcp_log_level_is_valid(level)) {
		return;
	}
	Dictionary log_data;
	if (p_data.get_type() == Variant::DICTIONARY) {
		log_data = Dictionary(p_data);
	} else if (p_data.get_type() != Variant::NIL) {
		log_data["message"] = String(p_data);
	}
	_append_mcp_notification_log(level, p_logger, log_data);
	if (!_should_emit_log(level)) {
		return;
	}
	call_deferred(SNAME("_emit_log_notification_deferred"), level, p_logger, log_data);
}

void JustAMCPServer::_emit_log_notification_deferred(const String &p_level, const String &p_logger, const Dictionary &p_data) {
#if defined(MODULE_HTTPSERVER_ENABLED)
	if (!server_started || !HTTPServer::get_singleton()) {
		return;
	}
	if (HTTPServer::get_singleton()->get_active_sse_connections().is_empty()) {
		return;
	}
	if (!_should_emit_log(p_level)) {
		return;
	}
	if (!_try_consume_log_rate_limit()) {
		return;
	}

	Dictionary notification;
	notification["jsonrpc"] = "2.0";
	notification["method"] = "notifications/message";

	Dictionary params;
	params["level"] = p_level;
	if (!p_logger.is_empty()) {
		params["logger"] = p_logger;
	}
	params["data"] = p_data;

	notification["params"] = params;
	notification_bus.broadcast_json(JSON::stringify(notification));
#endif
}

void JustAMCPServer::subscribe_resource(const String &p_uri) {
	if (p_uri.is_empty()) {
		return;
	}
	MutexLock lock(subscribed_resources_mutex);
	subscribed_resources.insert(p_uri);
}

void JustAMCPServer::unsubscribe_resource(const String &p_uri) {
	MutexLock lock(subscribed_resources_mutex);
	subscribed_resources.erase(p_uri);
}

bool JustAMCPServer::is_resource_subscribed(const String &p_uri) const {
	MutexLock lock(subscribed_resources_mutex);
	return subscribed_resources.has(p_uri);
}

void JustAMCPServer::broadcast_resources_list_changed() {
#if defined(MODULE_HTTPSERVER_ENABLED)
	Dictionary notification;
	notification["jsonrpc"] = "2.0";
	notification["method"] = "notifications/resources/list_changed";
	notification_bus.broadcast_json(JSON::stringify(notification));
#endif
}

void JustAMCPServer::broadcast_resource_updated(const String &p_uri) {
#if defined(MODULE_HTTPSERVER_ENABLED)
	MutexLock lock(subscribed_resources_mutex);
	if (!subscribed_resources.has(p_uri)) {
		return;
	}
	Dictionary notification;
	notification["jsonrpc"] = "2.0";
	notification["method"] = "notifications/resources/updated";
	Dictionary params;
	params["uri"] = p_uri;
	notification["params"] = params;
	notification_bus.broadcast_json(JSON::stringify(notification));
#endif
}

void JustAMCPServer::send_progress_notification(const String &p_token, double p_progress, double p_total, const String &p_message) {
#if defined(MODULE_HTTPSERVER_ENABLED)
	Dictionary notification;
	notification["jsonrpc"] = "2.0";
	notification["method"] = "notifications/progress";
	Dictionary params;
	params["progressToken"] = p_token;
	params["progress"] = p_progress;
	if (p_total > 0.0) {
		params["total"] = p_total;
	}
	if (!p_message.is_empty()) {
		params["message"] = p_message;
	}
	notification["params"] = params;

	String session_id;
	int connection_id = -1;
	{
		MutexLock lock(routing_mutex);
		if (active_progress_tokens.has(p_token)) {
			session_id = active_progress_tokens[p_token].session_id;
			connection_id = active_progress_tokens[p_token].connection_id;
		}
	}
	notification_bus.send_routed_json(JSON::stringify(notification), session_id, connection_id);
#endif
}

void JustAMCPServer::broadcast_task_status(const String &p_task_id) {
#if defined(MODULE_HTTPSERVER_ENABLED) && defined(TOOLS_ENABLED)
	if (task_manager) {
		Dictionary task_dict = task_manager->get_task(p_task_id);
		if (task_dict.get("ok", false)) {
			task_dict.erase("ok");
			Dictionary notification;
			notification["jsonrpc"] = "2.0";
			notification["method"] = "notifications/tasks/status";
			notification["params"] = task_dict;

			String session_id;
			int connection_id = -1;
			{
				MutexLock lock(routing_mutex);
				if (active_task_routes.has(p_task_id)) {
					session_id = active_task_routes[p_task_id].session_id;
					connection_id = active_task_routes[p_task_id].connection_id;
				}
			}
			notification_bus.send_routed_json(JSON::stringify(notification), session_id, connection_id);
		}
	}
#endif
}
