/**************************************************************************/
/*  test_justamcp_bridge_no_sync_fallback_on_main.cpp                     */
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

#include "test_justamcp_bridge_no_sync_fallback_on_main.h"

#include "test_justamcp_fixture.h"

#ifdef TESTS_ENABLED

#if defined(MODULE_HTTPSERVER_ENABLED)

#include "../justamcp_server.h" // IWYU pragma: keep
#include "../tools/justamcp_mcp_client_bridge.h"

#include "core/os/os.h"
#include "tests/test_macros.h"

void test_justamcp_bridge_no_sync_fallback_on_main() {
	JustAMCPTestServerFixture fixture;
	(void)fixture;

	JustAMCPMCPClientBridge bridge;
	Dictionary args;
	args["bridge_name"] = "missing-bridge";

	const uint64_t start_ms = OS::get_singleton()->get_ticks_msec();
	Dictionary result = bridge.execute_tool("mcp_client_list_remote_tools", args);
	const uint64_t elapsed_ms = OS::get_singleton()->get_ticks_msec() - start_ms;
	CHECK(elapsed_ms < 50);
	CHECK(bool(result.get("_justamcp_async_required", false)));
	CHECK(!bool(result.get("ok", true)));
}

#else
void test_justamcp_bridge_no_sync_fallback_on_main() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}
#endif

#endif
