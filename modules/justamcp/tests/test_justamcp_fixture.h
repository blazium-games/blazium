/**************************************************************************/
/*  test_justamcp_fixture.h                                               */
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

#ifdef TESTS_ENABLED

#include "../justamcp_server.h"
#include "../justamcp_session_manager.h"

// Non-blocking MCP calls must return immediately. Sanitizers inflate wall time
// enough that the 50ms budget flakes (CI saw 63ms under ASan/UBSan).
static inline uint64_t justamcp_nonblocking_call_budget_ms() {
#ifdef SANITIZERS_ENABLED
	return 250;
#else
	return 50;
#endif
}

class JustAMCPTestServerFixture {
	JustAMCPServer server;

public:
	JustAMCPServer &get_server() {
		return server;
	}

	JustAMCPServer *operator->() {
		return &server;
	}

	~JustAMCPTestServerFixture() {
		server.test_stop_server();
		server.test_clear_tool_queue();
		if (MCPSessionManager *session_manager = server.test_get_session_manager()) {
			session_manager->clear_all();
		}
	}
};

#endif
