/**************************************************************************/
/*  test_justamcp_elicitation.cpp                                         */
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

#include "test_justamcp_fixture.h"

#include "modules/modules_enabled.gen.h"

#if defined(MODULE_HTTPSERVER_ENABLED)

#include "../justamcp_json_rpc_transport.h"
#include "../justamcp_mcp_spec.h"
#include "../justamcp_server.h"
#include "../justamcp_session_manager.h"
#include "../tools/justamcp_json_rpc_helpers.h"
#include "../tools/justamcp_json_rpc_router.h"
#ifdef TOOLS_ENABLED
#include "../tools/justamcp_prompt_executor.h"
#include "../tools/justamcp_resource_manifest.h"
#include "../tools/justamcp_tool_executor.h"
#endif

#include "tests/test_macros.h"

void test_justamcp_initialize_2025_advertises_elicitation() {
	JustAMCPTestServerFixture fixture;
	Dictionary payload;
	Dictionary params;
	params["protocolVersion"] = "2025-11-25";
	params["capabilities"] = Dictionary();
	payload["params"] = params;
	Dictionary routed = JustAMCPJsonRpcRouter::route_initialize(&fixture.get_server(), payload, 1);
	Dictionary result = routed["result"];
	Dictionary capabilities = result["capabilities"];
	CHECK(capabilities.has("elicitation"));
	Dictionary elicitation = capabilities["elicitation"];
	CHECK(elicitation.has("form"));
	CHECK(elicitation.has("url"));
}

void test_justamcp_older_initialize_does_not_break() {
	JustAMCPTestServerFixture fixture;
	Dictionary payload;
	Dictionary params;
	params["protocolVersion"] = "2024-11-05";
	params["capabilities"] = Dictionary();
	payload["params"] = params;
	Dictionary routed = JustAMCPJsonRpcRouter::route_initialize(&fixture.get_server(), payload, 1);
	CHECK(bool(routed.get("handled", false)));
	Dictionary result = routed["result"];
	CHECK(String(result.get("protocolVersion", "")) == "2024-11-05");
	Dictionary capabilities = result["capabilities"];
	CHECK(capabilities.has("tools"));
	CHECK(!capabilities.has("elicitation"));
}

void test_justamcp_roots_list_changed_updates_session() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPSessionManager *session_manager = server.test_get_session_manager();
	REQUIRE(session_manager != nullptr);
	CHECK(session_manager->test_register_stream_for_session("roots-session", 4401));

	session_manager->mark_session_initialized("roots-session");
	CHECK(server.get_session_roots("roots-session").is_empty());

	Dictionary root;
	root["uri"] = "file:///workspace";
	root["name"] = "workspace";
	Array roots;
	roots.push_back(root);
	Dictionary result;
	result["roots"] = roots;
	Dictionary payload;
	payload["jsonrpc"] = "2.0";
	payload["id"] = "roots_list_roots-session";
	payload["result"] = result;
	JustAMCPJsonRpcTransport::handle_json_rpc_parsed(&server, payload, Ref<HTTPResponse>(), "roots-session");
	Array stored = server.get_session_roots("roots-session");
	CHECK(stored.size() == 1);
	CHECK(String(Dictionary(stored[0]).get("uri", "")) == "file:///workspace");

	Dictionary changed_root;
	changed_root["uri"] = "file:///other";
	Array changed;
	changed.push_back(changed_root);
	Dictionary notify;
	notify["jsonrpc"] = "2.0";
	notify["method"] = "notifications/roots/list_changed";
	Dictionary notify_params;
	notify_params["roots"] = changed;
	notify["params"] = notify_params;
	JustAMCPJsonRpcTransport::handle_json_rpc_parsed(&server, notify, Ref<HTTPResponse>(), "roots-session");
	stored = server.get_session_roots("roots-session");
	CHECK(stored.size() == 1);
	CHECK(String(Dictionary(stored[0]).get("uri", "")) == "file:///other");
}

void test_justamcp_elicitation_hold_and_decline() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	Dictionary queue_full;
	MCPToolQueueEntry *entry = server.test_enqueue_tool_request(77, "blazium_tags_add", Dictionary(), queue_full);
	REQUIRE(entry != nullptr);
	server.test_clear_last_send_tool_result();

	server.hold_tool_for_elicitation(77, "blazium_tags_add", Dictionary(), justamcp_confirm_enum_schema(), "form");
	CHECK(server.has_pending_elicitation(77));
	CHECK(server.test_peek_last_send_tool_result().is_empty());

	Dictionary elicit;
	elicit["action"] = "decline";
	server.complete_elicitation("77", elicit);
	CHECK(!server.has_pending_elicitation(77));
	Dictionary completed = server.test_peek_last_send_tool_result();
	CHECK(completed.has("result"));
	CHECK(bool(Dictionary(completed["result"]).get("isError", false)));
}

void test_justamcp_url_elicitation_error_shape() {
	Dictionary rpc = justamcp_url_elicitation_error_rpc(12, "elicitation_12", "https://example.test/consent", "URL elicitation required");
	CHECK(rpc.has("error"));
	Dictionary error = rpc["error"];
	CHECK(int(error.get("code", 0)) == -32042);
	CHECK(String(Dictionary(error.get("data", Dictionary())).get("url", "")).begins_with("https://"));

	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	Dictionary queue_full;
	MCPToolQueueEntry *entry = server.test_enqueue_tool_request(12, "blazium_logs_read", Dictionary(), queue_full);
	REQUIRE(entry != nullptr);
	server.test_clear_last_send_tool_result();
	server.send_url_elicitation_error("12", "elicitation_12", "https://example.test/consent", "URL elicitation required");
	Dictionary sent = server.test_peek_last_send_tool_result();
	CHECK(sent.has("error"));
	CHECK(int(Dictionary(sent["error"]).get("code", 0)) == -32042);
}

void test_justamcp_icons_and_invalid_tool_name() {
	CHECK(justamcp_is_valid_mcp_tool_name("blazium_search_tools"));
	CHECK(!justamcp_is_valid_mcp_tool_name("Bad-Name"));
	CHECK(!justamcp_is_valid_mcp_tool_name(""));

#ifdef TOOLS_ENABLED
	Array resources = JustAMCPResourceManifest::get_static_resource_schemas();
	REQUIRE(resources.size() > 0);
	Dictionary first = resources[0];
	CHECK(first.has("icons"));
	Array templates = JustAMCPResourceManifest::get_static_resource_template_schemas();
	REQUIRE(templates.size() > 0);
	CHECK(Dictionary(templates[0]).has("icons"));

	JustAMCPPromptExecutor prompts;
	Dictionary listed = prompts.list_prompts();
	Array prompt_list = listed.get("prompts", Array());
	if (prompt_list.size() > 0) {
		CHECK(Dictionary(prompt_list[0]).has("icons"));
	}

	JustAMCPToolExecutor executor;
	Dictionary invalid = executor.execute_tool("Bad-Name", Dictionary());
	CHECK(!bool(invalid.get("ok", true)));
	CHECK(String(invalid.get("error", "")).contains("Invalid tool name"));
	Dictionary formatted = JustAMCPJsonRpcHelpers::format_tool_result(false, invalid.get("error", ""), String(invalid.get("error", "")));
	CHECK(formatted.has("result"));
	CHECK(bool(Dictionary(formatted["result"]).get("isError", false)));
	CHECK(!formatted.has("error"));
#endif
}

#else

void test_justamcp_initialize_2025_advertises_elicitation() {}
void test_justamcp_older_initialize_does_not_break() {}
void test_justamcp_roots_list_changed_updates_session() {}
void test_justamcp_elicitation_hold_and_decline() {}
void test_justamcp_url_elicitation_error_shape() {}
void test_justamcp_icons_and_invalid_tool_name() {}

#endif

#endif
