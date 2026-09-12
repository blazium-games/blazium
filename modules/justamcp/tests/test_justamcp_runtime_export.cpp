/**************************************************************************/
/*  test_justamcp_runtime_export.cpp                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#ifdef TESTS_ENABLED

#include "test_justamcp_runtime_export.h"
#include "../justamcp_cli_args.h"
#include "../justamcp_json_rpc_transport.h"
#include "../justamcp_project_registry.h"
#include "../justamcp_runtime.h"
#include "../justamcp_server.h"
#include "../tools/justamcp_json_rpc_router.h"
#include "../tools/justamcp_settings_resolver.h"
#include "core/config/project_settings.h"
#include "core/object/callable_method_pointer.h"
#include "core/object/object.h"
#include "modules/modules_enabled.gen.h"
#ifdef MODULE_HTTPSERVER_ENABLED
#include "modules/httpserver/http_response.h"
#endif
#include "tests/test_macros.h"

class JustAMCPTestProjectHost : public Object {
public:
	String echo(const Dictionary &p_args) {
		return String(p_args.get("text", ""));
	}
	String hello(const Dictionary &p_args) {
		return "hello " + String(p_args.get("name", "world"));
	}
};

static Vector<String> _args(const char *p_a, const char *p_b = nullptr, const char *p_c = nullptr) {
	Vector<String> args;
	args.push_back(p_a);
	if (p_b) {
		args.push_back(p_b);
	}
	if (p_c) {
		args.push_back(p_c);
	}
	return args;
}

void test_justamcp_skip_autowork_unless_enable_mcp() {
	CHECK(JustAMCPCliArgs::skip_mcp_server_for_args(_args("--aw-dir", ".")));
	CHECK(!JustAMCPCliArgs::skip_mcp_server_for_args(_args("--aw-dir", ".", "--enable-mcp")));
	CHECK(!JustAMCPCliArgs::skip_mcp_server_for_args(_args("--aw-dir", ".", "--enable-mcp-game-control")));
	CHECK(JustAMCPCliArgs::skip_mcp_server_for_args(_args("--test")));
	CHECK(JustAMCPCliArgs::skip_mcp_server_for_args(_args("--export-release")));
	CHECK(!JustAMCPCliArgs::skip_mcp_server_for_args(Vector<String>()));
}

void test_justamcp_instantiate_gates() {
	CHECK(!JustAMCPCliArgs::should_instantiate_editor_server_for(true, true));
	CHECK(!JustAMCPCliArgs::should_instantiate_editor_server_for(false, false));
	CHECK(JustAMCPCliArgs::should_instantiate_editor_server_for(false, true));
	CHECK(!JustAMCPCliArgs::should_instantiate_runtime_for(true, true));
	CHECK(!JustAMCPCliArgs::should_instantiate_runtime_for(false, false));
	CHECK(JustAMCPCliArgs::should_instantiate_runtime_for(false, true));
}

void test_justamcp_runtime_port_is_editor_plus_one() {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	CHECK(ps);
	const int prev_port = int(ps->get_setting("blazium/justamcp/server_port", 6506));
	const int prev_export = int(ps->get_setting("blazium/justamcp/export_port", 0));
	const bool prev_override = bool(ps->get_setting("blazium/justamcp/override_editor_settings", false));

	ps->set_setting("blazium/justamcp/override_editor_settings", true);
	ps->set_setting("blazium/justamcp/server_port", 6506);
	ps->set_setting("blazium/justamcp/export_port", 0);
	JustAMCPCliArgs::clear_test_overrides();

	CHECK(JustAMCPSettingsResolver::resolve_server_port() == 6506);
	CHECK(JustAMCPSettingsResolver::resolve_runtime_port() == 6507);
	CHECK(!JustAMCPSettingsResolver::runtime_port_conflicts_with_editor());

	ps->set_setting("blazium/justamcp/export_port", 6600);
	CHECK(JustAMCPSettingsResolver::resolve_runtime_port() == 6600);

	JustAMCPCliArgs::set_test_mcp_port(6506);
	CHECK(JustAMCPSettingsResolver::resolve_server_port() == 6506);
	CHECK(JustAMCPSettingsResolver::resolve_runtime_port() == 6600);

	JustAMCPCliArgs::set_test_mcp_game_port(6700);
	CHECK(JustAMCPSettingsResolver::resolve_runtime_port() == 6700);

	JustAMCPCliArgs::clear_test_overrides();
	ps->set_setting("blazium/justamcp/export_port", 6506);
	JustAMCPCliArgs::set_test_mcp_port(6506);
	CHECK(JustAMCPSettingsResolver::runtime_port_conflicts_with_editor());

	JustAMCPCliArgs::clear_test_overrides();
	ps->set_setting("blazium/justamcp/server_port", prev_port);
	ps->set_setting("blazium/justamcp/export_port", prev_export);
	ps->set_setting("blazium/justamcp/override_editor_settings", prev_override);
}

void test_justamcp_register_tool_in_tools_list() {
	JustAMCPProjectRegistry::clear();
	Dictionary schema;
	schema["type"] = "object";
	JustAMCPProjectRegistry::register_tool("proj_echo", "Echo a value", schema, Callable());

	CHECK(JustAMCPProjectRegistry::has_tool("proj_echo"));
	const Array listed = JustAMCPProjectRegistry::list_tool_schemas();
	bool found = false;
	for (int i = 0; i < listed.size(); i++) {
		Dictionary tool = listed[i];
		if (String(tool.get("name", "")) == "proj_echo") {
			found = true;
		}
	}
	CHECK(found);

#ifdef TOOLS_ENABLED
	Dictionary routed = JustAMCPJsonRpcRouter::route_tools_list("", 42);
	CHECK(routed.has("result"));
	Dictionary result = routed["result"];
	Array tools = result.get("tools", Array());
	bool listed_in_rpc = false;
	for (int i = 0; i < tools.size(); i++) {
		Dictionary tool = tools[i];
		if (String(tool.get("name", "")) == "proj_echo") {
			listed_in_rpc = true;
		}
	}
	CHECK(listed_in_rpc);
#endif

	JustAMCPRuntime runtime;
	CHECK(runtime.list_tools().size() >= 1);
	CHECK(!runtime.is_listening());

	JustAMCPProjectRegistry::clear();
}

void test_justamcp_runtime_host_list_is_project_only() {
	JustAMCPProjectRegistry::clear();
	Dictionary schema;
	schema["type"] = "object";
	JustAMCPProjectRegistry::register_tool("project_echo", "Echo a value", schema, Callable());
	JustAMCPProjectRegistry::register_prompt("project_hello", "Greet", Callable());

	const Array tools = JustAMCPProjectRegistry::list_tool_schemas();
	CHECK(tools.size() == 1);
	bool found_echo = false;
	for (int i = 0; i < tools.size(); i++) {
		const Dictionary tool = tools[i];
		const String name = String(tool.get("name", ""));
		CHECK(!name.begins_with("blazium_"));
		if (name == "project_echo") {
			found_echo = true;
		}
	}
	CHECK(found_echo);

	const Array prompts = JustAMCPProjectRegistry::list_prompt_schemas();
	CHECK(prompts.size() == 1);
	CHECK(String(Dictionary(prompts[0]).get("name", "")) == "project_hello");

	Dictionary params;
	params["protocolVersion"] = "2025-11-25";
	Dictionary payload;
	payload["params"] = params;
	Dictionary routed = JustAMCPJsonRpcRouter::route_runtime_host_initialize(nullptr, payload, 1);
	CHECK(routed.has("result"));
	const Dictionary result = routed["result"];
	const Dictionary capabilities = result.get("capabilities", Dictionary());
	CHECK(capabilities.has("tools"));
	CHECK(capabilities.has("prompts"));
	CHECK(!capabilities.has("resources"));
	CHECK(!capabilities.has("tasks"));
	CHECK(String(Dictionary(result.get("serverInfo", Dictionary())).get("name", "")) == "blazium-game");

	JustAMCPProjectRegistry::clear();
}

void test_justamcp_runtime_host_refuses_editor_port() {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	CHECK(ps);
	const int prev_port = int(ps->get_setting("blazium/justamcp/server_port", 6506));
	const int prev_export = int(ps->get_setting("blazium/justamcp/export_port", 0));
	const bool prev_override = bool(ps->get_setting("blazium/justamcp/override_editor_settings", false));
	const bool prev_game = bool(ps->get_setting("blazium/justamcp/game_control_enabled", false));

	ps->set_setting("blazium/justamcp/override_editor_settings", true);
	ps->set_setting("blazium/justamcp/server_port", 16506);
	ps->set_setting("blazium/justamcp/export_port", 16506);
	ps->set_setting("blazium/justamcp/game_control_enabled", true);
	CHECK(JustAMCPSettingsResolver::runtime_port_conflicts_with_editor());

	JustAMCPServer server;
	server.set_runtime_host(true);
	server.test_start_server();
	CHECK(!server.is_server_started());

	ps->set_setting("blazium/justamcp/server_port", prev_port);
	ps->set_setting("blazium/justamcp/export_port", prev_export);
	ps->set_setting("blazium/justamcp/override_editor_settings", prev_override);
	ps->set_setting("blazium/justamcp/game_control_enabled", prev_game);
}

void test_justamcp_project_mcp_name_and_dir_validation() {
	CHECK(JustAMCPProjectRegistry::is_valid_entry_name("project_echo"));
	CHECK(!JustAMCPProjectRegistry::is_valid_entry_name(""));
	CHECK(!JustAMCPProjectRegistry::is_valid_entry_name(" project_echo"));
	CHECK(!JustAMCPProjectRegistry::is_valid_entry_name("project echo"));
	CHECK(!JustAMCPProjectRegistry::is_valid_entry_name("blazium_search_tools"));

	CHECK(JustAMCPRuntime::is_valid_project_mcp_dir("res://mcp"));
	CHECK(JustAMCPRuntime::is_valid_project_mcp_dir("res://"));
	CHECK(!JustAMCPRuntime::is_valid_project_mcp_dir("user://mcp"));
	CHECK(!JustAMCPRuntime::is_valid_project_mcp_dir("C:/mcp"));
	CHECK(!JustAMCPRuntime::is_valid_project_mcp_dir("res://foo/../bar"));
	CHECK(!JustAMCPRuntime::is_valid_project_mcp_dir("res://foo//bar"));

	JustAMCPProjectRegistry::clear();
	Dictionary schema;
	schema["type"] = "object";
	ERR_PRINT_OFF;
	JustAMCPProjectRegistry::register_tool("blazium_spoof", "nope", schema, Callable());
	JustAMCPProjectRegistry::register_prompt("bad name", "nope", Callable());
	ERR_PRINT_ON;
	CHECK(!JustAMCPProjectRegistry::has_tool("blazium_spoof"));
	CHECK(!JustAMCPProjectRegistry::has_prompt("bad name"));
	JustAMCPProjectRegistry::clear();
}

void test_justamcp_runtime_host_call_and_get() {
	JustAMCPProjectRegistry::clear();
	JustAMCPTestProjectHost *host = memnew(JustAMCPTestProjectHost);
	Dictionary schema;
	schema["type"] = "object";
	JustAMCPProjectRegistry::register_tool("proj_echo", "Echo", schema, callable_mp(host, &JustAMCPTestProjectHost::echo));
	JustAMCPProjectRegistry::register_prompt("proj_hello", "Greet", callable_mp(host, &JustAMCPTestProjectHost::hello));

	Dictionary echo_args;
	echo_args["text"] = "ping";
	const Dictionary tool_call = JustAMCPProjectRegistry::call_tool_mcp("proj_echo", echo_args);
	const Dictionary tool_result = tool_call.get("result", Dictionary());
	CHECK(!bool(tool_result.get("isError", true)));
	Array tool_content = tool_result.get("content", Array());
	CHECK(tool_content.size() >= 1);
	CHECK(String(Dictionary(tool_content[0]).get("text", "")) == "ping");

	Dictionary hello_args;
	hello_args["name"] = "cursor";
	const Dictionary prompt_call = JustAMCPProjectRegistry::call_prompt("proj_hello", hello_args);
	CHECK(bool(prompt_call.get("ok", false)));
	Array messages = prompt_call.get("messages", Array());
	CHECK(messages.size() >= 1);
	Array content = Dictionary(messages[0]).get("content", Array());
	CHECK(String(Dictionary(content[0]).get("text", "")) == "hello cursor");

#if defined(MODULE_HTTPSERVER_ENABLED)
	JustAMCPServer server;
	server.set_runtime_host(true);

	Dictionary call_params;
	call_params["name"] = "proj_echo";
	call_params["arguments"] = echo_args;
	Dictionary call_payload;
	call_payload["jsonrpc"] = "2.0";
	call_payload["id"] = 3;
	call_payload["method"] = "tools/call";
	call_payload["params"] = call_params;
	const Dictionary call_rpc = JustAMCPJsonRpcTransport::handle_json_rpc_parsed(&server, call_payload, Ref<HTTPResponse>());
	CHECK(call_rpc.has("result"));
	CHECK(!call_rpc.has("error"));
	Array rpc_content = Dictionary(call_rpc.get("result", Dictionary())).get("content", Array());
	CHECK(String(Dictionary(rpc_content[0]).get("text", "")) == "ping");

	Dictionary bad_call_params;
	bad_call_params["name"] = "missing_tool";
	Dictionary bad_call;
	bad_call["jsonrpc"] = "2.0";
	bad_call["id"] = 4;
	bad_call["method"] = "tools/call";
	bad_call["params"] = bad_call_params;
	const Dictionary unknown_tool = JustAMCPJsonRpcTransport::handle_json_rpc_parsed(&server, bad_call, Ref<HTTPResponse>());
	CHECK(unknown_tool.has("error"));

	Dictionary missing_name;
	missing_name["jsonrpc"] = "2.0";
	missing_name["id"] = 5;
	missing_name["method"] = "tools/call";
	missing_name["params"] = Dictionary();
	const Dictionary invalid_call = JustAMCPJsonRpcTransport::handle_json_rpc_parsed(&server, missing_name, Ref<HTTPResponse>());
	CHECK(invalid_call.has("error"));

	Dictionary get_params;
	get_params["name"] = "proj_hello";
	get_params["arguments"] = hello_args;
	Dictionary get_payload;
	get_payload["jsonrpc"] = "2.0";
	get_payload["id"] = 6;
	get_payload["method"] = "prompts/get";
	get_payload["params"] = get_params;
	const Dictionary get_rpc = JustAMCPJsonRpcTransport::handle_json_rpc_parsed(&server, get_payload, Ref<HTTPResponse>());
	CHECK(get_rpc.has("result"));
	Array get_messages = Dictionary(get_rpc.get("result", Dictionary())).get("messages", Array());
	Array get_content = Dictionary(get_messages[0]).get("content", Array());
	CHECK(String(Dictionary(get_content[0]).get("text", "")) == "hello cursor");

	Dictionary unknown_prompt_params;
	unknown_prompt_params["name"] = "missing_prompt";
	Dictionary unknown_prompt;
	unknown_prompt["jsonrpc"] = "2.0";
	unknown_prompt["id"] = 7;
	unknown_prompt["method"] = "prompts/get";
	unknown_prompt["params"] = unknown_prompt_params;
	CHECK(JustAMCPJsonRpcTransport::handle_json_rpc_parsed(&server, unknown_prompt, Ref<HTTPResponse>()).has("error"));
#endif

	JustAMCPProjectRegistry::clear();
	memdelete(host);
}

void test_justamcp_invalid_ports_are_rejected() {
	CHECK(JustAMCPCliArgs::is_valid_port(6506));
	CHECK(JustAMCPCliArgs::is_valid_port(1));
	CHECK(JustAMCPCliArgs::is_valid_port(65535));
	CHECK(!JustAMCPCliArgs::is_valid_port(0));
	CHECK(!JustAMCPCliArgs::is_valid_port(65536));
	CHECK(!JustAMCPCliArgs::is_valid_port(-1));

	ProjectSettings *ps = ProjectSettings::get_singleton();
	CHECK(ps);
	const int prev_port = int(ps->get_setting("blazium/justamcp/server_port", 6506));
	const int prev_export = int(ps->get_setting("blazium/justamcp/export_port", 0));
	const bool prev_override = bool(ps->get_setting("blazium/justamcp/override_editor_settings", false));

	ps->set_setting("blazium/justamcp/override_editor_settings", true);
	ps->set_setting("blazium/justamcp/server_port", 70000);
	ps->set_setting("blazium/justamcp/export_port", 70001);
	JustAMCPCliArgs::clear_test_overrides();
	CHECK(JustAMCPSettingsResolver::resolve_server_port() == 6506);
	CHECK(JustAMCPSettingsResolver::resolve_runtime_port() == 6507);

	JustAMCPCliArgs::set_test_mcp_port(70000);
	CHECK(JustAMCPCliArgs::mcp_port() == -1);
	JustAMCPCliArgs::set_test_mcp_game_port(0);
	CHECK(JustAMCPCliArgs::mcp_game_port() == -1);
	JustAMCPCliArgs::clear_test_overrides();

	ps->set_setting("blazium/justamcp/server_port", prev_port);
	ps->set_setting("blazium/justamcp/export_port", prev_export);
	ps->set_setting("blazium/justamcp/override_editor_settings", prev_override);
}

#endif
