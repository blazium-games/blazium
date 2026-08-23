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

#include "justamcp_cli_args.h"
#include "justamcp_cors_policy.h"
#include "justamcp_json_rpc_transport.h"
#include "justamcp_notification_bus.h"
#include "justamcp_pagination.h"
#include "justamcp_project_settings.h"
#include "justamcp_server_request_lookup.h"
#include "justamcp_session_manager.h"
#include "justamcp_tool_dispatch.h"
#include "justamcp_tool_queue_state.h"
#include "tools/justamcp_json_rpc_helpers.h"
#include "tools/justamcp_json_rpc_router.h"
#include "tools/justamcp_prompt_executor.h"
#include "tools/justamcp_resource_executor.h"
#include "tools/justamcp_settings_resolver.h"
#include "tools/justamcp_task_manager.h"
#include "tools/justamcp_tool_executor.h"
#include "tools/justamcp_tool_schema_cache.h"

#include "core/config/project_settings.h"
#include "core/object/callable_method_pointer.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "editor/editor_settings.h"

#include "modules/modules_enabled.gen.h"

static bool _justamcp_headless_project_server_requested() {
	if (!JustAMCPCliArgs::is_headless()) {
		return false;
	}
	return JustAMCPSettingsResolver::resolve_bool("blazium/justamcp/server_enabled", false);
}

void JustAMCPServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_server_started"), &JustAMCPServer::is_server_started);
	ClassDB::bind_method(D_METHOD("_process_pending_tools"), &JustAMCPServer::_process_pending_tools);
	ClassDB::bind_method(D_METHOD("_dispatch_task_augmented_tools_call", "request_id"), &JustAMCPServer::_dispatch_task_augmented_tools_call);
	ClassDB::bind_method(D_METHOD("_deferred_post_sse_json_rpc", "connection_id", "session_id", "payload"), &JustAMCPServer::_deferred_post_sse_json_rpc);
	ClassDB::bind_method(D_METHOD("_deferred_legacy_message_json_rpc", "body", "session_id_param"), &JustAMCPServer::_deferred_legacy_message_json_rpc);
	ClassDB::bind_method(D_METHOD("_deferred_held_json_rpc", "client_id", "body", "session_id", "response"), &JustAMCPServer::_deferred_held_json_rpc);
	ClassDB::bind_method(D_METHOD("_deferred_sse_replay", "connection_id", "last_event_id"), &JustAMCPServer::_deferred_sse_replay);
	ClassDB::bind_method(D_METHOD("_emit_log_notification_deferred", "level", "logger", "data"), &JustAMCPServer::_emit_log_notification_deferred);
	ClassDB::bind_method(D_METHOD("send_log_message", "level", "logger", "data"), &JustAMCPServer::send_log_message, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("get_session_roots", "session_id"), &JustAMCPServer::get_session_roots);
	ClassDB::bind_method(D_METHOD("_on_request_cancelled", "request_id", "reason", "caller_session_id"), &JustAMCPServer::_on_request_cancelled, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("_enforce_in_flight_cancel_deadline", "request_id"), &JustAMCPServer::_enforce_in_flight_cancel_deadline);
	ClassDB::bind_method(D_METHOD("_deferred_complete_tool_dict", "request_id", "result"), &JustAMCPServer::_deferred_complete_tool_dict);
	ClassDB::bind_method(D_METHOD("report_tool_progress", "token", "progress", "total", "message"), &JustAMCPServer::report_tool_progress);
	ADD_SIGNAL(MethodInfo("tool_requested", PropertyInfo(Variant::NIL, "request_id", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT), PropertyInfo(Variant::STRING, "tool_name"), PropertyInfo(Variant::DICTIONARY, "args")));
	ADD_SIGNAL(MethodInfo("server_status_changed", PropertyInfo(Variant::BOOL, "started")));
	ADD_SIGNAL(MethodInfo("elicitation_completed", PropertyInfo(Variant::NIL, "request_id", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT), PropertyInfo(Variant::DICTIONARY, "result")));
	ADD_SIGNAL(MethodInfo("request_cancelled", PropertyInfo(Variant::NIL, "request_id", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT), PropertyInfo(Variant::STRING, "reason"), PropertyInfo(Variant::STRING, "caller_session_id")));
}

JustAMCPServer *JustAMCPServer::singleton = nullptr;

JustAMCPServer *JustAMCPServer::get_singleton() {
	return singleton;
}

int JustAMCPServer::get_pending_tool_queue_size() {
	MutexLock lock(mcp_tool_queue.mutex);
	return mcp_tool_queue.pending.size();
}

#ifdef TESTS_ENABLED
MCPToolQueueEntry *JustAMCPServer::test_enqueue_tool_request(const Variant &p_request_id, const String &p_tool_name, const Dictionary &p_args, Dictionary &r_queue_full_error) {
#if defined(MODULE_HTTPSERVER_ENABLED)
	return _enqueue_tool_request(p_request_id, p_tool_name, p_args, Ref<HTTPResponse>(), r_queue_full_error);
#else
	(void)p_request_id;
	(void)p_tool_name;
	(void)p_args;
	(void)r_queue_full_error;
	return nullptr;
#endif
}

void JustAMCPServer::test_set_in_flight_entries(MCPToolQueueEntry *p_write, MCPToolQueueEntry *p_readonly) {
	MutexLock lock(mcp_tool_queue.mutex);
	mcp_tool_queue.current_write = p_write;
	mcp_tool_queue.current_readonly_inflight.clear();
	if (p_readonly) {
		mcp_tool_queue.current_readonly_inflight.push_back(p_readonly);
	}
	JustAMCPToolQueueState::sync_processing_flag(mcp_tool_queue.current_write, mcp_tool_queue.current_readonly_inflight, mcp_tool_queue.processing);
}

bool JustAMCPServer::test_get_tool_queue_processing() const {
	MutexLock lock(mcp_tool_queue.mutex);
	return mcp_tool_queue.processing;
}

void JustAMCPServer::test_start_server() {
	_start_server_internal(true);
}

void JustAMCPServer::test_stop_server() {
	_stop_server();
}

void JustAMCPServer::test_clear_tool_queue() {
#if defined(MODULE_HTTPSERVER_ENABLED)
	_clear_tool_queue();
#endif
}

MCPToolQueueEntry *JustAMCPServer::test_get_in_flight_write() const {
	MutexLock lock(mcp_tool_queue.mutex);
	return mcp_tool_queue.current_write;
}

MCPToolQueueEntry *JustAMCPServer::test_get_in_flight_readonly() const {
	MutexLock lock(mcp_tool_queue.mutex);
	return mcp_tool_queue.current_readonly_inflight.is_empty() ? nullptr : mcp_tool_queue.current_readonly_inflight[0];
}

void JustAMCPServer::test_register_task_route(const String &p_task_id, const String &p_session_id, int p_connection_id) {
	_register_task_route(p_task_id, p_session_id, p_connection_id);
}

bool JustAMCPServer::test_get_active_task_route(const String &p_task_id, String &r_session_id, int &r_connection_id) const {
	if (p_task_id.is_empty()) {
		return false;
	}
	MutexLock lock(routing_mutex);
	if (!active_task_routes.has(p_task_id)) {
		return false;
	}
	const JustAMCPActiveProgressContext &ctx = active_task_routes[p_task_id];
	r_session_id = ctx.session_id;
	r_connection_id = ctx.connection_id;
	return true;
}

void JustAMCPServer::test_set_current_sse_connection_id(int p_connection_id) {
	current_sse_connection_id = p_connection_id;
}

int JustAMCPServer::test_count_notification_broadcast_targets() const {
	return notification_bus.test_count_broadcast_targets("{}");
}

MCPSessionManager *JustAMCPServer::test_get_session_manager() const {
	return session_manager;
}

Dictionary JustAMCPServer::test_peek_last_send_tool_result() const {
	return test_last_send_tool_result;
}

void JustAMCPServer::test_clear_last_send_tool_result() {
	test_last_send_tool_result = Dictionary();
}

bool JustAMCPServer::test_validate_mcp_oauth(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	return _validate_mcp_oauth(p_context, p_response);
}

void JustAMCPServer::test_handle_mcp_stateless_post(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	_handle_mcp_stateless_post(p_context, p_response);
}

void JustAMCPServer::test_handle_message_post(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	_handle_message_post(p_context, p_response);
}

void JustAMCPServer::test_handle_oauth_protected_resource(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	_handle_oauth_protected_resource(p_context, p_response);
}

void JustAMCPServer::test_handle_oauth_authorization_server(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	_handle_oauth_authorization_server(p_context, p_response);
}

#ifdef TOOLS_ENABLED
void JustAMCPServer::test_handle_oauth_callback(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	_handle_oauth_callback(p_context, p_response);
}

void JustAMCPServer::test_handle_oauth_client_metadata(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	_handle_oauth_client_metadata(p_context, p_response);
}

void JustAMCPServer::test_handle_mcp_apps_host(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	_handle_mcp_apps_host(p_context, p_response);
}

void JustAMCPServer::test_handle_mcp_apps_proxy(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	_handle_mcp_apps_proxy(p_context, p_response);
}
#endif
#endif

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
	if (!JustAMCPSettingsResolver::resolve_bool("blazium/justamcp/forward_engine_logs", false)) {
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

Dictionary JustAMCPServer::get_engine_logs_page(const String &p_cursor) {
	int offset = 0;
	if (!justamcp_pagination_decode_cursor(p_cursor, offset)) {
		Dictionary err;
		err["ok"] = false;
		err["error_code"] = -32602;
		err["error"] = "Invalid pagination cursor.";
		return err;
	}
	Array page;
	int total = 0;
	{
		MutexLock lock(engine_logs_mutex);
		total = engine_logs.size();
		if (offset < 0) {
			offset = 0;
		}
		const int page_size = justamcp_pagination_page_size();
		for (int i = offset; i < total && page.size() < page_size; i++) {
			page.push_back(engine_logs[i]);
		}
	}
	Dictionary result;
	result["ok"] = true;
	result["logs"] = page;
	if (offset + page.size() < total) {
		result["nextCursor"] = justamcp_pagination_encode_cursor(offset + page.size());
	}
	return result;
}

JustAMCPServer::JustAMCPServer() {
	const bool claim_singleton = (singleton == nullptr);
	if (claim_singleton) {
		singleton = this;
	}
	if (!claim_singleton) {
		return;
	}

#if defined(MODULE_HTTPSERVER_ENABLED)
	session_manager = memnew(MCPSessionManager(this));
	notification_bus.set_owner(this);
	notification_bus.set_session_manager(session_manager);
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
	if (singleton != this && !prompt_executor && !resource_executor && !task_manager
#if defined(MODULE_HTTPSERVER_ENABLED)
			&& !session_manager
#endif
	) {
		return;
	}

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
	if (headless_tool_executor) {
		memdelete(headless_tool_executor);
		headless_tool_executor = nullptr;
	}
#endif
}

void JustAMCPServer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_setup_settings();
			_start_server();
		} break;
		case NOTIFICATION_READY: {
			_setup_settings();
#ifdef TOOLS_ENABLED
			if (EditorSettings::get_singleton() && !EditorSettings::get_singleton()->is_connected("settings_changed", callable_mp(this, &JustAMCPServer::_on_settings_changed))) {
				EditorSettings::get_singleton()->connect("settings_changed", callable_mp(this, &JustAMCPServer::_on_settings_changed));
			}
#endif
			if (ProjectSettings::get_singleton() && !ProjectSettings::get_singleton()->is_connected("settings_changed", callable_mp(this, &JustAMCPServer::_on_settings_changed))) {
				ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &JustAMCPServer::_on_settings_changed));
			}
			_start_server();
		} break;
		case NOTIFICATION_EXIT_TREE: {
			_stop_server();
		} break;
	}
}

int JustAMCPServer::_resolve_listening_port_from_settings() const {
	return JustAMCPSettingsResolver::resolve_server_port();
}

void JustAMCPServer::_on_settings_changed() {
#ifdef TOOLS_ENABLED
	JustAMCPJsonRpcHelpers::mark_mcp_tool_settings_dirty();
	bool is_enabled = JustAMCPSettingsResolver::resolve_server_enabled();

	if (_justamcp_headless_project_server_requested()) {
		is_enabled = true;
	}

	if (!is_enabled && server_started) {
		_stop_server();
	} else if (is_enabled && !server_started) {
		_start_server();
	} else if (is_enabled && server_started) {
		const int resolved_port = _resolve_listening_port_from_settings();
		if (resolved_port > 0 && resolved_port != active_listening_port) {
			_stop_server();
			_start_server();
		}
	}
	if (server_started && JustAMCPJsonRpcHelpers::should_broadcast_tools_list_changed()) {
		broadcast_tools_list_changed();
	}
#endif
}

void JustAMCPServer::_setup_settings() {
#ifdef TOOLS_ENABLED
	JustAMCPProjectSettings::register_editor_settings();
#endif
}

void JustAMCPServer::start_listening() {
	_start_server();
}

void JustAMCPServer::_start_server() {
	_start_server_internal(false);
}

void JustAMCPServer::_start_server_internal(bool p_ignore_cmdline_block) {
	if (server_started) {
		return;
	}

	const List<String> &args = OS::get_singleton()->get_cmdline_args();
	if (!p_ignore_cmdline_block && JustAMCPCliArgs::skip_mcp_server() && !_justamcp_headless_project_server_requested()) {
		print_line("JustAMCP: Server not started (cmdline skips MCP).");
		return;
	}

	bool enabled = JustAMCPSettingsResolver::resolve_server_enabled();
	int port = JustAMCPSettingsResolver::resolve_server_port();
	bool bind_to_localhost = JustAMCPSettingsResolver::resolve_bool("blazium/justamcp/bind_to_localhost_only", true);

	String cmd_client_id = "";
	String cmd_client_secret = "";

	for (const List<String>::Element *E = args.front(); E; E = E->next()) {
		if (E->get() == "--mcp-client-id" && E->next()) {
			cmd_client_id = E->next()->get();
		}
		if (E->get() == "--mcp-client-secret" && E->next()) {
			cmd_client_secret = E->next()->get();
		}
		if (E->get() == "--mcp-protocol-version" && E->next()) {
			const String cmd_protocol = E->next()->get();
			if (!MCPSessionManager::set_cli_protocol_version_override(cmd_protocol)) {
				ERR_PRINT("JustAMCP: --mcp-protocol-version '" + cmd_protocol + "' is not supported. Keeping setting/default.");
			}
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
		print_line("JustAMCP: Server not started (disabled; pass --enable-mcp or set blazium/justamcp/server_enabled).");
		return;
	}

	const bool oauth_enabled = JustAMCPSettingsResolver::resolve_bool("blazium/justamcp/oauth_enabled", false);
	if (!bind_to_localhost && !oauth_enabled) {
		ERR_PRINT("JustAMCP: Refusing to start server: bind_to_localhost_only is false without OAuth enabled. Enable OAuth or bind to localhost only.");
		return;
	}

#if defined(MODULE_HTTPSERVER_ENABLED)
	if (!HTTPServer::get_singleton()) {
		ERR_PRINT("JustAMCP: HTTPServer singleton is missing; cannot start MCP server.");
		return;
	}

	if (!HTTPServer::get_singleton()->is_listening()) {
		String bind_address = bind_to_localhost ? "127.0.0.1" : "*";
		Error listen_err = OK;
#ifdef TESTS_ENABLED
		if (test_forced_listen_error != OK) {
			listen_err = test_forced_listen_error;
		} else
#endif
		{
			listen_err = HTTPServer::get_singleton()->listen(port, bind_address, false);
		}
		if (listen_err != OK) {
#ifdef TESTS_ENABLED
			if (test_forced_listen_error == OK)
#endif
			{
				ERR_PRINT("JustAMCP: Failed to listen on port " + itos(port) + " (error " + itos(listen_err) + ").");
			}
			return;
		}
	} else {
		print_line("JustAMCP: HTTPServer already listening; registering MCP routes on the existing socket.");
	}

	HTTPServer::get_singleton()->set_cors_enabled(true);
	{
		const String allowed_origin = JustAMCPSettingsResolver::resolve_string("blazium/justamcp/streamable_http_allowed_origin");
		const String cors_origin = justamcp_compute_startup_cors_origin(bind_to_localhost, oauth_enabled, allowed_origin);
		if (!bind_to_localhost && cors_origin.is_empty()) {
			WARN_PRINT("JustAMCP: bind_to_localhost_only is false without OAuth or streamable_http_allowed_origin; CORS Access-Control-Allow-Origin will not be set to *.");
		}
		HTTPServer::get_singleton()->set_cors_origin(cors_origin);
	}

	HTTPServer::get_singleton()->register_route("GET", "/sse", callable_mp(this, &JustAMCPServer::_handle_legacy_sse_connect));
	HTTPServer::get_singleton()->register_route("POST", "/sse", callable_mp(this, &JustAMCPServer::_handle_legacy_sse_connect));
	HTTPServer::get_singleton()->register_route("OPTIONS", "/sse", callable_mp(this, &JustAMCPServer::_handle_cors_preflight));
	HTTPServer::get_singleton()->register_route("POST", "/message", callable_mp(this, &JustAMCPServer::_handle_message_post));
	HTTPServer::get_singleton()->register_route("OPTIONS", "/message", callable_mp(this, &JustAMCPServer::_handle_cors_preflight));
	HTTPServer::get_singleton()->register_route("GET", "/mcp", callable_mp(this, &JustAMCPServer::_handle_mcp_get));
	HTTPServer::get_singleton()->register_route("POST", "/mcp", callable_mp(this, &JustAMCPServer::_handle_mcp_post));
	HTTPServer::get_singleton()->register_route("DELETE", "/mcp", callable_mp(this, &JustAMCPServer::_handle_mcp_delete));
	HTTPServer::get_singleton()->register_route("OPTIONS", "/mcp", callable_mp(this, &JustAMCPServer::_handle_cors_preflight));
	HTTPServer::get_singleton()->register_route("GET", "/.well-known/oauth-protected-resource", callable_mp(this, &JustAMCPServer::_handle_oauth_protected_resource));
	HTTPServer::get_singleton()->register_route("GET", "/.well-known/oauth-protected-resource/mcp", callable_mp(this, &JustAMCPServer::_handle_oauth_protected_resource));
	HTTPServer::get_singleton()->register_route("GET", "/.well-known/oauth-authorization-server", callable_mp(this, &JustAMCPServer::_handle_oauth_authorization_server));
	HTTPServer::get_singleton()->register_route("GET", "/.well-known/openid-configuration", callable_mp(this, &JustAMCPServer::_handle_oauth_authorization_server));
	HTTPServer::get_singleton()->register_route("OPTIONS", "/.well-known/oauth-protected-resource", callable_mp(this, &JustAMCPServer::_handle_cors_preflight));
	HTTPServer::get_singleton()->register_route("OPTIONS", "/.well-known/oauth-protected-resource/mcp", callable_mp(this, &JustAMCPServer::_handle_cors_preflight));
	HTTPServer::get_singleton()->register_route("OPTIONS", "/.well-known/oauth-authorization-server", callable_mp(this, &JustAMCPServer::_handle_cors_preflight));
	HTTPServer::get_singleton()->register_route("OPTIONS", "/.well-known/openid-configuration", callable_mp(this, &JustAMCPServer::_handle_cors_preflight));
#ifdef TOOLS_ENABLED
	HTTPServer::get_singleton()->register_route("GET", "/oauth/callback", callable_mp(this, &JustAMCPServer::_handle_oauth_callback));
	HTTPServer::get_singleton()->register_route("GET", "/oauth/client-metadata.json", callable_mp(this, &JustAMCPServer::_handle_oauth_client_metadata));
	HTTPServer::get_singleton()->register_route("GET", "/mcp-apps/host", callable_mp(this, &JustAMCPServer::_handle_mcp_apps_host));
	HTTPServer::get_singleton()->register_route("POST", "/mcp-apps/proxy", callable_mp(this, &JustAMCPServer::_handle_mcp_apps_proxy));
	HTTPServer::get_singleton()->register_route("OPTIONS", "/oauth/callback", callable_mp(this, &JustAMCPServer::_handle_cors_preflight));
	HTTPServer::get_singleton()->register_route("OPTIONS", "/oauth/client-metadata.json", callable_mp(this, &JustAMCPServer::_handle_cors_preflight));
	HTTPServer::get_singleton()->register_route("OPTIONS", "/mcp-apps/host", callable_mp(this, &JustAMCPServer::_handle_cors_preflight));
	HTTPServer::get_singleton()->register_route("OPTIONS", "/mcp-apps/proxy", callable_mp(this, &JustAMCPServer::_handle_cors_preflight));
#endif

	if (!HTTPServer::get_singleton()->is_connected("sse_connection_opened", callable_mp(this, &JustAMCPServer::_on_sse_connection_opened))) {
		HTTPServer::get_singleton()->connect("sse_connection_opened", callable_mp(this, &JustAMCPServer::_on_sse_connection_opened));
	}
	if (!HTTPServer::get_singleton()->is_connected("sse_connection_closed", callable_mp(this, &JustAMCPServer::_on_sse_connection_closed))) {
		HTTPServer::get_singleton()->connect("sse_connection_closed", callable_mp(this, &JustAMCPServer::_on_sse_connection_closed));
	}

	server_started = true;
	active_listening_port = port;
	print_line("JustAMCP: Server Activated on port " + itos(port));
	emit_signal("server_status_changed", true);
#ifdef TOOLS_ENABLED
	_ensure_headless_tool_executor();
	JustAMCPToolSchemaCache::get_schemas(false, false, false, false);
#endif
#else
	ERR_PRINT("HTTPServer module is not enabled! JustAMCP requires it to act as an MCP server.");
#endif
}

#ifdef TOOLS_ENABLED
void JustAMCPServer::_ensure_headless_tool_executor() {
	if (!JustAMCPCliArgs::is_headless()) {
		return;
	}
	if (!headless_tool_executor) {
		headless_tool_executor = memnew(JustAMCPToolExecutor);
		headless_tool_executor->set_as_active_instance();
	}
	if (!is_connected(SNAME("tool_requested"), callable_mp(this, &JustAMCPServer::_on_headless_tool_requested))) {
		connect(SNAME("tool_requested"), callable_mp(this, &JustAMCPServer::_on_headless_tool_requested));
	}
	if (!is_connected(SNAME("request_cancelled"), callable_mp(this, &JustAMCPServer::_on_request_cancelled))) {
		connect(SNAME("request_cancelled"), callable_mp(this, &JustAMCPServer::_on_request_cancelled));
	}
}

void JustAMCPServer::_on_headless_tool_requested(const Variant &p_request_id, const String &p_tool_name, const Dictionary &p_args) {
	JustAMCPToolDispatch::execute_and_send(this, headless_tool_executor, p_request_id, p_tool_name, p_args);
}
#endif

void JustAMCPServer::_stop_server() {
	if (!server_started) {
		return;
	}

	{
		MutexLock lock(completed_tool_request_mutex);
		completed_tool_request_tombstones.clear();
		completed_tool_request_tombstone_order.clear();
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
#ifdef TOOLS_ENABLED
		HTTPServer::get_singleton()->unregister_route("GET", "/oauth/callback");
		HTTPServer::get_singleton()->unregister_route("GET", "/oauth/client-metadata.json");
		HTTPServer::get_singleton()->unregister_route("GET", "/mcp-apps/host");
		HTTPServer::get_singleton()->unregister_route("POST", "/mcp-apps/proxy");
		HTTPServer::get_singleton()->unregister_route("OPTIONS", "/oauth/callback");
		HTTPServer::get_singleton()->unregister_route("OPTIONS", "/oauth/client-metadata.json");
		HTTPServer::get_singleton()->unregister_route("OPTIONS", "/mcp-apps/host");
		HTTPServer::get_singleton()->unregister_route("OPTIONS", "/mcp-apps/proxy");
#endif
		if (session_manager) {
			session_manager->clear_all();
		}
	}
#endif
	server_started = false;
	active_listening_port = -1;
#if defined(MODULE_HTTPSERVER_ENABLED)
	_clear_tool_queue();
#endif
	print_line("JustAMCP: Server Disabled.");
	emit_signal("server_status_changed", false);
	current_sse_connection_id = -1;
}
