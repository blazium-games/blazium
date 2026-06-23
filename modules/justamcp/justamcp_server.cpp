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
#include "core/os/time.h"
#include "editor/editor_settings.h"
#include "justamcp_log_levels.h"
#include "justamcp_pagination.h"
#include "justamcp_streamable_http.h"
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

static bool _justamcp_headless_project_server_requested() {
	if (!_is_headless()) {
		return false;
	}
	if (!ProjectSettings::get_singleton()) {
		return false;
	}
	return GLOBAL_GET("blazium/justamcp/server_enabled");
}

static String _justamcp_extract_list_cursor(const Dictionary &p_payload) {
	if (!p_payload.has("params") || p_payload["params"].get_type() != Variant::DICTIONARY) {
		return String();
	}
	const Dictionary params = p_payload["params"];
	if (!params.has("cursor")) {
		return String();
	}
	return String(params["cursor"]);
}

static Dictionary _justamcp_extract_request_meta(const Dictionary &p_params) {
	if (p_params.has("_meta") && p_params["_meta"].get_type() == Variant::DICTIONARY) {
		return p_params["_meta"];
	}
	return Dictionary();
}

static bool _justamcp_request_ids_equal(const Variant &a, const Variant &b) {
	if (a == b) {
		return true;
	}
	if (a.get_type() == Variant::FLOAT && b.get_type() == Variant::INT) {
		return int64_t(Math::round(double(a))) == int64_t(b);
	}
	if (a.get_type() == Variant::INT && b.get_type() == Variant::FLOAT) {
		return int64_t(a) == int64_t(Math::round(double(b)));
	}
	return String(a) == String(b);
}

static String _justamcp_progress_token_from_meta(const Dictionary &p_meta) {
	if (!p_meta.has("progressToken")) {
		return String();
	}
	const Variant token_var = p_meta["progressToken"];
	if (token_var.get_type() == Variant::STRING) {
		return token_var;
	}
	if (token_var.get_type() == Variant::INT || token_var.get_type() == Variant::FLOAT) {
		return String(token_var);
	}
	return String();
}

#ifdef TOOLS_ENABLED
static String _justamcp_get_tool_task_support(const String &p_tool_name) {
	Array schemas = JustAMCPToolExecutor::get_tool_schemas(false, true);
	for (int i = 0; i < schemas.size(); i++) {
		const Dictionary schema = schemas[i];
		if (String(schema.get("name", "")) != p_tool_name) {
			continue;
		}
		if (schema.has("execution") && schema["execution"].get_type() == Variant::DICTIONARY) {
			const Dictionary execution = schema["execution"];
			return String(execution.get("taskSupport", "forbidden"));
		}
		return "forbidden";
	}
	return "forbidden";
}
#endif

static Dictionary _justamcp_finalize_list_result(const Dictionary &p_result, const Variant &p_req_id) {
	if (p_result.has("ok") && !bool(p_result.get("ok", true))) {
		Dictionary err;
		err["jsonrpc"] = "2.0";
		err["id"] = p_req_id;
		Dictionary error_dict;
		error_dict["code"] = p_result.get("error_code", -32602);
		error_dict["message"] = p_result.get("error", "Invalid params.");
		err["error"] = error_dict;
		return err;
	}

	Dictionary out = p_result.duplicate();
	out.erase("ok");
	Dictionary rpc_result;
	rpc_result["jsonrpc"] = "2.0";
	rpc_result["id"] = p_req_id;
	rpc_result["result"] = out;
	return rpc_result;
}

void JustAMCPServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_server_started"), &JustAMCPServer::is_server_started);
	ClassDB::bind_method(D_METHOD("_process_pending_tools"), &JustAMCPServer::_process_pending_tools);
	ClassDB::bind_method(D_METHOD("_dispatch_task_augmented_tools_call", "request_id"), &JustAMCPServer::_dispatch_task_augmented_tools_call);
	ClassDB::bind_method(D_METHOD("_emit_log_notification_deferred", "level", "logger", "data"), &JustAMCPServer::_emit_log_notification_deferred);
	ClassDB::bind_method(D_METHOD("send_log_message", "level", "logger", "data"), &JustAMCPServer::send_log_message, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("_on_request_cancelled", "request_id", "reason"), &JustAMCPServer::_on_request_cancelled);
	ClassDB::bind_method(D_METHOD("report_tool_progress", "token", "progress", "total", "message"), &JustAMCPServer::report_tool_progress);
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
	{
		MutexLock lock(server->engine_logs_mutex);
		String prefix = p_error ? "[ERROR] " : "";
		server->engine_logs.push_back((prefix + p_string).strip_escapes());
		if (server->engine_logs.size() > 500) {
			server->engine_logs.remove_at(0);
		}
	}

	if (!server->server_started) {
		return;
	}
	if (!ProjectSettings::get_singleton() || !GLOBAL_GET("blazium/justamcp/forward_engine_logs")) {
		return;
	}

	const String level = p_error ? "error" : "info";
	Dictionary log_data;
	log_data["message"] = p_string;
	server->call_deferred(SNAME("send_log_message"), level, String("engine"), log_data);
}

Vector<String> JustAMCPServer::get_engine_logs() {
	MutexLock lock(engine_logs_mutex);
	return engine_logs;
}

JustAMCPServer::JustAMCPServer() {
	singleton = this;
#if defined(MODULE_HTTPSERVER_ENABLED)
	session_manager = memnew(MCPSessionManager(this));
#endif
	print_handler.printfunc = _print_handler_callback;
	print_handler.userdata = this;
	add_print_handler(&print_handler);

#ifdef TOOLS_ENABLED
	prompt_executor = memnew(JustAMCPPromptExecutor);
	resource_executor = memnew(JustAMCPResourceExecutor);
	task_manager = memnew(JustAMCPTaskManager);
	task_manager->set_server(this);
#endif
}

JustAMCPServer::~JustAMCPServer() {
	remove_print_handler(&print_handler);
	if (singleton == this) {
		singleton = nullptr;
	}

	_stop_server();
#if defined(MODULE_HTTPSERVER_ENABLED)
	if (session_manager) {
		memdelete(session_manager);
		session_manager = nullptr;
	}
	_clear_tool_queue();
#endif
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

	if (_justamcp_headless_project_server_requested()) {
		is_enabled = true;
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

		EDITOR_DEF_BASIC("blazium/justamcp/forward_engine_logs", true);
		EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/justamcp/forward_engine_logs"));

		EDITOR_DEF_BASIC("blazium/justamcp/list_page_size", 50);
		EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/justamcp/list_page_size", PROPERTY_HINT_RANGE, "1,500,1"));

		EDITOR_DEF_BASIC("blazium/justamcp/mcp_log_buffer_size", 500);
		EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/justamcp/mcp_log_buffer_size", PROPERTY_HINT_RANGE, "1,5000,1"));

		EDITOR_DEF_BASIC("blazium/justamcp/task_default_ttl_ms", 600000);
		EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/justamcp/task_default_ttl_ms", PROPERTY_HINT_RANGE, "1000,3600000,1000"));

		EDITOR_DEF_BASIC("blazium/justamcp/task_poll_interval_ms", 1000);
		EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/justamcp/task_poll_interval_ms", PROPERTY_HINT_RANGE, "100,60000,100"));

		EDITOR_DEF_BASIC("blazium/justamcp/task_max_concurrent", 16);
		EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/justamcp/task_max_concurrent", PROPERTY_HINT_RANGE, "1,128,1"));

		EDITOR_DEF_BASIC("blazium/justamcp/bind_to_localhost_only", true);
		EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/justamcp/bind_to_localhost_only"));

		EDITOR_DEF_BASIC("blazium/justamcp/session_ttl_seconds", 3600);
		EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/justamcp/session_ttl_seconds", PROPERTY_HINT_RANGE, "0,86400,1"));

		EDITOR_DEF_BASIC("blazium/justamcp/session_allow_client_delete", true);
		EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/justamcp/session_allow_client_delete"));

		EDITOR_DEF_BASIC("blazium/justamcp/streamable_http_strict_origin", false);
		EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/justamcp/streamable_http_strict_origin"));

		EDITOR_DEF_BASIC("blazium/justamcp/streamable_http_allowed_origin", "");
		EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/justamcp/streamable_http_allowed_origin"));
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
	GLOBAL_DEF_BASIC("blazium/justamcp/forward_engine_logs", true);
	GLOBAL_DEF_BASIC("blazium/justamcp/list_page_size", 50);
	GLOBAL_DEF_BASIC("blazium/justamcp/mcp_log_buffer_size", 500);
	GLOBAL_DEF_BASIC("blazium/justamcp/task_default_ttl_ms", 600000);
	GLOBAL_DEF_BASIC("blazium/justamcp/task_poll_interval_ms", 1000);
	GLOBAL_DEF_BASIC("blazium/justamcp/task_max_concurrent", 16);
	GLOBAL_DEF_BASIC("blazium/justamcp/bind_to_localhost_only", true);
	GLOBAL_DEF_BASIC("blazium/justamcp/session_ttl_seconds", 3600);
	GLOBAL_DEF_BASIC("blazium/justamcp/session_allow_client_delete", true);
	GLOBAL_DEF_BASIC("blazium/justamcp/streamable_http_strict_origin", false);
	GLOBAL_DEF_BASIC("blazium/justamcp/streamable_http_allowed_origin", String());

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
	if (!_justamcp_headless_project_server_requested()) {
		for (const String &arg : args) {
			if (arg == "--test" || arg == "--tests" || arg.begins_with("--aw-") ||
					arg == "--help" || arg == "-h" || arg == "/?" || arg == "--version" ||
					arg == "--check-only" || arg.begins_with("--export")) {
				return; // Do not start Server during CLI tools or testing workflows
			}
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

		HTTPServer::get_singleton()->set_cors_enabled(true);
		HTTPServer::get_singleton()->set_cors_origin(bind_to_localhost ? "http://127.0.0.1" : "*");

		HTTPServer::get_singleton()->register_route("GET", "/sse", callable_mp(this, &JustAMCPServer::_handle_legacy_sse_connect));
		HTTPServer::get_singleton()->register_route("POST", "/sse", callable_mp(this, &JustAMCPServer::_handle_legacy_sse_connect));
		HTTPServer::get_singleton()->register_route("OPTIONS", "/sse", callable_mp(this, &JustAMCPServer::_handle_cors_preflight));
		HTTPServer::get_singleton()->register_route("POST", "/message", callable_mp(this, &JustAMCPServer::_handle_message_post));
		HTTPServer::get_singleton()->register_route("OPTIONS", "/message", callable_mp(this, &JustAMCPServer::_handle_cors_preflight));
		HTTPServer::get_singleton()->register_route("GET", "/mcp", callable_mp(this, &JustAMCPServer::_handle_mcp_get));
		HTTPServer::get_singleton()->register_route("POST", "/mcp", callable_mp(this, &JustAMCPServer::_handle_mcp_post));
		HTTPServer::get_singleton()->register_route("DELETE", "/mcp", callable_mp(this, &JustAMCPServer::_handle_mcp_delete));
		HTTPServer::get_singleton()->register_route("OPTIONS", "/mcp", callable_mp(this, &JustAMCPServer::_handle_cors_preflight));

		if (!HTTPServer::get_singleton()->is_connected("sse_connection_opened", callable_mp(this, &JustAMCPServer::_on_sse_connection_opened))) {
			HTTPServer::get_singleton()->connect("sse_connection_opened", callable_mp(this, &JustAMCPServer::_on_sse_connection_opened));
		}
		if (!HTTPServer::get_singleton()->is_connected("sse_connection_closed", callable_mp(this, &JustAMCPServer::_on_sse_connection_closed))) {
			HTTPServer::get_singleton()->connect("sse_connection_closed", callable_mp(this, &JustAMCPServer::_on_sse_connection_closed));
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
		HTTPServer::get_singleton()->unregister_route("DELETE", "/mcp");
		HTTPServer::get_singleton()->unregister_route("OPTIONS", "/mcp");
		if (session_manager) {
			session_manager->clear_all();
		}
	}
#endif
	server_started = false;
	print_line("JustAMCP: Server Disabled.");
	emit_signal("server_status_changed", false);
	current_sse_connection_id = -1;
}

#if defined(MODULE_HTTPSERVER_ENABLED)
void JustAMCPServer::_handle_cors_preflight(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	if (session_manager) {
		session_manager->handle_cors_preflight(p_context, p_response);
	} else {
		p_response->set_status(204);
		p_response->set_body("");
	}
}

bool JustAMCPServer::_validate_mcp_oauth(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
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
				return false;
			}
		}
	}
#else
	(void)p_context;
	(void)p_response;
#endif
	return true;
}

void JustAMCPServer::_handle_legacy_sse_connect(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	if (GLOBAL_GET("blazium/justamcp/enable_debug_logging")) {
		_mcp_debug_log("Incoming " + p_context->get_method() + " /sse connection attempt...");
	}
	if (!_validate_mcp_oauth(p_context, p_response)) {
		return;
	}
	p_response->start_sse();
}

void JustAMCPServer::_handle_mcp_get(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	if (!_validate_mcp_oauth(p_context, p_response)) {
		return;
	}
	if (session_manager && session_manager->handle_mcp_get(p_context, p_response)) {
		return;
	}
	p_response->set_status(405);
	p_response->set_body("Method not allowed");
}

void JustAMCPServer::_handle_mcp_post(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	if (!_validate_mcp_oauth(p_context, p_response)) {
		return;
	}
	if (session_manager && session_manager->handle_mcp_post(p_context, p_response)) {
		return;
	}
	_handle_mcp_stateless_post(p_context, p_response);
}

void JustAMCPServer::_handle_mcp_delete(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	if (!_validate_mcp_oauth(p_context, p_response)) {
		return;
	}
	if (session_manager) {
		session_manager->handle_mcp_delete(p_context, p_response);
	} else {
		p_response->set_status(405);
		p_response->set_body("Session DELETE not enabled");
	}
}

Dictionary JustAMCPServer::_transport_handle_json_rpc(const String &p_body, Ref<HTTPResponse> p_response) {
	return _handle_json_rpc(p_body, p_response);
}

void JustAMCPServer::_on_sse_connection_opened(int p_connection_id, const String &p_path, const Dictionary &p_headers) {
	if (p_path == "/mcp" && session_manager) {
		session_manager->on_sse_connection_opened(p_connection_id, p_path, p_headers);
		return;
	}
	if (p_path == "/sse") {
		if (GLOBAL_GET("blazium/justamcp/enable_debug_logging")) {
			_mcp_debug_log("New SSE connection opened on " + p_path + " (ID: " + itos(p_connection_id) + ")");
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

void JustAMCPServer::_on_sse_connection_closed(int p_connection_id) {
	if (session_manager) {
		session_manager->on_sse_connection_closed(p_connection_id);
	}
	if (current_sse_connection_id == p_connection_id) {
		current_sse_connection_id = -1;
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

	if (GLOBAL_GET("blazium/justamcp/enable_debug_logging")) {
		Array keys = p_context->get_headers().keys();
		String header_dump;
		for (int i = 0; i < keys.size(); i++) {
			header_dump += "  - " + String(keys[i]) + ": " + String(p_context->get_headers()[keys[i]]) + "\n";
		}
		_mcp_debug_log("Message POST Payload Headers:\n" + header_dump);
		_mcp_debug_log("Message POST Received Body: " + body);
	}

	// We reply HTTP 202 explicitly to acknowledge receipt of the POST buffer natively for MCP.
	p_response->set_status(202);
	p_response->set_body("Accepted");

	// Then handle it via JSON-RPC, emitting the event down the bound SSE connection.
	Dictionary result = _handle_json_rpc(body, p_response);
	if (!result.is_empty()) {
		const String session_id_param = p_context->get_query_param("sessionId");
		if (!session_id_param.is_empty() && session_id_param.is_valid_int()) {
			const int conn_id = session_id_param.to_int();
			if (session_manager) {
				session_manager->send_json_legacy_connection(conn_id, JSON::stringify(result));
			} else {
				HTTPServer::get_singleton()->send_sse_event(conn_id, "message", JSON::stringify(result));
			}
		} else {
			_send_sse_message(JSON::stringify(result));
		}
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

	const bool debug_logging = GLOBAL_GET("blazium/justamcp/enable_debug_logging");
	if (debug_logging) {
		Array keys = p_context->get_headers().keys();
		String header_dump;
		for (int i = 0; i < keys.size(); i++) {
			header_dump += "  - " + String(keys[i]) + ": " + String(p_context->get_headers()[keys[i]]) + "\n";
		}
		_mcp_debug_log("Stateless POST Headers:\n" + header_dump);
		_mcp_debug_log("Stateless POST Received Body: " + body);
	}

	Dictionary result = _handle_json_rpc(body, p_response);
	if (!p_response->is_sent()) {
		if (result.is_empty()) {
			if (debug_logging) {
				_mcp_debug_log("Stateless POST Replying HTTP 202 (Empty result object)");
			}
			p_response->set_status(202);
			p_response->set_body("");
		} else {
			if (debug_logging) {
				_mcp_debug_log("Stateless POST Replying HTTP 200 with Result JSON payload: " + JSON::stringify(result));
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

	const bool debug_logging = GLOBAL_GET("blazium/justamcp/enable_debug_logging");
	if (debug_logging) {
		_mcp_debug_log("Executing JSON-RPC Method: " + method + " (ID: " + request_id + ")");
	}

	auto invalid_params = [&](const String &msg) -> Dictionary {
		if (debug_logging) {
			_mcp_debug_log("Payload Validation Failed for " + method + ": " + msg);
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
		const String client_protocol = String(params["protocolVersion"]);
		transport_negotiated_protocol = MCPSessionManager::negotiate_protocol_version(client_protocol);
		if (!params.has("capabilities") || params["capabilities"].get_type() != Variant::DICTIONARY) {
			return invalid_params("initialize requires 'capabilities' object.");
		}
		if (!params.has("clientInfo") || params["clientInfo"].get_type() != Variant::DICTIONARY) {
			return invalid_params("initialize requires 'clientInfo' object.");
		}

		Dictionary result;
		result["protocolVersion"] = transport_negotiated_protocol;

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
		const String cursor = _justamcp_extract_list_cursor(payload);
#ifdef TOOLS_ENABLED
		return _justamcp_finalize_list_result(JustAMCPToolExecutor::list_tools(cursor), req_id_var);
#else
		Dictionary empty;
		empty["ok"] = true;
		empty["tools"] = Array();
		return _justamcp_finalize_list_result(empty, req_id_var);
#endif
	}

	if (method == "prompts/list") {
		const String cursor = _justamcp_extract_list_cursor(payload);
#ifdef TOOLS_ENABLED
		if (prompt_executor) {
			return _justamcp_finalize_list_result(prompt_executor->list_prompts(cursor), req_id_var);
		}
#endif
		Dictionary empty;
		empty["ok"] = true;
		empty["prompts"] = Array();
		return _justamcp_finalize_list_result(empty, req_id_var);
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
		const String cursor = _justamcp_extract_list_cursor(payload);
#ifdef TOOLS_ENABLED
		if (resource_executor) {
			return _justamcp_finalize_list_result(resource_executor->list_resources(cursor), req_id_var);
		}
#endif
		Dictionary empty;
		empty["ok"] = true;
		empty["resources"] = Array();
		return _justamcp_finalize_list_result(empty, req_id_var);
	}

	if (method == "resources/templates/list") {
		const String cursor = _justamcp_extract_list_cursor(payload);
#ifdef TOOLS_ENABLED
		if (resource_executor) {
			return _justamcp_finalize_list_result(resource_executor->list_resource_templates(cursor), req_id_var);
		}
#endif
		Dictionary empty;
		empty["ok"] = true;
		empty["resourceTemplates"] = Array();
		return _justamcp_finalize_list_result(empty, req_id_var);
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
		const String cursor = _justamcp_extract_list_cursor(payload);
#ifdef TOOLS_ENABLED
		if (task_manager) {
			return _justamcp_finalize_list_result(task_manager->list_tasks(cursor), req_id_var);
		}
#endif
		Dictionary empty;
		empty["ok"] = true;
		empty["tasks"] = Array();
		return _justamcp_finalize_list_result(empty, req_id_var);
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
			if (result.get("ok", false)) {
				request_task_queue_cancel(task_id);
			}
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
		const String level = justamcp_log_level_canonical(String(params["level"]));
		if (!justamcp_log_level_is_valid(level)) {
			return invalid_params("logging/setLevel: invalid log level.");
		}
		{
			MutexLock lock(minimum_log_level_mutex);
			minimum_log_level = level;
		}
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

#ifdef TOOLS_ENABLED
		const String task_support = _justamcp_get_tool_task_support(tool_name);
		const bool has_task_param = params.has("task") && params["task"].get_type() == Variant::DICTIONARY;
		if (task_support == "forbidden" && has_task_param) {
			Dictionary err;
			err["jsonrpc"] = "2.0";
			err["id"] = req_id_var;
			Dictionary error_dict;
			error_dict["code"] = -32601;
			error_dict["message"] = vformat("Tool '%s' does not support task-augmented execution.", tool_name);
			err["error"] = error_dict;
			return err;
		}
		if (task_support == "required" && !has_task_param) {
			Dictionary err;
			err["jsonrpc"] = "2.0";
			err["id"] = req_id_var;
			Dictionary error_dict;
			error_dict["code"] = -32601;
			error_dict["message"] = vformat("Tool '%s' requires task-augmented execution.", tool_name);
			err["error"] = error_dict;
			return err;
		}
#endif

		Dictionary enqueue_options;
		String progress_token = _justamcp_progress_token_from_meta(_justamcp_extract_request_meta(params));
		if (!progress_token.is_empty()) {
			enqueue_options["progress_token"] = progress_token;
		}

#ifdef TOOLS_ENABLED
		if (has_task_param) {
			int default_ttl = 600000;
			int default_poll = 1000;
			if (ProjectSettings::get_singleton()) {
				if (ProjectSettings::get_singleton()->has_setting("blazium/justamcp/task_default_ttl_ms")) {
					default_ttl = int(GLOBAL_GET("blazium/justamcp/task_default_ttl_ms"));
				}
				if (ProjectSettings::get_singleton()->has_setting("blazium/justamcp/task_poll_interval_ms")) {
					default_poll = int(GLOBAL_GET("blazium/justamcp/task_poll_interval_ms"));
				}
			}
			const Dictionary task_params = params["task"];
			const int ttl_ms = task_params.has("ttl") ? int(task_params["ttl"]) : default_ttl;
			const int poll_ms = task_params.has("pollInterval") ? int(task_params["pollInterval"]) : default_poll;

			if (!task_manager) {
				return invalid_params("Task manager unavailable.");
			}

			enqueue_options["is_task_augmented"] = true;
			enqueue_options["pending_task_dispatch"] = true;
			enqueue_options["task_ttl_ms"] = ttl_ms;
			enqueue_options["task_poll_ms"] = poll_ms;

			Dictionary queue_full_error;
			MCPToolQueueEntry *entry = _enqueue_tool_request(req_id_var, tool_name, args, p_response, queue_full_error, enqueue_options);
			if (!entry) {
				return queue_full_error;
			}

			call_deferred(SNAME("_dispatch_task_augmented_tools_call"), req_id_var);

			if (entry->has_stateless_response) {
				const int wait_ms = 120000;
				if (!_wait_for_stateless_tool_entry(entry, wait_ms)) {
					MutexLock lock(tool_queue_mutex);
					for (int i = 0; i < tool_queue.size(); i++) {
						if (tool_queue[i] == entry) {
							tool_queue.remove_at(i);
							break;
						}
					}
					if (current_tool_entry == entry) {
						current_tool_entry = nullptr;
						tool_queue_processing = false;
					}
					memdelete(entry);
					return _stateless_tool_timeout_error(req_id_var);
				}
				Dictionary res = entry->rpc_result;
				entry->stateless_response = Ref<HTTPResponse>();
				entry->has_stateless_response = false;
				call_deferred(SNAME("_process_pending_tools"));
				return res;
			}

			return Dictionary();
		}
#endif

		Dictionary queue_full_error;
		MCPToolQueueEntry *entry = _enqueue_tool_request(req_id_var, tool_name, args, p_response, queue_full_error, enqueue_options);
		if (!entry) {
			return queue_full_error;
		}

		if (!progress_token.is_empty()) {
			_register_progress_token(progress_token, String(), req_id_var);
		}

		call_deferred(SNAME("_process_pending_tools"));

		if (entry->has_stateless_response) {
			const int wait_ms = 120000;
			if (!_wait_for_stateless_tool_entry(entry, wait_ms)) {
				if (!progress_token.is_empty()) {
					_unregister_progress_token(progress_token);
				}
				MutexLock lock(tool_queue_mutex);
				for (int i = 0; i < tool_queue.size(); i++) {
					if (tool_queue[i] == entry) {
						tool_queue.remove_at(i);
						break;
					}
				}
				if (current_tool_entry == entry) {
					current_tool_entry = nullptr;
					tool_queue_processing = false;
				}
				memdelete(entry);
				return _stateless_tool_timeout_error(req_id_var);
			}
			Dictionary res = entry->rpc_result;
			if (!progress_token.is_empty()) {
				_unregister_progress_token(progress_token);
			}
			memdelete(entry);
			return res;
		}

		return Dictionary();
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

void JustAMCPServer::_send_sse_routed(const String &p_json_string, const String &p_session_id, int p_connection_id) {
#if defined(MODULE_HTTPSERVER_ENABLED)
	if (!HTTPServer::get_singleton()) {
		return;
	}
	if (session_manager) {
		if (!p_session_id.is_empty() && session_manager->send_json_to_session(p_session_id, p_json_string, p_connection_id)) {
			return;
		}
		if (p_connection_id >= 0 && session_manager->send_json_on_connection(p_connection_id, p_json_string)) {
			return;
		}
		session_manager->broadcast_json(p_json_string);
	}
	if (current_sse_connection_id >= 0) {
		HTTPServer::get_singleton()->send_sse_event(current_sse_connection_id, "message", p_json_string);
	}
#endif
}

void JustAMCPServer::_send_sse_message(const String &p_json_string) {
#if defined(MODULE_HTTPSERVER_ENABLED)
	if (session_manager) {
		session_manager->broadcast_json(p_json_string);
	}
	if (current_sse_connection_id >= 0 && HTTPServer::get_singleton()) {
		HTTPServer::get_singleton()->send_sse_event(current_sse_connection_id, "message", p_json_string);
	}
#endif
}

#endif // MODULE_HTTPSERVER_ENABLED

void JustAMCPServer::send_tool_result(const Variant &p_request_id, bool p_success, const Variant &p_result, const String &p_error) {
#if defined(MODULE_HTTPSERVER_ENABLED)
	MCPToolQueueEntry *entry = nullptr;
	{
		MutexLock lock(tool_queue_mutex);
		entry = current_tool_entry;
	}

	if (entry && entry->is_task_augmented) {
		_complete_task_tool_entry(entry, p_success, p_result, p_error);
		return;
	}

	const Dictionary rpc_result = _format_tool_result_dict(p_success, p_result, p_error);
	Dictionary rpc_with_id = rpc_result.duplicate();
	rpc_with_id["id"] = p_request_id;
	_complete_current_tool_request(rpc_with_id);
#else
	(void)p_request_id;
	(void)p_success;
	(void)p_result;
	(void)p_error;
#endif
}

#if defined(MODULE_HTTPSERVER_ENABLED)

MCPToolQueueEntry *JustAMCPServer::_enqueue_tool_request(const Variant &p_request_id, const String &p_tool_name, const Dictionary &p_args, Ref<HTTPResponse> p_response, Dictionary &r_queue_full_error, const Dictionary &p_options) {
	MutexLock lock(tool_queue_mutex);
	if (tool_queue.size() >= TOOL_QUEUE_MAX) {
		r_queue_full_error["jsonrpc"] = "2.0";
		r_queue_full_error["id"] = p_request_id;
		Dictionary error_dict;
		error_dict["code"] = -32003;
		error_dict["message"] = vformat("MCP tool queue full (%d pending). Retry later.", TOOL_QUEUE_MAX);
		r_queue_full_error["error"] = error_dict;
		return nullptr;
	}

	MCPToolQueueEntry *entry = memnew(MCPToolQueueEntry);
	entry->request_id = p_request_id;
	entry->tool_name = p_tool_name;
	entry->args = p_args;
	entry->stateless_response = p_response;
	entry->has_stateless_response = p_response.is_valid();
	if (p_options.has("task_id")) {
		entry->task_id = p_options["task_id"];
	}
	if (p_options.has("progress_token")) {
		entry->progress_token = p_options["progress_token"];
	}
	entry->is_task_augmented = p_options.get("is_task_augmented", false);
	entry->pending_task_dispatch = p_options.get("pending_task_dispatch", false);
	if (p_options.has("task_ttl_ms")) {
		entry->pending_task_ttl_ms = int(p_options["task_ttl_ms"]);
	}
	if (p_options.has("task_poll_ms")) {
		entry->pending_task_poll_interval_ms = int(p_options["task_poll_ms"]);
	}
	if (session_manager) {
		entry->session_id = session_manager->get_tool_route_session_id();
		entry->sse_connection_id = session_manager->get_tool_route_connection_id();
	}
	tool_queue.push_back(entry);
	return entry;
}

void JustAMCPServer::_dispatch_task_augmented_tools_call(const Variant &p_request_id) {
	MCPToolQueueEntry *entry = nullptr;
	{
		MutexLock lock(tool_queue_mutex);
		for (int i = 0; i < tool_queue.size(); i++) {
			MCPToolQueueEntry *candidate = tool_queue[i];
			if (candidate && _justamcp_request_ids_equal(candidate->request_id, p_request_id)) {
				entry = candidate;
				break;
			}
		}
	}

	if (!entry || !entry->pending_task_dispatch) {
		ERR_PRINT("JustAMCP: task-augmented tools/call dispatch could not find a pending queue entry.");
		return;
	}

	const String progress_token = entry->progress_token;
	const int ttl_ms = entry->pending_task_ttl_ms;
	const int poll_ms = entry->pending_task_poll_interval_ms;

	entry->pending_task_dispatch = false;

#ifdef TOOLS_ENABLED
	if (!task_manager) {
		Dictionary err;
		err["jsonrpc"] = "2.0";
		err["id"] = p_request_id;
		Dictionary error_dict;
		error_dict["code"] = -32603;
		error_dict["message"] = "Task manager unavailable.";
		err["error"] = error_dict;
		entry->rpc_result = err;
		if (entry->has_stateless_response) {
			entry->done_semaphore.post();
		}
		return;
	}

	const String task_id = task_manager->create_task(ttl_ms, poll_ms, progress_token);
	if (task_id.is_empty()) {
		Dictionary err;
		err["jsonrpc"] = "2.0";
		err["id"] = p_request_id;
		Dictionary error_dict;
		error_dict["code"] = -32003;
		error_dict["message"] = "Maximum concurrent MCP tasks reached.";
		err["error"] = error_dict;
		entry->rpc_result = err;
		if (entry->has_stateless_response) {
			entry->done_semaphore.post();
		}
		return;
	}

	entry->task_id = task_id;
	if (!progress_token.is_empty()) {
		_register_progress_token(progress_token, task_id, p_request_id);
	}

	Dictionary rpc_result;
	rpc_result["jsonrpc"] = "2.0";
	rpc_result["id"] = p_request_id;
	rpc_result["result"] = _build_create_task_result(task_id);
	entry->rpc_result = rpc_result;

	if (entry->has_stateless_response) {
		entry->done_semaphore.post();
	} else {
		_send_sse_routed(JSON::stringify(rpc_result), entry->session_id, entry->sse_connection_id);
		call_deferred(SNAME("_process_pending_tools"));
	}
#else
	(void)ttl_ms;
	(void)poll_ms;
#endif
}

void JustAMCPServer::_process_pending_tools() {
	MCPToolQueueEntry *entry = nullptr;
	{
		MutexLock lock(tool_queue_mutex);
		if (tool_queue_processing || tool_queue.is_empty()) {
			return;
		}
		entry = tool_queue[0];
		if (entry == current_tool_entry) {
			return;
		}
		if (entry->pending_task_dispatch) {
			return;
		}
		tool_queue_processing = true;
		current_tool_entry = entry;
	}

	emit_signal("tool_requested", entry->request_id, entry->tool_name, entry->args);
}

void JustAMCPServer::_complete_current_tool_request(const Dictionary &p_rpc_result) {
	MCPToolQueueEntry *completed_entry = nullptr;
	bool has_stateless = false;
	bool send_via_sse = false;
	{
		MutexLock lock(tool_queue_mutex);
		if (!current_tool_entry) {
			send_via_sse = true;
		} else {
			completed_entry = current_tool_entry;
			completed_entry->rpc_result = p_rpc_result;
			has_stateless = completed_entry->has_stateless_response;

			if (tool_queue.size() > 0 && tool_queue[0] == completed_entry) {
				tool_queue.remove_at(0);
			}
			current_tool_entry = nullptr;
			tool_queue_processing = false;
		}
	}

	if (send_via_sse) {
		_send_sse_routed(JSON::stringify(p_rpc_result), String(), -1);
		return;
	}

	if (!completed_entry->progress_token.is_empty()) {
		_unregister_progress_token(completed_entry->progress_token);
	}

	if (has_stateless) {
		completed_entry->done_semaphore.post();
	} else {
		_send_sse_routed(JSON::stringify(p_rpc_result), completed_entry->session_id, completed_entry->sse_connection_id);
	}

	call_deferred(SNAME("_process_pending_tools"));
}

void JustAMCPServer::_clear_tool_queue() {
	MutexLock lock(tool_queue_mutex);
	for (int i = 0; i < tool_queue.size(); i++) {
		MCPToolQueueEntry *entry = tool_queue[i];
		if (entry) {
			if (!entry->progress_token.is_empty()) {
				_unregister_progress_token(entry->progress_token);
			}
			if (entry->has_stateless_response || entry->pending_task_dispatch) {
				entry->done_semaphore.post();
			}
			memdelete(entry);
		}
	}
	tool_queue.clear();
	current_tool_entry = nullptr;
	tool_queue_processing = false;
}

Dictionary JustAMCPServer::_stateless_tool_timeout_error(const Variant &p_request_id) const {
	Dictionary err;
	err["jsonrpc"] = "2.0";
	err["id"] = p_request_id;
	Dictionary error_dict;
	error_dict["code"] = -32003;
	error_dict["message"] = "MCP tool request timed out waiting for dispatch.";
	err["error"] = error_dict;
	return err;
}

bool JustAMCPServer::_wait_for_stateless_tool_entry(MCPToolQueueEntry *p_entry, int p_timeout_ms) {
	ERR_FAIL_NULL_V(p_entry, false);
	const uint64_t deadline = OS::get_singleton()->get_ticks_msec() + (uint64_t)p_timeout_ms;
	while (OS::get_singleton()->get_ticks_msec() < deadline) {
		if (p_entry->done_semaphore.try_wait()) {
			return true;
		}
		OS::get_singleton()->delay_usec(1000);
	}
	return false;
}

Dictionary JustAMCPServer::_format_tool_result_dict(bool p_success, const Variant &p_result, const String &p_error) const {
	Dictionary rpc_result;
	rpc_result["jsonrpc"] = "2.0";

	if (p_success) {
		Dictionary result;
		if (p_result.get_type() == Variant::DICTIONARY && Dictionary(p_result).has("content")) {
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
		Dictionary error_dict = p_result.get_type() == Variant::DICTIONARY ? Dictionary(p_result) : Dictionary();
		if (!error_dict.is_empty() && error_dict.has("code")) {
			Dictionary error;
			error["code"] = error_dict["code"];
			error["message"] = error_dict.get("message", p_error);
			rpc_result["error"] = error;
		} else {
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

	return rpc_result;
}

Dictionary JustAMCPServer::_build_create_task_result(const String &p_task_id) const {
	Dictionary result;
#ifdef TOOLS_ENABLED
	if (task_manager) {
		Dictionary task_dict = task_manager->get_task(p_task_id);
		if (task_dict.get("ok", false)) {
			task_dict.erase("ok");
			result["task"] = task_dict;
			return result;
		}
	}
#endif
	Dictionary task;
	task["taskId"] = p_task_id;
	task["status"] = "working";
	result["task"] = task;
	return result;
}

void JustAMCPServer::_register_progress_token(const String &p_token, const String &p_task_id, const Variant &p_request_id) {
	if (p_token.is_empty()) {
		return;
	}
	MutexLock lock(progress_mutex);
	JustAMCPActiveProgressContext ctx;
	ctx.task_id = p_task_id;
	ctx.request_id = p_request_id;
	active_progress_tokens[p_token] = ctx;
}

void JustAMCPServer::_unregister_progress_token(const String &p_token) {
	if (p_token.is_empty()) {
		return;
	}
	MutexLock lock(progress_mutex);
	active_progress_tokens.erase(p_token);
}

void JustAMCPServer::_complete_task_tool_entry(MCPToolQueueEntry *p_entry, bool p_success, const Variant &p_result, const String &p_error) {
	if (!p_entry) {
		return;
	}

	const String progress_token = p_entry->progress_token;
	const String task_id = p_entry->task_id;
	const bool was_cancelled = p_entry->cancel_requested || String(p_error) == "cancelled" || (p_result.get_type() == Variant::DICTIONARY && String(Dictionary(p_result).get("error", "")) == "cancelled");

#ifdef TOOLS_ENABLED
	if (task_manager && !task_id.is_empty()) {
		if (was_cancelled) {
			task_manager->cancel_task_execution(task_id);
		} else {
			const Dictionary formatted = _format_tool_result_dict(p_success, p_result, p_error);
			Dictionary stored;
			if (formatted.has("result")) {
				stored = formatted["result"];
			} else if (formatted.has("error")) {
				task_manager->fail_task(task_id, String(formatted["error"].operator Dictionary().get("message", p_error)));
			} else {
				stored = formatted;
			}
			if (!was_cancelled && !formatted.has("error")) {
				const bool is_error = stored.get("isError", false);
				task_manager->complete_task(task_id, stored, is_error);
			}
		}
	}
#endif

	if (!progress_token.is_empty()) {
		_unregister_progress_token(progress_token);
	}

	MCPToolQueueEntry *completed_entry = p_entry;
	{
		MutexLock lock(tool_queue_mutex);
		if (tool_queue.size() > 0 && tool_queue[0] == completed_entry) {
			tool_queue.remove_at(0);
		}
		if (current_tool_entry == completed_entry) {
			current_tool_entry = nullptr;
		}
		tool_queue_processing = false;
	}

	memdelete(completed_entry);
	call_deferred(SNAME("_process_pending_tools"));
}

void JustAMCPServer::_on_request_cancelled(const Variant &p_request_id, const String &p_reason) {
	(void)p_reason;
#if defined(MODULE_HTTPSERVER_ENABLED)
	MCPToolQueueEntry *target = nullptr;
	{
		MutexLock lock(tool_queue_mutex);
		if (current_tool_entry && current_tool_entry->request_id == p_request_id && !current_tool_entry->is_task_augmented) {
			current_tool_entry->cancel_requested = true;
			target = current_tool_entry;
		} else {
			for (int i = 0; i < tool_queue.size(); i++) {
				MCPToolQueueEntry *entry = tool_queue[i];
				if (entry && entry->request_id == p_request_id && !entry->is_task_augmented) {
					entry->cancel_requested = true;
					target = entry;
					break;
				}
			}
		}
	}

	if (target && target == current_tool_entry && !target->cancel_requested) {
		target->cancel_requested = true;
		Dictionary cancelled;
		cancelled["ok"] = false;
		cancelled["error"] = "cancelled";
		send_tool_result(p_request_id, false, cancelled, "cancelled");
	}
#endif
}

bool JustAMCPServer::is_current_tool_cancel_requested() const {
	MutexLock lock(tool_queue_mutex);
	if (!current_tool_entry) {
		return false;
	}
	if (current_tool_entry->cancel_requested) {
		return true;
	}
#ifdef TOOLS_ENABLED
	if (!current_tool_entry->task_id.is_empty() && task_manager) {
		return task_manager->is_cancel_requested(current_tool_entry->task_id);
	}
#endif
	return false;
}

String JustAMCPServer::get_current_progress_token() const {
	MutexLock lock(tool_queue_mutex);
	return current_tool_entry ? current_tool_entry->progress_token : String();
}

String JustAMCPServer::get_current_task_id() const {
	MutexLock lock(tool_queue_mutex);
	return current_tool_entry ? current_tool_entry->task_id : String();
}

bool JustAMCPServer::is_task_cancel_requested(const String &p_task_id) const {
#ifdef TOOLS_ENABLED
	return task_manager && task_manager->is_cancel_requested(p_task_id);
#else
	(void)p_task_id;
	return false;
#endif
}

void JustAMCPServer::request_task_queue_cancel(const String &p_task_id) {
#if defined(MODULE_HTTPSERVER_ENABLED)
	MutexLock lock(tool_queue_mutex);
	for (int i = 0; i < tool_queue.size(); i++) {
		MCPToolQueueEntry *entry = tool_queue[i];
		if (entry && entry->task_id == p_task_id) {
			entry->cancel_requested = true;
		}
	}
	if (current_tool_entry && current_tool_entry->task_id == p_task_id) {
		current_tool_entry->cancel_requested = true;
	}
#endif
}

void JustAMCPServer::report_tool_progress(const String &p_token, double p_progress, double p_total, const String &p_message) {
	if (p_token.is_empty()) {
		return;
	}

	bool should_emit = false;
	{
		MutexLock lock(progress_mutex);
		if (!active_progress_tokens.has(p_token)) {
			return;
		}
		JustAMCPActiveProgressContext &ctx = active_progress_tokens[p_token];
		const uint64_t now = Time::get_singleton()->get_ticks_usec();
		if (ctx.last_emit_usec == 0 || now - ctx.last_emit_usec >= 100000) {
			ctx.last_emit_usec = now;
			should_emit = true;
		}
	}

	if (should_emit) {
		send_progress_notification(p_token, p_progress, p_total, p_message);
	}
}

#endif // MODULE_HTTPSERVER_ENABLED

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

	_complete_current_tool_request(rpc_result);
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

void JustAMCPServer::_append_mcp_notification_log(const String &p_level, const String &p_logger, const Dictionary &p_data) {
	Dictionary entry;
	entry["level"] = p_level;
	if (!p_logger.is_empty()) {
		entry["logger"] = p_logger;
	}
	entry["data"] = p_data;
	entry["timestamp_usec"] = Time::get_singleton()->get_ticks_usec();

	MutexLock lock(mcp_notification_log_mutex);
	mcp_notification_log.push_back(entry);
	const int max_entries = justamcp_mcp_log_buffer_size();
	while (mcp_notification_log.size() > max_entries) {
		mcp_notification_log.remove_at(0);
	}
}

Dictionary JustAMCPServer::get_mcp_notification_log_page(const String &p_cursor) {
	Array notifications;
	{
		MutexLock lock(mcp_notification_log_mutex);
		notifications.resize(mcp_notification_log.size());
		for (int i = 0; i < mcp_notification_log.size(); i++) {
			notifications[i] = mcp_notification_log[i];
		}
	}
	return justamcp_pagination_slice_array(notifications, p_cursor, "notifications");
}

void JustAMCPServer::_mcp_debug_log(const String &p_message) {
	if (!GLOBAL_GET("blazium/justamcp/enable_debug_logging")) {
		return;
	}
	Dictionary log_data;
	log_data["message"] = p_message;
	send_log_message("debug", "justamcp", log_data);
}

bool JustAMCPServer::_should_emit_log(const String &p_level) {
	if (!server_started) {
		return false;
	}
	if (!justamcp_log_level_is_valid(p_level)) {
		return false;
	}
	String min_level;
	{
		MutexLock lock(minimum_log_level_mutex);
		min_level = minimum_log_level;
	}
	return justamcp_log_level_passes(p_level, min_level);
}

bool JustAMCPServer::_try_consume_log_rate_limit() {
	MutexLock lock(log_rate_mutex);
	const uint64_t now_usec = Time::get_singleton()->get_ticks_usec();
	const uint64_t window_usec = 1000000;
	if (log_rate_window_start_usec == 0 || now_usec - log_rate_window_start_usec >= window_usec) {
		log_rate_window_start_usec = now_usec;
		log_rate_count = 0;
	}
	if (log_rate_count >= LOG_RATE_LIMIT_PER_SEC) {
		return false;
	}
	log_rate_count++;
	return true;
}

void JustAMCPServer::send_log_message(const String &p_level, const String &p_logger, const Variant &p_data) {
	const String level = justamcp_log_level_canonical(p_level);
	if (!justamcp_log_level_is_valid(level)) {
		return;
	}
	Dictionary log_data;
	if (p_data.get_type() == Variant::DICTIONARY) {
		log_data = Dictionary(p_data);
	} else if (p_data.get_type() != Variant::NIL) {
		log_data["message"] = String(p_data);
	}
	_append_mcp_notification_log(level, p_logger, log_data);
	if (!_should_emit_log(level)) {
		return;
	}
	call_deferred(SNAME("_emit_log_notification_deferred"), level, p_logger, log_data);
}

void JustAMCPServer::_emit_log_notification_deferred(const String &p_level, const String &p_logger, const Dictionary &p_data) {
#if defined(MODULE_HTTPSERVER_ENABLED)
	if (!server_started || !HTTPServer::get_singleton()) {
		return;
	}
	if (HTTPServer::get_singleton()->get_active_sse_connections().is_empty()) {
		return;
	}
	if (!_should_emit_log(p_level)) {
		return;
	}
	if (!_try_consume_log_rate_limit()) {
		return;
	}

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
