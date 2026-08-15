/**************************************************************************/
/*  rcon_client.h                                                         */
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
#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/os/thread.h"
#include "core/templates/hash_map.h"
#include "core/templates/list.h"

class RCONClient : public RefCounted {
	GDCLASS(RCONClient, RefCounted);

public:
	enum Protocol {
		PROTOCOL_SOURCE,
		PROTOCOL_BATTLEYE,
	};

	enum State {
		STATE_DISCONNECTED,
		STATE_CONNECTING,
		STATE_AUTHENTICATING,
		STATE_CONNECTED,
		STATE_ERROR,
	};

private:
	Protocol protocol = PROTOCOL_SOURCE;
	State state = STATE_DISCONNECTED;
	String host;
	int port = 0;
	String password;

	// TCP (Source RCON)
	Ref<StreamPeerTCP> tcp_peer;
	int next_request_id = 1;

	// UDP (BattlEye RCON)
	Ref<PacketPeerUDP> udp_peer;
	uint8_t command_seq = 0;

	// Threading
	Thread network_thread;
	Mutex mutex;
	bool thread_running = false;
	bool stop_thread = false;

	// Keep-alive (BattlEye)
	uint64_t last_keepalive_time = 0;
	const uint64_t KEEPALIVE_INTERVAL_MS = 30000; // 30 seconds

	// Command tracking
	struct PendingCommand {
		String command;
		Callable callback;
		int request_id = 0;
		uint8_t seq = 0;
		HashMap<int, String> multi_packet_data; // For BattlEye multi-packet responses
	};

	List<PendingCommand> pending_commands;

	// Event queue for main thread
	struct Event {
		enum Type {
			EVENT_CONNECTED,
			EVENT_DISCONNECTED,
			EVENT_AUTHENTICATED,
			EVENT_AUTH_FAILED,
			EVENT_COMMAND_RESPONSE,
			EVENT_SERVER_MESSAGE,
			EVENT_RAW_PACKET,
			EVENT_ERROR,
		};
		Type type;
		Dictionary data;
	};

	List<Event> event_queue;

	// Network thread functions
	static void _network_thread_func(void *p_user);
	void _process_network_source();
	void _process_network_battleye();
	void _send_keepalive_battleye();

	// Processing functions
	void _process_source_packet(const PackedByteArray &p_packet);
	void _process_battleye_packet(const PackedByteArray &p_packet);

	// Queue event for main thread
	void _queue_event(Event::Type p_type, const Dictionary &p_data = Dictionary());

protected:
	static void _bind_methods();

public:
	// High-level API
	Error connect_to_server(const String &p_host, int p_port, const String &p_password, Protocol p_protocol);
	void disconnect_from_server();
	void send_command(const String &p_command, const Callable &p_callback = Callable());
	String send_command_sync(const String &p_command, float p_timeout = 5.0);

	// Low-level API
	void send_raw_packet(const PackedByteArray &p_packet);

	// Status
	bool is_authenticated() const;
	State get_state() const;
	Protocol get_protocol() const;

	// Must be called regularly to dispatch events
	void poll();

	RCONClient();
	~RCONClient();
};

VARIANT_ENUM_CAST(RCONClient::Protocol);
VARIANT_ENUM_CAST(RCONClient::State);
