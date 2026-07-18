/**************************************************************************/
/*  mcp_tool_queue.h                                                      */
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

#pragma once

#include "core/os/mutex.h"
#include "core/templates/vector.h"
#include "core/variant/variant.h"

#include "mcp_tool_queue_entry.h"

class MCPToolQueue {
	friend class JustAMCPServer;

	Mutex mutex;
	Vector<MCPToolQueueEntry *> pending;
	MCPToolQueueEntry *current_write = nullptr;
	Vector<MCPToolQueueEntry *> current_readonly_inflight;
	bool processing = false;
	static const int MAX_SIZE = 32;
	static int max_size() { return MAX_SIZE; }

	mutable String last_fair_pick_session_id;

	bool _contains_readonly_locked(MCPToolQueueEntry *p_entry) const;
	void _remove_readonly_locked(MCPToolQueueEntry *p_entry);

public:
	struct Snapshot {
		int pending_size = 0;
		MCPToolQueueEntry *current_write = nullptr;
		Vector<MCPToolQueueEntry *> current_readonly_inflight;
		bool processing = false;
	};

	Snapshot snapshot() const;
	bool is_full() const;
	int size() const;
	MCPToolQueueEntry *enqueue(MCPToolQueueEntry *p_entry);
	bool remove(MCPToolQueueEntry *p_entry);
	MCPToolQueueEntry *find_by_request_id(const Variant &p_request_id) const;
	void set_in_flight(MCPToolQueueEntry *p_write, const Vector<MCPToolQueueEntry *> &p_readonly_inflight);
	void clear_in_flight_pointers();
	void sync_processing_flag();
	void clear_pending(bool p_signal_stateless_completion);
	void release_all_entries(Vector<MCPToolQueueEntry *> &r_pending, MCPToolQueueEntry *&r_write, Vector<MCPToolQueueEntry *> &r_readonly_inflight);

	MCPToolQueueEntry *pick_next(int p_max_readonly_inflight, bool p_prefer_readonly_lane = false) const;
	MCPToolQueueEntry *pick_next_locked(int p_max_readonly_inflight, bool p_prefer_readonly_lane) const;
	int count_pending_for_session(const String &p_session_id) const;
	int readonly_inflight_count() const;
	MCPToolQueueEntry *primary_readonly() const;
};
