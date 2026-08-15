/**************************************************************************/
/*  justamcp_server_results.cpp                                           */
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

#include "justamcp_server_request_lookup.h"
#include "justamcp_session_manager.h"
#include "justamcp_tool_context.h"
#include "justamcp_tool_queue_state.h"
#include "tools/justamcp_json_rpc_helpers.h"
#include "tools/justamcp_task_manager.h"

#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "core/os/mutex.h"
#include "core/os/time.h"
#include "modules/modules_enabled.gen.h"
#if defined(MODULE_HTTPSERVER_ENABLED)
#include "modules/httpserver/http_server.h"
#endif
#ifdef TOOLS_ENABLED
#include "editor/settings/editor_settings.h"
#endif

void JustAMCPServer::send_tool_result(const Variant &p_request_id, bool p_success, const Variant &p_result, const String &p_error) {
#if defined(MODULE_HTTPSERVER_ENABLED)
	const String tombstone_key = JustAMCPJsonRpcHelpers::request_id_to_string(p_request_id);
	{
		MutexLock lock(completed_tool_request_mutex);
		if (!tombstone_key.is_empty() && completed_tool_request_tombstones.has(tombstone_key)) {
			return;
		}
	}

	MCPToolQueueEntry *entry = nullptr;
	{
		MutexLock lock(mcp_tool_queue.mutex);
		entry = JustAMCPServerRequestLookup::find_entry_by_request_id(
				mcp_tool_queue.pending, mcp_tool_queue.current_write, mcp_tool_queue.current_readonly_inflight, p_request_id);
		if (entry) {
			if (entry->result_completed) {
				return;
			}
			entry->result_completed = true;
			entry->completion_generation++;
		}
	}

	if (!tombstone_key.is_empty()) {
		_insert_tool_result_tombstone(tombstone_key);
	}

	if (entry && entry->is_task_augmented) {
		_complete_task_tool_entry(entry, p_success, p_result, p_error);
		return;
	}

	const Dictionary rpc_result = _format_tool_result_dict(p_success, p_result, p_error);
	Dictionary rpc_with_id = rpc_result.duplicate();
	rpc_with_id["id"] = p_request_id;
#ifdef TESTS_ENABLED
	test_last_send_tool_result = rpc_with_id;
#endif
	if (entry) {
		_complete_tool_entry(entry, rpc_with_id);
	} else {
		String route_session_id;
		int route_connection_id = -1;
		if (session_manager && session_manager->get_request_tool_route(p_request_id, route_session_id, route_connection_id)) {
			_send_sse_routed(JSON::stringify(rpc_with_id), route_session_id, route_connection_id);
		} else {
			WARN_PRINT_ONCE("JustAMCP: send_tool_result could not find queue entry for request id.");
			_send_sse_routed(JSON::stringify(rpc_with_id), String(), -1);
		}
	}
#else
	(void)p_request_id;
	(void)p_success;
	(void)p_result;
	(void)p_error;
#endif
}

#if defined(MODULE_HTTPSERVER_ENABLED)

Dictionary JustAMCPServer::_format_tool_result_dict(bool p_success, const Variant &p_result, const String &p_error) const {
	return JustAMCPJsonRpcHelpers::format_tool_result(p_success, p_result, p_error);
}

Dictionary JustAMCPServer::_build_create_task_result(const String &p_task_id) const {
	Dictionary result;
#ifdef TOOLS_ENABLED
	if (task_manager) {
		Dictionary task_dict = task_manager->get_task(p_task_id);
		if (task_dict.get("ok", false)) {
			task_dict.erase("ok");
			result["task"] = task_dict;
			return result;
		}
	}
#endif
	Dictionary task;
	task["taskId"] = p_task_id;
	task["status"] = "working";
	result["task"] = task;
	return result;
}

void JustAMCPServer::_register_progress_token(const String &p_token, const String &p_task_id, const Variant &p_request_id) {
	if (p_token.is_empty()) {
		return;
	}
	MutexLock lock(routing_mutex);
	JustAMCPActiveProgressContext ctx;
	ctx.task_id = p_task_id;
	ctx.request_id = p_request_id;
	if (session_manager && p_request_id.get_type() != Variant::NIL) {
		session_manager->get_request_tool_route(p_request_id, ctx.session_id, ctx.connection_id);
	}
	active_progress_tokens[p_token] = ctx;
}

void JustAMCPServer::_unregister_progress_token(const String &p_token) {
	if (p_token.is_empty()) {
		return;
	}
	MutexLock lock(routing_mutex);
	active_progress_tokens.erase(p_token);
}

void JustAMCPServer::_register_task_route(const String &p_task_id, const String &p_session_id, int p_connection_id) {
	if (p_task_id.is_empty()) {
		return;
	}
	MutexLock lock(routing_mutex);
	JustAMCPActiveProgressContext ctx;
	ctx.task_id = p_task_id;
	ctx.session_id = p_session_id;
	ctx.connection_id = p_connection_id;
	active_task_routes[p_task_id] = ctx;
}

void JustAMCPServer::_unregister_task_route(const String &p_task_id) {
	if (p_task_id.is_empty()) {
		return;
	}
	MutexLock lock(routing_mutex);
	active_task_routes.erase(p_task_id);
}

bool JustAMCPServer::is_current_tool_cancel_requested() const {
	const Variant active_id = justamcp_get_active_tool_request_id();
	if (active_id.get_type() == Variant::NIL) {
		return false;
	}
	return is_tool_cancel_requested(active_id);
}

bool JustAMCPServer::is_tool_cancel_requested(const Variant &p_request_id) const {
	MutexLock lock(mcp_tool_queue.mutex);
	MCPToolQueueEntry *entry = JustAMCPServerRequestLookup::find_entry_by_request_id(
			mcp_tool_queue.pending, mcp_tool_queue.current_write, mcp_tool_queue.current_readonly_inflight, p_request_id);
	if (!entry) {
		return false;
	}
	if (entry->cancel_requested) {
		return true;
	}
#ifdef TOOLS_ENABLED
	if (!entry->task_id.is_empty() && task_manager) {
		return task_manager->is_cancel_requested(entry->task_id);
	}
#endif
	return false;
}

String JustAMCPServer::get_tool_progress_token(const Variant &p_request_id) const {
	MutexLock lock(mcp_tool_queue.mutex);
	MCPToolQueueEntry *entry = JustAMCPServerRequestLookup::find_entry_by_request_id(
			mcp_tool_queue.pending, mcp_tool_queue.current_write, mcp_tool_queue.current_readonly_inflight, p_request_id);
	return entry ? entry->progress_token : String();
}

String JustAMCPServer::get_current_progress_token() const {
	const Variant active_id = justamcp_get_active_tool_request_id();
	if (active_id.get_type() == Variant::NIL) {
		return String();
	}
	return get_tool_progress_token(active_id);
}

String JustAMCPServer::get_current_task_id() const {
	const Variant active_id = justamcp_get_active_tool_request_id();
	if (active_id.get_type() == Variant::NIL) {
		return String();
	}
	MutexLock lock(mcp_tool_queue.mutex);
	MCPToolQueueEntry *entry = JustAMCPServerRequestLookup::find_entry_by_request_id(
			mcp_tool_queue.pending, mcp_tool_queue.current_write, mcp_tool_queue.current_readonly_inflight, active_id);
	return entry ? entry->task_id : String();
}

bool JustAMCPServer::is_task_cancel_requested(const String &p_task_id) const {
#ifdef TOOLS_ENABLED
	return task_manager && task_manager->is_cancel_requested(p_task_id);
#else
	(void)p_task_id;
	return false;
#endif
}

void JustAMCPServer::request_task_queue_cancel(const String &p_task_id) {
#if defined(MODULE_HTTPSERVER_ENABLED)
	MutexLock lock(mcp_tool_queue.mutex);
	for (int i = 0; i < mcp_tool_queue.pending.size(); i++) {
		MCPToolQueueEntry *entry = mcp_tool_queue.pending[i];
		if (entry && entry->task_id == p_task_id) {
			entry->cancel_requested = true;
		}
	}
	if (mcp_tool_queue.current_write && mcp_tool_queue.current_write->task_id == p_task_id) {
		mcp_tool_queue.current_write->cancel_requested = true;
	}
	for (int i = 0; i < mcp_tool_queue.current_readonly_inflight.size(); i++) {
		MCPToolQueueEntry *entry = mcp_tool_queue.current_readonly_inflight[i];
		if (entry && entry->task_id == p_task_id) {
			entry->cancel_requested = true;
		}
	}
#endif
}

void JustAMCPServer::report_tool_progress(const String &p_token, double p_progress, double p_total, const String &p_message) {
	if (p_token.is_empty()) {
		return;
	}

	bool should_emit = false;
	{
		MutexLock lock(routing_mutex);
		if (!active_progress_tokens.has(p_token)) {
			return;
		}
		JustAMCPActiveProgressContext &ctx = active_progress_tokens[p_token];
		const uint64_t now = Time::get_singleton()->get_ticks_usec();
		if (ctx.last_emit_usec == 0 || now - ctx.last_emit_usec >= 100000) {
			ctx.last_emit_usec = now;
			should_emit = true;
		}
	}

	if (should_emit) {
		send_progress_notification(p_token, p_progress, p_total, p_message);
	}
}

#endif

void JustAMCPServer::send_elicitation_request(const String &p_request_id, const String &p_mode, const String &p_message, const Variant &p_url_or_schema) {
#if defined(MODULE_HTTPSERVER_ENABLED)
	Dictionary rpc_request;
	rpc_request["jsonrpc"] = "2.0";
	rpc_request["method"] = "elicitation/create";

	String elicitation_id = "elicitation_" + p_request_id;
	rpc_request["id"] = elicitation_id;

	Dictionary params;
	params["requestId"] = p_request_id;
	params["mode"] = p_mode;
	params["message"] = p_message;

	if (p_mode == "url") {
		params["url"] = p_url_or_schema;
	} else if (p_mode == "form") {
		params["schema"] = p_url_or_schema;
	}

	rpc_request["params"] = params;

	String session_id;
	int connection_id = -1;
	if (session_manager) {
		Variant route_id = p_request_id;
		if (p_request_id.is_valid_int()) {
			route_id = p_request_id.to_int();
		}
		session_manager->get_request_tool_route(route_id, session_id, connection_id);
	}
	notification_bus.send_routed_json(JSON::stringify(rpc_request), session_id, connection_id);
#endif
}

void JustAMCPServer::send_url_elicitation_error(const String &p_request_id, const String &p_elicitation_id, const String &p_url, const String &p_message) {
#if defined(MODULE_HTTPSERVER_ENABLED)
	Dictionary rpc_result;
	rpc_result["jsonrpc"] = "2.0";
	if (p_request_id.is_valid_int()) {
		rpc_result["id"] = p_request_id.to_int();
	} else {
		rpc_result["id"] = p_request_id;
	}

	Dictionary error;
	error["code"] = -32042;
	error["message"] = p_message;

	Dictionary error_data;
	error_data["url"] = p_url;
	error_data["elicitationId"] = p_elicitation_id;
	error["data"] = error_data;

	rpc_result["error"] = error;

	_complete_current_tool_request(rpc_result);
#endif
}
