/**************************************************************************/
/*  socketio_client.cpp                                                   */
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

#include "socketio_client.h"

#include "core/io/json.h"
#include "core/os/time.h"

SocketIOClient *SocketIOClient::singleton = nullptr;

SocketIOClient *SocketIOClient::get_singleton() {
	return singleton;
}

void SocketIOClient::_bind_methods() {
	// Connection management
	ClassDB::bind_method(D_METHOD("connect_to_url", "url", "auth", "tls_options"), &SocketIOClient::connect_to_url, DEFVAL(Dictionary()), DEFVAL(Ref<TLSOptions>()));
	ClassDB::bind_method(D_METHOD("close"), &SocketIOClient::close);
	ClassDB::bind_method(D_METHOD("poll"), &SocketIOClient::poll);

	// Namespace access
	ClassDB::bind_method(D_METHOD("get_namespace", "namespace"), &SocketIOClient::get_namespace, DEFVAL("/"));
	ClassDB::bind_method(D_METHOD("create_namespace", "namespace"), &SocketIOClient::create_namespace);
	ClassDB::bind_method(D_METHOD("has_namespace", "namespace"), &SocketIOClient::has_namespace);

	// Connection info
	ClassDB::bind_method(D_METHOD("is_socket_connected"), &SocketIOClient::is_socket_connected);
	ClassDB::bind_method(D_METHOD("get_connection_state"), &SocketIOClient::get_connection_state);
	ClassDB::bind_method(D_METHOD("get_connection_url"), &SocketIOClient::get_connection_url);
	ClassDB::bind_method(D_METHOD("get_engine_io_session_id"), &SocketIOClient::get_engine_io_session_id);

	// Enums
	BIND_ENUM_CONSTANT(STATE_DISCONNECTED);
	BIND_ENUM_CONSTANT(STATE_CONNECTING);
	BIND_ENUM_CONSTANT(STATE_CONNECTED);

	// Signals
	ADD_SIGNAL(MethodInfo("connected", PropertyInfo(Variant::STRING, "session_id")));
	ADD_SIGNAL(MethodInfo("disconnected", PropertyInfo(Variant::STRING, "reason")));
	ADD_SIGNAL(MethodInfo("connect_error", PropertyInfo(Variant::DICTIONARY, "error")));
	ADD_SIGNAL(MethodInfo("namespace_connected", PropertyInfo(Variant::STRING, "namespace_path")));
	ADD_SIGNAL(MethodInfo("namespace_disconnected", PropertyInfo(Variant::STRING, "namespace_path")));
}

Error SocketIOClient::connect_to_url(const String &p_url, const Dictionary &p_auth, const Ref<TLSOptions> &p_tls_options) {
	ERR_FAIL_COND_V_MSG(connection_state != STATE_DISCONNECTED, ERR_ALREADY_IN_USE, "Already connected or connecting");

	// Create WebSocket peer
	ws = Ref<WebSocketPeer>(WebSocketPeer::create());
	ERR_FAIL_COND_V_MSG(ws.is_null(), ERR_CANT_CREATE, "Failed to create WebSocket peer");

	// Configure WebSocket for Socket.IO/Engine.IO
	// Socket.IO uses Engine.IO protocol which is WebSocket-based
	connection_url = p_url;

	// Append /socket.io/ path if not present
	String full_url = p_url;
	if (!full_url.contains("/socket.io/")) {
		if (!full_url.ends_with("/")) {
			full_url += "/";
		}
		full_url += "socket.io/";
	}

	// Add Engine.IO query parameters
	String separator = full_url.contains("?") ? "&" : "?";
	full_url += separator + "EIO=4&transport=websocket";

	// Connect to the server
	Error err = ws->connect_to_url(full_url, p_tls_options);
	if (err != OK) {
		ws.unref();
		return err;
	}

	connection_state = STATE_CONNECTING;

	// Create and store main namespace
	Ref<SocketIONamespace> main_ns;
	main_ns.instantiate();
	main_ns->namespace_path = "/";
	main_ns->_set_client(this);
	namespaces["/"] = main_ns;

	return OK;
}

void SocketIOClient::close() {
	if (connection_state == STATE_DISCONNECTED) {
		return;
	}

	// Disconnect all namespaces
	for (HashMap<String, Ref<SocketIONamespace>>::Iterator it = namespaces.begin(); it != namespaces.end(); ++it) {
		if (it->value.is_valid() && it->value->is_namespace_connected()) {
			// Send disconnect packet
			SocketIOPacket packet(SocketIOPacket::PACKET_DISCONNECT);
			packet.namespace_path = it->key;
			_send_packet(packet);

			it->value->_set_disconnected("client disconnect");
		}
	}

	// Close WebSocket
	if (ws.is_valid()) {
		ws->close(1000, "Normal closure");
		ws.unref();
	}

	connection_state = STATE_DISCONNECTED;
	engine_io_session_id = "";

	emit_signal("disconnected", "client disconnect");
}

void SocketIOClient::poll() {
	if (ws.is_null()) {
		return;
	}

	ws->poll();

	WebSocketPeer::State ws_state = ws->get_ready_state();

	switch (ws_state) {
		case WebSocketPeer::STATE_CONNECTING:
			// Still connecting
			break;

		case WebSocketPeer::STATE_OPEN: {
			if (connection_state == STATE_CONNECTING) {
				// Just connected - send CONNECT packet for main namespace
				connection_state = STATE_CONNECTED;

				// In Socket.IO v5, we must send CONNECT packet
				Ref<SocketIONamespace> main_ns = namespaces["/"];
				if (main_ns.is_valid()) {
					Dictionary auth;
					_send_connect_packet("/", auth);
				}
			}

			// Process incoming messages
			_process_websocket_messages();

			// Poll all namespaces for ack timeouts
			double current_time = Time::get_singleton()->get_unix_time_from_system();
			for (HashMap<String, Ref<SocketIONamespace>>::Iterator it = namespaces.begin(); it != namespaces.end(); ++it) {
				if (it->value.is_valid()) {
					it->value->_poll(current_time);
				}
			}
			break;
		}

		case WebSocketPeer::STATE_CLOSING:
			// Close in progress
			break;

		case WebSocketPeer::STATE_CLOSED: {
			// Connection closed
			String close_reason = ws->get_close_reason();

			// Notify all namespaces
			for (HashMap<String, Ref<SocketIONamespace>>::Iterator it = namespaces.begin(); it != namespaces.end(); ++it) {
				if (it->value.is_valid() && it->value->is_namespace_connected()) {
					it->value->_set_disconnected(close_reason);
				}
			}

			ws.unref();
			connection_state = STATE_DISCONNECTED;

			emit_signal("disconnected", close_reason);
			break;
		}
	}
}

Ref<SocketIONamespace> SocketIOClient::get_namespace(const String &p_namespace) {
	if (namespaces.has(p_namespace)) {
		return namespaces[p_namespace];
	}
	return Ref<SocketIONamespace>();
}

Ref<SocketIONamespace> SocketIOClient::create_namespace(const String &p_namespace) {
	ERR_FAIL_COND_V_MSG(p_namespace.is_empty() || !p_namespace.begins_with("/"), Ref<SocketIONamespace>(), "Namespace must start with '/'");

	if (namespaces.has(p_namespace)) {
		return namespaces[p_namespace];
	}

	Ref<SocketIONamespace> ns;
	ns.instantiate();
	ns->namespace_path = p_namespace;
	ns->_set_client(this);
	namespaces[p_namespace] = ns;

	return ns;
}

bool SocketIOClient::has_namespace(const String &p_namespace) const {
	return namespaces.has(p_namespace);
}

bool SocketIOClient::is_socket_connected() const {
	return connection_state == STATE_CONNECTED && ws.is_valid() && ws->get_ready_state() == WebSocketPeer::STATE_OPEN;
}

SocketIOClient::ConnectionState SocketIOClient::get_connection_state() const {
	return connection_state;
}

String SocketIOClient::get_connection_url() const {
	return connection_url;
}

String SocketIOClient::get_engine_io_session_id() const {
	return engine_io_session_id;
}

Error SocketIOClient::_connect_namespace(const String &p_namespace, const Dictionary &p_auth) {
	ERR_FAIL_COND_V_MSG(connection_state != STATE_CONNECTED, ERR_UNAVAILABLE, "Not connected to server");

	return _send_connect_packet(p_namespace, p_auth);
}

void SocketIOClient::_disconnect_namespace(const String &p_namespace) {
	ERR_FAIL_COND(connection_state != STATE_CONNECTED);

	SocketIOPacket packet(SocketIOPacket::PACKET_DISCONNECT);
	packet.namespace_path = p_namespace;
	_send_packet(packet);
}

void SocketIOClient::_send_event(const String &p_namespace, const String &p_event, const Array &p_data, int p_ack_id) {
	ERR_FAIL_COND_MSG(connection_state != STATE_CONNECTED, "Not connected");

	SocketIOPacket packet(SocketIOPacket::PACKET_EVENT);
	packet.namespace_path = p_namespace;

	// Build event array [event_name, ...args]
	Array event_data;
	event_data.push_back(p_event);
	for (int i = 0; i < p_data.size(); i++) {
		event_data.push_back(p_data[i]);
	}
	packet.data = event_data;

	if (p_ack_id >= 0) {
		packet.ack_id = p_ack_id;
	}

	// Check for binary data
	packet.extract_binary_attachments();

	_send_packet(packet);
}

void SocketIOClient::_process_websocket_messages() {
	while (ws->get_available_packet_count() > 0) {
		const uint8_t *buffer;
		int buffer_size;

		Error err = ws->get_packet(&buffer, buffer_size);
		if (err != OK) {
			ERR_PRINT("Failed to get WebSocket packet");
			continue;
		}

		if (ws->was_string_packet()) {
			// Text packet - Socket.IO packet
			String packet_str;
			packet_str.parse_utf8((const char *)buffer, buffer_size);

			// Socket.IO packets are sent directly (no Engine.IO wrapper in WebSocket mode)
			SocketIOPacket packet = SocketIOPacket::decode(packet_str);
			_process_packet(packet);
		} else {
			// Binary packet - attachment for previous packet
			PackedByteArray binary_data;
			binary_data.resize(buffer_size);
			memcpy(binary_data.ptrw(), buffer, buffer_size);

			// Find the pending binary packet for this namespace
			// (We need to match based on order, as Socket.IO sends attachments in order)
			bool found = false;
			for (HashMap<String, PendingBinaryPacket>::Iterator it = pending_binary_packets.begin(); it != pending_binary_packets.end(); ++it) {
				PendingBinaryPacket &pending = it->value;
				if (pending.attachments_received < pending.packet.num_attachments) {
					pending.packet.attachments.push_back(binary_data);
					pending.attachments_received++;

					// Check if all attachments received
					if (pending.attachments_received >= pending.packet.num_attachments) {
						// Reconstruct and process
						pending.packet.reconstruct_binary_data();
						_process_packet(pending.packet);
						pending_binary_packets.remove(it);
					}

					found = true;
					break;
				}
			}

			if (!found) {
				WARN_PRINT("Received binary attachment without pending packet");
			}
		}
	}
}

void SocketIOClient::_process_packet(const SocketIOPacket &p_packet) {
	// Check if this is a binary packet that needs attachments
	if ((p_packet.type == SocketIOPacket::PACKET_BINARY_EVENT || p_packet.type == SocketIOPacket::PACKET_BINARY_ACK) &&
			p_packet.num_attachments > 0 && p_packet.attachments.size() < p_packet.num_attachments) {
		// Store as pending
		PendingBinaryPacket pending;
		pending.packet = p_packet;
		pending.attachments_received = p_packet.attachments.size();
		pending_binary_packets[p_packet.namespace_path] = pending;
		return;
	}

	// Process complete packet
	switch (p_packet.type) {
		case SocketIOPacket::PACKET_CONNECT:
			_handle_connect_packet(p_packet);
			break;
		case SocketIOPacket::PACKET_DISCONNECT:
			_handle_disconnect_packet(p_packet);
			break;
		case SocketIOPacket::PACKET_EVENT:
		case SocketIOPacket::PACKET_BINARY_EVENT:
			_handle_event_packet(p_packet);
			break;
		case SocketIOPacket::PACKET_ACK:
		case SocketIOPacket::PACKET_BINARY_ACK:
			_handle_ack_packet(p_packet);
			break;
		case SocketIOPacket::PACKET_CONNECT_ERROR:
			_handle_connect_error_packet(p_packet);
			break;
		default:
			WARN_PRINT(vformat("Unknown packet type: %d", p_packet.type));
			break;
	}
}

void SocketIOClient::_handle_connect_packet(const SocketIOPacket &p_packet) {
	// Extract session ID from data
	String session_id;
	if (p_packet.data.size() > 0) {
		Variant data_var = p_packet.data[0];
		if (data_var.get_type() == Variant::DICTIONARY) {
			Dictionary data = data_var;
			if (data.has("sid")) {
				session_id = data["sid"];
			}
		}
	}

	// Notify namespace
	if (namespaces.has(p_packet.namespace_path)) {
		Ref<SocketIONamespace> ns = namespaces[p_packet.namespace_path];
		if (ns.is_valid()) {
			ns->_set_connected(session_id);
		}
	}

	emit_signal("namespace_connected", p_packet.namespace_path);

	// For main namespace, also emit global connected signal
	if (p_packet.namespace_path == "/") {
		engine_io_session_id = session_id;
		emit_signal("connected", session_id);
	}
}

void SocketIOClient::_handle_disconnect_packet(const SocketIOPacket &p_packet) {
	// Notify namespace
	if (namespaces.has(p_packet.namespace_path)) {
		Ref<SocketIONamespace> ns = namespaces[p_packet.namespace_path];
		if (ns.is_valid()) {
			ns->_set_disconnected("server disconnect");
		}
	}

	emit_signal("namespace_disconnected", p_packet.namespace_path);
}

void SocketIOClient::_handle_event_packet(const SocketIOPacket &p_packet) {
	// Event data format: [event_name, ...args]
	if (p_packet.data.size() < 1) {
		ERR_PRINT("Invalid event packet: missing event name");
		return;
	}

	String event_name = p_packet.data[0];
	Array event_args;
	for (int i = 1; i < p_packet.data.size(); i++) {
		event_args.push_back(p_packet.data[i]);
	}

	// Route to namespace
	if (namespaces.has(p_packet.namespace_path)) {
		Ref<SocketIONamespace> ns = namespaces[p_packet.namespace_path];
		if (ns.is_valid()) {
			ns->_handle_event(event_name, event_args);
		}
	}

	// If this event requires an acknowledgment, we would handle it here
	// For now, namespaces handle their own acks via emit_with_ack
}

void SocketIOClient::_handle_ack_packet(const SocketIOPacket &p_packet) {
	// Route to namespace
	if (namespaces.has(p_packet.namespace_path)) {
		Ref<SocketIONamespace> ns = namespaces[p_packet.namespace_path];
		if (ns.is_valid()) {
			ns->_handle_ack(p_packet.ack_id, p_packet.data);
		}
	}
}

void SocketIOClient::_handle_connect_error_packet(const SocketIOPacket &p_packet) {
	Dictionary error;
	if (p_packet.data.size() > 0) {
		error = p_packet.data[0];
	}

	// Notify namespace
	if (namespaces.has(p_packet.namespace_path)) {
		Ref<SocketIONamespace> ns = namespaces[p_packet.namespace_path];
		if (ns.is_valid()) {
			String error_msg = "Connection refused";
			if (error.has("message")) {
				error_msg = error["message"];
			}
			ns->_set_disconnected(error_msg);
		}
	}

	emit_signal("connect_error", error);
}

void SocketIOClient::_send_packet(const SocketIOPacket &p_packet) {
	ERR_FAIL_COND(ws.is_null());
	ERR_FAIL_COND(ws->get_ready_state() != WebSocketPeer::STATE_OPEN);

	// Encode packet
	String packet_str = p_packet.encode();

	// Send text packet
	Error err = ws->send_text(packet_str);
	if (err != OK) {
		ERR_PRINT(vformat("Failed to send Socket.IO packet: %d", err));
		return;
	}

	// Send binary attachments if any
	for (int i = 0; i < p_packet.attachments.size(); i++) {
		const PackedByteArray &attachment = p_packet.attachments[i];
		err = ws->send(attachment.ptr(), attachment.size(), WebSocketPeer::WRITE_MODE_BINARY);
		if (err != OK) {
			ERR_PRINT(vformat("Failed to send binary attachment %d: %d", i, err));
		}
	}
}

Error SocketIOClient::_send_connect_packet(const String &p_namespace, const Dictionary &p_auth) {
	SocketIOPacket packet(SocketIOPacket::PACKET_CONNECT);
	packet.namespace_path = p_namespace;

	if (!p_auth.is_empty()) {
		packet.data.push_back(p_auth);
	}

	_send_packet(packet);
	return OK;
}

SocketIOClient::SocketIOClient() {
	singleton = this;
}

SocketIOClient::~SocketIOClient() {
	close();
	singleton = nullptr;
}
