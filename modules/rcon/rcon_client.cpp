/**************************************************************************/
/*  rcon_client.cpp                                                       */
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

#include "rcon_client.h"

#include "core/object/callable_method_pointer.h"
#include "core/os/os.h"
#include "rcon_packet.h"

void RCONClient::_bind_methods() {
	// High-level API
	ClassDB::bind_method(D_METHOD("connect_to_server", "host", "port", "password", "protocol"), &RCONClient::connect_to_server);
	ClassDB::bind_method(D_METHOD("disconnect_from_server"), &RCONClient::disconnect_from_server);
	ClassDB::bind_method(D_METHOD("send_command", "command", "callback"), &RCONClient::send_command, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("send_command_sync", "command", "timeout"), &RCONClient::send_command_sync, DEFVAL(5.0));

	// Low-level API
	ClassDB::bind_method(D_METHOD("send_raw_packet", "packet"), &RCONClient::send_raw_packet);

	// Status
	ClassDB::bind_method(D_METHOD("is_authenticated"), &RCONClient::is_authenticated);
	ClassDB::bind_method(D_METHOD("get_state"), &RCONClient::get_state);
	ClassDB::bind_method(D_METHOD("get_protocol"), &RCONClient::get_protocol);

	// Polling
	ClassDB::bind_method(D_METHOD("poll"), &RCONClient::poll);

	// Signals
	ADD_SIGNAL(MethodInfo("connected"));
	ADD_SIGNAL(MethodInfo("disconnected"));
	ADD_SIGNAL(MethodInfo("authenticated"));
	ADD_SIGNAL(MethodInfo("authentication_failed"));
	ADD_SIGNAL(MethodInfo("command_response", PropertyInfo(Variant::STRING, "command"), PropertyInfo(Variant::STRING, "response")));
	ADD_SIGNAL(MethodInfo("server_message", PropertyInfo(Variant::STRING, "message")));
	ADD_SIGNAL(MethodInfo("raw_packet_received", PropertyInfo(Variant::PACKED_BYTE_ARRAY, "packet")));
	ADD_SIGNAL(MethodInfo("connection_error", PropertyInfo(Variant::STRING, "error")));

	// Enums
	BIND_ENUM_CONSTANT(PROTOCOL_SOURCE);
	BIND_ENUM_CONSTANT(PROTOCOL_BATTLEYE);

	BIND_ENUM_CONSTANT(STATE_DISCONNECTED);
	BIND_ENUM_CONSTANT(STATE_CONNECTING);
	BIND_ENUM_CONSTANT(STATE_AUTHENTICATING);
	BIND_ENUM_CONSTANT(STATE_CONNECTED);
	BIND_ENUM_CONSTANT(STATE_ERROR);
}

Error RCONClient::connect_to_server(const String &p_host, int p_port, const String &p_password, Protocol p_protocol) {
	MutexLock lock(mutex);

	if (state != STATE_DISCONNECTED) {
		return ERR_ALREADY_IN_USE;
	}

	host = p_host;
	port = p_port;
	password = p_password;
	protocol = p_protocol;

	if (protocol == PROTOCOL_SOURCE) {
		tcp_peer.instantiate();
		Error err = tcp_peer->connect_to_host(host, port);
		if (err != OK) {
			state = STATE_ERROR;
			_queue_event(Event::EVENT_ERROR, Dictionary().duplicate());
			return err;
		}
	} else if (protocol == PROTOCOL_BATTLEYE) {
		udp_peer.instantiate();
		udp_peer->bind(0);
		udp_peer->set_dest_address(host, port);
	}
	state = STATE_CONNECTING;
	stop_thread = false;
	thread_running = true;
	network_thread.start(_network_thread_func, this);

	return OK;
}

void RCONClient::disconnect_from_server() {
	stop_thread = true;

	if (network_thread.is_started()) {
		network_thread.wait_to_finish();
	}

	MutexLock lock(mutex);
	thread_running = false;
	state = STATE_DISCONNECTED;
	host = "";
	port = 0;
	password = "";

	if (tcp_peer.is_valid()) {
		tcp_peer->disconnect_from_host();
		tcp_peer.unref(); // Added back unref
	}
	if (udp_peer.is_valid()) {
		udp_peer->close();
		udp_peer.unref(); // Added back unref
	}

	event_queue.clear();
	pending_commands.clear();
	command_seq = 0;
	next_request_id = 1;
	_queue_event(Event::EVENT_DISCONNECTED, Dictionary()); // This was part of the original logic
}

void RCONClient::send_command(const String &p_command, const Callable &p_callback) {
	MutexLock lock(mutex);

	if (state != STATE_CONNECTED) {
		ERR_PRINT("RCONClient: Cannot send command, not connected");
		return;
	}

	PendingCommand cmd;
	cmd.command = p_command;
	cmd.callback = p_callback;

	if (protocol == PROTOCOL_SOURCE) {
		cmd.request_id = next_request_id++;
		PackedByteArray packet = RCONPacket::create_source_command(cmd.request_id, p_command);

		if (tcp_peer.is_valid() && tcp_peer->get_status() == StreamPeerTCP::STATUS_CONNECTED) {
			tcp_peer->put_data(packet.ptr(), packet.size());
			pending_commands.push_back(cmd);
		}
	} else {
		cmd.seq = command_seq++;
		PackedByteArray packet = RCONPacket::create_battleye_command(cmd.seq, p_command);

		if (udp_peer.is_valid()) {
			udp_peer->put_packet(packet.ptr(), packet.size());
			pending_commands.push_back(cmd);
		}
	}
}

String RCONClient::send_command_sync(const String &p_command, float p_timeout) {
	// Send command without callback
	send_command(p_command, Callable());

	uint64_t start_time = OS::get_singleton()->get_ticks_msec();
	uint64_t timeout_ms = (uint64_t)(p_timeout * 1000);

	String result;

	// Wait for response via signal
	while (true) {
		// Must preserve events temporarily to find the response.
		// poll() will emit them immediately without giving us a chance to inspect locally,
		// so we intercept them manually.
		List<Event> events_to_process;
		{
			MutexLock lock(mutex);
			events_to_process = event_queue;
			event_queue.clear();
		}

		bool response_found = false;

		for (const Event &event : events_to_process) {
			if (event.type == Event::EVENT_COMMAND_RESPONSE) {
				String ev_command = event.data.get("command", "");
				if (ev_command == p_command) {
					result = event.data.get("response", "");
					response_found = true;
				} else {
					print_line(vformat("MISMATCH! ev_command: '%s', p_command: '%s'", ev_command, p_command));
				}
			}
			// Push it back to queue so poll() can still trigger signals securely?
			// But sync commands shouldn't fire async callbacks typically.
			// However for safety, we push them back. Or better: we emit immediately just like poll().
			switch (event.type) {
				case Event::EVENT_CONNECTED:
					emit_signal("connected");
					break;
				case Event::EVENT_DISCONNECTED:
					emit_signal("disconnected");
					break;
				case Event::EVENT_AUTHENTICATED:
					emit_signal("authenticated");
					break;
				case Event::EVENT_AUTH_FAILED:
					emit_signal("authentication_failed");
					break;
				case Event::EVENT_COMMAND_RESPONSE: {
					String command = event.data.get("command", "");
					String response = event.data.get("response", "");
					emit_signal("command_response", command, response);
					Callable callback = event.data.get("callback", Callable());
					if (callback.is_valid()) {
						callback.call(response);
					}
				} break;
				case Event::EVENT_SERVER_MESSAGE:
					emit_signal("server_message", event.data.get("message", ""));
					break;
				case Event::EVENT_RAW_PACKET:
					emit_signal("raw_packet_received", event.data.get("packet", PackedByteArray()));
					break;
				case Event::EVENT_ERROR:
					emit_signal("connection_error", event.data.get("error", "Unknown error"));
					break;
			}
		}

		if (response_found) {
			break;
		}

		{
			MutexLock lock(mutex);
			if (!pending_commands.is_empty()) {
				// Still waiting for response
				OS::get_singleton()->delay_usec(10000); // 10ms
			} else if (event_queue.is_empty()) {
				// If commands are completely empty AND event_queue is empty,
				// it means the event was already processed or lost
				break;
			}
			// If pending_commands are empty BUT event_queue has items,
			// DO NOT BREAK! Loop again to process the EVENT_COMMAND_RESPONSE!
		}

		if (OS::get_singleton()->get_ticks_msec() - start_time > timeout_ms) {
			ERR_PRINT("RCONClient: Command timeout");
			break;
		}
	}

	return result;
}

void RCONClient::send_raw_packet(const PackedByteArray &p_packet) {
	MutexLock lock(mutex);

	if (protocol == PROTOCOL_SOURCE && tcp_peer.is_valid() && tcp_peer->get_status() == StreamPeerTCP::STATUS_CONNECTED) {
		tcp_peer->put_data(p_packet.ptr(), p_packet.size());
	} else if (protocol == PROTOCOL_BATTLEYE && udp_peer.is_valid()) {
		udp_peer->put_packet(p_packet.ptr(), p_packet.size());
	}
}

bool RCONClient::is_authenticated() const {
	return state == STATE_CONNECTED;
}

RCONClient::State RCONClient::get_state() const {
	return state;
}

RCONClient::Protocol RCONClient::get_protocol() const {
	return protocol;
}

void RCONClient::poll() {
	List<Event> events_to_process;

	{
		MutexLock lock(mutex);
		events_to_process = event_queue;
		event_queue.clear();
	}

	for (const Event &event : events_to_process) {
		switch (event.type) {
			case Event::EVENT_CONNECTED:
				emit_signal("connected");
				break;

			case Event::EVENT_DISCONNECTED:
				emit_signal("disconnected");
				break;

			case Event::EVENT_AUTHENTICATED:
				emit_signal("authenticated");
				break;

			case Event::EVENT_AUTH_FAILED:
				emit_signal("authentication_failed");
				break;

			case Event::EVENT_COMMAND_RESPONSE: {
				String command = event.data.get("command", "");
				String response = event.data.get("response", "");
				emit_signal("command_response", command, response);

				// Execute callback if present
				Callable callback = event.data.get("callback", Callable());
				if (callback.is_valid()) {
					callback.call(response);
				}
			} break;

			case Event::EVENT_SERVER_MESSAGE: {
				String message = event.data.get("message", "");
				emit_signal("server_message", message);
			} break;

			case Event::EVENT_RAW_PACKET: {
				PackedByteArray packet = event.data.get("packet", PackedByteArray());
				emit_signal("raw_packet_received", packet);
			} break;

			case Event::EVENT_ERROR: {
				String error = event.data.get("error", "Unknown error");
				emit_signal("connection_error", error);
			} break;
		}
	}
}

void RCONClient::_network_thread_func(void *p_user) {
	RCONClient *client = static_cast<RCONClient *>(p_user);

	if (client->protocol == PROTOCOL_SOURCE) {
		client->_process_network_source();
	} else {
		client->_process_network_battleye();
	}

	MutexLock lock(client->mutex);
	client->thread_running = false;
}

void RCONClient::_process_network_source() {
	// Wait for connection
	while (!stop_thread) {
		tcp_peer->poll();
		StreamPeerTCP::Status status = tcp_peer->get_status();

		if (status == StreamPeerTCP::STATUS_CONNECTED) {
			mutex.lock();
			state = STATE_AUTHENTICATING;
			mutex.unlock();

			// Send auth packet
			PackedByteArray auth_packet = RCONPacket::create_source_auth(0, password);
			tcp_peer->put_data(auth_packet.ptr(), auth_packet.size());
			break;
		} else if (status == StreamPeerTCP::STATUS_ERROR) {
			Dictionary error_data;
			error_data["error"] = "Failed to connect";
			_queue_event(Event::EVENT_ERROR, error_data);
			return;
		}

		OS::get_singleton()->delay_usec(10000); // 10ms
	}

	// Main receive loop
	PackedByteArray buffer;

	while (!stop_thread) {
		tcp_peer->poll();

		if (tcp_peer->get_status() != StreamPeerTCP::STATUS_CONNECTED) {
			_queue_event(Event::EVENT_DISCONNECTED, Dictionary());
			break;
		}

		int available = tcp_peer->get_available_bytes();
		if (available > 0) {
			PackedByteArray chunk;
			chunk.resize(available);
			tcp_peer->get_data(chunk.ptrw(), available);
			buffer.append_array(chunk);

			// Try to parse packets
			while (buffer.size() >= 4) {
				int32_t packet_size = buffer[0] |
						(buffer[1] << 8) |
						(buffer[2] << 16) |
						(buffer[3] << 24);

				int total_size = packet_size + 4;

				if (total_size <= 4) {
					buffer.clear();
					Dictionary error_data;
					error_data["error"] = "Corrupt packet size";
					_queue_event(Event::EVENT_ERROR, error_data);
					break;
				}

				if (buffer.size() >= total_size) {
					PackedByteArray packet = buffer.slice(0, total_size);
					buffer = buffer.slice(total_size);
					_process_source_packet(packet);
				} else {
					break;
				}
			}
		}

		OS::get_singleton()->delay_usec(1000); // 1ms
	}
}

void RCONClient::_process_network_battleye() {
	// Send login packet immediately
	PackedByteArray login_packet = RCONPacket::create_battleye_login(password);
	udp_peer->put_packet(login_packet.ptr(), login_packet.size());

	mutex.lock();
	state = STATE_AUTHENTICATING;
	last_keepalive_time = OS::get_singleton()->get_ticks_msec();
	mutex.unlock();

	// Main receive loop
	while (!stop_thread) {
		if (udp_peer->get_available_packet_count() > 0) {
			int len;
			const uint8_t *buffer;
			Error err = udp_peer->get_packet(&buffer, len);
			if (err == OK) {
				PackedByteArray packet;
				packet.resize(len);
				memcpy(packet.ptrw(), buffer, len);
				_process_battleye_packet(packet);
			}
		}

		// Send keep-alive if needed
		uint64_t current_time = OS::get_singleton()->get_ticks_msec();
		mutex.lock();
		if (state == STATE_CONNECTED && (current_time - last_keepalive_time) >= KEEPALIVE_INTERVAL_MS) {
			_send_keepalive_battleye();
			last_keepalive_time = current_time;
		}
		mutex.unlock();

		OS::get_singleton()->delay_usec(1000); // 1ms
	}
}

void RCONClient::_send_keepalive_battleye() {
	// Send empty command packet
	uint8_t seq = command_seq++;
	PackedByteArray packet = RCONPacket::create_battleye_command(seq, "");
	udp_peer->put_packet(packet.ptr(), packet.size());
}

void RCONClient::_process_source_packet(const PackedByteArray &p_packet) {
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
	_queue_event(Event::EVENT_RAW_PACKET, raw_data);

	// Handle authentication response
	if (id == 0 || id == -1) {
		mutex.lock();
		if (state == STATE_AUTHENTICATING) {
			// Second packet with id=0 or id=-1 indicates auth result
			if (type == RCONPacket::SOURCE_SERVERDATA_AUTH_RESPONSE) {
				if (id == -1) {
					state = STATE_ERROR;
					_queue_event(Event::EVENT_AUTH_FAILED, Dictionary());
				} else {
					state = STATE_CONNECTED;
					_queue_event(Event::EVENT_AUTHENTICATED, Dictionary());
					_queue_event(Event::EVENT_CONNECTED, Dictionary());
				}
			}
		}
		mutex.unlock();
		return;
	}

	// Handle command responses
	mutex.lock();
	for (List<PendingCommand>::Element *E = pending_commands.front(); E; E = E->next()) {
		if (E->get().request_id == id) {
			PendingCommand cmd = E->get();
			pending_commands.erase(E);
			mutex.unlock();

			Dictionary response_data;
			response_data["command"] = cmd.command;
			response_data["response"] = body;
			response_data["callback"] = cmd.callback;
			_queue_event(Event::EVENT_COMMAND_RESPONSE, response_data);
			return;
		}
	}
	mutex.unlock();
}

void RCONClient::_process_battleye_packet(const PackedByteArray &p_packet) {
	Dictionary parsed = RCONPacket::parse_battleye_packet(p_packet);

	if (!parsed.get("valid", false)) {
		return;
	}

	int type = parsed.get("type", -1);

	// Queue raw packet event
	Dictionary raw_data;
	raw_data["packet"] = p_packet;
	_queue_event(Event::EVENT_RAW_PACKET, raw_data);

	if (type == RCONPacket::BATTLEYE_LOGIN) {
		// Login response
		bool success = parsed.get("success", false);
		mutex.lock();
		if (success) {
			state = STATE_CONNECTED;
			_queue_event(Event::EVENT_AUTHENTICATED, Dictionary());
			_queue_event(Event::EVENT_CONNECTED, Dictionary());
		} else {
			state = STATE_ERROR;
			_queue_event(Event::EVENT_AUTH_FAILED, Dictionary());
		}
		mutex.unlock();
	} else if (type == RCONPacket::BATTLEYE_COMMAND) {
		// Command response
		int seq = parsed.get("seq", -1);
		String data = parsed.get("data", "");
		bool multi_packet = parsed.get("multi_packet", false);

		mutex.lock();
		for (List<PendingCommand>::Element *E = pending_commands.front(); E; E = E->next()) {
			if (E->get().seq == seq) {
				if (multi_packet) {
					int total = parsed.get("total_packets", 1);
					int index = parsed.get("packet_index", 0);
					E->get().multi_packet_data[index] = data;

					if ((int)E->get().multi_packet_data.size() == total) {
						// All packets received, reassemble
						String complete_response;
						for (int i = 0; i < total; i++) {
							complete_response += E->get().multi_packet_data[i];
						}

						PendingCommand cmd = E->get();
						pending_commands.erase(E);
						mutex.unlock();

						Dictionary response_data;
						response_data["command"] = cmd.command;
						response_data["response"] = complete_response;
						response_data["callback"] = cmd.callback;
						_queue_event(Event::EVENT_COMMAND_RESPONSE, response_data);
						return;
					}
				} else {
					if (data.is_empty() && !multi_packet) {
						// It's an ACK, ignore and keep waiting for actual response
						mutex.unlock();
						return;
					}

					PendingCommand cmd = E->get();
					pending_commands.erase(E);
					mutex.unlock();

					Dictionary response_data;
					response_data["command"] = cmd.command;
					response_data["response"] = data;
					response_data["callback"] = cmd.callback;
					_queue_event(Event::EVENT_COMMAND_RESPONSE, response_data);
					return;
				}
				mutex.unlock();
				return;
			}
		}
		mutex.unlock();
	} else if (type == RCONPacket::BATTLEYE_MESSAGE) {
		// Server message - acknowledge and emit
		int seq = parsed.get("seq", -1);
		String message = parsed.get("message", "");

		// Send acknowledgment
		PackedByteArray ack = RCONPacket::create_battleye_message_ack(seq);
		udp_peer->put_packet(ack.ptr(), ack.size());

		Dictionary msg_data;
		msg_data["message"] = message;
		_queue_event(Event::EVENT_SERVER_MESSAGE, msg_data);
	}
}

void RCONClient::_queue_event(Event::Type p_type, const Dictionary &p_data) {
	MutexLock lock(mutex);
	Event event;
	event.type = p_type;
	event.data = p_data;
	event_queue.push_back(event);
}

RCONClient::RCONClient() {
}

RCONClient::~RCONClient() {
	if (thread_running) {
		disconnect_from_server();
	}
}
