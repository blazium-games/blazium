/**************************************************************************/
/*  justamcp_server_request_lookup.cpp                                    */
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

#include "justamcp_server_request_lookup.h"

#include "justamcp_server.h"
#include "tools/justamcp_json_rpc_helpers.h"

namespace JustAMCPServerRequestLookup {

MCPToolQueueEntry *find_entry_by_request_id(
		const Vector<MCPToolQueueEntry *> &p_queue,
		MCPToolQueueEntry *p_current_write,
		const Vector<MCPToolQueueEntry *> &p_readonly_inflight,
		const Variant &p_request_id) {
	if (p_current_write && JustAMCPJsonRpcHelpers::request_ids_equal(p_current_write->request_id, p_request_id)) {
		return p_current_write;
	}
	for (int i = 0; i < p_readonly_inflight.size(); i++) {
		MCPToolQueueEntry *entry = p_readonly_inflight[i];
		if (entry && JustAMCPJsonRpcHelpers::request_ids_equal(entry->request_id, p_request_id)) {
			return entry;
		}
	}
	for (int i = 0; i < p_queue.size(); i++) {
		MCPToolQueueEntry *entry = p_queue[i];
		if (entry && JustAMCPJsonRpcHelpers::request_ids_equal(entry->request_id, p_request_id)) {
			return entry;
		}
	}
	return nullptr;
}

} //namespace JustAMCPServerRequestLookup
