/**************************************************************************/
/*  test_justamcp_runtime_smoke.cpp                                       */
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

#include "test_justamcp_runtime_smoke.h"

#ifdef TESTS_ENABLED

#include "../justamcp_runtime.h"
#include "core/config/project_settings.h"
#include "tests/test_macros.h"

void test_justamcp_runtime_ping_command() {
	JustAMCPRuntime runtime;
	const Dictionary result = runtime.execute_command("ping", Dictionary());
	CHECK(String(result.get("type", "")) == "pong");
	CHECK(result.has("timestamp"));
}

void test_justamcp_runtime_unknown_command() {
	JustAMCPRuntime runtime;
	const Dictionary result = runtime.execute_command("not_a_real_command", Dictionary());
	CHECK(String(result.get("type", "")) == "error");
}

void test_justamcp_runtime_blocks_call_and_callv() {
	JustAMCPRuntime runtime;
	Dictionary call_params;
	call_params["path"] = "/root";
	call_params["method"] = "call";
	const Dictionary call_result = runtime.execute_command("call_method", call_params);
	CHECK(String(call_result.get("message", "")).contains("not allowed"));

	Dictionary callv_params;
	callv_params["node"] = "/root";
	callv_params["method"] = "callv";
	const Dictionary callv_result = runtime.execute_command("call_node_method", callv_params);
	CHECK(String(callv_result.get("message", "")).contains("not allowed"));

	Dictionary kept;
	kept["node"] = "/root";
	kept["method"] = "execute";
	const Dictionary kept_result = runtime.execute_command("call_node_method", kept);
	CHECK(String(kept_result.get("message", "")).contains("not allowed"));
}

void test_justamcp_runtime_tcp_requires_token_and_loopback() {
	CHECK(JustAMCPRuntime::tcp_bind_address() == "127.0.0.1");
	ProjectSettings *settings = ProjectSettings::get_singleton();
	REQUIRE(settings != nullptr);
	const bool had = settings->has_setting("blazium/justamcp/runtime_token");
	const Variant previous = had ? settings->get_setting("blazium/justamcp/runtime_token") : Variant();
	settings->set_setting("blazium/justamcp/runtime_token", "");
	CHECK_FALSE(JustAMCPRuntime::tcp_token_matches("runtime-secret"));
	CHECK(JustAMCPRuntime::configured_tcp_token().is_empty());
	settings->set_setting("blazium/justamcp/runtime_token", "runtime-secret");
	CHECK(JustAMCPRuntime::tcp_token_matches("runtime-secret"));
	CHECK_FALSE(JustAMCPRuntime::tcp_token_matches("runtime-secret-nope"));
	CHECK_FALSE(JustAMCPRuntime::tcp_token_matches(""));
	if (had) {
		settings->set_setting("blazium/justamcp/runtime_token", previous);
	} else {
		settings->set_setting("blazium/justamcp/runtime_token", "");
	}
}

#endif
