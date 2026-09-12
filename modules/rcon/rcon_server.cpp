/**************************************************************************/
/*  rcon_server.cpp                                                       */
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

#include "rcon_server.h"

#include "core/os/os.h"
#include "rcon_packet.h"

void RCONServer::_bind_methods() {
	// High-level API
	ClassDB::bind_method(D_METHOD("start_server", "port", "password", "protocol"), &RCONServer::start_server);
	ClassDB::bind_method(D_METHOD("stop_server"), &RCONServer::stop_server);
	ClassDB::bind_method(D_METHOD("register_command", "command", "callback"), &RCONServer::register_command);
	ClassDB::bind_method(D_METHOD("unregister_command", "command"), &RCONServer::unregister_command);
	ClassDB::bind_method(D_METHOD("send_response", "client_id", "request_id", "response"), &RCONServer::send_response);

	// Low-level API
	ClassDB::bind_method(D_METHOD("send_raw_packet", "client_id", "packet"), &RCONServer::send_raw_packet);

	// Status
	ClassDB::bind_method(D_METHOD("is_running"), &RCONServer::is_running);
	ClassDB::bind_method(D_METHOD("get_state"), &RCONServer::get_state);
	ClassDB::bind_method(D_METHOD("get_protocol"), &RCONServer::get_protocol);
	ClassDB::bind_method(D_METHOD("get_connected_clients"), &RCONServer::get_connected_clients);

	// Polling
	ClassDB::bind_method(D_METHOD("poll"), &RCONServer::poll);

	// Signals - Connection lifecycle
	ADD_SIGNAL(MethodInfo("server_started"));
	ADD_SIGNAL(MethodInfo("server_stopped"));
	ADD_SIGNAL(MethodInfo("client_connected", PropertyInfo(Variant::INT, "client_id"), PropertyInfo(Variant::STRING, "address")));
	ADD_SIGNAL(MethodInfo("client_disconnected", PropertyInfo(Variant::INT, "client_id")));
	ADD_SIGNAL(MethodInfo("client_authenticated", PropertyInfo(Variant::INT, "client_id")));
	ADD_SIGNAL(MethodInfo("authentication_failed", PropertyInfo(Variant::INT, "client_id"), PropertyInfo(Variant::STRING, "address")));

	// Signals - Command handling
	ADD_SIGNAL(MethodInfo("command_received", PropertyInfo(Variant::INT, "client_id"), PropertyInfo(Variant::STRING, "command"), PropertyInfo(Variant::INT, "request_id")));

	// Signals - Low-level packet events
	ADD_SIGNAL(MethodInfo("raw_packet_received", PropertyInfo(Variant::INT, "client_id"), PropertyInfo(Variant::PACKED_BYTE_ARRAY, "packet")));
	ADD_SIGNAL(MethodInfo("packet_sent", PropertyInfo(Variant::INT, "client_id"), PropertyInfo(Variant::PACKED_BYTE_ARRAY, "packet")));
	ADD_SIGNAL(MethodInfo("packet_send_failed", PropertyInfo(Variant::INT, "client_id"), PropertyInfo(Variant::STRING, "error")));

	// Signals - Keep-alive monitoring
	ADD_SIGNAL(MethodInfo("keep_alive_sent", PropertyInfo(Variant::INT, "client_id")));
	ADD_SIGNAL(MethodInfo("keep_alive_timeout", PropertyInfo(Variant::INT, "client_id")));
	ADD_SIGNAL(MethodInfo("client_timeout_warning", PropertyInfo(Variant::INT, "client_id"), PropertyInfo(Variant::INT, "seconds_remaining")));

	// Signals - Errors
	ADD_SIGNAL(MethodInfo("server_error", PropertyInfo(Variant::STRING, "error")));

	// Enums
	BIND_ENUM_CONSTANT(PROTOCOL_SOURCE);
	BIND_ENUM_CONSTANT(PROTOCOL_BATTLEYE);

	BIND_ENUM_CONSTANT(STATE_STOPPED);
	BIND_ENUM_CONSTANT(STATE_STARTING);
	BIND_ENUM_CONSTANT(STATE_LISTENING);
	BIND_ENUM_CONSTANT(STATE_ERROR);
}

Error RCONServer::start_server(int p_port, const String &p_password, Protocol p_protocol) {
	MutexLock lock(mutex);

	if (state != STATE_STOPPED) {
		return ERR_ALREADY_IN_USE;
	}

	port = p_port;
	password = p_password;
	protocol = p_protocol;

	if (protocol == PROTOCOL_SOURCE) {
		tcp_server.instantiate();
		Error err = tcp_server->listen(port);
		if (err != OK) {
			state = STATE_ERROR;
			Dictionary error_data;
			error_data["error"] = "Failed to start TCP server on port " + itos(port);
			_queue_event(Event::EVENT_SERVER_ERROR, 0, error_data);
			return err;
		}
	} else {
		udp_peer.instantiate();
		Error err = udp_peer->bind(port);
		if (err != OK) {
			state = STATE_ERROR;
			Dictionary error_data;
			error_data["error"] = "Failed to bind UDP socket on port " + itos(port);
			_queue_event(Event::EVENT_SERVER_ERROR, 0, error_data);
			return err;
		}
	}

	state = STATE_LISTENING;
	stop_thread = false;
	thread_running = true;
	network_thread.start(_network_thread_func, this);

	_queue_event(Event::EVENT_SERVER_STARTED, 0, Dictionary());

	return OK;
}

void RCONServer::stop_server() {
	stop_thread = true;

	if (network_thread.is_started()) {
		network_thread.wait_to_finish();
	}

	// Clean up clientsock(mutex);

	// Disconnect all clients
	for (KeyValue<int, Client> &E : clients) {
		if (E.value.tcp_peer.is_valid()) {
			E.value.tcp_peer->disconnect_from_host();
		}
	}
	clients.clear();

	if (tcp_server.is_valid()) {
		tcp_server->stop();
		tcp_server.unref();
	}

	if (udp_peer.is_valid()) {
		udp_peer->close();
		udp_peer.unref();
	}

	state = STATE_STOPPED;
	_queue_event(Event::EVENT_SERVER_STOPPED, 0, Dictionary());
}

void RCONServer::register_command(const String &p_command, const Callable &p_callback) {
	MutexLock lock(mutex);

	RegisteredCommand cmd;
	cmd.name = p_command;
	cmd.callback = p_callback;
	registered_commands[p_command] = cmd;

	// Add dynamic signal for this command
	String signal_name = "command_" + p_command;
	if (!has_signal(signal_name)) {
		add_user_signal(MethodInfo(signal_name,
				PropertyInfo(Variant::INT, "client_id"),
				PropertyInfo(Variant::STRING, "args"),
				PropertyInfo(Variant::INT, "request_id")));
	}
}

void RCONServer::unregister_command(const String &p_command) {
	MutexLock lock(mutex);
	registered_commands.erase(p_command);
}

void RCONServer::send_response(int p_client_id, int p_request_id, const String &p_response) {
	MutexLock lock(mutex);

	Response response;
	response.client_id = p_client_id;
	response.request_id = p_request_id;
	response.data = p_response;
	response.is_raw = false;
	response_queue.push_back(response);
}

void RCONServer::send_raw_packet(int p_client_id, const PackedByteArray &p_packet) {
	MutexLock lock(mutex);

	Response response;
	response.client_id = p_client_id;
	response.request_id = 0;
	response.raw_packet = p_packet;
	response.is_raw = true;
	response_queue.push_back(response);
}

bool RCONServer::is_running() const {
	return state == STATE_LISTENING;
}

RCONServer::State RCONServer::get_state() const {
	return state;
}

RCONServer::Protocol RCONServer::get_protocol() const {
	return protocol;
}

Array RCONServer::get_connected_clients() const {
	MutexLock lock(mutex);
	Array result;
	for (const KeyValue<int, Client> &E : clients) {
		result.push_back(E.key);
	}
	return result;
}

void RCONServer::poll() {
	List<Event> events_to_process;

	{
		MutexLock lock(mutex);
		events_to_process = event_queue;
		event_queue.clear();
	}

	for (const Event &event : events_to_process) {
		switch (event.type) {
			case Event::EVENT_SERVER_STARTED:
				emit_signal("server_started");
				break;

			case Event::EVENT_SERVER_STOPPED:
				emit_signal("server_stopped");
				break;

			case Event::EVENT_CLIENT_CONNECTED: {
				String address = event.data.get("address", "");
				emit_signal("client_connected", event.client_id, address);
			} break;

			case Event::EVENT_CLIENT_DISCONNECTED:
				emit_signal("client_disconnected", event.client_id);
				break;

			case Event::EVENT_CLIENT_AUTHENTICATED:
				emit_signal("client_authenticated", event.client_id);
				break;

			case Event::EVENT_AUTH_FAILED: {
				String address = event.data.get("address", "");
				emit_signal("authentication_failed", event.client_id, address);
			} break;

			case Event::EVENT_COMMAND_RECEIVED: {
				String command = event.data.get("command", "");
				int request_id = event.data.get("request_id", 0);

				// Emit general command_received signal
				emit_signal("command_received", event.client_id, command, request_id);

				// Parse command and arguments
				String cmd_name = command;
				String args;
				int space_pos = command.find(" ");
				if (space_pos != -1) {
					cmd_name = command.substr(0, space_pos);
					args = command.substr(space_pos + 1);
				}

				// Check for registered command
				Callable callback;
				{
					MutexLock lock(mutex);
					if (registered_commands.has(cmd_name)) {
						RegisteredCommand &reg_cmd = registered_commands[cmd_name];
						callback = reg_cmd.callback;
					}
				}

				if (callback.is_valid()) {
					print_line("Executing callback via callv() for " + cmd_name);
					// Call the callback
					Array cb_args;
					cb_args.push_back(event.client_id);
					cb_args.push_back(args);
					cb_args.push_back(request_id);
					callback.callv(cb_args);
					print_line("Finished callv() for " + cmd_name);

					// Emit per-command signal
					String signal_name = "command_" + cmd_name;
					if (has_signal(signal_name)) {
						print_line("Emitting dynamic signal " + signal_name);
						emit_signal(signal_name, event.client_id, args, request_id);
						print_line("Finished dynamic signal " + signal_name);
					}
				}
			} break;

			case Event::EVENT_ECHO_REQUEST: {
				int request_id = event.data.get("request_id", -1);
				_send_response_to_client(event.client_id, request_id, "");
			} break;

			case Event::EVENT_RAW_PACKET_RECEIVED: {
				PackedByteArray packet = event.data.get("packet", PackedByteArray());
				emit_signal("raw_packet_received", event.client_id, packet);
			} break;

			case Event::EVENT_PACKET_SENT: {
				PackedByteArray packet = event.data.get("packet", PackedByteArray());
				emit_signal("packet_sent", event.client_id, packet);
			} break;

			case Event::EVENT_PACKET_SEND_FAILED: {
				String error = event.data.get("error", "");
				emit_signal("packet_send_failed", event.client_id, error);
			} break;

			case Event::EVENT_KEEPALIVE_SENT:
				emit_signal("keep_alive_sent", event.client_id);
				break;

			case Event::EVENT_KEEPALIVE_TIMEOUT:
				emit_signal("keep_alive_timeout", event.client_id);
				break;

			case Event::EVENT_CLIENT_TIMEOUT_WARNING: {
				int seconds = event.data.get("seconds_remaining", 0);
				emit_signal("client_timeout_warning", event.client_id, seconds);
			} break;

			case Event::EVENT_SERVER_ERROR: {
				String error = event.data.get("error", "Unknown error");
				emit_signal("server_error", error);
			} break;
		}
	}
}

void RCONServer::_network_thread_func(void *p_user) {
	RCONServer *server = static_cast<RCONServer *>(p_user);

	if (server->protocol == PROTOCOL_SOURCE) {
		server->_process_network_source();
	} else {
		server->_process_network_battleye();
	}

	MutexLock lock(server->mutex);
	server->thread_running = false;
}

void RCONServer::_process_network_source() {
	while (!stop_thread) {
		// Accept new clients
		_accept_new_clients_tcp();

		// Process existing clients
		List<int> clients_to_remove;

		mutex.lock();
		List<int> client_ids;
		for (KeyValue<int, Client> &E : clients) {
			client_ids.push_back(E.key);
		}
		mutex.unlock();

		for (int id : client_ids) {
			mutex.lock();
			if (!clients.has(id)) {
				mutex.unlock();
				continue;
			}
			Client &c = clients[id];
			if (!c.tcp_peer.is_valid() || c.tcp_peer->get_status() != StreamPeerTCP::STATUS_CONNECTED) {
				clients_to_remove.push_back(id);
				mutex.unlock();
				continue;
			}
			mutex.unlock();

			// Safe to call _process_client_tcp because it will only act if peer valid
			_process_client_tcp(c);

			mutex.lock();
			if (clients.has(id)) {
				Client &c_check = clients[id];
				if (c_check.tcp_peer.is_valid() && c_check.tcp_peer->get_status() != StreamPeerTCP::STATUS_CONNECTED) {
					clients_to_remove.push_back(id);
				}
			}
			mutex.unlock();
		}

		// Remove disconnected clients
		for (int client_id : clients_to_remove) {
			_disconnect_client(client_id);
		}

		// Process response queue
		mutex.lock();
		List<Response> responses_to_send;
		for (const Response &response : response_queue) {
			responses_to_send.push_back(response);
		}
		response_queue.clear();
		mutex.unlock();

		// Send responses (outside global clock)
		for (const Response &response : responses_to_send) {
			if (response.is_raw) {
				_send_raw_to_client(response.client_id, response.raw_packet);
			} else {
				_send_response_to_client(response.client_id, response.request_id, response.data);
			}
		}

		OS::get_singleton()->delay_usec(1000); // 1ms
	}
}

void RCONServer::_process_network_battleye() {
	while (!stop_thread) {
		// Process incoming packets
		_process_packet_udp();

		// Check client timeouts
		_check_client_timeouts();

		// Process response queue
		mutex.lock();
		List<Response> responses_to_send;
		for (const Response &response : response_queue) {
			responses_to_send.push_back(response);
		}
		response_queue.clear();
		mutex.unlock();

		// Send responses (outside global lock to prevent deadlocks)
		for (const Response &response : responses_to_send) {
			if (response.is_raw) {
				_send_raw_to_client(response.client_id, response.raw_packet);
			} else {
				_send_response_to_client(response.client_id, response.request_id, response.data);
			}
		}

		OS::get_singleton()->delay_usec(1000); // 1ms
	}
}

void RCONServer::_accept_new_clients_tcp() {
	if (tcp_server->is_connection_available()) {
		Ref<StreamPeerTCP> peer = tcp_server->take_connection();
		if (peer.is_valid()) {
			MutexLock lock(mutex);

			Client client;
			client.id = next_client_id++;
			client.tcp_peer = peer;
			client.authenticated = false;
			client.last_activity = OS::get_singleton()->get_ticks_msec();

			clients[client.id] = client;

			Dictionary data;
			String addr_str = peer->get_connected_host();
			data["address"] = addr_str + String(":") + itos(peer->get_connected_port());
			_queue_event_unlocked(Event::EVENT_CLIENT_CONNECTED, client.id, data);
		}
	}
}

void RCONServer::_process_client_tcp(Client &p_client) {
	if (!p_client.tcp_peer.is_valid() || p_client.tcp_peer->get_status() != StreamPeerTCP::STATUS_CONNECTED) {
		return;
	}

	p_client.tcp_peer->poll();

	if (p_client.tcp_peer->get_status() != StreamPeerTCP::STATUS_CONNECTED) {
		return;
	}

	int available = p_client.tcp_peer->get_available_bytes();
	if (available > 0) {
		PackedByteArray chunk;
		chunk.resize(available);
		p_client.tcp_peer->get_data(chunk.ptrw(), available);
		p_client.tcp_buffer.append_array(chunk);

		// Try to parse packets
		while (p_client.tcp_buffer.size() >= 4) {
			int32_t packet_size = p_client.tcp_buffer[0] |
					(p_client.tcp_buffer[1] << 8) |
					(p_client.tcp_buffer[2] << 16) |
					(p_client.tcp_buffer[3] << 24);

			int total_size = packet_size + 4;

			if (total_size <= 4) {
				p_client.tcp_buffer.clear();
				Dictionary error_data;
				error_data["error"] = "Corrupt packet size";
				_queue_event_unlocked(Event::EVENT_SERVER_ERROR, 0, error_data);
				break;
			}

			if (p_client.tcp_buffer.size() >= total_size) {
				PackedByteArray packet = p_client.tcp_buffer.slice(0, total_size);
				p_client.tcp_buffer = p_client.tcp_buffer.slice(total_size);
				_process_source_packet(p_client, packet);
			} else {
				break;
			}
		}

		p_client.last_activity = OS::get_singleton()->get_ticks_msec();
	}
}

void RCONServer::_process_packet_udp() {
	if (udp_peer->get_available_packet_count() > 0) {
		int len;
		const uint8_t *buffer;
		Error err = udp_peer->get_packet(&buffer, len);
		if (err == OK) {
			IPAddress sender_address = udp_peer->get_packet_address();
			int sender_port = udp_peer->get_packet_port();

			PackedByteArray packet;
			packet.resize(len);
			memcpy(packet.ptrw(), buffer, len);

			_process_battleye_packet(sender_address, sender_port, packet);
		}
	}
}

void RCONServer::_check_client_timeouts() {
	if (protocol != PROTOCOL_BATTLEYE) {
		return;
	}

	uint64_t current_time = OS::get_singleton()->get_ticks_msec();
	List<int> clients_to_remove;

	MutexLock lock(mutex);

	for (KeyValue<int, Client> &E : clients) {
		uint64_t time_since_activity = current_time - E.value.last_activity;

		if (time_since_activity >= CLIENT_TIMEOUT_MS) {
			// Client timed out
			_queue_event(Event::EVENT_KEEPALIVE_TIMEOUT, E.key, Dictionary());
			clients_to_remove.push_back(E.key);
		} else if (time_since_activity >= TIMEOUT_WARNING_MS) {
			// Approaching timeout
			int seconds_remaining = (CLIENT_TIMEOUT_MS - time_since_activity) / 1000;
			Dictionary data;
			data["seconds_remaining"] = seconds_remaining;
			_queue_event(Event::EVENT_CLIENT_TIMEOUT_WARNING, E.key, data);
		}
	}

	for (int client_id : clients_to_remove) {
		_disconnect_client(client_id);
	}
}

void RCONServer::_process_source_packet(Client &p_client, const PackedByteArray &p_packet) {
	Dictionary parsed = RCONPacket::parse_source_packet(p_packet);

	if (!parsed.get("valid", false)) {
		return;
	}

	int type = parsed.get("type", -1);
	int id = parsed.get("id", -1);
	String body = parsed.get("body", "");

	// Queue raw packet event
	Dictionary raw_data;
	raw_data["packet"] = p_packet;
	_queue_event(Event::EVENT_RAW_PACKET_RECEIVED, p_client.id, raw_data);

	// Handle authentication
	if (type == RCONPacket::SOURCE_SERVERDATA_AUTH) {
		// Check password
		if (body == password) {
			p_client.authenticated = true;

			// Send response value packet (id matches)
			PackedByteArray response1 = RCONPacket::create_source_command(id, "");
			response1.ptrw()[8] = RCONPacket::SOURCE_SERVERDATA_RESPONSE_VALUE;
			p_client.tcp_peer->put_data(response1.ptr(), response1.size());

			// Send auth response packet (id matches)
			PackedByteArray response2 = RCONPacket::create_source_command(id, "");
			response2.ptrw()[8] = RCONPacket::SOURCE_SERVERDATA_AUTH_RESPONSE;
			p_client.tcp_peer->put_data(response2.ptr(), response2.size());

			_queue_event(Event::EVENT_CLIENT_AUTHENTICATED, p_client.id, Dictionary());
		} else {
			// Authentication failed
			// Auth failed (send -1 id)
			PackedByteArray response = RCONPacket::create_source_command(-1, "");
			response.ptrw()[8] = RCONPacket::SOURCE_SERVERDATA_AUTH_RESPONSE;

			_send_raw_to_client(p_client.id, response);

			Dictionary data;
			data["address"] = String(p_client.tcp_peer->get_connected_host()) + String(":") + itos(p_client.tcp_peer->get_connected_port());
			_queue_event(Event::EVENT_AUTH_FAILED, p_client.id, data);
		}
	} else if (type == RCONPacket::SOURCE_SERVERDATA_EXECCOMMAND) {
		// Execute command
		if (p_client.authenticated) {
			Dictionary cmd_data;
			cmd_data["command"] = body;
			cmd_data["request_id"] = id;
			_queue_event(Event::EVENT_COMMAND_RECEIVED, p_client.id, cmd_data);
		}
	} else if (type == RCONPacket::SOURCE_SERVERDATA_RESPONSE_VALUE) {
		// Client sending us an empty packet (multi-packet response trick)
		// We queue it so it echoes back from the main thread staying chronologically ordered after command execution!
		Dictionary data;
		data["request_id"] = id;
		_queue_event(Event::EVENT_ECHO_REQUEST, p_client.id, data);
	} else {
		// Event already queued at the top of the function, but leaving this for explicit unhandled case
		Dictionary unhandled_raw;
		unhandled_raw["packet"] = p_packet;
		_queue_event(Event::EVENT_RAW_PACKET_RECEIVED, p_client.id, unhandled_raw);
	}
}

void RCONServer::_process_battleye_packet(const IPAddress &p_address, int p_port, const PackedByteArray &p_packet) {
	Dictionary parsed = RCONPacket::parse_battleye_packet(p_packet);

	int type = parsed.get("type", -1);

	// Find or create client
	Client *client = _find_client_by_udp_address(p_address, p_port);
	bool is_new_client = (client == nullptr);

	if (is_new_client) {
		MutexLock lock(mutex);
		Client new_client;
		new_client.id = next_client_id++;
		new_client.udp_address = p_address;
		new_client.udp_port = p_port;
		new_client.authenticated = false;
		new_client.last_activity = OS::get_singleton()->get_ticks_msec();
		clients[new_client.id] = new_client;
		client = &clients[new_client.id];

		Dictionary data;
		data["address"] = p_address.operator String() + String(":") + itos(p_port);
		_queue_event_unlocked(Event::EVENT_CLIENT_CONNECTED, client->id, data);
	} else {
		client->last_activity = OS::get_singleton()->get_ticks_msec();
	}

	if (type == RCONPacket::BATTLEYE_LOGIN) {
		// Login request
		String login_password = "";
		// Extract password from payload directly
		PackedByteArray payload = p_packet.slice(7);
		if (payload.size() > 1) {
			PackedByteArray pwd_data = payload.slice(1);
			login_password = String::utf8((const char *)pwd_data.ptr(), pwd_data.size());
		}

		if (login_password == password) {
			client->authenticated = true;

			// Send success response
			PackedByteArray response;
			response.push_back('B');
			response.push_back('E');

			PackedByteArray response_payload;
			response_payload.push_back(RCONPacket::BATTLEYE_LOGIN);
			response_payload.push_back(0x01); // Success

			uint32_t crc = RCONPacket::_calculate_crc32(response_payload);

			response.push_back((crc) & 0xFF);
			response.push_back((crc >> 8) & 0xFF);
			response.push_back((crc >> 16) & 0xFF);
			response.push_back((crc >> 24) & 0xFF);
			response.push_back(0xFF);
			response.append_array(response_payload);

			udp_peer->set_dest_address(p_address, p_port);
			udp_peer->put_packet(response.ptr(), response.size());

			_queue_event(Event::EVENT_CLIENT_AUTHENTICATED, client->id, Dictionary());
		} else {
			// Send failed login response
			PackedByteArray response;
			response.push_back('B');
			response.push_back('E');

			PackedByteArray response_payload;
			response_payload.push_back(RCONPacket::BATTLEYE_LOGIN);
			response_payload.push_back(0x00); // Fail

			uint32_t crc = RCONPacket::_calculate_crc32(response_payload);

			response.push_back((crc) & 0xFF);
			response.push_back((crc >> 8) & 0xFF);
			response.push_back((crc >> 16) & 0xFF);
			response.push_back((crc >> 24) & 0xFF);
			response.push_back(0xFF);
			response.append_array(response_payload);

			udp_peer->set_dest_address(client->udp_address, client->udp_port);
			udp_peer->put_packet(response.ptr(), response.size());

			Dictionary data;
			data["address"] = client->udp_address.operator String() + String(":") + itos(client->udp_port);
			_queue_event(Event::EVENT_AUTH_FAILED, client->id, data);
		}
	} else if (type == RCONPacket::BATTLEYE_COMMAND) {
		if (client->authenticated) {
			int id = parsed.get("seq", -1); // Use 'seq' as 'id' for BattlEye
			String body = parsed.get("data", "");

			// Send ACK
			PackedByteArray ack = RCONPacket::create_battleye_command(id, "");
			udp_peer->set_dest_address(client->udp_address, client->udp_port);
			udp_peer->put_packet(ack.ptr(), ack.size());

			Dictionary cmd_data;
			cmd_data["command"] = body;
			cmd_data["request_id"] = id;

			if (body.is_empty()) {
				_queue_event(Event::EVENT_KEEPALIVE_SENT, client->id, Dictionary());
			} else {
				_queue_event(Event::EVENT_COMMAND_RECEIVED, client->id, cmd_data);
			}
		}
	} else {
		// Queue raw packet event for unhandled types
		Dictionary raw_data;
		raw_data["packet"] = p_packet;
		_queue_event(Event::EVENT_RAW_PACKET_RECEIVED, client->id, raw_data);
	}
}

RCONServer::Client *RCONServer::_find_client_by_udp_address(const IPAddress &p_address, int p_port) {
	for (KeyValue<int, Client> &E : clients) {
		if (E.value.udp_address == p_address && E.value.udp_port == p_port) {
			return &E.value;
		}
	}
	return nullptr;
}

void RCONServer::_disconnect_client(int p_client_id) {
	MutexLock lock(mutex);

	if (clients.has(p_client_id)) {
		Client &client = clients[p_client_id];

		if (client.tcp_peer.is_valid()) {
			client.tcp_peer->disconnect_from_host();
		}

		Dictionary data;
		data["address"] = client.udp_address.operator String() + String(":") + itos(client.udp_port);
		_queue_event_unlocked(Event::EVENT_CLIENT_DISCONNECTED, p_client_id, data);

		clients.erase(p_client_id);
	}
}

void RCONServer::_send_response_to_client(int p_client_id, int p_request_id, const String &p_response) {
	Ref<StreamPeerTCP> peer;

	{
		MutexLock lock(mutex);
		if (!clients.has(p_client_id)) {
			return;
		}

		Client &client = clients[p_client_id];

		if (protocol == PROTOCOL_SOURCE) {
			if (!client.tcp_peer.is_valid() || client.tcp_peer->get_status() != StreamPeerTCP::STATUS_CONNECTED) {
				Dictionary error_data;
				error_data["error"] = "Client disconnected";
				_queue_event_unlocked(Event::EVENT_PACKET_SEND_FAILED, p_client_id, error_data);
				return;
			}
			peer = client.tcp_peer;
		}
	}

	if (peer.is_valid()) {
		PackedByteArray packet = RCONPacket::create_source_command(p_request_id, p_response);
		packet.ptrw()[8] = RCONPacket::SOURCE_SERVERDATA_RESPONSE_VALUE;

		Error err = peer->put_data(packet.ptr(), packet.size());

		MutexLock lock(mutex);
		if (err == OK) {
			Dictionary data;
			data["packet"] = packet;
			_queue_event_unlocked(Event::EVENT_PACKET_SENT, p_client_id, data);
		} else {
			Dictionary error_data;
			error_data["error"] = "Failed to send packet";
			_queue_event_unlocked(Event::EVENT_PACKET_SEND_FAILED, p_client_id, error_data);
		}
	} else if (protocol == PROTOCOL_BATTLEYE) {
		// BattlEye udp block
		MutexLock lock(mutex);
		if (clients.has(p_client_id)) {
			Client &client = clients[p_client_id];
			PackedByteArray packet = RCONPacket::create_battleye_command(p_request_id, p_response);
			if (client.udp_port != 0 && udp_peer.is_valid()) {
				udp_peer->set_dest_address(client.udp_address, client.udp_port);
				Error err = udp_peer->put_packet(packet.ptr(), packet.size());
				if (err == OK) {
					Dictionary data;
					data["packet"] = packet;
					_queue_event_unlocked(Event::EVENT_PACKET_SENT, p_client_id, data);
				} else {
					Dictionary error_data;
					error_data["error"] = "Failed to send packet";
					_queue_event_unlocked(Event::EVENT_PACKET_SEND_FAILED, p_client_id, error_data);
				}
			}
		}
	}
}

void RCONServer::_send_raw_to_client(int p_client_id, const PackedByteArray &p_packet) {
	Ref<StreamPeerTCP> peer;

	{
		MutexLock lock(mutex);
		if (!clients.has(p_client_id)) {
			return;
		}

		Client &client = clients[p_client_id];

		if (protocol == PROTOCOL_SOURCE) {
			if (!client.tcp_peer.is_valid() || client.tcp_peer->get_status() != StreamPeerTCP::STATUS_CONNECTED) {
				Dictionary error_data;
				error_data["error"] = "Client disconnected";
				_queue_event_unlocked(Event::EVENT_PACKET_SEND_FAILED, p_client_id, error_data);
				return;
			}
			peer = client.tcp_peer;
		}
	}

	if (peer.is_valid()) {
		Error err = peer->put_data(p_packet.ptr(), p_packet.size());

		MutexLock lock(mutex);
		if (err == OK) {
			Dictionary data;
			data["packet"] = p_packet;
			_queue_event_unlocked(Event::EVENT_PACKET_SENT, p_client_id, data);
		} else {
			Dictionary error_data;
			error_data["error"] = "Failed to send raw packet";
			_queue_event_unlocked(Event::EVENT_PACKET_SEND_FAILED, p_client_id, error_data);
		}
	} else if (protocol == PROTOCOL_BATTLEYE) {
		MutexLock lock(mutex);
		if (clients.has(p_client_id)) {
			Client &client = clients[p_client_id];
			if (client.udp_port != 0 && udp_peer.is_valid()) {
				udp_peer->set_dest_address(client.udp_address, client.udp_port);
				Error err = udp_peer->put_packet(p_packet.ptr(), p_packet.size());
				if (err == OK) {
					Dictionary data;
					data["packet"] = p_packet;
					_queue_event_unlocked(Event::EVENT_PACKET_SENT, p_client_id, data);
				} else {
					Dictionary error_data;
					error_data["error"] = "Failed to send raw packet";
					_queue_event_unlocked(Event::EVENT_PACKET_SEND_FAILED, p_client_id, error_data);
				}
			}
		}
	}
}

void RCONServer::_queue_event_unlocked(Event::Type p_type, int p_client_id, const Dictionary &p_data) {
	Event event;
	event.type = p_type;
	event.client_id = p_client_id;
	event.data = p_data;
	event_queue.push_back(event);
}

void RCONServer::_queue_event(Event::Type p_type, int p_client_id, const Dictionary &p_data) {
	MutexLock lock(mutex);
	_queue_event_unlocked(p_type, p_client_id, p_data);
}

RCONServer::RCONServer() {
}

RCONServer::~RCONServer() {
	if (thread_running) {
		stop_server();
	}
}
