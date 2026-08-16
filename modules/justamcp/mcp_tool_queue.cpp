/**************************************************************************/
/*  mcp_tool_queue.cpp                                                    */
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

#include "mcp_tool_queue.h"

#include "justamcp_server.h" // IWYU pragma: keep
#include "justamcp_server_request_lookup.h"
#include "justamcp_tool_queue_state.h"

#include "core/templates/hash_map.h"

bool MCPToolQueue::_contains_readonly_locked(MCPToolQueueEntry *p_entry) const {
	if (!p_entry) {
		return false;
	}
	for (int i = 0; i < current_readonly_inflight.size(); i++) {
		if (current_readonly_inflight[i] == p_entry) {
			return true;
		}
	}
	return false;
}

void MCPToolQueue::_remove_readonly_locked(MCPToolQueueEntry *p_entry) {
	for (int i = 0; i < current_readonly_inflight.size(); i++) {
		if (current_readonly_inflight[i] == p_entry) {
			current_readonly_inflight.remove_at(i);
			return;
		}
	}
}

MCPToolQueue::Snapshot MCPToolQueue::snapshot() const {
	MutexLock lock(mutex);
	Snapshot snap;
	snap.pending_size = pending.size();
	snap.current_write = current_write;
	snap.current_readonly_inflight = current_readonly_inflight;
	snap.processing = processing;
	return snap;
}

bool MCPToolQueue::is_full() const {
	MutexLock lock(mutex);
	return pending.size() >= MAX_SIZE;
}

int MCPToolQueue::size() const {
	MutexLock lock(mutex);
	return pending.size();
}

int MCPToolQueue::readonly_inflight_count() const {
	MutexLock lock(mutex);
	return current_readonly_inflight.size();
}

MCPToolQueueEntry *MCPToolQueue::primary_readonly() const {
	MutexLock lock(mutex);
	return current_readonly_inflight.is_empty() ? nullptr : current_readonly_inflight[0];
}

MCPToolQueueEntry *MCPToolQueue::enqueue(MCPToolQueueEntry *p_entry) {
	ERR_FAIL_NULL_V(p_entry, nullptr);
	MutexLock lock(mutex);
	if (pending.size() >= MAX_SIZE) {
		return nullptr;
	}
	pending.push_back(p_entry);
	return p_entry;
}

bool MCPToolQueue::remove(MCPToolQueueEntry *p_entry) {
	MutexLock lock(mutex);
	for (int i = 0; i < pending.size(); i++) {
		if (pending[i] == p_entry) {
			pending.remove_at(i);
			if (current_write == p_entry) {
				current_write = nullptr;
			}
			_remove_readonly_locked(p_entry);
			sync_processing_flag();
			return true;
		}
	}
	return false;
}

MCPToolQueueEntry *MCPToolQueue::find_by_request_id(const Variant &p_request_id) const {
	MutexLock lock(mutex);
	return JustAMCPServerRequestLookup::find_entry_by_request_id(pending, current_write, current_readonly_inflight, p_request_id);
}

void MCPToolQueue::set_in_flight(MCPToolQueueEntry *p_write, const Vector<MCPToolQueueEntry *> &p_readonly_inflight) {
	MutexLock lock(mutex);
	current_write = p_write;
	current_readonly_inflight = p_readonly_inflight;
	sync_processing_flag();
}

void MCPToolQueue::clear_in_flight_pointers() {
	MutexLock lock(mutex);
	current_write = nullptr;
	current_readonly_inflight.clear();
	processing = false;
}

void MCPToolQueue::sync_processing_flag() {
	JustAMCPToolQueueState::sync_processing_flag(current_write, current_readonly_inflight, processing);
}

void MCPToolQueue::clear_pending(bool p_signal_stateless_completion) {
	MutexLock lock(mutex);
	for (int i = 0; i < pending.size(); i++) {
		MCPToolQueueEntry *entry = pending[i];
		if (entry) {
			entry->cancel_requested = true;
			if (p_signal_stateless_completion && entry->has_stateless_response) {
				entry->signal_and_join_waiters();
			}
		}
	}
	pending.clear();
	current_write = nullptr;
	current_readonly_inflight.clear();
	processing = false;
}

void MCPToolQueue::release_all_entries(Vector<MCPToolQueueEntry *> &r_pending, MCPToolQueueEntry *&r_write, Vector<MCPToolQueueEntry *> &r_readonly_inflight) {
	MutexLock lock(mutex);
	r_pending = pending;
	pending.clear();
	r_write = current_write;
	r_readonly_inflight = current_readonly_inflight;
	current_write = nullptr;
	current_readonly_inflight.clear();
	processing = false;
}

MCPToolQueueEntry *MCPToolQueue::pick_next_locked(int p_max_readonly_inflight, bool p_prefer_readonly_lane) const {
	const bool multi_readonly = p_max_readonly_inflight > 0;
	auto _candidate_eligible = [&](MCPToolQueueEntry *p_candidate) -> bool {
		if (!p_candidate || p_candidate == current_write || _contains_readonly_locked(p_candidate) ||
				p_candidate->pending_task_dispatch || p_candidate->cancel_requested) {
			return false;
		}
		if (p_prefer_readonly_lane && !p_candidate->is_readonly_tool) {
			return false;
		}
		if (!p_prefer_readonly_lane && multi_readonly && current_write && p_candidate->is_readonly_tool) {
			return false;
		}
		if (p_candidate->is_readonly_tool) {
			if (multi_readonly && current_readonly_inflight.size() < p_max_readonly_inflight) {
				return true;
			}
			if (!multi_readonly && current_write == nullptr && current_readonly_inflight.is_empty()) {
				return true;
			}
			return false;
		}
		return current_write == nullptr;
	};

	HashMap<String, MCPToolQueueEntry *> first_by_session;
	Vector<String> session_order;
	for (int i = 0; i < pending.size(); i++) {
		MCPToolQueueEntry *candidate = pending[i];
		if (!candidate || !_candidate_eligible(candidate)) {
			continue;
		}
		const String session_key = candidate->session_id.is_empty() ? String("__anonymous__") : candidate->session_id;
		if (first_by_session.has(session_key)) {
			continue;
		}
		first_by_session.insert(session_key, candidate);
		session_order.push_back(session_key);
	}
	if (session_order.is_empty()) {
		return nullptr;
	}

	int start_index = 0;
	if (!last_fair_pick_session_id.is_empty()) {
		for (int i = 0; i < session_order.size(); i++) {
			if (session_order[i] == last_fair_pick_session_id) {
				start_index = (i + 1) % session_order.size();
				break;
			}
		}
	}

	const String &session_key = session_order[start_index];
	last_fair_pick_session_id = session_key;
	return first_by_session[session_key];
}

int MCPToolQueue::count_pending_for_session(const String &p_session_id) const {
	MutexLock lock(mutex);
	int count = 0;
	for (int i = 0; i < pending.size(); i++) {
		MCPToolQueueEntry *entry = pending[i];
		if (!entry) {
			continue;
		}
		if (entry->session_id == p_session_id) {
			count++;
		}
	}
	return count;
}

MCPToolQueueEntry *MCPToolQueue::pick_next(int p_max_readonly_inflight, bool p_prefer_readonly_lane) const {
	MutexLock lock(mutex);
	return pick_next_locked(p_max_readonly_inflight, p_prefer_readonly_lane);
}
