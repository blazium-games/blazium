/**************************************************************************/
/*  enet_client.h                                                         */
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

#include "core/object/object.h"
#include "core/os/mutex.h"
#include "core/os/thread.h"
#include "modules/enet/enet_connection.h"
#include "modules/enet/enet_packet_peer.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"

#include <enet/enet.h>

class ENetClient : public Object {
	GDCLASS(ENetClient, Object);

public:
	enum ConnectionStatus {
		STATUS_DISCONNECTED,
		STATUS_CONNECTING,
		STATUS_CONNECTED,
	};

	enum EventType {
		EVENT_CONNECTED,
		EVENT_DISCONNECTED,
		EVENT_PACKET_RECEIVED,
	};

	struct QueuedEvent {
		EventType type;
		Variant data;
		int channel;
		String reason;
	};

private:
	static ENetClient *singleton;

	Ref<ENetConnection> connection;
	Ref<ENetPacketPeer> server_peer;
	Mutex events_mutex;
	Thread polling_thread;
	List<QueuedEvent> event_queue;

	ConnectionStatus connection_status;
	bool thread_running;
	int poll_rate_ms;
	String server_address;
	int server_port;

	// Custom events
	HashMap<String, Callable> event_handlers;
	Mutex event_handlers_mutex;

	// Threading
	static void _poll_thread_func(void *p_user_data);
	void _poll_thread();
	void _process_enet_events();
	void _queue_event(const QueuedEvent &p_event);

	// Event processing (main thread)
	void _process_event_queue();
	void _emit_event(const QueuedEvent &p_event);

protected:
	static void _bind_methods();
	void _process_events();

public:
	static ENetClient *get_singleton();

	// Connection management
	Error connect_to_server(const String &p_address, int p_port, int p_channels = 2);
	void disconnect_from_server(const String &p_reason = "");
	bool is_connected_to_server() const;
	ConnectionStatus get_connection_status() const;
	String get_server_address() const;
	int get_server_port() const;
	int get_ping() const;

	// Configuration
	void set_compression_mode(ENetConnection::CompressionMode p_mode);
	void set_poll_rate(int p_rate_ms);
	int get_poll_rate() const;

	// Packet sending
	Error send_packet(const Variant &p_packet, int p_channel = 0, bool p_reliable = true);
	Error send_raw_packet(const PackedByteArray &p_data, int p_channel = 0, bool p_reliable = true);
	Error send_node_state(Node *p_node, int p_flags, int p_channel = 0);

	// Statistics
	double get_statistic(ENetPacketPeer::PeerStatistic p_stat);

	// Custom events
	void register_event(const String &p_event_name, const Callable &p_callback);
	void unregister_event(const String &p_event_name);
	bool has_event(const String &p_event_name) const;
	Error trigger_event(const String &p_event_name, const Dictionary &p_payload, int p_channel = 0, bool p_reliable = true);

	ENetClient();
	~ENetClient();
};

VARIANT_ENUM_CAST(ENetClient::ConnectionStatus);
