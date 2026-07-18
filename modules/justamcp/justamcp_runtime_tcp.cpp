/**************************************************************************/
/*  justamcp_runtime_tcp.cpp                                              */
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

#include "justamcp_runtime.h"

#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "core/object/message_queue.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "tools/justamcp_tool_executor.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_settings.h"
#endif

void JustAMCPRuntime::_thread_poll_wrapper(void *p_user) {
	JustAMCPRuntime *runtime = (JustAMCPRuntime *)p_user;
	if (runtime) {
		runtime->_thread_poll();
	}
}

void JustAMCPRuntime::_thread_poll() {
	while (!quit_thread) {
		poll();
		OS::get_singleton()->delay_usec(16000);
	}
}

void JustAMCPRuntime::poll() {
	if (!enabled || server.is_null()) {
		return;
	}

	Vector<Ref<StreamPeerTCP>> accepted_clients;
	while (server->is_connection_available()) {
		Ref<StreamPeerTCP> client = server->take_connection();
		if (client.is_valid()) {
			accepted_clients.push_back(client);
		} else {
			break;
		}
	}

	struct PendingMessage {
		Ref<StreamPeerTCP> client;
		String message;
	};
	Vector<PendingMessage> pending_messages;
	Vector<Ref<StreamPeerTCP>> clients_to_remove;
	{
		MutexLock lock(clients_mutex);

		for (int i = 0; i < accepted_clients.size(); i++) {
			Ref<StreamPeerTCP> client = accepted_clients[i];
			clients.push_back(client);
			print_line("[MCP Runtime] Client connected");
			call_deferred("emit_signal", "client_connected");
			_send_welcome(client);
		}

		for (int i = 0; i < clients.size(); i++) {
			Ref<StreamPeerTCP> client = clients[i];
			if (client->get_status() != StreamPeerTCP::STATUS_CONNECTED) {
				clients_to_remove.push_back(client);
				continue;
			}

			client->poll();
			int available = client->get_available_bytes();
			if (available > 0) {
				String msg_data = client->get_utf8_string(available);
				String &buf = client_buffers[client];
				buf += msg_data;

				if (buf.length() > 16777216) {
					client->disconnect_from_host();
					clients_to_remove.push_back(client);
					continue;
				}

				int newline_pos = buf.find("\n");
				while (newline_pos != -1) {
					String complete_msg = buf.substr(0, newline_pos);
					buf = buf.substr(newline_pos + 1);
					if (!complete_msg.is_empty()) {
						PendingMessage pending;
						pending.client = client;
						pending.message = complete_msg;
						pending_messages.push_back(pending);
					}
					newline_pos = buf.find("\n");
				}
			}
		}

		for (int i = 0; i < clients_to_remove.size(); i++) {
			Ref<StreamPeerTCP> c = clients_to_remove[i];
			clients.erase(c);
			client_buffers.erase(c);
			print_line("[MCP Runtime] Client disconnected");
			call_deferred("emit_signal", "client_disconnected");
		}
	}

	for (int i = 0; i < pending_messages.size(); i++) {
		_handle_message(pending_messages[i].client, pending_messages[i].message);
	}
}

void JustAMCPRuntime::set_port(int p_port) {
	port = p_port;
	if (enabled) {
		_start_server();
	}
}

int JustAMCPRuntime::get_port() const {
	return port;
}

void JustAMCPRuntime::set_enabled(bool p_enabled) {
	ERR_FAIL_COND_MSG(this != get_singleton(), "JustAMCPRuntime: only the module singleton may enable/disable the runtime server.");
	enabled = p_enabled;
	if (enabled) {
		_start_server();
	} else if (!enabled) {
		_cleanup();
	}
}

bool JustAMCPRuntime::is_enabled() const {
	return enabled;
}

void JustAMCPRuntime::_print_handler_callback(void *p_user_data, const String &p_string, bool p_error, bool p_rich) {
	JustAMCPRuntime *runtime = static_cast<JustAMCPRuntime *>(p_user_data);
	if (runtime) {
		runtime->push_error_log(p_string, p_error);
	}
}

void JustAMCPRuntime::push_error_log(const String &p_message, bool p_is_error) {
	MutexLock lock(_error_log_mutex);
	Dictionary entry;
	entry["message"] = p_message;
	if (p_is_error) {
		entry["type"] = "error";
	} else if (p_message.begins_with("WARNING:")) {
		entry["type"] = "warning";
	} else {
		entry["type"] = "log";
	}
	entry["timestamp"] = Time::get_singleton()->get_unix_time_from_system();
	_error_log.push_back(entry);

	if (_error_log.size() > 500) {
		_error_log.remove_at(0);
	}
}

void JustAMCPRuntime::_start_server() {
	const List<String> &args = OS::get_singleton()->get_cmdline_args();
	for (const String &arg : args) {
		if (arg == "--test" || arg == "--tests" || arg.begins_with("--aw-") ||
				arg == "--help" || arg == "-h" || arg == "/?" || arg == "--version" ||
				arg == "--check-only" || arg.begins_with("--export")) {
			return;
		}
	}

	bool mcp_disabled = false;
	for (const String &E : args) {
		if (E == "--disable-game-mcp") {
			mcp_disabled = true;
			break;
		}
	}
	if (mcp_disabled) {
		return;
	}

#ifdef TOOLS_ENABLED
	bool is_headless = false;
	for (const String &E : args) {
		if (E == "--headless") {
			is_headless = true;
			break;
		}
	}

	bool use_project_override = GLOBAL_GET("blazium/justamcp/override_editor_settings");

	if (is_headless) {
		use_project_override = true;
	}

	if (use_project_override || !EditorSettings::get_singleton()) {
		enabled = GLOBAL_GET("blazium/justamcp/server_enabled");
		port = GLOBAL_GET("blazium/justamcp/server_port");
	} else {
		if (EditorSettings::get_singleton()->has_setting("blazium/justamcp/server_enabled")) {
			enabled = EditorSettings::get_singleton()->get_setting("blazium/justamcp/server_enabled");
		}
		if (EditorSettings::get_singleton()->has_setting("blazium/justamcp/server_port")) {
			port = EditorSettings::get_singleton()->get_setting("blazium/justamcp/server_port");
		}
	}
#else

	if (ProjectSettings::get_singleton()->has_setting("blazium/justamcp/disable_game_mcp")) {
		if ((bool)GLOBAL_GET("blazium/justamcp/disable_game_mcp")) {
			return;
		}
	}

	bool game_control_enabled = false;
	if (ProjectSettings::get_singleton()->has_setting("blazium/justamcp/game_control_enabled")) {
		game_control_enabled = GLOBAL_GET("blazium/justamcp/game_control_enabled");
	}
	if (!game_control_enabled && !args.find("--enable-mcp-game-control")) {
		return;
	}

	enabled = GLOBAL_GET("blazium/justamcp/server_enabled");
	port = GLOBAL_GET("blazium/justamcp/server_port");
#endif

	if (OS::get_singleton()->get_cmdline_args().find("--enable-mcp")) {
		enabled = true;
	}
	if (OS::get_singleton()->get_cmdline_args().find("--enable-mcp-game-control")) {
		enabled = true;
	}

	if (!enabled) {
		return;
	}

	if (server.is_valid() && server->is_listening()) {
		server->stop();
	}

	bool bind_to_localhost = true;
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/justamcp/bind_to_localhost_only")) {
		bind_to_localhost = GLOBAL_GET("blazium/justamcp/bind_to_localhost_only");
	}
	String bind_address = bind_to_localhost ? "127.0.0.1" : "*";

	Error err = server->listen(port, bind_address);
	if (err != OK) {
		ERR_PRINT(vformat("[MCP Runtime] Failed to start server on port %d: %d", port, err));
		enabled = false;
	} else {
		print_line(vformat("[MCP Runtime] Server listening on port %d", port));
		print_line(vformat("BLAZIUM_READY:{\"proto\":\"blazium/1\",\"port\":%d,\"engine\":\"Blazium\"}", port));
		quit_thread = false;
		if (!server_thread) {
			server_thread = memnew(Thread);
		}
		if (!server_thread->is_started()) {
			server_thread->start(_thread_poll_wrapper, this);
		}
	}
}

void JustAMCPRuntime::_cleanup() {
	quit_thread = true;
	if (server_thread) {
		if (server_thread->is_started()) {
			server_thread->wait_to_finish();
		}
		memdelete(server_thread);
		server_thread = nullptr;
	}

	if (executor) {
		memdelete(executor);
		executor = nullptr;
	}

	{
		MutexLock lock(clients_mutex);
		for (int i = 0; i < clients.size(); i++) {
			if (clients[i].is_valid()) {
				clients[i]->disconnect_from_host();
			}
		}
		clients.clear();
		client_buffers.clear();
	}

	if (server.is_valid()) {
		server->stop();
	}
}

void JustAMCPRuntime::_send_welcome(Ref<StreamPeerTCP> p_client) {
	Dictionary welcome;
	welcome["type"] = "welcome";
	welcome["engine"] = "Blazium JustAMCP";
	welcome["protocol_version"] = "1.0";
	welcome["godot_version"] = Engine::get_singleton()->get_version_info();
	welcome["project_name"] = ProjectSettings::get_singleton()->get_setting("application/config/name", "Unknown");
	_send_response(p_client, welcome);
}

void JustAMCPRuntime::_send_response(Ref<StreamPeerTCP> p_client, const Dictionary &p_data) {
	if (p_client.is_valid() && p_client->get_status() == StreamPeerTCP::STATUS_CONNECTED) {
		String json_str = JSON::stringify(p_data) + "\n";
		p_client->put_utf8_string(json_str);
	}
}

void JustAMCPRuntime::_send_error(Ref<StreamPeerTCP> p_client, const String &p_message) {
	Dictionary err;
	err["type"] = "error";
	err["message"] = p_message;
	_send_response(p_client, err);
}

void JustAMCPRuntime::_handle_message(Ref<StreamPeerTCP> p_client, const String &p_data) {
	Ref<JSON> json;
	json.instantiate();
	Error err = json->parse(p_data);
	if (err != OK) {
		_send_error(p_client, "Invalid JSON: " + json->get_error_message());
		return;
	}

	Variant result = json->get_data();
	if (result.get_type() != Variant::DICTIONARY) {
		_send_error(p_client, "Message must be an object");
		return;
	}

	Dictionary message = result;
	String command = message.get("command", "");
	Dictionary params = message.get("params", Dictionary());
	Variant request_id = message.get("id", Variant());

	call_deferred("emit_signal", "command_received", command, params);

	MessageQueue::get_singleton()->push_callable(callable_mp(this, &JustAMCPRuntime::_deferred_execute_command), p_client, command, params, request_id);
}

void JustAMCPRuntime::_deferred_execute_command(Ref<StreamPeerTCP> p_client, String p_command, Dictionary p_params, Variant p_request_id) {
	Dictionary response_data = execute_command(p_command, p_params);
	if (p_request_id.get_type() != Variant::NIL && p_request_id.get_type() != Variant::OBJECT) {
		response_data["id"] = p_request_id;
	}
	_send_response(p_client, response_data);
}
