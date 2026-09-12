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

#include "justamcp_cli_args.h"
#include "justamcp_server.h"
#include "tools/justamcp_settings_resolver.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "core/object/message_queue.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#ifdef TOOLS_ENABLED
#include "tools/justamcp_tool_executor.h"
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
			_debug_print("[MCP Runtime] Client connected");
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
			_debug_print("[MCP Runtime] Client disconnected");
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

static void _justamcp_load_project_scripts_idle() {
	if (JustAMCPRuntime *runtime = JustAMCPRuntime::get_singleton()) {
		runtime->load_project_mcp_scripts();
	}
}

void JustAMCPRuntime::_start_server() {
	if (JustAMCPCliArgs::skip_mcp_server()) {
		return;
	}
	if (!JustAMCPSettingsResolver::resolve_runtime_enabled()) {
		return;
	}

	port = JustAMCPSettingsResolver::resolve_runtime_port();
	if (JustAMCPSettingsResolver::runtime_port_conflicts_with_editor()) {
		ERR_PRINT(vformat("[MCP Runtime] Game port %d equals the editor MCP port. Set blazium/justamcp/export_port or --mcp-game-port.", port));
		return;
	}
	enabled = true;

	bool start_http_host = true;
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton() && Engine::get_singleton()->is_editor_hint()) {
		start_http_host = false;
	}
#endif
	if (start_http_host) {
		if (!http_host) {
			http_host = memnew(JustAMCPServer);
			http_host->set_runtime_host(true);
		}
		http_host->start_listening();
		if (http_host->is_server_started() && http_host->get_listening_port() > 0) {
			port = http_host->get_listening_port();
		}
		if (Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop())) {
			load_project_mcp_scripts();
		} else {
			SceneTree::add_idle_callback(_justamcp_load_project_scripts_idle);
		}
	}

	if (server.is_valid() && server->is_listening()) {
		server->stop();
	}

	const bool bind_to_localhost = JustAMCPSettingsResolver::resolve_bool("blazium/justamcp/bind_to_localhost_only", true);
	String bind_address = bind_to_localhost ? "127.0.0.1" : "*";

	int tcp_port = port;
	if (http_host && http_host->is_server_started()) {
		tcp_port = port + 1;
		if (tcp_port == JustAMCPSettingsResolver::resolve_server_port()) {
			tcp_port++;
		}
	}

	Error err = server->listen(tcp_port, bind_address);
	if (err != OK) {
		const int retry_port = tcp_port + 1;
		if (retry_port != JustAMCPSettingsResolver::resolve_server_port()) {
			err = server->listen(retry_port, bind_address);
			if (err == OK) {
				tcp_port = retry_port;
			}
		}
	}
	if (err != OK) {
		ERR_PRINT(vformat("[MCP Runtime] Failed to start TCP bridge on port %d: %d", tcp_port, err));
		if (!http_host || !http_host->is_server_started()) {
			enabled = false;
		}
	} else {
		_debug_print(vformat("[MCP Runtime] TCP bridge listening on port %d", tcp_port));
		if (!http_host || !http_host->is_server_started()) {
			print_line(vformat("BLAZIUM_READY:{\"proto\":\"blazium/1\",\"port\":%d,\"engine\":\"Blazium\"}", tcp_port));
		}
		quit_thread = false;
		if (!server_thread) {
			server_thread = memnew(Thread);
		}
		if (!server_thread->is_started()) {
			server_thread->start(_thread_poll_wrapper, this);
		}
	}
	if (http_host && http_host->is_server_started()) {
		_debug_print(vformat("[MCP Runtime] HTTP MCP host listening on port %d", port));
		print_line(vformat("BLAZIUM_READY:{\"proto\":\"blazium/1\",\"port\":%d,\"engine\":\"Blazium\",\"mcp\":\"http://127.0.0.1:%d/mcp\"}", port, port));
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

#ifdef TOOLS_ENABLED
	if (executor) {
		memdelete(executor);
		executor = nullptr;
	}
#endif
	if (http_host) {
		memdelete(http_host);
		http_host = nullptr;
	}
	for (int i = 0; i < project_mcp_instances.size(); i++) {
		if (Object *inst = project_mcp_instances[i]) {
			if (Node *node = Object::cast_to<Node>(inst)) {
				if (node->is_inside_tree()) {
					node->queue_free();
					continue;
				}
			}
			memdelete(inst);
		}
	}
	project_mcp_instances.clear();
	project_scripts_loaded = false;

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
