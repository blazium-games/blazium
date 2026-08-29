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

#include "justamcp_mcp_spec.h"
#include "justamcp_server_request_lookup.h"
#include "justamcp_session_manager.h"
#include "justamcp_tool_context.h"
#include "justamcp_tool_dispatch.h"
#include "justamcp_tool_queue_state.h"
#include "tools/justamcp_json_rpc_helpers.h"
#ifdef TOOLS_ENABLED
#include "tools/justamcp_task_manager.h"
#include "tools/justamcp_tool_executor.h"
#endif

#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "core/os/mutex.h"
#include "core/os/time.h"
#include "modules/modules_enabled.gen.h"
#if defined(MODULE_HTTPSERVER_ENABLED)
#include "modules/httpserver/http_server.h"
#endif
#ifdef TOOLS_ENABLED
#include "editor/editor_settings.h"
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
	if (justamcp_protocol_at_least(transport_negotiated_protocol, "2026-07-28")) {
		Variant request_id = p_request_id;
		if (p_request_id.is_valid_int()) {
			request_id = p_request_id.to_int();
		}
		send_tool_result(request_id, true, justamcp_input_required_result(p_mode, p_message, p_url_or_schema), "");
		return;
	}
	Dictionary rpc_request;
	rpc_request["jsonrpc"] = "2.0";
	rpc_request["method"] = "elicitation/create";

	String elicitation_id = "elicitation_" + p_request_id;
	rpc_request["id"] = elicitation_id;

	Dictionary params;
	params["requestId"] = p_request_id;
	params["message"] = p_message;

	String mode = p_mode;
	if (mode == "url" && !justamcp_protocol_supports(transport_negotiated_protocol, JUSTAMCP_FEATURE_ELICITATION_URL)) {
		mode = "form";
	}
	params["mode"] = mode;
	if (mode == "url") {
		params["url"] = p_url_or_schema;
	} else if (mode == "form") {
		if (p_url_or_schema.get_type() == Variant::DICTIONARY) {
			params["schema"] = p_url_or_schema;
		} else {
			params["schema"] = justamcp_confirm_enum_schema();
		}
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
	Variant request_id = p_request_id;
	if (p_request_id.is_valid_int()) {
		request_id = p_request_id.to_int();
	}
	justamcp_push_active_tool_request_id(request_id);
	_complete_current_tool_request(justamcp_url_elicitation_error_rpc(request_id, p_elicitation_id, p_url, p_message));
	justamcp_pop_active_tool_request_id();
#else
	(void)p_request_id;
	(void)p_elicitation_id;
	(void)p_url;
	(void)p_message;
#endif
}

void JustAMCPServer::hold_tool_for_elicitation(const Variant &p_request_id, const String &p_tool_name, const Dictionary &p_args, const Dictionary &p_schema, const String &p_mode) {
	PendingElicitation pending;
	pending.tools_call_id = p_request_id;
	pending.tool_name = p_tool_name;
	pending.args = p_args.duplicate();
	pending.schema = p_schema.is_empty() ? justamcp_confirm_enum_schema() : p_schema;
	pending.mode = p_mode.is_empty() ? String("form") : p_mode;
	pending.created_usec = Time::get_singleton() ? Time::get_singleton()->get_ticks_usec() : 0;
	const String key = JustAMCPJsonRpcHelpers::request_id_to_string(p_request_id);
	{
		MutexLock lock(pending_elicitation_mutex);
		pending_elicitations[key] = pending;
	}
}

void JustAMCPServer::complete_elicitation(const String &p_request_id, const Dictionary &p_result) {
	String lookup = p_request_id;
	if (lookup.begins_with("elicitation_")) {
		lookup = lookup.substr(String("elicitation_").length());
	}
	Variant lookup_id = lookup;
	if (lookup.is_valid_int()) {
		lookup_id = lookup.to_int();
	}
	const String typed_key = JustAMCPJsonRpcHelpers::request_id_to_string(lookup_id);
	PendingElicitation pending;
	bool found = false;
	{
		MutexLock lock(pending_elicitation_mutex);
		auto take = [&](const String &p_key) -> bool {
			if (p_key.is_empty() || !pending_elicitations.has(p_key)) {
				return false;
			}
			pending = pending_elicitations[p_key];
			pending_elicitations.erase(p_key);
			return true;
		};
		found = take(typed_key) || take(lookup) || take(p_request_id);
		if (!found) {
			for (const KeyValue<String, PendingElicitation> &E : pending_elicitations) {
				if (JustAMCPJsonRpcHelpers::request_ids_equal(E.value.tools_call_id, lookup_id) || String(E.value.tools_call_id) == lookup) {
					pending = E.value;
					pending_elicitations.erase(E.key);
					found = true;
					break;
				}
			}
		}
	}
	emit_signal("elicitation_completed", found ? JustAMCPJsonRpcHelpers::request_id_to_string(pending.tools_call_id) : p_request_id, p_result);
	if (!found) {
		return;
	}

	String action;
	Dictionary content;
	String parse_error;
	if (!justamcp_parse_elicit_result(p_result, action, content, parse_error)) {
		send_tool_result(pending.tools_call_id, false, parse_error, parse_error);
		return;
	}
	if (action == "decline" || action == "cancel") {
		const String msg = action == "cancel" ? "Elicitation cancelled." : "Elicitation declined.";
		send_tool_result(pending.tools_call_id, false, msg, msg);
		return;
	}

	String schema_error;
	if (!justamcp_validate_elicit_content(pending.schema, content, schema_error)) {
		send_tool_result(pending.tools_call_id, false, schema_error, schema_error);
		return;
	}
	content = justamcp_apply_schema_defaults(pending.schema, content);
	if (!justamcp_elicit_content_is_confirmed(content)) {
		send_tool_result(pending.tools_call_id, false, "Elicitation was not confirmed.", "Elicitation was not confirmed.");
		return;
	}

	Dictionary retry_args = pending.args.duplicate();
	retry_args["confirmed"] = true;
#ifdef TOOLS_ENABLED
	JustAMCPToolDispatch::execute_and_send(this, JustAMCPToolExecutor::get_active_instance(), pending.tools_call_id, pending.tool_name, retry_args);
#else
	send_tool_result(pending.tools_call_id, false, "Elicitation resume is unavailable.", "Elicitation resume is unavailable.");
#endif
}

void JustAMCPServer::handle_client_rpc_result(const String &p_session_id, const Dictionary &p_payload) {
	const Variant id = p_payload.get("id", Variant());
	const String id_text = String(id);
	const Dictionary result = p_payload.has("result") && p_payload["result"].get_type() == Variant::DICTIONARY ? Dictionary(p_payload["result"]) : Dictionary();
	if (id_text.begins_with("roots_list_")) {
		if (session_manager) {
			session_manager->apply_roots_list_result(p_session_id, id, result);
		}
		return;
	}
	if (id_text.begins_with("elicitation_")) {
		complete_elicitation(id_text, result);
	}
}

Array JustAMCPServer::get_session_roots(const String &p_session_id) const {
#if defined(MODULE_HTTPSERVER_ENABLED)
	if (session_manager) {
		return session_manager->get_session_roots(p_session_id);
	}
#else
	(void)p_session_id;
#endif
	return Array();
}

bool JustAMCPServer::apply_input_responses(const String &p_tool_name, Dictionary &r_args, const Dictionary &p_input_responses) {
	PendingElicitation pending;
	String found_key;
	bool found = false;
	{
		MutexLock lock(pending_elicitation_mutex);
		for (const KeyValue<String, PendingElicitation> &E : pending_elicitations) {
			if (E.value.tool_name == p_tool_name) {
				pending = E.value;
				found_key = E.key;
				found = true;
			}
		}
	}
	if (!found) {
		return false;
	}

	String action;
	Dictionary content;
	String parse_error;
	if (!justamcp_parse_elicit_result(p_input_responses, action, content, parse_error)) {
		return false;
	}
	if (action == "decline" || action == "cancel") {
		MutexLock lock(pending_elicitation_mutex);
		pending_elicitations.erase(found_key);
		r_args["confirmed"] = false;
		return true;
	}
	String schema_error;
	if (!justamcp_validate_elicit_content(pending.schema, content, schema_error)) {
		return false;
	}
	{
		MutexLock lock(pending_elicitation_mutex);
		pending_elicitations.erase(found_key);
	}
	content = justamcp_apply_schema_defaults(pending.schema, content);
	for (const Variant &key : content.keys()) {
		r_args[key] = content[key];
	}
	r_args["confirmed"] = justamcp_elicit_content_is_confirmed(content);
	return true;
}

bool JustAMCPServer::has_pending_elicitation(const Variant &p_request_id) const {
	const String key = JustAMCPJsonRpcHelpers::request_id_to_string(p_request_id);
	MutexLock lock(pending_elicitation_mutex);
	return pending_elicitations.has(key);
}
