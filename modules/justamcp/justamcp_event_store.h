/**************************************************************************/
/*  justamcp_event_store.h                                                */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/templates/vector.h"

struct MCPEventRecord {
	String id;
	String event_type;
	String data;
};

class MCPEventStore {
	String session_id;
	String stream_id;
	Vector<MCPEventRecord> events;
	uint64_t next_seq = 1;
	int max_entries = 500;

public:
	void configure(const String &p_session_id, const String &p_stream_id, int p_max_entries);

	String append_event(const String &p_event_type, const String &p_data);
	const Vector<MCPEventRecord> &get_events() const { return events; }

	int find_index_after(const String &p_last_event_id) const;
	Vector<MCPEventRecord> events_after(const String &p_last_event_id) const;

	void clear();
};
