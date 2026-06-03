/**************************************************************************/
/*  justamcp_event_store.cpp                                              */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#include "justamcp_event_store.h"

void MCPEventStore::configure(const String &p_session_id, const String &p_stream_id, int p_max_entries) {
	session_id = p_session_id;
	stream_id = p_stream_id;
	max_entries = MAX(1, p_max_entries);
	next_seq = 1;
	events.clear();
}

void MCPEventStore::clear() {
	events.clear();
	next_seq = 1;
}

String MCPEventStore::append_event(const String &p_event_type, const String &p_data) {
	MCPEventRecord record;
	record.id = session_id + ":" + stream_id + ":" + itos(next_seq++);
	record.event_type = p_event_type;
	record.data = p_data;
	events.push_back(record);
	while (events.size() > max_entries) {
		events.remove_at(0);
	}
	return record.id;
}

int MCPEventStore::find_index_after(const String &p_last_event_id) const {
	if (p_last_event_id.is_empty()) {
		return -1;
	}
	for (int i = 0; i < events.size(); i++) {
		if (events[i].id == p_last_event_id) {
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
	for (int i = idx + 1; i < events.size(); i++) {
		replay.push_back(events[i]);
	}
	return replay;
}
