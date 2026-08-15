/**************************************************************************/
/*  socketio_namespace.h                                                  */
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

#include "core/object/ref_counted.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "core/variant/callable.h"

class SocketIOClient;

class SocketIONamespace : public RefCounted {
	GDCLASS(SocketIONamespace, RefCounted);

public:
	enum ConnectionState {
		STATE_DISCONNECTED,
		STATE_CONNECTING,
		STATE_CONNECTED
	};

private:
	friend class SocketIOClient;

	SocketIOClient *client = nullptr;
	String namespace_path = "/";
	ConnectionState state = STATE_DISCONNECTED;
	String session_id;

	// Event listeners
	HashMap<String, Vector<Callable>> event_listeners;

	// Acknowledgment tracking
	struct AckCallback {
		Callable callback;
		double timeout_time;
		int ack_id;
	};
	Vector<AckCallback> pending_acks;
	int next_ack_id = 0;

	// Internal methods
	void _process_acks(double p_current_time);

protected:
	static void _bind_methods();

public:
	// Event system
	void emit(const String &p_event, const Array &p_data = Array());
	void emit_with_ack(const String &p_event, const Array &p_data, const Callable &p_callback, float p_timeout = 5.0);
	void on(const String &p_event, const Callable &p_callback);
	void off(const String &p_event, const Callable &p_callback);

	// Connection
	Error connect_to_namespace(const Dictionary &p_auth = Dictionary());
	void disconnect_from_namespace();
	bool is_namespace_connected() const;
	String get_namespace_path() const;
	ConnectionState get_connection_state() const;
	String get_session_id() const;

	// Internal: called by SocketIOClient
	void _handle_event(const String &p_event, const Array &p_data);
	void _handle_ack(int p_ack_id, const Array &p_data);
	void _set_connected(const String &p_session_id);
	void _set_disconnected(const String &p_reason);
	void _set_client(SocketIOClient *p_client);
	int _get_next_ack_id();
	void _poll(double p_current_time);

	SocketIONamespace();
	SocketIONamespace(const String &p_namespace);
	~SocketIONamespace();
};

VARIANT_ENUM_CAST(SocketIONamespace::ConnectionState);
