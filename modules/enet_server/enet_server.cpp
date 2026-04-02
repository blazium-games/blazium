/**************************************************************************/
/*  enet_server.cpp                                                       */
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

#include "enet_server.h"

#include "core/os/os.h"
#include "core/variant/callable.h"
#include "core/variant/typed_array.h"
#include "enet_packet.h"

ENetServer *ENetServer::singleton = nullptr;

ENetServer *ENetServer::get_singleton() {
	return singleton;
}

void ENetServer::_bind_methods() {
	// Server management
	ClassDB::bind_method(D_METHOD("create_server", "port", "max_peers", "max_channels"), &ENetServer::create_server, DEFVAL(32), DEFVAL(2));
	ClassDB::bind_method(D_METHOD("stop_server"), &ENetServer::stop_server);
	ClassDB::bind_method(D_METHOD("is_server_active"), &ENetServer::is_server_active);
	ClassDB::bind_method(D_METHOD("get_local_port"), &ENetServer::get_local_port);
	ClassDB::bind_method(D_METHOD("set_compression_mode", "mode"), &ENetServer::set_compression_mode);
	ClassDB::bind_method(D_METHOD("set_poll_rate", "rate_ms"), &ENetServer::set_poll_rate);
	ClassDB::bind_method(D_METHOD("get_poll_rate"), &ENetServer::get_poll_rate);

	// Peer management
	ClassDB::bind_method(D_METHOD("get_peers"), &ENetServer::get_peers);
	ClassDB::bind_method(D_METHOD("get_peer", "peer_id"), &ENetServer::get_peer);
	ClassDB::bind_method(D_METHOD("kick_peer", "peer_id", "reason"), &ENetServer::kick_peer, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("get_peer_count"), &ENetServer::get_peer_count);
	ClassDB::bind_method(D_METHOD("get_authenticated_peer_count"), &ENetServer::get_authenticated_peer_count);

	// Packet sending
	ClassDB::bind_method(D_METHOD("send_packet", "peer_id", "packet", "channel", "reliable"), &ENetServer::send_packet, DEFVAL(0), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("send_packet_to_all", "packet", "channel", "reliable", "exclude"), &ENetServer::send_packet_to_all, DEFVAL(0), DEFVAL(true), DEFVAL(TypedArray<int>()));
	ClassDB::bind_method(D_METHOD("send_packet_to_multiple", "peer_ids", "packet", "channel", "reliable"), &ENetServer::send_packet_to_multiple, DEFVAL(0), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("broadcast_packet", "packet", "channel", "reliable"), &ENetServer::broadcast_packet, DEFVAL(0), DEFVAL(true));

	// Node-based sending
	ClassDB::bind_method(D_METHOD("send_node_state", "peer_id", "node", "flags", "channel"), &ENetServer::send_node_state, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("send_node_to_all", "node", "flags", "channel"), &ENetServer::send_node_to_all, DEFVAL(0));

	// Authentication
	ClassDB::bind_method(D_METHOD("set_authentication_mode", "mode"), &ENetServer::set_authentication_mode);
	ClassDB::bind_method(D_METHOD("get_authentication_mode"), &ENetServer::get_authentication_mode);
	ClassDB::bind_method(D_METHOD("set_authentication_timeout", "timeout"), &ENetServer::set_authentication_timeout);
	ClassDB::bind_method(D_METHOD("get_authentication_timeout"), &ENetServer::get_authentication_timeout);
	ClassDB::bind_method(D_METHOD("set_custom_authenticator", "callable"), &ENetServer::set_custom_authenticator);

	// Custom events
	ClassDB::bind_method(D_METHOD("register_event", "event_name", "callback"), &ENetServer::register_event);
	ClassDB::bind_method(D_METHOD("unregister_event", "event_name"), &ENetServer::unregister_event);
	ClassDB::bind_method(D_METHOD("has_event", "event_name"), &ENetServer::has_event);
	ClassDB::bind_method(D_METHOD("trigger_event", "peer_id", "event_name", "payload", "channel", "reliable"), &ENetServer::trigger_event, DEFVAL(0), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("trigger_event_to_all", "event_name", "payload", "channel", "reliable", "exclude"), &ENetServer::trigger_event_to_all, DEFVAL(0), DEFVAL(true), DEFVAL(TypedArray<int>()));

	// Signals
	ADD_SIGNAL(MethodInfo("peer_connecting", PropertyInfo(Variant::OBJECT, "peer", PROPERTY_HINT_RESOURCE_TYPE, "ENetServerPeer"), PropertyInfo(Variant::INT, "data")));
	ADD_SIGNAL(MethodInfo("peer_prelogin", PropertyInfo(Variant::OBJECT, "peer", PROPERTY_HINT_RESOURCE_TYPE, "ENetServerPeer"), PropertyInfo(Variant::DICTIONARY, "login_data")));
	ADD_SIGNAL(MethodInfo("peer_authenticated", PropertyInfo(Variant::OBJECT, "peer", PROPERTY_HINT_RESOURCE_TYPE, "ENetServerPeer")));
	ADD_SIGNAL(MethodInfo("peer_disconnected", PropertyInfo(Variant::OBJECT, "peer", PROPERTY_HINT_RESOURCE_TYPE, "ENetServerPeer"), PropertyInfo(Variant::STRING, "reason")));
	ADD_SIGNAL(MethodInfo("packet_received", PropertyInfo(Variant::OBJECT, "peer", PROPERTY_HINT_RESOURCE_TYPE, "ENetServerPeer"), PropertyInfo(Variant::NIL, "packet"), PropertyInfo(Variant::INT, "channel")));
	ADD_SIGNAL(MethodInfo("raw_packet_received", PropertyInfo(Variant::OBJECT, "peer", PROPERTY_HINT_RESOURCE_TYPE, "ENetServerPeer"), PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data"), PropertyInfo(Variant::INT, "channel")));
	ADD_SIGNAL(MethodInfo("custom_event_received", PropertyInfo(Variant::OBJECT, "peer", PROPERTY_HINT_RESOURCE_TYPE, "ENetServerPeer"), PropertyInfo(Variant::STRING, "event_name"), PropertyInfo(Variant::NIL, "payload"), PropertyInfo(Variant::INT, "channel")));
	ADD_SIGNAL(MethodInfo("unknown_event_received", PropertyInfo(Variant::OBJECT, "peer", PROPERTY_HINT_RESOURCE_TYPE, "ENetServerPeer"), PropertyInfo(Variant::STRING, "event_name"), PropertyInfo(Variant::NIL, "payload"), PropertyInfo(Variant::INT, "channel")));
	ADD_SIGNAL(MethodInfo("server_started", PropertyInfo(Variant::INT, "port")));
	ADD_SIGNAL(MethodInfo("server_stopped"));

	// Enums
	BIND_ENUM_CONSTANT(AUTH_NONE);
	BIND_ENUM_CONSTANT(AUTH_PRELOGIN_ONLY);
	BIND_ENUM_CONSTANT(AUTH_CUSTOM);
}

void ENetServer::_process_events() {
	if (server_active) {
		_process_event_queue();
	}
}

Error ENetServer::create_server(int p_port, int p_max_peers, int p_max_channels) {
	ERR_FAIL_COND_V(server_active, ERR_ALREADY_IN_USE);

	connection.instantiate();
	Error err = connection->create_host_bound(IPAddress("*"), p_port, p_max_peers, p_max_channels, 0, 0);
	if (err != OK) {
		connection.unref();
		return err;
	}

	local_port = p_port;
	server_active = true;
	thread_running = true;
	next_peer_id = 1;

	// Start polling thread
	polling_thread.start(_poll_thread_func, this);

	// Connect to SceneTree for processing (if available)
	SceneTree *scene_tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (scene_tree && !scene_tree->is_connected("process_frame", callable_mp(this, &ENetServer::_process_events))) {
		scene_tree->connect("process_frame", callable_mp(this, &ENetServer::_process_events));
	}

	call_deferred("emit_signal", "server_started", p_port);

	return OK;
}

void ENetServer::stop_server() {
	if (!server_active) {
		return;
	}

	// Stop thread
	thread_running = false;
	if (polling_thread.is_started()) {
		polling_thread.wait_to_finish();
	}

	// Disconnect all peers
	peers_mutex.lock();
	for (KeyValue<int, Ref<ENetServerPeer>> &E : peers) {
		if (E.value.is_valid() && E.value->_get_packet_peer().is_valid()) {
			E.value->_get_packet_peer()->peer_disconnect_now(0);
		}
	}
	peers.clear();
	peers_mutex.unlock();

	// Disconnect from SceneTree
	SceneTree *scene_tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (scene_tree && scene_tree->is_connected("process_frame", callable_mp(this, &ENetServer::_process_events))) {
		scene_tree->disconnect("process_frame", callable_mp(this, &ENetServer::_process_events));
	}

	// Destroy connection
	if (connection.is_valid()) {
		connection->destroy();
		connection.unref();
	}

	server_active = false;
	auth_mode = AUTH_NONE;
	custom_authenticator = Callable();
	event_handlers.clear();

	call_deferred("emit_signal", "server_stopped");
}

bool ENetServer::is_server_active() const {
	return server_active;
}

int ENetServer::get_local_port() const {
	return local_port;
}

void ENetServer::set_compression_mode(ENetConnection::CompressionMode p_mode) {
	if (connection.is_valid()) {
		connection->compress(p_mode);
	}
}

void ENetServer::set_poll_rate(int p_rate_ms) {
	poll_rate_ms = MAX(1, p_rate_ms);
}

int ENetServer::get_poll_rate() const {
	return poll_rate_ms;
}

TypedArray<ENetServerPeer> ENetServer::get_peers() {
	TypedArray<ENetServerPeer> result;

	peers_mutex.lock();
	for (KeyValue<int, Ref<ENetServerPeer>> &E : peers) {
		result.push_back(E.value);
	}
	peers_mutex.unlock();

	return result;
}

Ref<ENetServerPeer> ENetServer::get_peer(int p_peer_id) {
	peers_mutex.lock();
	Ref<ENetServerPeer> peer = peers.has(p_peer_id) ? peers[p_peer_id] : Ref<ENetServerPeer>();
	peers_mutex.unlock();
	return peer;
}

void ENetServer::kick_peer(int p_peer_id, const String &p_reason) {
	Ref<ENetServerPeer> peer = get_peer(p_peer_id);
	if (peer.is_valid()) {
		peer->disconnect_peer(p_reason);
	}
}

int ENetServer::get_peer_count() const {
	peers_mutex.lock();
	int count = peers.size();
	peers_mutex.unlock();
	return count;
}

int ENetServer::get_authenticated_peer_count() const {
	int count = 0;

	peers_mutex.lock();
	for (const KeyValue<int, Ref<ENetServerPeer>> &E : peers) {
		if (E.value.is_valid() && E.value->get_auth_state() == ENetServerPeer::AUTH_APPROVED) {
			count++;
		}
	}
	peers_mutex.unlock();

	return count;
}

Error ENetServer::send_packet(int p_peer_id, const Variant &p_packet, int p_channel, bool p_reliable) {
	Ref<ENetServerPeer> peer = get_peer(p_peer_id);
	ERR_FAIL_COND_V(!peer.is_valid(), ERR_INVALID_PARAMETER);
	return peer->send_packet(p_packet, p_channel, p_reliable);
}

void ENetServer::send_packet_to_all(const Variant &p_packet, int p_channel, bool p_reliable, const TypedArray<int> &p_exclude) {
	PackedByteArray data = ENetPacketUtils::get_singleton()->encode_auto(p_packet);

	peers_mutex.lock();
	for (KeyValue<int, Ref<ENetServerPeer>> &E : peers) {
		bool excluded = false;
		for (int i = 0; i < p_exclude.size(); i++) {
			if ((int)p_exclude[i] == E.key) {
				excluded = true;
				break;
			}
		}

		if (!excluded && E.value.is_valid()) {
			E.value->send_packet(p_packet, p_channel, p_reliable);
		}
	}
	peers_mutex.unlock();
}

void ENetServer::send_packet_to_multiple(const TypedArray<int> &p_peer_ids, const Variant &p_packet, int p_channel, bool p_reliable) {
	for (int i = 0; i < p_peer_ids.size(); i++) {
		send_packet((int)p_peer_ids[i], p_packet, p_channel, p_reliable);
	}
}

void ENetServer::broadcast_packet(const Variant &p_packet, int p_channel, bool p_reliable) {
	send_packet_to_all(p_packet, p_channel, p_reliable, TypedArray<int>());
}

Error ENetServer::send_node_state(int p_peer_id, Node *p_node, int p_flags, int p_channel) {
	ERR_FAIL_NULL_V(p_node, ERR_INVALID_PARAMETER);
	PackedByteArray data = ENetPacketUtils::get_singleton()->encode_node(p_node, p_flags);
	Ref<ENetServerPeer> peer = get_peer(p_peer_id);
	ERR_FAIL_COND_V(peer.is_null(), ERR_DOES_NOT_EXIST);
	return peer->send_raw_packet(data, p_channel, true);
}

void ENetServer::send_node_to_all(Node *p_node, int p_flags, int p_channel) {
	ERR_FAIL_NULL(p_node);
	PackedByteArray data = ENetPacketUtils::get_singleton()->encode_node(p_node, p_flags);

	peers_mutex.lock();
	for (KeyValue<int, Ref<ENetServerPeer>> &E : peers) {
		E.value->send_raw_packet(data, p_channel, true);
	}
	peers_mutex.unlock();
}

void ENetServer::set_authentication_mode(AuthMode p_mode) {
	auth_mode = p_mode;
}

ENetServer::AuthMode ENetServer::get_authentication_mode() const {
	return auth_mode;
}

void ENetServer::set_authentication_timeout(float p_timeout) {
	auth_timeout = p_timeout;
}

float ENetServer::get_authentication_timeout() const {
	return auth_timeout;
}

void ENetServer::set_custom_authenticator(const Callable &p_callable) {
	custom_authenticator = p_callable;
}

void ENetServer::queue_peer_authenticated(int p_peer_id) {
	QueuedEvent auth_qe;
	auth_qe.type = EVENT_PEER_AUTHENTICATED;
	auth_qe.peer_id = p_peer_id;
	_queue_event(auth_qe);
}

void ENetServer::register_event(const String &p_event_name, const Callable &p_callback) {
	event_handlers_mutex.lock();
	event_handlers[p_event_name] = p_callback;
	event_handlers_mutex.unlock();
}

void ENetServer::unregister_event(const String &p_event_name) {
	event_handlers_mutex.lock();
	event_handlers.erase(p_event_name);
	event_handlers_mutex.unlock();
}

bool ENetServer::has_event(const String &p_event_name) const {
	event_handlers_mutex.lock();
	bool has = event_handlers.has(p_event_name);
	event_handlers_mutex.unlock();
	return has;
}

Error ENetServer::trigger_event(int p_peer_id, const String &p_event_name, const Dictionary &p_payload, int p_channel, bool p_reliable) {
	Dictionary event_packet = p_payload.duplicate();
	event_packet["_event"] = p_event_name;
	return send_packet(p_peer_id, event_packet, p_channel, p_reliable);
}

void ENetServer::trigger_event_to_all(const String &p_event_name, const Dictionary &p_payload, int p_channel, bool p_reliable, const TypedArray<int> &p_exclude) {
	Dictionary event_packet = p_payload.duplicate();
	event_packet["_event"] = p_event_name;
	send_packet_to_all(event_packet, p_channel, p_reliable, p_exclude);
}

void ENetServer::_poll_thread_func(void *p_user_data) {
	ENetServer *server = static_cast<ENetServer *>(p_user_data);
	server->_poll_thread();
}

void ENetServer::_poll_thread() {
	while (thread_running) {
		_process_enet_events();
		OS::get_singleton()->delay_usec(poll_rate_ms * 1000);
	}
}

void ENetServer::_process_enet_events() {
	if (!connection.is_valid()) {
		return;
	}

	ENetConnection::Event event;
	ENetConnection::EventType event_type = connection->service(0, event);

	if (event_type == ENetConnection::EVENT_CONNECT) {
		_handle_peer_connect(event.peer, event.data);
	} else if (event_type == ENetConnection::EVENT_DISCONNECT) {
		// Find peer by packet_peer
		int peer_id = -1;
		peers_mutex.lock();
		for (KeyValue<int, Ref<ENetServerPeer>> &E : peers) {
			if (E.value.is_valid() && E.value->_get_packet_peer() == event.peer) {
				peer_id = E.key;
				break;
			}
		}
		peers_mutex.unlock();

		if (peer_id != -1) {
			_handle_peer_disconnect(peer_id, "Connection lost");
		}
	} else if (event_type == ENetConnection::EVENT_RECEIVE) {
		int peer_id = -1;
		peers_mutex.lock();
		for (KeyValue<int, Ref<ENetServerPeer>> &E : peers) {
			if (E.value.is_valid() && E.value->_get_packet_peer() == event.peer) {
				peer_id = E.key;
				break;
			}
		}
		peers_mutex.unlock();

		if (peer_id != -1 && event.packet) {
			PackedByteArray packet_data;
			packet_data.resize(event.packet->dataLength);
			memcpy(packet_data.ptrw(), event.packet->data, event.packet->dataLength);

			QueuedEvent queued_event;
			queued_event.type = EVENT_PACKET_RECEIVED;
			queued_event.peer_id = peer_id;
			queued_event.peer = get_peer(peer_id);
			queued_event.data = packet_data;
			queued_event.channel = event.channel_id;

			_queue_event(queued_event);
		}
		if (event.packet) {
			enet_packet_destroy(event.packet);
		}
	}

	// Check for pending disconnects
	peers_mutex.lock();
	List<int> to_disconnect;
	for (KeyValue<int, Ref<ENetServerPeer>> &E : peers) {
		if (E.value.is_valid() && E.value->_has_pending_disconnect()) {
			to_disconnect.push_back(E.key);
		}
	}
	peers_mutex.unlock();

	for (int peer_id : to_disconnect) {
		Ref<ENetServerPeer> peer = get_peer(peer_id);
		if (peer.is_valid()) {
			String reason = peer->_get_disconnect_reason();
			if (peer->_get_packet_peer().is_valid()) {
				peer->_get_packet_peer()->peer_disconnect(0);
			}
			_handle_peer_disconnect(peer_id, reason);
		}
	}
}

void ENetServer::_handle_peer_connect(const Ref<ENetPacketPeer> &p_packet_peer, int p_data) {
	Ref<ENetServerPeer> peer = memnew(ENetServerPeer);
	int peer_id = next_peer_id++;

	peer->_set_packet_peer(p_packet_peer);
	peer->_set_peer_id(peer_id);
	peer->_set_connection_state(ENetServerPeer::STATE_CONNECTING);

	peers_mutex.lock();
	peers[peer_id] = peer;
	peers_mutex.unlock();

	QueuedEvent event;
	event.type = EVENT_PEER_CONNECTING;
	event.peer_id = peer_id;
	event.data = p_data;
	_queue_event(event);
}

void ENetServer::_handle_peer_disconnect(int p_peer_id, const String &p_reason) {
	Ref<ENetServerPeer> peer = get_peer(p_peer_id);
	if (!peer.is_valid()) {
		return;
	}

	QueuedEvent event;
	event.type = EVENT_PEER_DISCONNECTED;
	event.peer_id = p_peer_id;
	event.reason = p_reason;
	_queue_event(event);

	peers_mutex.lock();
	peers.erase(p_peer_id);
	peers_mutex.unlock();
}

void ENetServer::_handle_packet_received(int p_peer_id, const PackedByteArray &p_data, int p_channel) {
	QueuedEvent event;
	event.type = EVENT_PACKET_RECEIVED;
	event.peer_id = p_peer_id;
	event.data = p_data;
	event.channel = p_channel;
	_queue_event(event);
}

void ENetServer::_queue_event(const QueuedEvent &p_event) {
	QueuedEvent e = p_event;
	e.peer = get_peer(p_event.peer_id);
	events_mutex.lock();
	event_queue.push_back(e);
	events_mutex.unlock();
}

void ENetServer::_process_event_queue() {
	events_mutex.lock();
	List<QueuedEvent> events = event_queue;
	event_queue.clear();
	events_mutex.unlock();

	for (const QueuedEvent &event : events) {
		_emit_event(event);
	}
}

void ENetServer::_emit_event(const QueuedEvent &p_event) {
	Ref<ENetServerPeer> peer = p_event.peer;
	if (!peer.is_valid()) {
		return;
	}

	switch (p_event.type) {
		case EVENT_PEER_CONNECTING: {
			emit_signal("peer_connecting", peer, p_event.data);

			// Check if peer was rejected
			if (peer->get_auth_state() == ENetServerPeer::AUTH_REJECTED) {
				return;
			}

			// Move to prelogin if authentication is enabled
			if (auth_mode != AUTH_NONE) {
				peer->_set_connection_state(ENetServerPeer::STATE_PRELOGIN);
			} else {
				peer->_set_connection_state(ENetServerPeer::STATE_AUTHENTICATED);
				peer->_set_auth_state(ENetServerPeer::AUTH_APPROVED);

				QueuedEvent auth_event;
				auth_event.type = EVENT_PEER_AUTHENTICATED;
				auth_event.peer_id = peer->get_peer_id();
				_queue_event(auth_event);
			}
		} break;

		case EVENT_PEER_PRELOGIN: {
			// Unused internally; prelogin signaling is routed through packet reception
		} break;

		case EVENT_PEER_AUTHENTICATED: {
			emit_signal("peer_authenticated", peer);
		} break;

		case EVENT_PEER_DISCONNECTED: {
			emit_signal("peer_disconnected", peer, p_event.reason);
		} break;

		case EVENT_PACKET_RECEIVED: {
			PackedByteArray raw_data = p_event.data;
			Variant decoded = ENetPacketUtils::get_singleton()->decode_auto(raw_data);

			if (peer->get_connection_state() == ENetServerPeer::STATE_PRELOGIN) {
				if (decoded.get_type() == Variant::DICTIONARY) {
					Dictionary login_data = decoded;
					if (auth_mode == AUTH_CUSTOM && custom_authenticator.is_valid()) {
						Variant args[2] = { peer, login_data };
						const Variant *argp[2] = { &args[0], &args[1] };
						Callable::CallError ce;
						Variant ret;
						custom_authenticator.callp(argp, 2, ret, ce);
						if (ce.error == Callable::CallError::CALL_OK && ret.booleanize()) {
							peer->authenticate();

							QueuedEvent auth_event;
							auth_event.type = EVENT_PEER_AUTHENTICATED;
							auth_event.peer_id = peer->get_peer_id();
							_queue_event(auth_event);
						} else {
							peer->reject("Authentication failed");
						}
					} else {
						emit_signal("peer_prelogin", peer, login_data);
					}
				} else {
					peer->reject("Invalid authentication packet format");
				}
				return;
			}

			// Check if this is a custom event
			if (decoded.get_type() == Variant::DICTIONARY) {
				Dictionary dict = decoded;
				if (dict.has("_event") && dict["_event"].get_type() == Variant::STRING) {
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
						handler.call(peer, payload, p_event.channel);
						// Emit custom event signal
						emit_signal("custom_event_received", peer, event_name, payload, p_event.channel);
						return; // Don't emit regular packet_received for known custom events
					} else {
						// No handler registered - emit unknown event signal
						emit_signal("unknown_event_received", peer, event_name, payload, p_event.channel);
					}
				}
			}

			emit_signal("packet_received", peer, decoded, p_event.channel);
			emit_signal("raw_packet_received", peer, raw_data, p_event.channel);
		} break;
	}
}

ENetServer::ENetServer() {
	singleton = this;
	server_active = false;
	thread_running = false;
	poll_rate_ms = 10;
	next_peer_id = 1;
	local_port = 0;
	auth_mode = AUTH_NONE;
	auth_timeout = 10.0f;
}

ENetServer::~ENetServer() {
	stop_server();
	singleton = nullptr;
}
