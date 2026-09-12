/**************************************************************************/
/*  test_justamcp_bridge_url_allow_list.cpp                               */
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

#include "test_justamcp_bridge_url_allow_list.h"

#ifdef TESTS_ENABLED

#ifdef TOOLS_ENABLED

#include "../tools/justamcp_mcp_client_bridge.h"

#include "core/config/project_settings.h"
#include "tests/test_macros.h"

void test_justamcp_bridge_url_allow_list() {
	JustAMCPMCPClientBridge bridge;

	Dictionary ok_args;
	ok_args["name"] = "local-bridge";
	ok_args["url"] = "http://127.0.0.1:8765/mcp";
	Dictionary ok = bridge.execute_tool("mcp_client_add_bridge", ok_args);
	CHECK(ok.get("ok", false));

	Dictionary https_args;
	https_args["name"] = "local-https";
	https_args["url"] = "https://localhost/mcp";
	Dictionary https_ok = bridge.execute_tool("mcp_client_add_bridge", https_args);
	CHECK(https_ok.get("ok", false));

	Dictionary evil_args;
	evil_args["name"] = "evil-bridge";
	evil_args["url"] = "http://evil.example/mcp";
	Dictionary evil = bridge.execute_tool("mcp_client_add_bridge", evil_args);
	CHECK(!evil.get("ok", true));
	CHECK(String(evil.get("error", "")).contains("allow-listed"));

	Dictionary ssrf_args;
	ssrf_args["name"] = "ssrf-bridge";
	ssrf_args["url"] = "http://127.0.0.1.evil.example/mcp";
	Dictionary ssrf = bridge.execute_tool("mcp_client_add_bridge", ssrf_args);
	CHECK(!ssrf.get("ok", true));

	ProjectSettings::get_singleton()->set_setting("blazium/justamcp/mcp_clients", Array());
}

#else
void test_justamcp_bridge_url_allow_list() {
	TEST_FAIL_COND(true, "TOOLS_ENABLED is required");
}
#endif

#endif
