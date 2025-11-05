/**************************************************************************/
/*  socketio_client.h                                                     */
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
#include "core/templates/hash_map.h"
#include "modules/websocket/websocket_peer.h"
#include "socketio_namespace.h"
#include "socketio_packet.h"

class SocketIOClient : public Object {
	GDCLASS(SocketIOClient, Object);

	static SocketIOClient *singleton;

public:
	enum ConnectionState {
		STATE_DISCONNECTED,
		STATE_CONNECTING,
		STATE_CONNECTED
	};

private:
	// WebSocket connection
	Ref<WebSocketPeer> ws;
	ConnectionState connection_state = STATE_DISCONNECTED;
	String connection_url;

	// Namespaces
	HashMap<String, Ref<SocketIONamespace>> namespaces;

	// Pending binary packets (when receiving multi-frame binary messages)
	struct PendingBinaryPacket {
		SocketIOPacket packet;
		int attachments_received = 0;
	};
	HashMap<String, PendingBinaryPacket> pending_binary_packets;

	// Connection info
	String engine_io_session_id;

	// Helper methods
	void _process_websocket_messages();
	void _process_packet(const SocketIOPacket &p_packet);
	void _handle_connect_packet(const SocketIOPacket &p_packet);
	void _handle_disconnect_packet(const SocketIOPacket &p_packet);
	void _handle_event_packet(const SocketIOPacket &p_packet);
	void _handle_ack_packet(const SocketIOPacket &p_packet);
	void _handle_connect_error_packet(const SocketIOPacket &p_packet);

	void _send_packet(const SocketIOPacket &p_packet);
	Error _send_connect_packet(const String &p_namespace, const Dictionary &p_auth);

protected:
	static void _bind_methods();

public:
	static SocketIOClient *get_singleton();

	// Connection management
	Error connect_to_url(const String &p_url, const Dictionary &p_auth = Dictionary(), const Ref<TLSOptions> &p_tls_options = Ref<TLSOptions>());
	void close();
	void poll();

	// Namespace access
	Ref<SocketIONamespace> get_namespace(const String &p_namespace = "/");
	Ref<SocketIONamespace> create_namespace(const String &p_namespace);
	bool has_namespace(const String &p_namespace) const;

	// Connection info
	bool is_socket_connected() const;
	ConnectionState get_connection_state() const;
	String get_connection_url() const;
	String get_engine_io_session_id() const;

	// Internal: called by SocketIONamespace
	Error _connect_namespace(const String &p_namespace, const Dictionary &p_auth);
	void _disconnect_namespace(const String &p_namespace);
	void _send_event(const String &p_namespace, const String &p_event, const Array &p_data, int p_ack_id);

	SocketIOClient();
	~SocketIOClient();
};

VARIANT_ENUM_CAST(SocketIOClient::ConnectionState);
