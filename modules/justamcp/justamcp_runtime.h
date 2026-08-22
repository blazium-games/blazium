/**************************************************************************/
/*  justamcp_runtime.h                                                    */
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

#include "core/io/stream_peer_tcp.h"
#include "core/io/tcp_server.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/os/mutex.h"
#include "core/os/thread.h"
#include <atomic>

class JustAMCPRuntime : public Object {
	GDCLASS(JustAMCPRuntime, Object);

private:
	static JustAMCPRuntime *singleton;

	Ref<TCPServer> server;
	Vector<Ref<StreamPeerTCP>> clients;
	HashMap<Ref<StreamPeerTCP>, String> client_buffers;
	Mutex clients_mutex;
	int port = 6506;
	bool enabled = false;
	Thread *server_thread = nullptr;
	std::atomic<bool> quit_thread{ false };
	void _thread_poll();
	static void _thread_poll_wrapper(void *p_user);
	HashMap<String, Callable> watched_signals;
	HashMap<String, Callable> _custom_commands;

	static const int _SCREENSHOT_RATE_MAX = 10;
	static const int _SCREENSHOT_RATE_WINDOW_MS = 1000;
	Vector<uint64_t> _screenshot_timestamps;

	class JustAMCPToolExecutor *executor = nullptr;

	Vector<Dictionary> _error_log;
	Mutex _error_log_mutex;
	PrintHandlerList _print_handler;
	static void _print_handler_callback(void *p_user_data, const String &p_string, bool p_error, bool p_rich);

	static const char *_BLOCKED_METHODS[];
	static const char *_EVAL_BLOCKED_PATTERNS[];

	void _start_server();
	void _send_welcome(Ref<StreamPeerTCP> p_client);
	void _handle_message(Ref<StreamPeerTCP> p_client, const String &p_data);
	void _deferred_execute_command(Ref<StreamPeerTCP> p_client, String p_command, Dictionary p_params, Variant p_request_id);

public:
	Dictionary execute_command(const String &p_command, const Dictionary &p_params);

private:
	Dictionary _cmd_get_tree(const Dictionary &p_params);
	Dictionary _cmd_get_node(const Dictionary &p_params);
	Dictionary _cmd_set_property(const Dictionary &p_params);
	Dictionary _cmd_call_method(const Dictionary &p_params);
	Dictionary _cmd_get_metrics(const Dictionary &p_params);

	Dictionary _cmd_capture_screenshot(const Dictionary &p_params);
	Dictionary _cmd_capture_viewport(const Dictionary &p_params);
	Dictionary _cmd_inject_action(const Dictionary &p_params);
	Dictionary _cmd_inject_key(const Dictionary &p_params);
	Dictionary _cmd_inject_mouse_click(const Dictionary &p_params);
	Dictionary _cmd_inject_mouse_motion(const Dictionary &p_params);
	Dictionary _cmd_watch_signal(const Dictionary &p_params);
	Dictionary _cmd_unwatch_signal(const Dictionary &p_params);

	Dictionary _cmd_inject_drag(const Dictionary &p_params);
	Dictionary _cmd_inject_scroll(const Dictionary &p_params);
	Dictionary _cmd_inject_gesture(const Dictionary &p_params);
	Dictionary _cmd_find_nodes(const Dictionary &p_params);
	Dictionary _cmd_get_node_property(const Dictionary &p_params);
	Dictionary _cmd_call_node_method(const Dictionary &p_params);
	Dictionary _cmd_press_button(const Dictionary &p_params);
	Dictionary _cmd_wait_for_property(const Dictionary &p_params);
	Dictionary _cmd_runtime_info(const Dictionary &p_params);
	Dictionary _cmd_runtime_get_errors(const Dictionary &p_params);
	Dictionary _cmd_runtime_capabilities(const Dictionary &p_params);
	Dictionary _cmd_eval_expression(const Dictionary &p_params);
	Dictionary _cmd_set_paused(const Dictionary &p_params);
	Dictionary _cmd_runtime_quit(const Dictionary &p_params);
	Dictionary _cmd_get_network_info(const Dictionary &p_params);
	Dictionary _cmd_get_audio_info(const Dictionary &p_params);
	Dictionary _cmd_inject_gamepad(const Dictionary &p_params);
	Dictionary _cmd_run_custom_command(const Dictionary &p_params);
	Dictionary _cmd_get_autoload(const Dictionary &p_params);
	Dictionary _cmd_find_nodes_by_script(const Dictionary &p_params);
	Dictionary _cmd_batch_get_properties(const Dictionary &p_params);
	Dictionary _cmd_find_ui_elements(const Dictionary &p_params);
	Dictionary _cmd_click_button_by_text(const Dictionary &p_params);
	Dictionary _cmd_move_node(const Dictionary &p_params);
	Dictionary _cmd_monitor_properties(const Dictionary &p_params);

	void _find_nodes_recursive(Node *p_node, const String &p_name, const String &p_type, const String &p_group, int p_limit, Array &r_results);
	void _find_nodes_by_script_recursive(Node *p_node, const String &p_script_path, const String &p_class_name, int p_limit, Array &r_results);
	void _find_ui_elements_recursive(Node *p_node, const String &p_text, const String &p_type, bool p_visible_only, int p_limit, Array &r_results);
	Node *_find_button_by_text_recursive(Node *p_node, const String &p_text);
	Node *_find_button_recursive(Node *p_node, const String &p_name);
	void _inject_event(const Ref<InputEvent> &p_event);

	void _broadcast_signal_event(const String &p_node_path, const String &p_signal_name, const Array &p_args);
	void _quit_engine();
	Dictionary _cmd_auth_info(const Dictionary &p_params);
	Dictionary _cmd_tool_call(const Dictionary &p_params);
	Dictionary _cmd_list_tools(const Dictionary &p_params);
	Dictionary _cmd_diagnose(const Dictionary &p_params);
	Dictionary _cmd_get_log_tail(const Dictionary &p_params);
	Dictionary _cmd_tags_get_on_asset(const Dictionary &p_params);
	Dictionary _cmd_tags_list(const Dictionary &p_params);
	Dictionary _cmd_tags_find_assets(const Dictionary &p_params);

	Dictionary _serialize_node_tree(Node *p_node, int p_depth, int p_max_depth, bool p_include_properties);
	Dictionary _serialize_node(Node *p_node, bool p_include_properties);
	Variant _serialize_value(const Variant &p_value);
	Variant _deserialize_value(const Variant &p_value);

	void _send_response(Ref<StreamPeerTCP> p_client, const Dictionary &p_data);
	void _send_error(Ref<StreamPeerTCP> p_client, const String &p_message);
	void _cleanup();

protected:
	static void _bind_methods();
	void poll();

public:
	static JustAMCPRuntime *get_singleton();

	void set_port(int p_port);
	int get_port() const;

	void set_enabled(bool p_enabled);
	bool is_enabled() const;

	void push_error_log(const String &p_message, bool p_is_error);

	void register_custom_command(const String &p_name, const Callable &p_callable);
	void unregister_custom_command(const String &p_name);

	JustAMCPRuntime();
	~JustAMCPRuntime();
};
