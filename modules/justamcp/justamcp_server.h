/**************************************************************************/
/*  justamcp_server.h                                                     */
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

#pragma once

#include "modules/modules_enabled.gen.h"
#include "scene/main/node.h"

#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"

#if defined(MODULE_HTTPSERVER_ENABLED)
#include "core/os/semaphore.h"
#include "modules/httpserver/http_request_context.h"
#include "modules/httpserver/http_response.h"
#include "modules/httpserver/http_server.h"
#endif

#include "core/string/print_string.h"
#include "justamcp_notification_bus.h"
#include "mcp_tool_queue.h"
#include "mcp_tool_queue_entry.h"

class JustAMCPNotificationBus;

class MCPSessionManager;

struct JustAMCPActiveProgressContext {
	String task_id;
	Variant request_id;
	String session_id;
	int connection_id = -1;
	uint64_t last_emit_usec = 0;
};

class JustAMCPServer : public Node {
	GDCLASS(JustAMCPServer, Node);

	friend class MCPSessionManager;
	friend class JustAMCPJsonRpcTransport;
	friend class JustAMCPJsonRpcRouter;
	friend class JustAMCPNotificationBus;

private:
	MCPSessionManager *session_manager = nullptr;
	JustAMCPNotificationBus notification_bus;
	String transport_negotiated_protocol = "2025-11-25";
	MCPToolQueue mcp_tool_queue;

	Mutex routing_mutex;
	HashMap<String, JustAMCPActiveProgressContext> active_progress_tokens;
	HashMap<String, JustAMCPActiveProgressContext> active_task_routes;

	int current_sse_connection_id = -1;
	bool server_started = false;
	int active_listening_port = -1;
	HashMap<String, Vector<uint64_t>> session_enqueue_timestamps_usec;
	Mutex session_enqueue_rate_mutex;

	HashSet<String> completed_tool_request_tombstones;
	Vector<String> completed_tool_request_tombstone_order;
	static const int COMPLETED_TOOL_REQUEST_TOMBSTONE_MAX = 2048;
	mutable Mutex completed_tool_request_mutex;
#ifdef TESTS_ENABLED
	mutable Dictionary test_last_send_tool_result;
#endif
	String minimum_log_level = "info";
	Mutex minimum_log_level_mutex;

	static const int LOG_RATE_LIMIT_PER_SEC = 50;
	uint64_t log_rate_window_start_usec = 0;
	int log_rate_count = 0;
	Mutex log_rate_mutex;

	class JustAMCPPromptExecutor *prompt_executor = nullptr;
	class JustAMCPResourceExecutor *resource_executor = nullptr;
	class JustAMCPTaskManager *task_manager = nullptr;
#ifdef TOOLS_ENABLED
	class JustAMCPToolExecutor *headless_tool_executor = nullptr;
	void _on_headless_tool_requested(const Variant &p_request_id, const String &p_tool_name, const Dictionary &p_args);
	void _ensure_headless_tool_executor();
#endif
	HashSet<String> subscribed_resources;
	mutable Mutex subscribed_resources_mutex;

	void _setup_settings();
	void _start_server();
	void _stop_server();
	void _on_settings_changed();
	int _resolve_listening_port_from_settings() const;

	static JustAMCPServer *singleton;
	Vector<String> engine_logs;
	Mutex engine_logs_mutex;
	Vector<Dictionary> mcp_notification_log;
	Mutex mcp_notification_log_mutex;
	PrintHandlerList print_handler;
	static void _print_handler_callback(void *p_user_data, const String &p_string, bool p_error, bool p_rich);

#if defined(MODULE_HTTPSERVER_ENABLED)
	void _handle_legacy_sse_connect(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response);
	void _handle_cors_preflight(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response);
	void _handle_message_post(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response);
	void _handle_mcp_get(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response);
	void _handle_mcp_post(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response);
	void _handle_mcp_delete(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response);
	void _handle_mcp_stateless_post(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response);
	Dictionary _handle_json_rpc(const String &p_body, Ref<HTTPResponse> p_response);
	Dictionary _transport_handle_json_rpc(const String &p_body, Ref<HTTPResponse> p_response);
	void _send_sse_message(const String &p_json_string);
	void _send_sse_routed(const String &p_json_string, const String &p_session_id, int p_connection_id);
	void _on_sse_connection_opened(int p_connection_id, const String &p_path, const Dictionary &p_headers);
	void _on_sse_connection_closed(int p_connection_id);
	void _deferred_post_sse_json_rpc(int p_connection_id, const String &p_session_id, const Dictionary &p_payload);
	void _deferred_legacy_message_json_rpc(const String &p_body, const String &p_session_id_param);
	void _deferred_held_json_rpc(int p_client_id, const String &p_body, const String &p_session_id, const Ref<HTTPResponse> &p_response);
	void _deferred_sse_replay(int p_connection_id, const String &p_last_event_id);
	bool _validate_mcp_oauth(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response);
	MCPToolQueueEntry *_enqueue_tool_request(const Variant &p_request_id, const String &p_tool_name, const Dictionary &p_args, Ref<HTTPResponse> p_response, Dictionary &r_queue_full_error, const Dictionary &p_options = Dictionary());
	void _dispatch_task_augmented_tools_call(const Variant &p_request_id);
	void _process_pending_tools();
	void _schedule_process_pending_tools();
	void _on_pending_tools_process_frame();
	bool pending_tools_drain_scheduled = false;
	void _fail_and_remove_task_dispatch_entry(MCPToolQueueEntry *p_entry);
	void _complete_tool_entry(MCPToolQueueEntry *p_entry, const Dictionary &p_rpc_result);
	void _complete_current_tool_request(const Dictionary &p_rpc_result);
	void _complete_task_tool_entry(MCPToolQueueEntry *p_entry, bool p_success, const Variant &p_result, const String &p_error);
	void _insert_tool_result_tombstone(const String &p_tombstone_key);
	bool _has_tool_result_tombstone(const String &p_tombstone_key) const;
	void _enforce_in_flight_cancel_deadline(const Variant &p_request_id);
	void _deferred_complete_tool_dict(const Variant &p_request_id, const Dictionary &p_result);
	Dictionary _format_tool_result_dict(bool p_success, const Variant &p_result, const String &p_error) const;
	Dictionary _build_create_task_result(const String &p_task_id) const;
	void _register_progress_token(const String &p_token, const String &p_task_id, const Variant &p_request_id);
	void _unregister_progress_token(const String &p_token);
	void _register_task_route(const String &p_task_id, const String &p_session_id, int p_connection_id);
	void _unregister_task_route(const String &p_task_id);
	void _clear_tool_queue();
	bool _wait_for_stateless_tool_entry(MCPToolQueueEntry *p_entry, int p_timeout_ms);
	Dictionary _stateless_tool_timeout_error(const Variant &p_request_id) const;
#endif

	void _append_mcp_notification_log(const String &p_level, const String &p_logger, const Dictionary &p_data);
	void _mcp_debug_log(const String &p_message);
	void _emit_log_notification_deferred(const String &p_level, const String &p_logger, const Dictionary &p_data);
	bool _should_emit_log(const String &p_level);
	bool _try_consume_log_rate_limit();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void send_tool_result(const Variant &p_request_id, bool p_success, const Variant &p_result = Variant(), const String &p_error = "");
	void send_elicitation_request(const String &p_request_id, const String &p_mode, const String &p_message, const Variant &p_url_or_schema);
	void send_url_elicitation_error(const String &p_request_id, const String &p_elicitation_id, const String &p_url, const String &p_message);
	void broadcast_prompts_list_changed();
	void broadcast_tools_list_changed();
	void broadcast_resources_list_changed();
	void broadcast_resource_updated(const String &p_uri);
	void subscribe_resource(const String &p_uri);
	void unsubscribe_resource(const String &p_uri);
	bool is_resource_subscribed(const String &p_uri) const;
	void send_log_message(const String &p_level, const String &p_logger, const Variant &p_data = Variant());
	void send_progress_notification(const String &p_token, double p_progress, double p_total, const String &p_message);
	void report_tool_progress(const String &p_token, double p_progress, double p_total, const String &p_message);
	void broadcast_task_status(const String &p_task_id);
	void _on_request_cancelled(const Variant &p_request_id, const String &p_reason, const String &p_caller_session_id = String());
	bool is_current_tool_cancel_requested() const;
	bool is_tool_cancel_requested(const Variant &p_request_id) const;
	String get_current_progress_token() const;
	String get_tool_progress_token(const Variant &p_request_id) const;
	String get_current_task_id() const;
	bool is_task_cancel_requested(const String &p_task_id) const;
	void request_task_queue_cancel(const String &p_task_id);

	static JustAMCPServer *get_singleton();
	class JustAMCPResourceExecutor *get_resource_executor() const { return resource_executor; }
	Vector<String> get_engine_logs();
	Dictionary get_engine_logs_page(const String &p_cursor);
	Dictionary get_mcp_notification_log_page(const String &p_cursor);

#ifdef TESTS_ENABLED
	MCPToolQueueEntry *test_enqueue_tool_request(const Variant &p_request_id, const String &p_tool_name, const Dictionary &p_args, Dictionary &r_queue_full_error);
	void test_set_in_flight_entries(MCPToolQueueEntry *p_write, MCPToolQueueEntry *p_readonly);
	bool test_get_tool_queue_processing() const;
	void test_stop_server();
	void test_clear_tool_queue();
	MCPToolQueueEntry *test_get_in_flight_write() const;
	MCPToolQueueEntry *test_get_in_flight_readonly() const;
	static int test_tool_queue_max() { return MCPToolQueue::max_size(); }
	void test_dispatch_task_augmented_tools_call(const Variant &p_request_id) { _dispatch_task_augmented_tools_call(p_request_id); }
	void test_process_pending_tools() { _process_pending_tools(); }
	void test_enforce_in_flight_cancel_deadline(const Variant &p_request_id) { _enforce_in_flight_cancel_deadline(p_request_id); }
	void test_complete_current_tool_request(const Dictionary &p_rpc_result) { _complete_current_tool_request(p_rpc_result); }
	void test_insert_tool_result_tombstone(const String &p_tombstone_key) { _insert_tool_result_tombstone(p_tombstone_key); }
	void test_register_task_route(const String &p_task_id, const String &p_session_id, int p_connection_id);
	bool test_get_active_task_route(const String &p_task_id, String &r_session_id, int &r_connection_id) const;
	void test_set_current_sse_connection_id(int p_connection_id);
	int test_count_notification_broadcast_targets() const;
	MCPSessionManager *test_get_session_manager() const;
	Dictionary test_peek_last_send_tool_result() const;
	void test_clear_last_send_tool_result();
	String test_get_transport_negotiated_protocol() const { return transport_negotiated_protocol; }
	void test_set_transport_negotiated_protocol(const String &p_protocol) { transport_negotiated_protocol = p_protocol; }
	bool test_validate_mcp_oauth(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response);
	void test_handle_mcp_stateless_post(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response);
	void test_handle_message_post(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response);
#endif

	bool is_server_started() const { return server_started; }
	int get_listening_port() const { return active_listening_port; }
	int get_pending_tool_queue_size();

	JustAMCPServer();
	~JustAMCPServer();
};
