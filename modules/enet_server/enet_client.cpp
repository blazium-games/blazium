/**************************************************************************/
/*  enet_client.cpp                                                       */
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

#include "enet_client.h"

#include "core/os/os.h"
#include "core/variant/callable.h"
#include "enet_packet.h"

ENetClient *ENetClient::singleton = nullptr;

ENetClient *ENetClient::get_singleton() {
	return singleton;
}

void ENetClient::_bind_methods() {
	// Connection management
	ClassDB::bind_method(D_METHOD("connect_to_server", "address", "port", "channels"), &ENetClient::connect_to_server, DEFVAL(2));
	ClassDB::bind_method(D_METHOD("disconnect_from_server", "reason"), &ENetClient::disconnect_from_server, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("is_connected_to_server"), &ENetClient::is_connected_to_server);
	ClassDB::bind_method(D_METHOD("get_connection_status"), &ENetClient::get_connection_status);
	ClassDB::bind_method(D_METHOD("get_server_address"), &ENetClient::get_server_address);
	ClassDB::bind_method(D_METHOD("get_server_port"), &ENetClient::get_server_port);
	ClassDB::bind_method(D_METHOD("get_ping"), &ENetClient::get_ping);

	// Configuration
	ClassDB::bind_method(D_METHOD("set_compression_mode", "mode"), &ENetClient::set_compression_mode);
	ClassDB::bind_method(D_METHOD("set_poll_rate", "rate_ms"), &ENetClient::set_poll_rate);
	ClassDB::bind_method(D_METHOD("get_poll_rate"), &ENetClient::get_poll_rate);

	// Packet sending
	ClassDB::bind_method(D_METHOD("send_packet", "packet", "channel", "reliable"), &ENetClient::send_packet, DEFVAL(0), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("send_raw_packet", "data", "channel", "reliable"), &ENetClient::send_raw_packet, DEFVAL(0), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("send_node_state", "node", "flags", "channel"), &ENetClient::send_node_state, DEFVAL(0));

	// Statistics
	ClassDB::bind_method(D_METHOD("get_statistic", "statistic"), &ENetClient::get_statistic);

	// Custom events
	ClassDB::bind_method(D_METHOD("register_event", "event_name", "callback"), &ENetClient::register_event);
	ClassDB::bind_method(D_METHOD("unregister_event", "event_name"), &ENetClient::unregister_event);
	ClassDB::bind_method(D_METHOD("has_event", "event_name"), &ENetClient::has_event);
	ClassDB::bind_method(D_METHOD("trigger_event", "event_name", "payload", "channel", "reliable"), &ENetClient::trigger_event, DEFVAL(0), DEFVAL(true));

	// Signals
	ADD_SIGNAL(MethodInfo("connected_to_server"));
	ADD_SIGNAL(MethodInfo("disconnected_from_server", PropertyInfo(Variant::STRING, "reason")));
	ADD_SIGNAL(MethodInfo("connection_failed", PropertyInfo(Variant::STRING, "reason")));
	ADD_SIGNAL(MethodInfo("packet_received", PropertyInfo(Variant::NIL, "packet"), PropertyInfo(Variant::INT, "channel")));
	ADD_SIGNAL(MethodInfo("raw_packet_received", PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data"), PropertyInfo(Variant::INT, "channel")));
	ADD_SIGNAL(MethodInfo("custom_event_received", PropertyInfo(Variant::STRING, "event_name"), PropertyInfo(Variant::NIL, "payload"), PropertyInfo(Variant::INT, "channel")));
	ADD_SIGNAL(MethodInfo("unknown_event_received", PropertyInfo(Variant::STRING, "event_name"), PropertyInfo(Variant::NIL, "payload"), PropertyInfo(Variant::INT, "channel")));

	// Enums
	BIND_ENUM_CONSTANT(STATUS_DISCONNECTED);
	BIND_ENUM_CONSTANT(STATUS_CONNECTING);
	BIND_ENUM_CONSTANT(STATUS_CONNECTED);
}

void ENetClient::_process_events() {
	if (connection_status != STATUS_DISCONNECTED) {
		_process_event_queue();
	}
}

Error ENetClient::connect_to_server(const String &p_address, int p_port, int p_channels) {
	ERR_FAIL_COND_V(connection_status != STATUS_DISCONNECTED, ERR_ALREADY_IN_USE);

	connection.instantiate();
	Error err = connection->create_host(1, p_channels, 0, 0);
	if (err != OK) {
		connection.unref();
		return err;
	}

	server_peer = connection->connect_to_host(p_address, p_port, p_channels, 0);
	if (!server_peer.is_valid()) {
		connection->destroy();
		connection.unref();
		return ERR_CANT_CONNECT;
	}

	server_address = p_address;
	server_port = p_port;
	connection_status = STATUS_CONNECTING;
	thread_running = true;

	// Start polling thread
	polling_thread.start(_poll_thread_func, this);

	// Connect to SceneTree for processing (if available)
	SceneTree *scene_tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (scene_tree && !scene_tree->is_connected("process_frame", callable_mp(this, &ENetClient::_process_events))) {
		scene_tree->connect("process_frame", callable_mp(this, &ENetClient::_process_events));
	}

	return OK;
}

void ENetClient::disconnect_from_server(const String &p_reason) {
	// Stop thread
	thread_running = false;
	if (polling_thread.is_started()) {
		polling_thread.wait_to_finish();
	}

	// Disconnect from server
	if (server_peer.is_valid()) {
		if (connection_status != STATUS_DISCONNECTED) {
			server_peer->peer_disconnect_now(0);
		}
		server_peer.unref();
	}

	// Disconnect from SceneTree
	SceneTree *scene_tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (scene_tree && scene_tree->is_connected("process_frame", callable_mp(this, &ENetClient::_process_events))) {
		scene_tree->disconnect("process_frame", callable_mp(this, &ENetClient::_process_events));
	}

	// Destroy connection
	if (connection.is_valid()) {
		connection->destroy();
		connection.unref();
	}

	if (connection_status != STATUS_DISCONNECTED) {
		connection_status = STATUS_DISCONNECTED;
		call_deferred("emit_signal", "disconnected_from_server", p_reason);
	}
}

bool ENetClient::is_connected_to_server() const {
	return connection_status == STATUS_CONNECTED;
}

ENetClient::ConnectionStatus ENetClient::get_connection_status() const {
	return connection_status;
}

String ENetClient::get_server_address() const {
	return server_address;
}

int ENetClient::get_server_port() const {
	return server_port;
}

int ENetClient::get_ping() const {
	if (server_peer.is_valid()) {
		return server_peer->get_statistic(ENetPacketPeer::PEER_ROUND_TRIP_TIME);
	}
	return 0;
}

void ENetClient::set_compression_mode(ENetConnection::CompressionMode p_mode) {
	if (connection.is_valid()) {
		connection->compress(p_mode);
	}
}

void ENetClient::set_poll_rate(int p_rate_ms) {
	poll_rate_ms = MAX(1, p_rate_ms);
}

int ENetClient::get_poll_rate() const {
	return poll_rate_ms;
}

Error ENetClient::send_packet(const Variant &p_packet, int p_channel, bool p_reliable) {
	ERR_FAIL_COND_V(!server_peer.is_valid(), ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(connection_status != STATUS_CONNECTED, ERR_UNCONFIGURED);

	// Encode packet using auto-detection
	PackedByteArray data = ENetPacketUtils::get_singleton()->encode_auto(p_packet);
	ERR_FAIL_COND_V(data.size() == 0, ERR_INVALID_DATA);

	// Send via ENet using public send method
	int flags = p_reliable ? ENetPacketPeer::FLAG_RELIABLE : ENetPacketPeer::FLAG_UNSEQUENCED;

	// Create ENet packet
	ENetPacket *enet_packet = enet_packet_create(data.ptr(), data.size(), flags);
	ERR_FAIL_NULL_V(enet_packet, ERR_CANT_CREATE);

	return (Error)server_peer->send(p_channel, enet_packet);
}

Error ENetClient::send_raw_packet(const PackedByteArray &p_data, int p_channel, bool p_reliable) {
	ERR_FAIL_COND_V(!server_peer.is_valid(), ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(connection_status != STATUS_CONNECTED, ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(p_data.size() == 0, ERR_INVALID_DATA);

	int flags = p_reliable ? ENetPacketPeer::FLAG_RELIABLE : ENetPacketPeer::FLAG_UNSEQUENCED;
	ENetPacket *enet_packet = enet_packet_create(p_data.ptr(), p_data.size(), flags);
	ERR_FAIL_NULL_V(enet_packet, ERR_CANT_CREATE);

	return (Error)server_peer->send(p_channel, enet_packet);
}

Error ENetClient::send_node_state(Node *p_node, int p_flags, int p_channel) {
	ERR_FAIL_NULL_V(p_node, ERR_INVALID_PARAMETER);
	PackedByteArray data = ENetPacketUtils::get_singleton()->encode_node(p_node, p_flags);
	return send_raw_packet(data, p_channel, true);
}

double ENetClient::get_statistic(ENetPacketPeer::PeerStatistic p_stat) {
	if (server_peer.is_valid()) {
		return server_peer->get_statistic(p_stat);
	}
	return 0.0;
}

void ENetClient::register_event(const String &p_event_name, const Callable &p_callback) {
	event_handlers_mutex.lock();
	event_handlers[p_event_name] = p_callback;
	event_handlers_mutex.unlock();
}

void ENetClient::unregister_event(const String &p_event_name) {
	event_handlers_mutex.lock();
	event_handlers.erase(p_event_name);
	event_handlers_mutex.unlock();
}

bool ENetClient::has_event(const String &p_event_name) const {
	event_handlers_mutex.lock();
	bool has = event_handlers.has(p_event_name);
	event_handlers_mutex.unlock();
	return has;
}

Error ENetClient::trigger_event(const String &p_event_name, const Dictionary &p_payload, int p_channel, bool p_reliable) {
	Dictionary event_packet = p_payload.duplicate();
	event_packet["_event"] = p_event_name;
	return send_packet(event_packet, p_channel, p_reliable);
}

void ENetClient::_poll_thread_func(void *p_user_data) {
	ENetClient *client = static_cast<ENetClient *>(p_user_data);
	client->_poll_thread();
}

void ENetClient::_poll_thread() {
	while (thread_running) {
		_process_enet_events();
		OS::get_singleton()->delay_usec(poll_rate_ms * 1000);
	}
}

void ENetClient::_process_enet_events() {
	if (!connection.is_valid()) {
		return;
	}

	ENetConnection::Event event;
	ENetConnection::EventType event_type = connection->service(0, event);

	if (event_type == ENetConnection::EVENT_CONNECT) {
		connection_status = STATUS_CONNECTED;
		QueuedEvent queued_event;
		queued_event.type = EVENT_CONNECTED;
		_queue_event(queued_event);
	} else if (event_type == ENetConnection::EVENT_DISCONNECT) {
		connection_status = STATUS_DISCONNECTED;
		QueuedEvent queued_event;
		queued_event.type = EVENT_DISCONNECTED;
		queued_event.reason = "Connection lost";
		_queue_event(queued_event);
		thread_running = false;
	} else if (event_type == ENetConnection::EVENT_RECEIVE) {
		if (event.packet) {
			PackedByteArray packet_data;
			packet_data.resize(event.packet->dataLength);
			memcpy(packet_data.ptrw(), event.packet->data, event.packet->dataLength);

			QueuedEvent queued_event;
			queued_event.type = EVENT_PACKET_RECEIVED;
			queued_event.data = packet_data;
			queued_event.channel = event.channel_id;
			_queue_event(queued_event);

			enet_packet_destroy(event.packet);
		}
	}
}

void ENetClient::_queue_event(const QueuedEvent &p_event) {
	events_mutex.lock();
	event_queue.push_back(p_event);
	events_mutex.unlock();
}

void ENetClient::_process_event_queue() {
	events_mutex.lock();
	List<QueuedEvent> events = event_queue;
	event_queue.clear();
	events_mutex.unlock();

	for (const QueuedEvent &event : events) {
		_emit_event(event);
	}
}

void ENetClient::_emit_event(const QueuedEvent &p_event) {
	switch (p_event.type) {
		case EVENT_CONNECTED: {
			emit_signal("connected_to_server");
		} break;

		case EVENT_DISCONNECTED: {
			emit_signal("disconnected_from_server", p_event.reason);
		} break;

		case EVENT_PACKET_RECEIVED: {
			PackedByteArray raw_data = p_event.data;
			Variant decoded = ENetPacketUtils::get_singleton()->decode_auto(raw_data);

			// Check if this is a custom event
			if (decoded.get_type() == Variant::DICTIONARY) {
				Dictionary dict = decoded;
				if (dict.has("_event")) {
					String event_name = dict["_event"];
					Dictionary payload = dict.duplicate();
					payload.erase("_event");

					// Call registered handler if exists
					event_handlers_mutex.lock();
					bool has_handler = event_handlers.has(event_name);
					Callable handler;
					if (has_handler) {
						handler = event_handlers[event_name];
					}
					event_handlers_mutex.unlock();

					if (has_handler && handler.is_valid()) {
						// Call handler on main thread
						handler.call(payload, p_event.channel);
						// Emit custom event signal
						emit_signal("custom_event_received", event_name, payload, p_event.channel);
						return; // Don't emit regular packet_received for known custom events
					} else {
						// No handler registered - emit unknown event signal
						emit_signal("unknown_event_received", event_name, payload, p_event.channel);
					}
				}
			}

			emit_signal("packet_received", decoded, p_event.channel);
			emit_signal("raw_packet_received", raw_data, p_event.channel);
		} break;
	}
}

ENetClient::ENetClient() {
	singleton = this;
	connection_status = STATUS_DISCONNECTED;
	thread_running = false;
	poll_rate_ms = 10;
	server_port = 0;
}

ENetClient::~ENetClient() {
	disconnect_from_server();
	singleton = nullptr;
}
