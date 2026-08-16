/**************************************************************************/
/*  justamcp_server_tool_queue.cpp                                        */
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
#include "justamcp_tool_dispatch.h"
#include "justamcp_tool_queue_state.h"
#include "tools/justamcp_json_rpc_helpers.h"
#include "tools/justamcp_readonly_tools.h"
#include "tools/justamcp_task_manager.h"

#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "core/os/time.h"

#if defined(MODULE_HTTPSERVER_ENABLED)

static int _justamcp_readonly_worker_concurrency() {
	int concurrency = 2;
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/justamcp/readonly_worker_concurrency")) {
		concurrency = int(GLOBAL_GET("blazium/justamcp/readonly_worker_concurrency"));
	}
	if (concurrency < 0) {
		concurrency = 0;
	}

	if (concurrency == 0 && ProjectSettings::get_singleton() && bool(GLOBAL_GET("blazium/justamcp/parallel_readonly_lane"))) {
		concurrency = 1;
	}
	return concurrency;
}

void JustAMCPServer::_fail_and_remove_task_dispatch_entry(MCPToolQueueEntry *p_entry) {
	if (!p_entry) {
		return;
	}
	{
		MutexLock lock(mcp_tool_queue.mutex);
		for (int i = 0; i < mcp_tool_queue.pending.size(); i++) {
			if (mcp_tool_queue.pending[i] == p_entry) {
				mcp_tool_queue.pending.remove_at(i);
				break;
			}
		}
		if (mcp_tool_queue.current_write == p_entry) {
			mcp_tool_queue.current_write = nullptr;
		}
		JustAMCPToolQueueState::remove_readonly_inflight(mcp_tool_queue.current_readonly_inflight, p_entry);
		JustAMCPToolQueueState::sync_processing_flag(mcp_tool_queue.current_write, mcp_tool_queue.current_readonly_inflight, mcp_tool_queue.processing);
	}
	if (p_entry->has_stateless_response) {
		p_entry->signal_and_join_waiters();
	} else if (!p_entry->rpc_result.is_empty()) {
		_send_sse_routed(JSON::stringify(p_entry->rpc_result), p_entry->session_id, p_entry->sse_connection_id);
	}
	if (session_manager && p_entry->request_id.get_type() != Variant::NIL) {
		session_manager->clear_request_tool_route(p_entry->request_id);
	}
	_unregister_task_route(p_entry->task_id);
	if (!p_entry->has_completion_waiters()) {
		memdelete(p_entry);
	}
	_schedule_process_pending_tools();
}

MCPToolQueueEntry *JustAMCPServer::_enqueue_tool_request(const Variant &p_request_id, const String &p_tool_name, const Dictionary &p_args, Ref<HTTPResponse> p_response, Dictionary &r_queue_full_error, const Dictionary &p_options) {
	if (mcp_tool_queue.is_full()) {
		r_queue_full_error["jsonrpc"] = "2.0";
		r_queue_full_error["id"] = p_request_id;
		Dictionary error_dict;
		error_dict["code"] = -32003;
		error_dict["message"] = vformat("MCP tool queue full (%d pending). Retry later.", MCPToolQueue::max_size());
		r_queue_full_error["error"] = error_dict;
		return nullptr;
	}

	MCPToolQueueEntry *entry = memnew(MCPToolQueueEntry);
	entry->request_id = p_request_id;
	entry->tool_name = p_tool_name;
	entry->args = p_args;
	entry->stateless_response = p_response;
	entry->has_stateless_response = p_response.is_valid();
	if (p_options.has("task_id")) {
		entry->task_id = p_options["task_id"];
	}
	if (p_options.has("progress_token")) {
		entry->progress_token = p_options["progress_token"];
	}
	entry->is_task_augmented = p_options.get("is_task_augmented", false);
	entry->pending_task_dispatch = p_options.get("pending_task_dispatch", false);
	entry->is_readonly_tool = JustAMCPReadonlyTools::is_readonly_tool(p_tool_name);
	if (p_options.has("task_ttl_ms")) {
		entry->pending_task_ttl_ms = int(p_options["task_ttl_ms"]);
	}
	if (p_options.has("task_poll_ms")) {
		entry->pending_task_poll_interval_ms = int(p_options["task_poll_ms"]);
	}
	if (session_manager) {
		String route_session_id;
		int route_connection_id = -1;
		if (p_request_id.get_type() != Variant::NIL && session_manager->get_request_tool_route(p_request_id, route_session_id, route_connection_id)) {
			entry->session_id = route_session_id;
			entry->sse_connection_id = route_connection_id;
		}
	}
	int max_per_session = 8;
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/justamcp/max_pending_per_session")) {
		max_per_session = int(GLOBAL_GET("blazium/justamcp/max_pending_per_session"));
	}
	if (max_per_session > 0 && !entry->session_id.is_empty() && mcp_tool_queue.count_pending_for_session(entry->session_id) >= max_per_session) {
		memdelete(entry);
		r_queue_full_error["jsonrpc"] = "2.0";
		r_queue_full_error["id"] = p_request_id;
		Dictionary error_dict;
		error_dict["code"] = -32005;
		error_dict["message"] = vformat("MCP tool queue full for session (%d pending). Retry later.", max_per_session);
		r_queue_full_error["error"] = error_dict;
		return nullptr;
	}
	int max_enqueue_per_sec = 10;
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/justamcp/max_enqueue_per_sec_per_session")) {
		max_enqueue_per_sec = int(GLOBAL_GET("blazium/justamcp/max_enqueue_per_sec_per_session"));
	}
	if (max_enqueue_per_sec > 0) {
		const String rate_key = entry->session_id.is_empty() ? String("__anonymous__") : entry->session_id;
		MutexLock rate_lock(session_enqueue_rate_mutex);
		const uint64_t now_usec = Time::get_singleton()->get_ticks_usec();
		const uint64_t window_usec = 1000000ULL;
		Vector<uint64_t> &stamps = session_enqueue_timestamps_usec[rate_key];
		while (!stamps.is_empty() && now_usec - stamps[0] > window_usec) {
			stamps.remove_at(0);
		}
		if (stamps.size() >= (uint32_t)max_enqueue_per_sec) {
			memdelete(entry);
			r_queue_full_error["jsonrpc"] = "2.0";
			r_queue_full_error["id"] = p_request_id;
			Dictionary error_dict;
			error_dict["code"] = -32004;
			error_dict["message"] = vformat("MCP tool enqueue rate limit exceeded (%d/sec per session). Retry later.", max_enqueue_per_sec);
			r_queue_full_error["error"] = error_dict;
			return nullptr;
		}
		stamps.push_back(now_usec);
	}
	if (!mcp_tool_queue.enqueue(entry)) {
		memdelete(entry);
		r_queue_full_error["jsonrpc"] = "2.0";
		r_queue_full_error["id"] = p_request_id;
		Dictionary error_dict;
		error_dict["code"] = -32003;
		error_dict["message"] = vformat("MCP tool queue full (%d pending). Retry later.", MCPToolQueue::max_size());
		r_queue_full_error["error"] = error_dict;
		return nullptr;
	}
	return entry;
}

static Dictionary _justamcp_task_dispatch_cancelled_rpc(const Variant &p_request_id) {
	Dictionary err;
	err["jsonrpc"] = "2.0";
	err["id"] = p_request_id;
	Dictionary error_dict;
	error_dict["code"] = -32000;
	error_dict["message"] = "cancelled";
	err["error"] = error_dict;
	return err;
}

void JustAMCPServer::_dispatch_task_augmented_tools_call(const Variant &p_request_id) {
	MCPToolQueueEntry *entry = nullptr;
	String progress_token;
	int ttl_ms = 0;
	int poll_ms = 0;
	bool has_stateless = false;
	String session_id;
	int sse_connection_id = -1;

	{
		MutexLock lock(mcp_tool_queue.mutex);
		for (int i = 0; i < mcp_tool_queue.pending.size(); i++) {
			MCPToolQueueEntry *candidate = mcp_tool_queue.pending[i];
			if (candidate && JustAMCPJsonRpcHelpers::request_ids_equal(candidate->request_id, p_request_id)) {
				entry = candidate;
				break;
			}
		}

		if (!entry || !entry->pending_task_dispatch) {
			ERR_PRINT("JustAMCP: task-augmented tools/call dispatch could not find a pending queue entry.");
			return;
		}

		if (entry->cancel_requested) {
			entry->pending_task_dispatch = false;
			entry->rpc_result = _justamcp_task_dispatch_cancelled_rpc(p_request_id);
			has_stateless = entry->has_stateless_response;
			session_id = entry->session_id;
			sse_connection_id = entry->sse_connection_id;
			for (int i = 0; i < mcp_tool_queue.pending.size(); i++) {
				if (mcp_tool_queue.pending[i] == entry) {
					mcp_tool_queue.pending.remove_at(i);
					break;
				}
			}
			if (mcp_tool_queue.current_write == entry) {
				mcp_tool_queue.current_write = nullptr;
			}
			JustAMCPToolQueueState::remove_readonly_inflight(mcp_tool_queue.current_readonly_inflight, entry);
			JustAMCPToolQueueState::sync_processing_flag(mcp_tool_queue.current_write, mcp_tool_queue.current_readonly_inflight, mcp_tool_queue.processing);
		} else {
			progress_token = entry->progress_token;
			ttl_ms = entry->pending_task_ttl_ms;
			poll_ms = entry->pending_task_poll_interval_ms;
			entry->pending_task_dispatch = false;
		}
	}

	if (!entry) {
		return;
	}

	if (entry->cancel_requested) {
		if (has_stateless) {
			entry->signal_and_join_waiters();
		} else {
			_send_sse_routed(JSON::stringify(entry->rpc_result), session_id, sse_connection_id);
		}
		if (!entry->has_completion_waiters()) {
			memdelete(entry);
		}
		_schedule_process_pending_tools();
		return;
	}

#ifdef TOOLS_ENABLED
	if (!task_manager) {
		Dictionary err;
		err["jsonrpc"] = "2.0";
		err["id"] = p_request_id;
		Dictionary error_dict;
		error_dict["code"] = -32603;
		error_dict["message"] = "Task manager unavailable.";
		err["error"] = error_dict;
		entry->rpc_result = err;
		_fail_and_remove_task_dispatch_entry(entry);
		return;
	}

	const String task_id = task_manager->create_task(ttl_ms, poll_ms, progress_token);
	if (task_id.is_empty()) {
		Dictionary err;
		err["jsonrpc"] = "2.0";
		err["id"] = p_request_id;
		Dictionary error_dict;
		error_dict["code"] = -32003;
		error_dict["message"] = "Maximum concurrent MCP tasks reached.";
		err["error"] = error_dict;
		entry->rpc_result = err;
		_fail_and_remove_task_dispatch_entry(entry);
		return;
	}

	Dictionary rpc_result;
	rpc_result["jsonrpc"] = "2.0";
	rpc_result["id"] = p_request_id;
	rpc_result["result"] = _build_create_task_result(task_id);

	bool send_sse = false;
	{
		MutexLock lock(mcp_tool_queue.mutex);
		MCPToolQueueEntry *still_entry = nullptr;
		for (int i = 0; i < mcp_tool_queue.pending.size(); i++) {
			if (mcp_tool_queue.pending[i] == entry) {
				still_entry = entry;
				break;
			}
		}
		if (!still_entry || still_entry->cancel_requested) {
#ifdef TOOLS_ENABLED
			if (task_manager && !task_id.is_empty()) {
				task_manager->cancel_task_execution(task_id);
			}
#endif
			if (!still_entry) {
				return;
			}
			entry->rpc_result = _justamcp_task_dispatch_cancelled_rpc(p_request_id);
			_fail_and_remove_task_dispatch_entry(entry);
			return;
		}

		still_entry->task_id = task_id;
		still_entry->rpc_result = rpc_result;
		has_stateless = still_entry->has_stateless_response;
		session_id = still_entry->session_id;
		sse_connection_id = still_entry->sse_connection_id;
		send_sse = !has_stateless;
	}

	if (!progress_token.is_empty()) {
		_register_progress_token(progress_token, task_id, p_request_id);
	}
	_register_task_route(task_id, session_id, sse_connection_id);

	if (has_stateless) {
		entry->signal_completion();
		_schedule_process_pending_tools();
	} else if (send_sse) {
		_send_sse_routed(JSON::stringify(rpc_result), session_id, sse_connection_id);
		_schedule_process_pending_tools();
	}
#else
	(void)ttl_ms;
	(void)poll_ms;
	(void)progress_token;
#endif
}

void JustAMCPServer::_schedule_process_pending_tools() {
	if (pending_tools_drain_scheduled) {
		return;
	}
	pending_tools_drain_scheduled = true;

	call_deferred(SNAME("_process_pending_tools"));
}

void JustAMCPServer::_on_pending_tools_process_frame() {
	pending_tools_drain_scheduled = false;
	_process_pending_tools();
}

void JustAMCPServer::_process_pending_tools() {
	pending_tools_drain_scheduled = false;
	MCPToolQueueEntry *entry = nullptr;
	bool readonly_lane = false;
	const int max_readonly = _justamcp_readonly_worker_concurrency();
	{
		MutexLock lock(mcp_tool_queue.mutex);
		if (mcp_tool_queue.pending.is_empty()) {
			return;
		}

		const bool can_pick_write = (mcp_tool_queue.current_write == nullptr);
		const bool can_pick_readonly = (max_readonly > 0)
				? (mcp_tool_queue.current_readonly_inflight.size() < max_readonly)
				: (mcp_tool_queue.current_write == nullptr && mcp_tool_queue.current_readonly_inflight.is_empty());

		if (!mcp_tool_queue.processing || can_pick_write || can_pick_readonly) {
			if (!mcp_tool_queue.processing) {
				entry = mcp_tool_queue.pick_next_locked(max_readonly, false);
			} else if (can_pick_readonly && max_readonly > 0) {
				entry = mcp_tool_queue.pick_next_locked(max_readonly, true);
			} else if (can_pick_write) {
				entry = mcp_tool_queue.pick_next_locked(max_readonly, false);
			}
			if (!entry) {
				return;
			}
			mcp_tool_queue.processing = true;
			if (entry->is_readonly_tool && max_readonly > 0) {
				mcp_tool_queue.current_readonly_inflight.push_back(entry);
				readonly_lane = true;
			} else if (entry->is_readonly_tool && mcp_tool_queue.current_write == nullptr && mcp_tool_queue.current_readonly_inflight.is_empty()) {
				mcp_tool_queue.current_readonly_inflight.push_back(entry);
				readonly_lane = true;
			} else {
				mcp_tool_queue.current_write = entry;
			}
		} else {
			return;
		}
		for (int i = 0; i < mcp_tool_queue.pending.size(); i++) {
			if (mcp_tool_queue.pending[i] == entry) {
				mcp_tool_queue.pending.remove_at(i);
				break;
			}
		}
		entry->readonly_lane = readonly_lane;
		JustAMCPToolQueueState::sync_processing_flag(mcp_tool_queue.current_write, mcp_tool_queue.current_readonly_inflight, mcp_tool_queue.processing);
	}

	const bool schedule_worker = readonly_lane && JustAMCPReadonlyTools::is_worker_safe_tool(entry->tool_name);
	if (schedule_worker && JustAMCPToolDispatch::try_schedule_worker_execute(this, entry->request_id, entry->tool_name, entry->args)) {
		if (max_readonly > 1) {
			call_deferred(SNAME("_process_pending_tools"));
		}
		return;
	}

	emit_signal("tool_requested", entry->request_id, entry->tool_name, entry->args);
}

void JustAMCPServer::_complete_tool_entry(MCPToolQueueEntry *p_entry, const Dictionary &p_rpc_result) {
	if (!p_entry) {
		return;
	}
	bool has_stateless = false;
	{
		MutexLock lock(mcp_tool_queue.mutex);
		p_entry->rpc_result = p_rpc_result;
		has_stateless = p_entry->has_stateless_response;
		for (int i = 0; i < mcp_tool_queue.pending.size(); i++) {
			if (mcp_tool_queue.pending[i] == p_entry) {
				mcp_tool_queue.pending.remove_at(i);
				break;
			}
		}
		if (mcp_tool_queue.current_write == p_entry) {
			mcp_tool_queue.current_write = nullptr;
		}
		JustAMCPToolQueueState::remove_readonly_inflight(mcp_tool_queue.current_readonly_inflight, p_entry);
		JustAMCPToolQueueState::sync_processing_flag(mcp_tool_queue.current_write, mcp_tool_queue.current_readonly_inflight, mcp_tool_queue.processing);
	}

	if (!p_entry->progress_token.is_empty()) {
		_unregister_progress_token(p_entry->progress_token);
	}

	if (session_manager && p_entry->request_id.get_type() != Variant::NIL) {
		session_manager->clear_request_tool_route(p_entry->request_id);
	}
	_unregister_task_route(p_entry->task_id);

	if (has_stateless) {
		p_entry->signal_and_join_waiters();
	} else {
		_send_sse_routed(JSON::stringify(p_rpc_result), p_entry->session_id, p_entry->sse_connection_id);
	}

	if (!p_entry->has_completion_waiters()) {
		memdelete(p_entry);
	}
	_schedule_process_pending_tools();
}

void JustAMCPServer::_insert_tool_result_tombstone(const String &p_tombstone_key) {
	if (p_tombstone_key.is_empty()) {
		return;
	}
	MutexLock lock(completed_tool_request_mutex);
	if (completed_tool_request_tombstones.has(p_tombstone_key)) {
		return;
	}
	while (completed_tool_request_tombstone_order.size() >= COMPLETED_TOOL_REQUEST_TOMBSTONE_MAX) {
		const String oldest = completed_tool_request_tombstone_order[0];
		completed_tool_request_tombstone_order.remove_at(0);
		completed_tool_request_tombstones.erase(oldest);
	}
	completed_tool_request_tombstones.insert(p_tombstone_key);
	completed_tool_request_tombstone_order.push_back(p_tombstone_key);
}

bool JustAMCPServer::_has_tool_result_tombstone(const String &p_tombstone_key) const {
	if (p_tombstone_key.is_empty()) {
		return false;
	}
	MutexLock lock(completed_tool_request_mutex);
	return completed_tool_request_tombstones.has(p_tombstone_key);
}

void JustAMCPServer::_complete_current_tool_request(const Dictionary &p_rpc_result) {
	MCPToolQueueEntry *completed_entry = nullptr;
	Variant request_id = justamcp_get_active_tool_request_id();
	if (request_id.get_type() == Variant::NIL) {
		WARN_PRINT_ONCE("JustAMCP: _complete_current_tool_request ignored — missing request-id context.");
		return;
	}
	{
		MutexLock lock(mcp_tool_queue.mutex);
		completed_entry = JustAMCPServerRequestLookup::find_entry_by_request_id(
				mcp_tool_queue.pending, mcp_tool_queue.current_write, mcp_tool_queue.current_readonly_inflight, request_id);
		if (completed_entry && completed_entry->result_completed) {
			return;
		}
	}

	if (!completed_entry) {
		const String tombstone_key = JustAMCPJsonRpcHelpers::request_id_to_string(request_id);
		if (_has_tool_result_tombstone(tombstone_key)) {
			return;
		}
		WARN_PRINT_ONCE("JustAMCP: _complete_current_tool_request ignored — no in-flight entry.");
		return;
	}

	const String tombstone_key = JustAMCPJsonRpcHelpers::request_id_to_string(request_id);
	if (_has_tool_result_tombstone(tombstone_key)) {
		return;
	}
	{
		MutexLock lock(mcp_tool_queue.mutex);
		if (completed_entry->result_completed) {
			return;
		}
		completed_entry->result_completed = true;
		completed_entry->completion_generation++;
	}
	_insert_tool_result_tombstone(tombstone_key);
#ifdef TESTS_ENABLED
	test_last_send_tool_result = p_rpc_result;
#endif
	_complete_tool_entry(completed_entry, p_rpc_result);
}

void JustAMCPServer::_clear_tool_queue() {
	Vector<MCPToolQueueEntry *> pending_entries;
	MCPToolQueueEntry *inflight_write = nullptr;
	Vector<MCPToolQueueEntry *> inflight_readonly;
	mcp_tool_queue.release_all_entries(pending_entries, inflight_write, inflight_readonly);

	auto _release_entry = [&](MCPToolQueueEntry *p_entry) {
		if (!p_entry) {
			return;
		}
		if (!p_entry->progress_token.is_empty()) {
			_unregister_progress_token(p_entry->progress_token);
		}
		p_entry->cancel_requested = true;
		if (p_entry->has_stateless_response || p_entry->pending_task_dispatch) {
			p_entry->signal_and_join_waiters();
		}
		if (!p_entry->has_completion_waiters()) {
			memdelete(p_entry);
		}
	};

	for (int i = 0; i < pending_entries.size(); i++) {
		_release_entry(pending_entries[i]);
	}
	_release_entry(inflight_write);
	for (int i = 0; i < inflight_readonly.size(); i++) {
		_release_entry(inflight_readonly[i]);
	}
	Vector<MCPToolQueueEntry *> empty_readonly;
	JustAMCPToolQueueState::sync_processing_flag(nullptr, empty_readonly, mcp_tool_queue.processing);
}

Dictionary JustAMCPServer::_stateless_tool_timeout_error(const Variant &p_request_id) const {
	Dictionary err;
	err["jsonrpc"] = "2.0";
	err["id"] = p_request_id;
	Dictionary error_dict;
	error_dict["code"] = -32003;
	error_dict["message"] = "MCP tool request timed out waiting for dispatch.";
	err["error"] = error_dict;
	return err;
}

bool JustAMCPServer::_wait_for_stateless_tool_entry(MCPToolQueueEntry *p_entry, int p_timeout_ms) {
	ERR_FAIL_NULL_V(p_entry, false);
	return p_entry->wait_for_completion(p_timeout_ms);
}

#endif
