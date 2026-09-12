/**************************************************************************/
/*  rcon_server.h                                                         */
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

#include "core/io/packet_peer_udp.h"
#include "core/io/stream_peer_tcp.h"
#include "core/io/tcp_server.h"
#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/os/thread.h"
#include "core/templates/hash_map.h"
#include "core/templates/list.h"

class RCONServer : public RefCounted {
	GDCLASS(RCONServer, RefCounted);

public:
	enum Protocol {
		PROTOCOL_SOURCE,
		PROTOCOL_BATTLEYE,
	};

	enum State {
		STATE_STOPPED,
		STATE_STARTING,
		STATE_LISTENING,
		STATE_ERROR,
	};

private:
	Protocol protocol = PROTOCOL_SOURCE;
	State state = STATE_STOPPED;
	int port = 0;
	String password;

	// TCP Server (Source RCON)
	Ref<TCPServer> tcp_server;

	// UDP Server (BattlEye RCON)
	Ref<PacketPeerUDP> udp_peer;

	// Client management
	struct Client {
		int id;
		bool authenticated = false;
		Ref<StreamPeerTCP> tcp_peer;
		IPAddress udp_address;
		int udp_port = 0;
		PackedByteArray tcp_buffer;
		uint64_t last_activity = 0;
		int next_request_id = 1;
		HashMap<int, String> multi_packet_responses; // For Source multi-packet responses
	};

	HashMap<int, Client> clients;
	int next_client_id = 1;

	// Command registration
	struct RegisteredCommand {
		String name;
		Callable callback;
	};
	HashMap<String, RegisteredCommand> registered_commands;

	// Threading
	Thread network_thread;
	Mutex mutex;
	bool thread_running = false;
	bool stop_thread = false;

	// Keep-alive monitoring (BattlEye)
	const uint64_t CLIENT_TIMEOUT_MS = 45000; // 45 seconds
	const uint64_t TIMEOUT_WARNING_MS = 35000; // Warn at 35 seconds

	// Event queue for main thread
	struct Event {
		enum Type {
			EVENT_SERVER_STARTED,
			EVENT_SERVER_STOPPED,
			EVENT_CLIENT_CONNECTED,
			EVENT_CLIENT_DISCONNECTED,
			EVENT_CLIENT_AUTHENTICATED,
			EVENT_AUTH_FAILED,
			EVENT_COMMAND_RECEIVED,
			EVENT_RAW_PACKET_RECEIVED,
			EVENT_PACKET_SENT,
			EVENT_PACKET_SEND_FAILED,
			EVENT_KEEPALIVE_SENT,
			EVENT_KEEPALIVE_TIMEOUT,
			EVENT_CLIENT_TIMEOUT_WARNING,
			EVENT_SERVER_ERROR,
			EVENT_ECHO_REQUEST,
		};
		Type type;
		int client_id = 0;
		Dictionary data;
	};

	List<Event> event_queue;

	// Response queue (from main thread to network thread)
	struct Response {
		int client_id;
		int request_id;
		String data;
		PackedByteArray raw_packet;
		bool is_raw = false;
	};
	List<Response> response_queue;

	// Network thread functions
	static void _network_thread_func(void *p_user);
	void _process_network_source();
	void _process_network_battleye();
	void _accept_new_clients_tcp();
	void _process_client_tcp(Client &p_client);
	void _process_packet_udp();
	void _check_client_timeouts();

	// Packet processing
	void _process_source_packet(Client &p_client, const PackedByteArray &p_packet);
	void _process_battleye_packet(const IPAddress &p_address, int p_port, const PackedByteArray &p_packet);

	// Helper methods
	Client *_find_client_by_udp_address(const IPAddress &p_address, int p_port);
	void _disconnect_client(int p_client_id);
	void _send_response_to_client(int p_client_id, int p_request_id, const String &p_response);
	void _send_raw_to_client(int p_client_id, const PackedByteArray &p_packet);

	// Queue event for main thread
	void _queue_event(Event::Type p_type, int p_client_id, const Dictionary &p_data = Dictionary());
	void _queue_event_unlocked(Event::Type p_type, int p_client_id, const Dictionary &p_data = Dictionary());

protected:
	static void _bind_methods();

public:
	// High-level API
	Error start_server(int p_port, const String &p_password, Protocol p_protocol);
	void stop_server();
	void register_command(const String &p_command, const Callable &p_callback);
	void unregister_command(const String &p_command);
	void send_response(int p_client_id, int p_request_id, const String &p_response);

	// Low-level API
	void send_raw_packet(int p_client_id, const PackedByteArray &p_packet);

	// Status
	bool is_running() const;
	State get_state() const;
	Protocol get_protocol() const;
	Array get_connected_clients() const;

	// Must be called regularly to dispatch events
	void poll();

	RCONServer();
	~RCONServer();
};

VARIANT_ENUM_CAST(RCONServer::Protocol);
VARIANT_ENUM_CAST(RCONServer::State);
