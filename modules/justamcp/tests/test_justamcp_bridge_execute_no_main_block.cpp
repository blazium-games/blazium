/**************************************************************************/
/*  test_justamcp_bridge_execute_no_main_block.cpp                        */
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

#include "test_justamcp_bridge_execute_no_main_block.h"

#include "test_justamcp_fixture.h"

#ifdef TESTS_ENABLED

#if defined(MODULE_HTTPSERVER_ENABLED)

#include "../justamcp_server.h"
#include "../justamcp_tool_context.h"
#include "../tools/justamcp_mcp_client_bridge.h"

#include "core/config/project_settings.h"
#include "core/object/message_queue.h"
#include "core/os/os.h"
#include "tests/test_macros.h"

void test_justamcp_bridge_execute_no_main_block() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	(void)server;

	JustAMCPMCPClientBridge bridge;
	Dictionary add_args;
	add_args["name"] = "slow-bridge";
	add_args["url"] = "http://127.0.0.1:1/mcp";
	Dictionary added = bridge.execute_tool("mcp_client_add_bridge", add_args);
	CHECK(added.get("ok", false));

	Dictionary args;
	args["bridge_name"] = "slow-bridge";

	JustAMCPToolContextScope scope(8801);
	const uint64_t start_ms = OS::get_singleton()->get_ticks_msec();
	Dictionary result = bridge.execute_tool("mcp_client_list_remote_tools", args);
	const uint64_t elapsed_ms = OS::get_singleton()->get_ticks_msec() - start_ms;
	CHECK(elapsed_ms < 50);
	CHECK(bool(result.get("_justamcp_async_pending", false)));

	OS::get_singleton()->delay_usec(500000);
	if (MessageQueue::get_singleton()) {
		MessageQueue::get_singleton()->flush();
	}
	ProjectSettings::get_singleton()->set_setting("blazium/justamcp/mcp_clients", Array());
}

#else
void test_justamcp_bridge_execute_no_main_block() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}
#endif

#endif
