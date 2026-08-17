/**************************************************************************/
/*  justamcp_server_tool_lifecycle.cpp                                    */
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

#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "core/os/time.h"
#include "justamcp_server_request_lookup.h"
#include "justamcp_session_manager.h"
#include "justamcp_tool_queue_state.h"
#include "scene/main/scene_tree.h"
#include "tools/justamcp_json_rpc_helpers.h"
#include "tools/justamcp_task_manager.h"

#if defined(MODULE_HTTPSERVER_ENABLED)

void JustAMCPServer::_complete_task_tool_entry(MCPToolQueueEntry *p_entry, bool p_success, const Variant &p_result, const String &p_error) {
	if (!p_entry) {
		return;
	}

	const String progress_token = p_entry->progress_token;
	const String task_id = p_entry->task_id;
	const bool was_cancelled = p_entry->cancel_requested || String(p_error) == "cancelled" || (p_result.get_type() == Variant::DICTIONARY && String(Dictionary(p_result).get("error", "")) == "cancelled");

#ifdef TOOLS_ENABLED
	if (task_manager && !task_id.is_empty()) {
		if (was_cancelled) {
			task_manager->cancel_task_execution(task_id);
		} else {
			const Dictionary formatted = _format_tool_result_dict(p_success, p_result, p_error);
			Dictionary stored;
			if (formatted.has("result")) {
				stored = formatted["result"];
			} else if (formatted.has("error")) {
				task_manager->fail_task(task_id, String(formatted["error"].operator Dictionary().get("message", p_error)));
			} else {
				stored = formatted;
			}
			if (!was_cancelled && !formatted.has("error")) {
				const bool is_error = stored.get("isError", false);
				task_manager->complete_task(task_id, stored, is_error);
			}
		}
	}
#endif

	if (!progress_token.is_empty()) {
		_unregister_progress_token(progress_token);
	}
	_unregister_task_route(task_id);

	MCPToolQueueEntry *completed_entry = p_entry;
	{
		MutexLock lock(mcp_tool_queue.mutex);
		for (int i = 0; i < mcp_tool_queue.pending.size(); i++) {
			if (mcp_tool_queue.pending[i] == completed_entry) {
				mcp_tool_queue.pending.remove_at(i);
				break;
			}
		}
		if (mcp_tool_queue.current_write == completed_entry) {
			mcp_tool_queue.current_write = nullptr;
		}
		JustAMCPToolQueueState::remove_readonly_inflight(mcp_tool_queue.current_readonly_inflight, completed_entry);
		JustAMCPToolQueueState::sync_processing_flag(mcp_tool_queue.current_write, mcp_tool_queue.current_readonly_inflight, mcp_tool_queue.processing);
	}

	if (session_manager && completed_entry->request_id.get_type() != Variant::NIL) {
		session_manager->clear_request_tool_route(completed_entry->request_id);
	}

	if (completed_entry->has_stateless_response) {
		if (completed_entry->rpc_result.is_empty()) {
			const Dictionary formatted = _format_tool_result_dict(p_success, p_result, p_error);
			Dictionary rpc_with_id = formatted.duplicate();
			rpc_with_id["id"] = completed_entry->request_id;
			completed_entry->rpc_result = rpc_with_id;
		}
		completed_entry->signal_and_join_waiters();
	}

	memdelete(completed_entry);
	_schedule_process_pending_tools();
}

void JustAMCPServer::_on_request_cancelled(const Variant &p_request_id, const String &p_reason, const String &p_caller_session_id) {
	(void)p_reason;
	MCPToolQueueEntry *target = nullptr;
	bool is_in_flight = false;
	int queued_index = -1;
	bool cancel_associated_task = false;
	bool defer_cleanup_to_dispatcher = false;
	String task_id_to_cancel;
	{
		MutexLock lock(mcp_tool_queue.mutex);
		target = JustAMCPServerRequestLookup::find_entry_by_request_id(
				mcp_tool_queue.pending, mcp_tool_queue.current_write, mcp_tool_queue.current_readonly_inflight, p_request_id);
		if (!target) {
			return;
		}

		if (!p_caller_session_id.is_empty() && !target->session_id.is_empty() && target->session_id != p_caller_session_id) {
			return;
		}
		target->cancel_requested = true;
		target->cancel_requested_usec = Time::get_singleton()->get_ticks_usec();
		is_in_flight = (target == mcp_tool_queue.current_write || JustAMCPToolQueueState::contains_readonly_inflight(mcp_tool_queue.current_readonly_inflight, target));
		if (!is_in_flight) {
			for (int i = 0; i < mcp_tool_queue.pending.size(); i++) {
				if (mcp_tool_queue.pending[i] == target) {
					queued_index = i;
					break;
				}
			}
		}
		if (target->is_task_augmented && !target->task_id.is_empty()) {
			cancel_associated_task = true;
			task_id_to_cancel = target->task_id;
		}

		if (target->is_task_augmented && !target->pending_task_dispatch && target->task_id.is_empty()) {
			defer_cleanup_to_dispatcher = true;
		}
	}

	if (cancel_associated_task) {
#ifdef TOOLS_ENABLED
		if (task_manager) {
			task_manager->cancel_task(task_id_to_cancel);
		}
#endif
		request_task_queue_cancel(task_id_to_cancel);
	}

	if (defer_cleanup_to_dispatcher) {
		return;
	}

	if (!is_in_flight && queued_index >= 0) {
		{
			MutexLock lock(mcp_tool_queue.mutex);
			MCPToolQueueEntry *still = JustAMCPServerRequestLookup::find_entry_by_request_id(
					mcp_tool_queue.pending, mcp_tool_queue.current_write, mcp_tool_queue.current_readonly_inflight, p_request_id);
			if (!still) {
				return;
			}

			if (still->is_task_augmented && !still->pending_task_dispatch && still->task_id.is_empty()) {
				return;
			}
		}
		Dictionary cancelled;
		cancelled["ok"] = false;
		cancelled["error"] = "cancelled";
		send_tool_result(p_request_id, false, cancelled, "cancelled");
		return;
	}

	if (is_in_flight) {
		int deadline_ms = 5000;
		if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/justamcp/in_flight_cancel_deadline_ms")) {
			deadline_ms = int(GLOBAL_GET("blazium/justamcp/in_flight_cancel_deadline_ms"));
		}
		if (deadline_ms <= 0) {
			Dictionary cancelled;
			cancelled["ok"] = false;
			cancelled["error"] = "cancelled";
			send_tool_result(p_request_id, false, cancelled, "cancelled");
			return;
		}
		if (is_inside_tree() && get_tree()) {
			Ref<SceneTreeTimer> timer = get_tree()->create_timer(double(deadline_ms) / 1000.0);
			timer->connect("timeout", callable_mp(this, &JustAMCPServer::_enforce_in_flight_cancel_deadline).bind(p_request_id));
		} else {
			call_deferred(SNAME("_enforce_in_flight_cancel_deadline"), p_request_id);
		}
	}
}

void JustAMCPServer::_enforce_in_flight_cancel_deadline(const Variant &p_request_id) {
	int deadline_ms = 5000;
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/justamcp/in_flight_cancel_deadline_ms")) {
		deadline_ms = int(GLOBAL_GET("blazium/justamcp/in_flight_cancel_deadline_ms"));
	}

	MCPToolQueueEntry *target = nullptr;
	uint64_t cancel_usec = 0;
	int sse_connection_id = -1;
	{
		MutexLock lock(mcp_tool_queue.mutex);
		target = JustAMCPServerRequestLookup::find_entry_by_request_id(
				mcp_tool_queue.pending, mcp_tool_queue.current_write, mcp_tool_queue.current_readonly_inflight, p_request_id);
		if (!target || !target->cancel_requested) {
			return;
		}
		const bool still_in_flight = (target == mcp_tool_queue.current_write || JustAMCPToolQueueState::contains_readonly_inflight(mcp_tool_queue.current_readonly_inflight, target));
		if (!still_in_flight) {
			return;
		}
		cancel_usec = target->cancel_requested_usec;
		sse_connection_id = target->sse_connection_id;
	}

	const uint64_t now = Time::get_singleton()->get_ticks_usec();
	const bool past_deadline = deadline_ms <= 0 || cancel_usec == 0 || now >= cancel_usec + uint64_t(deadline_ms) * 1000ULL;
	if (!past_deadline && is_inside_tree()) {
		return;
	}

	Dictionary cancelled;
	cancelled["ok"] = false;
	cancelled["error"] = "cancelled";
	send_tool_result(p_request_id, false, cancelled, "cancelled");
	if (session_manager && sse_connection_id >= 0) {
		session_manager->complete_post_sse_stream_if_needed(sse_connection_id);
	}
}

void JustAMCPServer::_deferred_complete_tool_dict(const Variant &p_request_id, const Dictionary &p_result) {
	Dictionary result = p_result;
	result.erase("_justamcp_async_pending");
	if (is_tool_cancel_requested(p_request_id)) {
		Dictionary cancelled;
		cancelled["ok"] = false;
		cancelled["error"] = "cancelled";
		send_tool_result(p_request_id, false, cancelled, "cancelled");
		return;
	}
	if (result.get("elicitation_required", false)) {
		Dictionary schema = result.get("elicitation_schema", Dictionary());
		const String mode = result.get("elicitation_mode", "form");
		String tool_name;
		Dictionary args;
		{
			MutexLock lock(mcp_tool_queue.mutex);
			MCPToolQueueEntry *entry = JustAMCPServerRequestLookup::find_entry_by_request_id(
					mcp_tool_queue.pending, mcp_tool_queue.current_write, mcp_tool_queue.current_readonly_inflight, p_request_id);
			if (entry) {
				tool_name = entry->tool_name;
				args = entry->args;
			}
		}
		hold_tool_for_elicitation(p_request_id, tool_name, args, schema, mode);
		return;
	}

	const bool success = result.get("ok", false);
	if (success) {
		Dictionary payload = result.duplicate();
		payload.erase("ok");
		send_tool_result(p_request_id, true, payload, "");
		return;
	}
	const Variant err = result.get("error", Variant());
	String error_msg;
	if (err.get_type() == Variant::DICTIONARY) {
		error_msg = String(Dictionary(err).get("message", "Unknown error"));
	} else if (err.get_type() == Variant::STRING) {
		error_msg = err;
	} else {
		error_msg = "Unknown error";
	}
	send_tool_result(p_request_id, false, err, error_msg);
}

#endif
