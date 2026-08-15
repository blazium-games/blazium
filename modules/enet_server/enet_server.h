/**************************************************************************/
/*  enet_server.h                                                         */
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

#include "enet_server_peer.h"

#include "core/object/object.h"
#include "core/os/mutex.h"
#include "core/os/thread.h"
#include "core/variant/typed_array.h"
#include "modules/enet/enet_connection.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"

class ENetServer : public Object {
	GDCLASS(ENetServer, Object);

public:
	enum AuthMode {
		AUTH_NONE,
		AUTH_PRELOGIN_ONLY,
		AUTH_CUSTOM,
	};

	enum EventType {
		EVENT_PEER_CONNECTING,
		EVENT_PEER_PRELOGIN,
		EVENT_PEER_AUTHENTICATED,
		EVENT_PEER_DISCONNECTED,
		EVENT_PACKET_RECEIVED,
	};

	struct QueuedEvent {
		EventType type;
		int peer_id;
		Ref<ENetServerPeer> peer;
		Variant data;
		int channel;
		String reason;
	};

private:
	static ENetServer *singleton;

	Ref<ENetConnection> connection;
	HashMap<int, Ref<ENetServerPeer>> peers;
	Mutex peers_mutex;
	Mutex events_mutex;
	Thread polling_thread;
	List<QueuedEvent> event_queue;

	bool server_active;
	bool thread_running;
	int poll_rate_ms;
	int next_peer_id;
	int local_port;

	// Authentication
	AuthMode auth_mode;
	float auth_timeout;
	Callable custom_authenticator;

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

	// Peer management
	void _handle_peer_connect(const Ref<ENetPacketPeer> &p_packet_peer, int p_data);
	void _handle_peer_disconnect(int p_peer_id, const String &p_reason);
	void _handle_packet_received(int p_peer_id, const PackedByteArray &p_data, int p_channel);

protected:
	static void _bind_methods();
	void _process_events();

public:
	static ENetServer *get_singleton();

	// Server management
	Error create_server(int p_port, int p_max_peers = 32, int p_max_channels = 2);
	void stop_server();
	bool is_server_active() const;
	int get_local_port() const;
	void set_compression_mode(ENetConnection::CompressionMode p_mode);
	void set_poll_rate(int p_rate_ms);
	int get_poll_rate() const;

	// Peer management
	TypedArray<ENetServerPeer> get_peers();
	Ref<ENetServerPeer> get_peer(int p_peer_id);
	void kick_peer(int p_peer_id, const String &p_reason = "");
	int get_peer_count() const;
	int get_authenticated_peer_count() const;

	// Packet sending
	Error send_packet(int p_peer_id, const Variant &p_packet, int p_channel = 0, bool p_reliable = true);
	void send_packet_to_all(const Variant &p_packet, int p_channel = 0, bool p_reliable = true, const TypedArray<int> &p_exclude = TypedArray<int>());
	void send_packet_to_multiple(const TypedArray<int> &p_peer_ids, const Variant &p_packet, int p_channel = 0, bool p_reliable = true);
	void broadcast_packet(const Variant &p_packet, int p_channel = 0, bool p_reliable = true);

	// Node-based sending
	Error send_node_state(int p_peer_id, Node *p_node, int p_flags, int p_channel = 0);
	void send_node_to_all(Node *p_node, int p_flags, int p_channel = 0);

	// Authentication
	void set_authentication_mode(AuthMode p_mode);
	AuthMode get_authentication_mode() const;
	void set_authentication_timeout(float p_timeout);
	float get_authentication_timeout() const;
	void set_custom_authenticator(const Callable &p_callable);
	void queue_peer_authenticated(int p_peer_id);

	// Custom events
	void register_event(const String &p_event_name, const Callable &p_callback);
	void unregister_event(const String &p_event_name);
	bool has_event(const String &p_event_name) const;
	Error trigger_event(int p_peer_id, const String &p_event_name, const Dictionary &p_payload, int p_channel = 0, bool p_reliable = true);
	void trigger_event_to_all(const String &p_event_name, const Dictionary &p_payload, int p_channel = 0, bool p_reliable = true, const TypedArray<int> &p_exclude = TypedArray<int>());

	ENetServer();
	~ENetServer();
};

VARIANT_ENUM_CAST(ENetServer::AuthMode);
