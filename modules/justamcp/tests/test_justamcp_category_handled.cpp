/**************************************************************************/
/*  test_justamcp_category_handled.cpp                                    */
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

#include "test_justamcp_category_handled.h"

#ifdef TESTS_ENABLED

#include "../justamcp_mcp_tool_macros.h"
#include "../justamcp_server.h"
#include "../mcp_tool_queue_entry.h"
#include "../tools/justamcp_category_dispatch.h"
#include "../tools/justamcp_readonly_tools.h"
#include "../tools/justamcp_scene_tools.h"
#include "../tools/justamcp_tool_executor.h"
#include "test_justamcp_fixture.h"

#include "core/config/project_settings.h"
#include "core/object/callable_method_pointer.h"
#include "modules/modules_enabled.gen.h"
#include "tests/test_macros.h"

void test_justamcp_ok_false_stays_handled() {
	JustAMCPToolExecutor executor;

	Dictionary routed = JustAMCPToolCategoryDispatch::dispatch_module_tools(
			&executor, "analysis_tools", "analyze_signal_flow", Dictionary());
	CHECK(bool(routed.get("handled", false)));
	CHECK(routed.has("ok"));
	CHECK(!bool(routed.get("ok", true)));

	Dictionary via_executor = executor.execute_tool("blazium_analyze_signal_flow", Dictionary());
	CHECK(via_executor.has("ok"));
	CHECK(!bool(via_executor.get("ok", true)));
	const Variant err = via_executor.get("error", Variant());
	String err_text;
	if (err.get_type() == Variant::DICTIONARY) {
		err_text = String(Dictionary(err).get("message", ""));
	} else if (err.get_type() == Variant::STRING) {
		err_text = err;
	}
	CHECK(!err_text.to_lower().contains("unknown tool"));
}

void test_justamcp_empty_category_result_unhandled() {
	JustAMCPToolExecutor executor;
	Dictionary routed = JustAMCPToolCategoryDispatch::dispatch_module_tools(
			&executor, "analysis_tools", "definitely_not_a_real_analysis_tool", Dictionary());
	CHECK(!bool(routed.get("handled", true)));
}

static bool s_tool_requested = false;
static String s_tool_requested_name;

static void _on_tool_requested_probe(const Variant &p_request_id, const String &p_tool_name, const Dictionary &p_args) {
	(void)p_request_id;
	(void)p_args;
	s_tool_requested = true;
	s_tool_requested_name = p_tool_name;
}

void test_justamcp_sse_bound_worker_safe_uses_worker() {
#if defined(MODULE_HTTPSERVER_ENABLED)
	const int prev = int(GLOBAL_GET("blazium/justamcp/readonly_worker_concurrency"));
	ProjectSettings::get_singleton()->set_setting("blazium/justamcp/readonly_worker_concurrency", 2);

	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	JustAMCPToolExecutor executor;
	executor.set_as_active_instance();

	s_tool_requested = false;
	s_tool_requested_name = String();
	server.connect("tool_requested", callable_mp_static(_on_tool_requested_probe));

	Dictionary queue_full;
	MCPToolQueueEntry *entry = server.test_enqueue_tool_request(9001, "blazium_logs_read", Dictionary(), queue_full);
	CHECK(entry != nullptr);
	CHECK(JustAMCPReadonlyTools::is_worker_safe_tool("blazium_logs_read"));
	entry->sse_connection_id = 42;

	server.test_process_pending_tools();

	CHECK(!s_tool_requested);
	CHECK(s_tool_requested_name.is_empty());

	server.disconnect("tool_requested", callable_mp_static(_on_tool_requested_probe));
	ProjectSettings::get_singleton()->set_setting("blazium/justamcp/readonly_worker_concurrency", prev);
#else
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
#endif
}

void test_justamcp_crash_guards_bundle() {
	Dictionary bridge_err = MCP_ERROR(-32000, "Game input bridge inactive");
	CHECK(bridge_err.has("ok"));
	CHECK(!bool(bridge_err.get("ok", true)));
	CHECK(bridge_err.has("error"));

	ProjectSettings::get_singleton()->set_setting("application/run/main_scene", "res://main.tscn");
	JustAMCPSceneTools scene_tools;
	Dictionary del_args;
	del_args["path"] = "res://main.tscn";
	Dictionary del = scene_tools.delete_scene_file(del_args);
	CHECK(del.has("ok"));
	CHECK(!bool(del.get("ok", true)));
	CHECK(String(del.get("error", "")).contains("protected"));

	Dictionary godot_args;
	godot_args["path"] = "res://project.godot";
	Dictionary del_godot = scene_tools.delete_scene_file(godot_args);
	CHECK(del_godot.has("ok"));
	CHECK(!bool(del_godot.get("ok", true)));

	Dictionary blazium_args;
	blazium_args["path"] = "res://project.blazium";
	Dictionary del_blazium = scene_tools.delete_scene_file(blazium_args);
	CHECK(del_blazium.has("ok"));
	CHECK(!bool(del_blazium.get("ok", true)));

	JustAMCPToolExecutor executor;
	Dictionary bad_script;
	bad_script["path"] = "res://main.tscn";
	Dictionary via = executor.execute_tool("blazium_validate_script", bad_script);
	CHECK(via.has("ok"));
	CHECK(!bool(via.get("ok", true)));
}

#endif
