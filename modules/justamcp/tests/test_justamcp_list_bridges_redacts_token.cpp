/**************************************************************************/
/*  test_justamcp_list_bridges_redacts_token.cpp                          */
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

#include "test_justamcp_list_bridges_redacts_token.h"

#ifdef TESTS_ENABLED

#ifdef TOOLS_ENABLED

#include "../tools/justamcp_mcp_client_bridge.h"

#include "core/config/project_settings.h"
#include "tests/test_macros.h"

void test_justamcp_list_bridges_redacts_token() {
	JustAMCPMCPClientBridge bridge;
	Dictionary add_args;
	add_args["name"] = "secret-bridge";
	add_args["url"] = "http://127.0.0.1:9999/mcp";
	add_args["auth_token"] = "super-secret-token";
	Dictionary added = bridge.execute_tool("mcp_client_add_bridge", add_args);
	CHECK(added.get("ok", false));

	Dictionary listed = bridge.execute_tool("mcp_client_list_bridges", Dictionary());
	CHECK(listed.get("ok", false));
	Array bridges = listed.get("bridges", Array());
	bool found = false;
	for (int i = 0; i < bridges.size(); i++) {
		Dictionary b = bridges[i];
		if (String(b.get("name", "")) != "secret-bridge") {
			continue;
		}
		found = true;
		CHECK(!b.has("auth_token"));
		CHECK(bool(b.get("has_auth_token", false)));
	}
	CHECK(found);

	ProjectSettings::get_singleton()->set_setting("blazium/justamcp/mcp_clients", Array());
}

#else
void test_justamcp_list_bridges_redacts_token() {
	TEST_FAIL_COND(true, "TOOLS_ENABLED is required");
}
#endif

#endif
