/**************************************************************************/
/*  justamcp_event_store.cpp                                              */
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

#include "justamcp_event_store.h"

void MCPEventStore::configure(const String &p_session_id, const String &p_stream_id, int p_max_entries) {
	session_id = p_session_id;
	stream_id = p_stream_id;
	max_entries = MAX(1, p_max_entries);
	next_seq = 1;
	events.clear();
	event_count = 0;
	event_next = 0;
}

void MCPEventStore::clear() {
	events.clear();
	next_seq = 1;
	event_count = 0;
	event_next = 0;
}

int MCPEventStore::_logical_slot(int p_logical) const {
	const int cap = events.size();
	if (cap <= 0 || p_logical < 0 || p_logical >= event_count) {
		return 0;
	}
	const int start = event_count < cap ? 0 : event_next;
	return (start + p_logical) % cap;
}

String MCPEventStore::append_event(const String &p_event_type, const String &p_data) {
	if (events.size() != max_entries) {
		events.resize(max_entries);
		event_count = 0;
		event_next = 0;
	}
	MCPEventRecord record;
	record.id = session_id + ":" + stream_id + ":" + itos(next_seq++);
	record.event_type = p_event_type;
	record.data = p_data;
	events.write[event_next] = record;
	event_next = (event_next + 1) % max_entries;
	if (event_count < max_entries) {
		event_count++;
	}
	return record.id;
}

int MCPEventStore::find_index_after(const String &p_last_event_id) const {
	if (p_last_event_id.is_empty()) {
		return -1;
	}
	for (int i = 0; i < event_count; i++) {
		if (events[_logical_slot(i)].id == p_last_event_id) {
			return i;
		}
	}
	return -2;
}

Vector<MCPEventRecord> MCPEventStore::events_after(const String &p_last_event_id) const {
	Vector<MCPEventRecord> replay;
	const int idx = find_index_after(p_last_event_id);
	if (idx == -1) {
		return replay;
	}
	if (idx == -2) {
		return replay;
	}
	for (int i = idx + 1; i < event_count; i++) {
		replay.push_back(events[_logical_slot(i)]);
	}
	return replay;
}
