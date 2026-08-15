/**************************************************************************/
/*  justamcp_tool_queue_state.cpp                                         */
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

#include "justamcp_tool_queue_state.h"

#include "justamcp_server.h"

namespace JustAMCPToolQueueState {

void sync_processing_flag(MCPToolQueueEntry *p_current_write, const Vector<MCPToolQueueEntry *> &p_readonly_inflight, bool &r_tool_queue_processing) {
	r_tool_queue_processing = (p_current_write != nullptr || !p_readonly_inflight.is_empty());
}

bool contains_readonly_inflight(const Vector<MCPToolQueueEntry *> &p_readonly_inflight, MCPToolQueueEntry *p_entry) {
	if (!p_entry) {
		return false;
	}
	for (int i = 0; i < p_readonly_inflight.size(); i++) {
		if (p_readonly_inflight[i] == p_entry) {
			return true;
		}
	}
	return false;
}

void remove_readonly_inflight(Vector<MCPToolQueueEntry *> &r_readonly_inflight, MCPToolQueueEntry *p_entry) {
	for (int i = 0; i < r_readonly_inflight.size(); i++) {
		if (r_readonly_inflight[i] == p_entry) {
			r_readonly_inflight.remove_at(i);
			return;
		}
	}
}

MCPToolQueueEntry *get_preferred_in_flight_entry(MCPToolQueueEntry *p_current_write, const Vector<MCPToolQueueEntry *> &p_readonly_inflight, bool p_prefer_readonly_lane) {
	const int readonly_count = p_readonly_inflight.size();
	const int total = (p_current_write ? 1 : 0) + readonly_count;
	if (total != 1) {
		return nullptr;
	}
	if (p_prefer_readonly_lane && readonly_count == 1) {
		return p_readonly_inflight[0];
	}
	if (p_current_write) {
		return p_current_write;
	}
	return readonly_count == 1 ? p_readonly_inflight[0] : nullptr;
}

} //namespace JustAMCPToolQueueState
