/**************************************************************************/
/*  justamcp_server.cpp                                                   */
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

#include "justamcp_server.h"
#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "editor/editor_settings.h"
#include "modules/modules_enabled.gen.h"
#include "servers/display_server.h"
#include "tools/justamcp_prompt_executor.h"
#include "tools/justamcp_resource_executor.h"
#include "tools/justamcp_task_manager.h"
#include "tools/justamcp_tool_executor.h"

static bool _is_headless() {
	if (DisplayServer::get_singleton() != nullptr) {
		return DisplayServer::get_singleton()->get_name() == "headless";
	}
	if (OS::get_singleton() && OS::get_singleton()->get_cmdline_args().find("--headless")) {
		return true;
	}
	return false;
}

void JustAMCPServer::_bind_methods() {
	ADD_SIGNAL(MethodInfo("tool_requested", PropertyInfo(Variant::NIL, "request_id", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT), PropertyInfo(Variant::STRING, "tool_name"), PropertyInfo(Variant::DICTIONARY, "args")));
	ADD_SIGNAL(MethodInfo("server_status_changed", PropertyInfo(Variant::BOOL, "started")));
	ADD_SIGNAL(MethodInfo("elicitation_completed", PropertyInfo(Variant::NIL, "request_id", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT), PropertyInfo(Variant::DICTIONARY, "result")));
	ADD_SIGNAL(MethodInfo("request_cancelled", PropertyInfo(Variant::NIL, "request_id", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT), PropertyInfo(Variant::STRING, "reason")));
}

JustAMCPServer *JustAMCPServer::singleton = nullptr;

JustAMCPServer *JustAMCPServer::get_singleton() {
	return singleton;
}

void JustAMCPServer::_print_handler_callback(void *p_user_data, const String &p_string, bool p_error, bool p_rich) {
	JustAMCPServer *server = static_cast<JustAMCPServer *>(p_user_data);
	if (!server) {
		return;
	}
	MutexLock lock(server->engine_logs_mutex);

	String prefix = p_error ? "[ERROR] " : "";
	server->engine_logs.push_back(prefix + p_string);

	if (server->engine_logs.size() > 500) {
		server->engine_logs.remove_at(0);
	}
}

Vector<String> JustAMCPServer::get_engine_logs() {
	MutexLock lock(engine_logs_mutex);
	return engine_logs;
}

JustAMCPServer::JustAMCPServer() {
	singleton = this;
	print_handler.printfunc = _print_handler_callback;
	print_handler.userdata = this;
	add_print_handler(&print_handler);

#ifdef TOOLS_ENABLED
	prompt_executor = memnew(JustAMCPPromptExecutor);
	resource_executor = memnew(JustAMCPResourceExecutor);
	task_manager = memnew(JustAMCPTaskManager);
#endif
}

JustAMCPServer::~JustAMCPServer() {
	remove_print_handler(&print_handler);
	if (singleton == this) {
		singleton = nullptr;
	}

	_stop_server();
#ifdef TOOLS_ENABLED
	if (prompt_executor) {
		memdelete(prompt_executor);
	}
	if (resource_executor) {
		memdelete(resource_executor);
	}
	if (task_manager) {
		memdelete(task_manager);
	}
#endif
}

void JustAMCPServer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			_setup_settings();
#ifdef TOOLS_ENABLED
			if (EditorSettings::get_singleton()) {
				EditorSettings::get_singleton()->connect("settings_changed", callable_mp(this, &JustAMCPServer::_on_settings_changed));
			}
#endif
			if (ProjectSettings::get_singleton()) {
				ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &JustAMCPServer::_on_settings_changed));
			}
			_start_server();
		} break;
		case NOTIFICATION_EXIT_TREE: {
			_stop_server();
		} break;
	}
}

void JustAMCPServer::_on_settings_changed() {
#ifdef TOOLS_ENABLED
	bool is_enabled = false;
	bool use_project_override = GLOBAL_GET("blazium/justamcp/override_editor_settings");

	if (_is_headless()) {
		use_project_override = true;
	}

	if (use_project_override || !EditorSettings::get_singleton()) {
		is_enabled = GLOBAL_GET("blazium/justamcp/server_enabled");
	} else if (EditorSettings::get_singleton()->has_setting("blazium/justamcp/server_enabled")) {
		is_enabled = EditorSettings::get_singleton()->get_setting("blazium/justamcp/server_enabled");
	}

	// If it's disabled but we are running, stop it
	if (!is_enabled && server_started) {
		_stop_server();
	}
	// If it's enabled but NOT running, start it
	else if (is_enabled && !server_started) {
		_start_server();
	}
	// If it IS enabled and IS running, but the port changed, we might need a restart.
	// We'll leave that to manual restarts for now to prevent spamming server stops.
#endif
}

void JustAMCPServer::_setup_settings() {
#ifdef TOOLS_ENABLED
	if (EditorSettings::get_singleton()) {
		EDITOR_DEF_BASIC("blazium/justamcp/server_enabled", false);
		EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/justamcp/server_enabled"));

		EDITOR_DEF_BASIC("blazium/justamcp/server_port", 6506);
		EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/justamcp/server_port"));

		EDITOR_DEF_BASIC("blazium/justamcp/oauth_enabled", false);
		EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/justamcp/oauth_enabled"));

		EDITOR_DEF_BASIC("blazium/justamcp/client_id", "");
		EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/justamcp/client_id"));

		EDITOR_DEF_BASIC("blazium/justamcp/client_secret", "");
		EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/justamcp/client_secret"));

		EDITOR_DEF_BASIC("blazium/justamcp/z_mcp_config", "");
		EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/justamcp/z_mcp_config"));

		EDITOR_DEF_BASIC("blazium/justamcp/enable_debug_logging", true);
		EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/justamcp/enable_debug_logging"));

		EDITOR_DEF_BASIC("blazium/justamcp/bind_to_localhost_only", true);
		EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/justamcp/bind_to_localhost_only"));
	}
#endif

	// Ensure ProjectSettings equivalents exist for override
	GLOBAL_DEF_BASIC("blazium/justamcp/override_editor_settings", false);
	GLOBAL_DEF_BASIC("blazium/justamcp/server_enabled", false);
	GLOBAL_DEF_BASIC("blazium/justamcp/server_port", 6506);
	GLOBAL_DEF_BASIC("blazium/justamcp/oauth_enabled", false);
	GLOBAL_DEF_BASIC("blazium/justamcp/client_id", String());
	GLOBAL_DEF_BASIC("blazium/justamcp/client_secret", String());
	GLOBAL_DEF_BASIC("blazium/justamcp/z_mcp_config", String());
	GLOBAL_DEF_BASIC("blazium/justamcp/enable_debug_logging", true);
	GLOBAL_DEF_BASIC("blazium/justamcp/bind_to_localhost_only", true);

#ifdef TOOLS_ENABLED
	JustAMCPToolExecutor::register_tool_settings();
	JustAMCPPromptExecutor::register_settings();
	JustAMCPResourceExecutor::register_settings();
#endif
}

void JustAMCPServer::_start_server() {
	if (server_started) {
		return;
	}

	const List<String> &args = OS::get_singleton()->get_cmdline_args();
	for (const String &arg : args) {
		if (arg == "--test" || arg == "--tests" || arg.begins_with("--aw-") ||
				arg == "--help" || arg == "-h" || arg == "/?" || arg == "--version" ||
				arg == "--check-only" || arg.begins_with("--export")) {
			return; // Do not start Server during CLI tools or testing workflows
		}
	}

	bool enabled = false;
	int port = 6506;
	bool bind_to_localhost = true;

#ifdef TOOLS_ENABLED
	bool use_project_override = GLOBAL_GET("blazium/justamcp/override_editor_settings");

	if (_is_headless()) {
		use_project_override = true;
	}

	if (use_project_override || !EditorSettings::get_singleton()) {
		enabled = GLOBAL_GET("blazium/justamcp/server_enabled");
		port = GLOBAL_GET("blazium/justamcp/server_port");
		if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/justamcp/bind_to_localhost_only")) {
			bind_to_localhost = GLOBAL_GET("blazium/justamcp/bind_to_localhost_only");
		}
	} else {
		if (EditorSettings::get_singleton()->has_setting("blazium/justamcp/server_enabled")) {
			enabled = EditorSettings::get_singleton()->get_setting("blazium/justamcp/server_enabled");
		}
		if (EditorSettings::get_singleton()->has_setting("blazium/justamcp/server_port")) {
			port = EditorSettings::get_singleton()->get_setting("blazium/justamcp/server_port");
		}
		if (EditorSettings::get_singleton()->has_setting("blazium/justamcp/bind_to_localhost_only")) {
			bind_to_localhost = EditorSettings::get_singleton()->get_setting("blazium/justamcp/bind_to_localhost_only");
		}
	}
#endif

	bool cmd_enable_mcp = false;
	int cmd_port = -1;
	String cmd_client_id = "";
	String cmd_client_secret = "";

	for (const List<String>::Element *E = args.front(); E; E = E->next()) {
		if (E->get() == "--enable-mcp") {
			cmd_enable_mcp = true;
		}
		if (E->get() == "--mcp-port" && E->next()) {
			cmd_port = E->next()->get().to_int();
		}
		if (E->get() == "--mcp-client-id" && E->next()) {
			cmd_client_id = E->next()->get();
		}
		if (E->get() == "--mcp-client-secret" && E->next()) {
			cmd_client_secret = E->next()->get();
		}
	}

	if (!cmd_client_id.is_empty() && cmd_client_secret.is_empty()) {
		ERR_PRINT("JustAMCP: --mcp-client-id provided but --mcp-client-secret is missing. OAuth configuration failed.");
		return;
	}
	if (!cmd_client_secret.is_empty() && cmd_client_id.is_empty()) {
		ERR_PRINT("JustAMCP: --mcp-client-secret provided but --mcp-client-id is missing. OAuth configuration failed.");
		return;
	}

	if (cmd_enable_mcp) {
		enabled = true;
	}
	if (cmd_port > 0) {
		port = cmd_port;
	}
	if (!cmd_client_id.is_empty() && !cmd_client_secret.is_empty()) {
		if (ProjectSettings::get_singleton()) {
			ProjectSettings::get_singleton()->set_setting("blazium/justamcp/oauth_enabled", true);
			ProjectSettings::get_singleton()->set_setting("blazium/justamcp/client_id", cmd_client_id);
			ProjectSettings::get_singleton()->set_setting("blazium/justamcp/client_secret", cmd_client_secret);
		}
#ifdef TOOLS_ENABLED
		if (EditorSettings::get_singleton()) {
			EditorSettings::get_singleton()->set_setting("blazium/justamcp/oauth_enabled", true);
			EditorSettings::get_singleton()->set_setting("blazium/justamcp/client_id", cmd_client_id);
			EditorSettings::get_singleton()->set_setting("blazium/justamcp/client_secret", cmd_client_secret);
		}
#endif
	}

	if (!enabled) {
		return;
	}

#if defined(MODULE_HTTPSERVER_ENABLED)
	if (HTTPServer::get_singleton()) {
		if (!HTTPServer::get_singleton()->is_listening()) {
			String bind_address = bind_to_localhost ? "127.0.0.1" : "*";
			HTTPServer::get_singleton()->listen(port, bind_address, false);
		}

		HTTPServer::get_singleton()->register_route("GET", "/sse", callable_mp(this, &JustAMCPServer::_handle_sse_connect));
		HTTPServer::get_singleton()->register_route("POST", "/sse", callable_mp(this, &JustAMCPServer::_handle_sse_connect));
		HTTPServer::get_singleton()->register_route("OPTIONS", "/sse", callable_mp(this, &JustAMCPServer::_handle_cors_preflight));
		HTTPServer::get_singleton()->register_route("POST", "/message", callable_mp(this, &JustAMCPServer::_handle_message_post));
		HTTPServer::get_singleton()->register_route("OPTIONS", "/message", callable_mp(this, &JustAMCPServer::_handle_cors_preflight));
		HTTPServer::get_singleton()->register_route("GET", "/mcp", callable_mp(this, &JustAMCPServer::_handle_sse_connect));
		HTTPServer::get_singleton()->register_route("POST", "/mcp", callable_mp(this, &JustAMCPServer::_handle_mcp_stateless_post));
		HTTPServer::get_singleton()->register_route("OPTIONS", "/mcp", callable_mp(this, &JustAMCPServer::_handle_cors_preflight));

		if (!HTTPServer::get_singleton()->is_connected("sse_connection_opened", callable_mp(this, &JustAMCPServer::_on_sse_connection_opened))) {
			HTTPServer::get_singleton()->connect("sse_connection_opened", callable_mp(this, &JustAMCPServer::_on_sse_connection_opened));
		}

		server_started = true;
		print_line("JustAMCP: Server Activated on port " + itos(port));
		emit_signal("server_status_changed", true);
	}
#else
	ERR_PRINT("HTTPServer module is not enabled! JustAMCP requires it to act as an MCP server.");
#endif
}

void JustAMCPServer::_stop_server() {
	if (!server_started) {
		return;
	}

#if defined(MODULE_HTTPSERVER_ENABLED)
	if (HTTPServer::get_singleton()) {
		HTTPServer::get_singleton()->unregister_route("GET", "/sse");
		HTTPServer::get_singleton()->unregister_route("POST", "/sse");
		HTTPServer::get_singleton()->unregister_route("OPTIONS", "/sse");
		HTTPServer::get_singleton()->unregister_route("POST", "/message");
		HTTPServer::get_singleton()->unregister_route("OPTIONS", "/message");
		HTTPServer::get_singleton()->unregister_route("GET", "/mcp");
		HTTPServer::get_singleton()->unregister_route("POST", "/mcp");
		HTTPServer::get_singleton()->unregister_route("OPTIONS", "/mcp");
	}
#endif
	server_started = false;
	print_line("JustAMCP: Server Disabled.");
	emit_signal("server_status_changed", false);
	current_sse_connection_id = -1;
}

#if defined(MODULE_HTTPSERVER_ENABLED)
void JustAMCPServer::_handle_cors_preflight(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	p_response->set_status(204);
	p_response->set_body("");
}

void JustAMCPServer::_handle_sse_connect(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	bool debug_logging = GLOBAL_GET("blazium/justamcp/enable_debug_logging");
	if (debug_logging) {
		print_line("JustAMCP: Incoming " + p_context->get_method() + " /sse connection attempt...");
	}
	// Optional OAuth validation
#ifdef TOOLS_ENABLED
	bool oauth_enabled = false;
	String required_client_id = "";
	String required_client_secret = "";

	bool use_project_override = GLOBAL_GET("blazium/justamcp/override_editor_settings");

	if (_is_headless()) {
		use_project_override = true;
	}

	if (use_project_override || !EditorSettings::get_singleton()) {
		oauth_enabled = GLOBAL_GET("blazium/justamcp/oauth_enabled");
		required_client_id = String(GLOBAL_GET("blazium/justamcp/client_id"));
		required_client_secret = String(GLOBAL_GET("blazium/justamcp/client_secret"));
	} else if (EditorSettings::get_singleton()) {
		if (EditorSettings::get_singleton()->has_setting("blazium/justamcp/oauth_enabled")) {
			oauth_enabled = EditorSettings::get_singleton()->get_setting("blazium/justamcp/oauth_enabled");
		}
		if (EditorSettings::get_singleton()->has_setting("blazium/justamcp/client_id")) {
			required_client_id = String(EditorSettings::get_singleton()->get_setting("blazium/justamcp/client_id"));
		}
		if (EditorSettings::get_singleton()->has_setting("blazium/justamcp/client_secret")) {
			required_client_secret = String(EditorSettings::get_singleton()->get_setting("blazium/justamcp/client_secret"));
		}
	}

	if (oauth_enabled) {
		if (!required_client_id.is_empty() || !required_client_secret.is_empty()) {
			Dictionary headers = p_context->get_headers();
			String authorization = headers.get("authorization", headers.get("Authorization", ""));
			String client_id_header = headers.get("x-client-id", headers.get("X-Client-Id", ""));
			String client_secret_header = headers.get("x-client-secret", headers.get("X-Client-Secret", ""));
			String bearer_prefix = "Bearer ";
			String basic_prefix = "Basic ";
			String expected_pair = required_client_id + ":" + required_client_secret;
			bool authorized = false;

			if (!required_client_secret.is_empty()) {
				authorized = authorization == bearer_prefix + required_client_secret ||
						authorization == bearer_prefix + expected_pair ||
						authorization == basic_prefix + expected_pair ||
						client_secret_header == required_client_secret;
			}
			if (authorized && !required_client_id.is_empty()) {
				authorized = client_id_header == required_client_id || authorization.ends_with(expected_pair);
			}
			if (!authorized && required_client_secret.is_empty() && !required_client_id.is_empty()) {
				authorized = authorization == bearer_prefix + required_client_id || client_id_header == required_client_id;
			}

			if (!authorized) {
				ERR_PRINT("JustAMCP: Unauthorized connection attempt.");
				p_response->set_status(401);
				p_response->set_body("Unauthorized - Invalid OAuth credentials");
				return;
			}
		}
	}
#endif

	p_response->start_sse();
}

void JustAMCPServer::_on_sse_connection_opened(int p_connection_id, const String &p_path, const Dictionary &p_headers) {
	if (p_path == "/sse") {
		bool debug_logging = GLOBAL_GET("blazium/justamcp/enable_debug_logging");
		if (debug_logging) {
			print_line("JustAMCP: New SSE connection opened on " + p_path + " (ID: " + itos(p_connection_id) + ")");
		}
		current_sse_connection_id = p_connection_id;

		int port = 6506;
		bool use_project_override = GLOBAL_GET("blazium/justamcp/override_editor_settings");

		if (_is_headless()) {
			use_project_override = true;
		}

		if (use_project_override || !EditorSettings::get_singleton()) {
			port = GLOBAL_GET("blazium/justamcp/server_port");
		} else if (EditorSettings::get_singleton() && EditorSettings::get_singleton()->has_setting("blazium/justamcp/server_port")) {
			port = EditorSettings::get_singleton()->get_setting("blazium/justamcp/server_port");
		}

		String endpoint_url = "http://127.0.0.1:" + itos(port) + "/message?sessionId=" + itos(p_connection_id);
		HTTPServer::get_singleton()->send_sse_event(p_connection_id, "endpoint", endpoint_url);
	}
}

void JustAMCPServer::_handle_message_post(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	String body = p_context->get_body();
	if (body.is_empty()) {
		ERR_PRINT("JustAMCP: Received empty message POST body.");
		p_response->set_status(400);
		p_response->set_body("Empty request body");
		return;
	}

	bool debug_logging = GLOBAL_GET("blazium/justamcp/enable_debug_logging");
	if (debug_logging) {
		print_line("JustAMCP: Message POST Payload Headers:");
		Array keys = p_context->get_headers().keys();
		for (int i = 0; i < keys.size(); i++) {
			print_line("  - " + String(keys[i]) + ": " + String(p_context->get_headers()[keys[i]]));
		}
		print_line("JustAMCP: Message POST Received Body: " + body);
	}

	// We reply HTTP 202 explicitly to acknowledge receipt of the POST buffer natively for MCP.
	p_response->set_status(202);
	p_response->set_body("Accepted");

	// Then handle it via JSON-RPC, emitting the event down our active SSE pipeline!
	Dictionary result = _handle_json_rpc(body, p_response);
	if (!result.is_empty()) {
		_send_sse_message(JSON::stringify(result));
	}
}

void JustAMCPServer::_handle_mcp_stateless_post(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	String body = p_context->get_body();
	if (body.is_empty()) {
		ERR_PRINT("JustAMCP: Received empty message POST body on /mcp.");
		p_response->set_status(400);
		Dictionary err;
		err["jsonrpc"] = "2.0";
		Dictionary error_dict;
		error_dict["code"] = -32700;
		error_dict["message"] = "Invalid JSON or empty body";
		err["error"] = error_dict;
		p_response->set_json(err);
		return;
	}

	bool debug_logging = GLOBAL_GET("blazium/justamcp/enable_debug_logging");
	if (debug_logging) {
		print_line("JustAMCP: Stateless POST Headers:");
		Array keys = p_context->get_headers().keys();
		for (int i = 0; i < keys.size(); i++) {
			print_line("  - " + String(keys[i]) + ": " + String(p_context->get_headers()[keys[i]]));
		}
		print_line("JustAMCP: Stateless POST Received Body: " + body);
	}

	Dictionary result = _handle_json_rpc(body, p_response);
	if (!p_response->is_sent()) {
		if (result.is_empty()) {
			if (debug_logging) {
				print_line("JustAMCP: Stateless POST Replying HTTP 202 (Empty result object)");
			}
			p_response->set_status(202);
			p_response->set_body("");
		} else {
			if (debug_logging) {
				print_line("JustAMCP: Stateless POST Replying HTTP 200 with Result JSON payload: " + JSON::stringify(result));
			}
			p_response->set_status(200);
			p_response->set_json(result);
		}
	}
}

Dictionary JustAMCPServer::_handle_json_rpc(const String &p_body, Ref<HTTPResponse> p_response) {
	Ref<JSON> json;
	json.instantiate();

	if (json->parse(p_body) != OK) {
		ERR_PRINT("JustAMCP: Failed to parse MCP JSON-RPC Payload: " + p_body);
		Dictionary err;
		err["jsonrpc"] = "2.0";
		Dictionary error_dict;
		error_dict["code"] = -32700;
		error_dict["message"] = "Invalid JSON";
		err["error"] = error_dict;
		return err;
	}

	Dictionary payload = json->get_data();
	if (!payload.has("method")) {
		return Dictionary(); // Ignore non-requests
	}

	String method = payload["method"];
	String request_id = payload.has("id") ? String(Variant(payload["id"])) : "";

	Variant req_id_var;
	if (payload.has("id")) {
		req_id_var = payload["id"];
		if (req_id_var.get_type() == Variant::FLOAT) {
			double d = req_id_var;
			if (Math::is_equal_approx(d, Math::round(d))) {
				req_id_var = (int64_t)Math::round(d);
			}
		}
	}

	bool debug_logging = GLOBAL_GET("blazium/justamcp/enable_debug_logging");
	if (debug_logging) {
		print_line("JustAMCP: Executing JSON-RPC Method: " + method + " (ID: " + request_id + ")");
	}

	auto invalid_params = [&](const String &msg) -> Dictionary {
		if (debug_logging) {
			print_line("JustAMCP: Payload Validation Failed for " + method + ": " + msg);
		}
		Dictionary err;
		err["jsonrpc"] = "2.0";
		err["id"] = req_id_var;
		Dictionary error_dict;
		error_dict["code"] = -32602;
		error_dict["message"] = msg;
		err["error"] = error_dict;
		return err;
	};

	if (method == "notifications/initialized") {
		return Dictionary(); // Ignore safely
	}

	if (method == "initialize") {
		if (!payload.has("params") || payload["params"].get_type() != Variant::DICTIONARY) {
			return invalid_params("initialize requires 'params' object.");
		}
		Dictionary params = payload["params"];
		if (!params.has("protocolVersion") || params["protocolVersion"].get_type() != Variant::STRING) {
			return invalid_params("initialize requires 'protocolVersion' string.");
		}
		if (!params.has("capabilities") || params["capabilities"].get_type() != Variant::DICTIONARY) {
			return invalid_params("initialize requires 'capabilities' object.");
		}
		if (!params.has("clientInfo") || params["clientInfo"].get_type() != Variant::DICTIONARY) {
			return invalid_params("initialize requires 'clientInfo' object.");
		}

		Dictionary result;
		result["protocolVersion"] = "2024-11-05";

		Dictionary capabilities;
		Dictionary tools_cap;
		tools_cap["listChanged"] = true;
		capabilities["tools"] = tools_cap;

		Dictionary prompts_cap;
		prompts_cap["listChanged"] = true;
		capabilities["prompts"] = prompts_cap;

		Dictionary resources_cap;
		resources_cap["listChanged"] = true;
		resources_cap["subscribe"] = true;
		capabilities["resources"] = resources_cap;

		capabilities["logging"] = Dictionary();

		Dictionary tasks_cap;
		tasks_cap["list"] = Dictionary();
		tasks_cap["cancel"] = Dictionary();
		Dictionary requests;
		Dictionary tools;
		tools["call"] = Dictionary();
		requests["tools"] = tools;
		tasks_cap["requests"] = requests;
		capabilities["tasks"] = tasks_cap;

		result["capabilities"] = capabilities;

		Dictionary serverInfo;
		serverInfo["name"] = "blazium-mcp-server";
		serverInfo["version"] = "1.0.0";
		serverInfo["instructions"] = "Use blazium_* tools and blazium:// resources. Prefer editor tools for scene/resource edits, runtime_* tools only when a game bridge is active, and guide resources such as blazium://guide/tool-index for workflow orientation.";
		result["serverInfo"] = serverInfo;

		Dictionary rpc_result;
		rpc_result["jsonrpc"] = "2.0";
		rpc_result["id"] = req_id_var;
		rpc_result["result"] = result;

		return rpc_result;
	}

	if (method == "ping") {
		Dictionary result;

		Dictionary rpc_result;
		rpc_result["jsonrpc"] = "2.0";
		rpc_result["id"] = req_id_var;
		rpc_result["result"] = result;

		return rpc_result;
	}

	if (method == "tools/list") {
		Dictionary result;
#ifdef TOOLS_ENABLED
		result["tools"] = JustAMCPToolExecutor::get_tool_schemas();
#else
		result["tools"] = Array();
#endif

		Dictionary rpc_result;
		rpc_result["jsonrpc"] = "2.0";
		rpc_result["id"] = req_id_var;
		rpc_result["result"] = result;

		return rpc_result;
	}

	if (method == "prompts/list") {
		Dictionary result;
#ifdef TOOLS_ENABLED
		String cursor = "";
		if (payload.has("params") && Dictionary(payload["params"]).has("cursor")) {
			cursor = String(Variant(Dictionary(payload["params"])["cursor"]));
		}
		if (prompt_executor) {
			result = prompt_executor->list_prompts(cursor);
		}
#else
		result["prompts"] = Array();
#endif
		Dictionary rpc_result;
		rpc_result["jsonrpc"] = "2.0";
		rpc_result["id"] = req_id_var;
		rpc_result["result"] = result;
		return rpc_result;
	}

	if (method == "prompts/get") {
		if (!payload.has("params") || payload["params"].get_type() != Variant::DICTIONARY) {
			return invalid_params("prompts/get requires 'params' object.");
		}
		Dictionary params = payload["params"];
		if (!params.has("name") || params["name"].get_type() != Variant::STRING) {
			return invalid_params("prompts/get requires 'name' string.");
		}
		String prompt_name = params["name"];
		Dictionary args = params.has("arguments") && params["arguments"].get_type() == Variant::DICTIONARY ? Dictionary(params["arguments"]) : Dictionary();
		Dictionary result;
#ifdef TOOLS_ENABLED
		if (prompt_executor) {
			result = prompt_executor->get_prompt(prompt_name, args);
		}
#endif
		Dictionary rpc_result;
		rpc_result["jsonrpc"] = "2.0";
		rpc_result["id"] = req_id_var;
		bool ok = result.get("ok", false);
		if (ok) {
			result.erase("ok");
			rpc_result["result"] = result;
		} else {
			Dictionary error_dict;
			error_dict["code"] = result.get("error_code", -32603);
			error_dict["message"] = result.get("error", "Failed to retrieve prompt");
			rpc_result["error"] = error_dict;
		}
		return rpc_result;
	}

	if (method == "resources/list") {
		Dictionary result;
#ifdef TOOLS_ENABLED
		String cursor = payload.has("params") && Dictionary(payload["params"]).has("cursor") ? String(Variant(Dictionary(payload["params"])["cursor"])) : "";
		if (resource_executor) {
			result = resource_executor->list_resources(cursor);
		}
#else
		result["resources"] = Array();
#endif
		Dictionary rpc_result;
		rpc_result["jsonrpc"] = "2.0";
		rpc_result["id"] = req_id_var;
		rpc_result["result"] = result;
		return rpc_result;
	}

	if (method == "resources/templates/list") {
		Dictionary result;
#ifdef TOOLS_ENABLED
		String cursor = payload.has("params") && Dictionary(payload["params"]).has("cursor") ? String(Variant(Dictionary(payload["params"])["cursor"])) : "";
		if (resource_executor) {
			result = resource_executor->list_resource_templates(cursor);
		}
#else
		result["resourceTemplates"] = Array();
#endif
		Dictionary rpc_result;
		rpc_result["jsonrpc"] = "2.0";
		rpc_result["id"] = req_id_var;
		rpc_result["result"] = result;
		return rpc_result;
	}

	if (method == "resources/read") {
		if (!payload.has("params") || payload["params"].get_type() != Variant::DICTIONARY) {
			return invalid_params("resources/read requires 'params' object.");
		}
		Dictionary params = payload["params"];
		if (!params.has("uri") || params["uri"].get_type() != Variant::STRING) {
			return invalid_params("resources/read requires 'uri' string.");
		}
		String uri = params["uri"];
		Dictionary result;
#ifdef TOOLS_ENABLED
		if (resource_executor) {
			result = resource_executor->read_resource(uri);
		}
#endif
		Dictionary rpc_result;
		rpc_result["jsonrpc"] = "2.0";
		rpc_result["id"] = req_id_var;
		bool ok = result.get("ok", false);
		if (ok) {
			result.erase("ok");
			rpc_result["result"] = result;
		} else {
			Dictionary error_dict;
			error_dict["code"] = result.get("error_code", -32603);
			error_dict["message"] = result.get("error", "Failed to retrieve resource " + uri);
			rpc_result["error"] = error_dict;
		}
		return rpc_result;
	}

	if (method == "resources/subscribe" || method == "resources/unsubscribe") {
		Dictionary rpc_result;
		rpc_result["jsonrpc"] = "2.0";
		rpc_result["id"] = req_id_var;
		rpc_result["result"] = Dictionary(); // Ack subscription
		return rpc_result;
	}

	if (method == "notifications/cancelled") {
		Dictionary params = payload.has("params") ? Dictionary(payload["params"]) : Dictionary();
		Variant req_id;
		if (params.has("requestId")) {
			req_id = params["requestId"];
			if (req_id.get_type() == Variant::FLOAT) {
				double d = req_id;
				if (Math::is_equal_approx(d, Math::round(d))) {
					req_id = (int64_t)Math::round(d);
				}
			}
		}
		String reason = params.has("reason") ? String(Variant(params["reason"])) : "";
		call_deferred(SNAME("emit_signal"), "request_cancelled", req_id, reason);
		return Dictionary();
	}

	if (method == "tasks/list") {
		Dictionary result;
#ifdef TOOLS_ENABLED
		String cursor = payload.has("params") && Dictionary(payload["params"]).has("cursor") ? String(Variant(Dictionary(payload["params"])["cursor"])) : "";
		if (task_manager) {
			result = task_manager->list_tasks(cursor);
		}
#else
		result["tasks"] = Array();
#endif
		Dictionary rpc_result;
		rpc_result["jsonrpc"] = "2.0";
		rpc_result["id"] = req_id_var;
		rpc_result["result"] = result;
		return rpc_result;
	}

	if (method == "tasks/get") {
		if (!payload.has("params") || payload["params"].get_type() != Variant::DICTIONARY) {
			return invalid_params("tasks/get requires 'params' object.");
		}
		Dictionary params = payload["params"];
		if (!params.has("taskId") || params["taskId"].get_type() != Variant::STRING) {
			return invalid_params("tasks/get requires 'taskId' string.");
		}
		String task_id = params["taskId"];
		Dictionary result;
#ifdef TOOLS_ENABLED
		if (task_manager) {
			result = task_manager->get_task(task_id);
		}
#endif
		Dictionary rpc_result;
		rpc_result["jsonrpc"] = "2.0";
		rpc_result["id"] = req_id_var;
		if (result.get("ok", false)) {
			result.erase("ok");
			rpc_result["result"] = result;
		} else {
			Dictionary error_dict;
			error_dict["code"] = result.get("error_code", -32603);
			error_dict["message"] = result.get("error", "Task error.");
			rpc_result["error"] = error_dict;
		}
		return rpc_result;
	}

	if (method == "tasks/result") {
		if (!payload.has("params") || payload["params"].get_type() != Variant::DICTIONARY) {
			return invalid_params("tasks/result requires 'params' object.");
		}
		Dictionary params = payload["params"];
		if (!params.has("taskId") || params["taskId"].get_type() != Variant::STRING) {
			return invalid_params("tasks/result requires 'taskId' string.");
		}
		String task_id = params["taskId"];
		Dictionary result;
#ifdef TOOLS_ENABLED
		if (task_manager) {
			result = task_manager->get_task_result(task_id);
		}
#endif
		Dictionary rpc_result;
		rpc_result["jsonrpc"] = "2.0";
		rpc_result["id"] = req_id_var;
		if (result.get("ok", false)) {
			result.erase("ok");
			rpc_result["result"] = result;
		} else {
			Dictionary error_dict;
			error_dict["code"] = result.get("error_code", -32603);
			error_dict["message"] = result.get("error", "Task error.");
			rpc_result["error"] = error_dict;
		}
		return rpc_result;
	}

	if (method == "tasks/cancel") {
		if (!payload.has("params") || payload["params"].get_type() != Variant::DICTIONARY) {
			return invalid_params("tasks/cancel requires 'params' object.");
		}
		Dictionary params = payload["params"];
		if (!params.has("taskId") || params["taskId"].get_type() != Variant::STRING) {
			return invalid_params("tasks/cancel requires 'taskId' string.");
		}
		String task_id = params["taskId"];
		Dictionary result;
#ifdef TOOLS_ENABLED
		if (task_manager) {
			result = task_manager->cancel_task(task_id);
		}
#endif
		Dictionary rpc_result;
		rpc_result["jsonrpc"] = "2.0";
		rpc_result["id"] = req_id_var;
		if (result.get("ok", false)) {
			result.erase("ok");
			rpc_result["result"] = result;
		} else {
			Dictionary error_dict;
			error_dict["code"] = result.get("error_code", -32603);
			error_dict["message"] = result.get("error", "Task error.");
			rpc_result["error"] = error_dict;
		}
		return rpc_result;
	}

	if (method == "notifications/elicitation/complete") {
		Dictionary params = payload.has("params") ? Dictionary(payload["params"]) : Dictionary();
		String req_id = params.has("requestId") ? String(Variant(params["requestId"])) : "";
		Dictionary elicitation_result = params.has("result") ? Dictionary(params["result"]) : Dictionary();
		call_deferred(SNAME("emit_signal"), "elicitation_completed", req_id, elicitation_result);
		return Dictionary();
	}

	if (method == "logging/setLevel") {
		if (!payload.has("params") || payload["params"].get_type() != Variant::DICTIONARY) {
			return invalid_params("logging/setLevel requires 'params' object.");
		}
		Dictionary params = payload["params"];
		if (!params.has("level") || params["level"].get_type() != Variant::STRING) {
			return invalid_params("logging/setLevel requires 'level' string.");
		}
		current_log_level = String(params["level"]);
		Dictionary rpc_result;
		rpc_result["jsonrpc"] = "2.0";
		rpc_result["id"] = req_id_var;
		rpc_result["result"] = Dictionary();
		return rpc_result;
	}

	if (method == "completion/complete") {
		Dictionary params = payload.has("params") ? Dictionary(payload["params"]) : Dictionary();
		Dictionary ref = params.has("ref") ? Dictionary(params["ref"]) : Dictionary();
		Dictionary argument = params.has("argument") ? Dictionary(params["argument"]) : Dictionary();
		Dictionary result;
#ifdef TOOLS_ENABLED
		if (prompt_executor) {
			result = prompt_executor->complete_prompt(ref, argument);
		}
#endif
		Dictionary rpc_result;
		rpc_result["jsonrpc"] = "2.0";
		rpc_result["id"] = req_id_var;
		rpc_result["result"] = result;
		return rpc_result;
	}

	if (method == "tools/call") {
		if (!payload.has("params") || payload["params"].get_type() != Variant::DICTIONARY) {
			return invalid_params("tools/call requires 'params' object.");
		}
		Dictionary params = payload["params"];
		if (!params.has("name") || params["name"].get_type() != Variant::STRING) {
			return invalid_params("tools/call requires 'name' string.");
		}
		String tool_name = params["name"];
		Dictionary args = params.has("arguments") && params["arguments"].get_type() == Variant::DICTIONARY ? Dictionary(params["arguments"]) : Dictionary();

		// Remember the response wrapper if it's statistically POST routed.
		// And execute across the emitter!
		pending_stateless_mutex.lock();
		pending_stateless_response = p_response;

		if (p_response.is_valid()) {
			pending_stateless_result.clear();
			pending_stateless_mutex.unlock();

			call_deferred(SNAME("emit_signal"), "tool_requested", req_id_var, tool_name, args);
			pending_stateless_semaphore.wait();

			pending_stateless_mutex.lock();
			Dictionary res = pending_stateless_result;
			pending_stateless_mutex.unlock();
			return res;
		} else {
			pending_stateless_mutex.unlock();
			call_deferred(SNAME("emit_signal"), "tool_requested", req_id_var, tool_name, args);
			return Dictionary();
		}
	}

	// Method not found fallback
	if (!payload.has("id")) {
		// JSON-RPC 2.0 Notification requires NO response.
		return Dictionary();
	}

	Dictionary err;
	err["jsonrpc"] = "2.0";
	err["id"] = req_id_var;
	Dictionary error_dict;
	error_dict["code"] = -32601;
	error_dict["message"] = "Method not found: " + method;
	err["error"] = error_dict;
	return err;
}

void JustAMCPServer::_send_sse_message(const String &p_json_string) {
	Array active_connections = HTTPServer::get_singleton()->get_active_sse_connections();
	for (int i = 0; i < active_connections.size(); i++) {
		int conn_id = active_connections[i];
		// 'message' is the default standard SSE event type for MCP Json-RPC streams
		HTTPServer::get_singleton()->send_sse_event(conn_id, "message", p_json_string);
	}
}

#endif // MODULE_HTTPSERVER_ENABLED

void JustAMCPServer::send_tool_result(const Variant &p_request_id, bool p_success, const Variant &p_result, const String &p_error) {
#if defined(MODULE_HTTPSERVER_ENABLED)
	Dictionary rpc_result;
	rpc_result["jsonrpc"] = "2.0";

	// request_id is correctly typed as a Variant (Integer or String)
	rpc_result["id"] = p_request_id;

	if (p_success) {
		Dictionary result;
		if (p_result.get_type() == Variant::DICTIONARY && Dictionary(p_result).has("content")) {
			// The tool returned strict MCP "content" natively!
			Variant content_val = Dictionary(p_result)["content"];
			if (content_val.get_type() == Variant::ARRAY) {
				result["content"] = content_val;
			} else if (content_val.get_type() == Variant::STRING) {
				Array content;
				Dictionary content_item;
				content_item["type"] = "text";
				content_item["text"] = content_val;
				content.push_back(content_item);
				result["content"] = content;
			} else {
				Array content;
				Dictionary content_item;
				content_item["type"] = "text";
				content_item["text"] = JSON::stringify(content_val);
				content.push_back(content_item);
				result["content"] = content;
			}
			result["isError"] = Dictionary(p_result).get("isError", false);
		} else {
			// The tool returned a custom dictionary (e.g. {"result": ...})
			// We format it as a single TextContent block.
			Array content;
			Dictionary content_item;
			content_item["type"] = "text";

			String text_val;
			if (p_result.get_type() == Variant::DICTIONARY && Dictionary(p_result).has("result") && Dictionary(p_result).size() == 1) {
				Variant inner_result = Dictionary(p_result)["result"];
				if (inner_result.get_type() == Variant::STRING) {
					text_val = inner_result;
				} else {
					text_val = JSON::stringify(inner_result);
				}
			} else if (p_result.get_type() == Variant::STRING) {
				text_val = p_result;
			} else {
				text_val = JSON::stringify(p_result);
			}

			content_item["text"] = text_val;
			content.push_back(content_item);

			result["content"] = content;
			result["isError"] = false;
		}

		rpc_result["result"] = result;
	} else {
		// Differentiate Protocol Errors dynamically
		Dictionary error_dict = p_result.get_type() == Variant::DICTIONARY ? Dictionary(p_result) : Dictionary();
		if (!error_dict.is_empty() && error_dict.has("code")) {
			// Pure JSON-RPC Protocol Error (-32xxx)
			Dictionary error;
			error["code"] = error_dict["code"];
			error["message"] = error_dict.get("message", p_error);
			rpc_result["error"] = error;
		} else {
			// Native Runtime Tool Execution error
			Dictionary result;
			Array content;
			Dictionary content_item;
			content_item["type"] = "text";
			content_item["text"] = p_error;
			content.push_back(content_item);

			result["content"] = content;
			result["isError"] = true;

			rpc_result["result"] = result;
		}
	}

	// Output to pending stateless HTTP paths
	pending_stateless_mutex.lock();
	if (pending_stateless_response.is_valid()) {
		pending_stateless_result = rpc_result;
		pending_stateless_response.unref(); // Clear reference before waking thread to prevent concurrent refcount destruction
		pending_stateless_semaphore.post();
	} else {
		// Output to active SSE paths only if not tracked by a stateless execution path,
		// as the stateless path will dispatch the SSE itself to avoid deadlocks.
		_send_sse_message(JSON::stringify(rpc_result));
	}
	pending_stateless_mutex.unlock();
#endif
}

void JustAMCPServer::send_elicitation_request(const String &p_request_id, const String &p_mode, const String &p_message, const Variant &p_url_or_schema) {
#if defined(MODULE_HTTPSERVER_ENABLED)
	Dictionary rpc_request;
	rpc_request["jsonrpc"] = "2.0";
	rpc_request["method"] = "elicitation/create";

	String elicitation_id = "elicitation_" + p_request_id;
	rpc_request["id"] = elicitation_id;

	Dictionary params;
	params["requestId"] = p_request_id;
	params["mode"] = p_mode;
	params["message"] = p_message;

	if (p_mode == "url") {
		params["url"] = p_url_or_schema;
	} else if (p_mode == "form") {
		params["schema"] = p_url_or_schema;
	}

	rpc_request["params"] = params;

	_send_sse_message(JSON::stringify(rpc_request));
#endif
}

void JustAMCPServer::send_url_elicitation_error(const String &p_request_id, const String &p_elicitation_id, const String &p_url, const String &p_message) {
#if defined(MODULE_HTTPSERVER_ENABLED)
	Dictionary rpc_result;
	rpc_result["jsonrpc"] = "2.0";
	if (p_request_id.is_valid_int()) {
		rpc_result["id"] = p_request_id.to_int();
	} else {
		rpc_result["id"] = p_request_id;
	}

	Dictionary error;
	error["code"] = -32042;
	error["message"] = p_message;

	Dictionary error_data;
	error_data["url"] = p_url;
	error_data["elicitationId"] = p_elicitation_id;
	error["data"] = error_data;

	rpc_result["error"] = error;

	pending_stateless_mutex.lock();
	if (pending_stateless_response.is_valid()) {
		if (!pending_stateless_response->is_sent()) {
			pending_stateless_response->set_status(200);
			pending_stateless_response->set_json(rpc_result);
		}
		pending_stateless_response.unref();
	} else {
		_send_sse_message(JSON::stringify(rpc_result));
	}
	pending_stateless_mutex.unlock();
#endif
}

void JustAMCPServer::broadcast_prompts_list_changed() {
#if defined(MODULE_HTTPSERVER_ENABLED)
	Dictionary notification;
	notification["jsonrpc"] = "2.0";
	notification["method"] = "notifications/prompts/list_changed";
	_send_sse_message(JSON::stringify(notification));
#endif
}

void JustAMCPServer::broadcast_tools_list_changed() {
#if defined(MODULE_HTTPSERVER_ENABLED)
	Dictionary notification;
	notification["jsonrpc"] = "2.0";
	notification["method"] = "notifications/tools/list_changed";
	_send_sse_message(JSON::stringify(notification));
#endif
}

void JustAMCPServer::send_log_message(const String &p_level, const String &p_logger, const Variant &p_data) {
#if defined(MODULE_HTTPSERVER_ENABLED)
	Dictionary notification;
	notification["jsonrpc"] = "2.0";
	notification["method"] = "notifications/message";

	Dictionary params;
	params["level"] = p_level;
	if (!p_logger.is_empty()) {
		params["logger"] = p_logger;
	}
	params["data"] = p_data;

	notification["params"] = params;
	_send_sse_message(JSON::stringify(notification));
#endif
}

void JustAMCPServer::broadcast_resources_list_changed() {
#if defined(MODULE_HTTPSERVER_ENABLED)
	Dictionary notification;
	notification["jsonrpc"] = "2.0";
	notification["method"] = "notifications/resources/list_changed";
	_send_sse_message(JSON::stringify(notification));
#endif
}

void JustAMCPServer::broadcast_resource_updated(const String &p_uri) {
#if defined(MODULE_HTTPSERVER_ENABLED)
	Dictionary notification;
	notification["jsonrpc"] = "2.0";
	notification["method"] = "notifications/resources/updated";
	Dictionary params;
	params["uri"] = p_uri;
	notification["params"] = params;
	_send_sse_message(JSON::stringify(notification));
#endif
}

void JustAMCPServer::send_progress_notification(const String &p_token, double p_progress, double p_total, const String &p_message) {
#if defined(MODULE_HTTPSERVER_ENABLED)
	Dictionary notification;
	notification["jsonrpc"] = "2.0";
	notification["method"] = "notifications/progress";
	Dictionary params;
	params["progressToken"] = p_token;
	params["progress"] = p_progress;
	if (p_total > 0.0) {
		params["total"] = p_total;
	}
	if (!p_message.is_empty()) {
		params["message"] = p_message;
	}
	notification["params"] = params;
	_send_sse_message(JSON::stringify(notification));
#endif
}

void JustAMCPServer::broadcast_task_status(const String &p_task_id) {
#if defined(MODULE_HTTPSERVER_ENABLED) && defined(TOOLS_ENABLED)
	if (task_manager) {
		Dictionary task_dict = task_manager->get_task(p_task_id);
		if (task_dict.get("ok", false)) {
			task_dict.erase("ok");
			Dictionary notification;
			notification["jsonrpc"] = "2.0";
			notification["method"] = "notifications/tasks/status";
			notification["params"] = task_dict;
			_send_sse_message(JSON::stringify(notification));
		}
	}
#endif
}
