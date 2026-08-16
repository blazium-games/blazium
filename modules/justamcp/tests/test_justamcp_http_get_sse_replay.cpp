/**************************************************************************/
/*  test_justamcp_http_get_sse_replay.cpp                                 */
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

#ifdef TESTS_ENABLED

#include "test_justamcp_http_get_sse_replay.h"

#include "../justamcp_event_store.h"

#include "tests/test_macros.h"

void test_justamcp_event_store_replays_after_last_event_id() {
	MCPEventStore store;
	store.configure("session-1", "stream-a", 32);
	const String first_id = store.append_event("message", "{\"seq\":1}");
	const String second_id = store.append_event("message", "{\"seq\":2}");
	const String third_id = store.append_event("message", "{\"seq\":3}");

	const Vector<MCPEventRecord> replay = store.events_after(first_id);
	CHECK(replay.size() == 2);
	CHECK(replay[0].id == second_id);
	CHECK(replay[1].id == third_id);
	CHECK(store.find_index_after(third_id) == 2);
	CHECK(store.events_after(third_id).is_empty());
	CHECK(store.find_index_after("missing-id") == -2);
}

#endif
