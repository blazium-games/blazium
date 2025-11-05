/**************************************************************************/
/*  socketio_namespace.cpp                                                */
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

#include "socketio_namespace.h"

#include "core/os/time.h"
#include "socketio_client.h"

void SocketIONamespace::_bind_methods() {
	// Event system
	ClassDB::bind_method(D_METHOD("emit", "event", "data"), &SocketIONamespace::emit, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("emit_with_ack", "event", "data", "callback", "timeout"), &SocketIONamespace::emit_with_ack, DEFVAL(5.0));
	ClassDB::bind_method(D_METHOD("on", "event", "callback"), &SocketIONamespace::on);
	ClassDB::bind_method(D_METHOD("off", "event", "callback"), &SocketIONamespace::off);

	// Connection
	ClassDB::bind_method(D_METHOD("connect_to_namespace", "auth"), &SocketIONamespace::connect_to_namespace, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("disconnect_from_namespace"), &SocketIONamespace::disconnect_from_namespace);
	ClassDB::bind_method(D_METHOD("is_namespace_connected"), &SocketIONamespace::is_namespace_connected);
	ClassDB::bind_method(D_METHOD("get_namespace_path"), &SocketIONamespace::get_namespace_path);
	ClassDB::bind_method(D_METHOD("get_connection_state"), &SocketIONamespace::get_connection_state);
	ClassDB::bind_method(D_METHOD("get_session_id"), &SocketIONamespace::get_session_id);

	// Enums
	BIND_ENUM_CONSTANT(STATE_DISCONNECTED);
	BIND_ENUM_CONSTANT(STATE_CONNECTING);
	BIND_ENUM_CONSTANT(STATE_CONNECTED);

	// Signals
	ADD_SIGNAL(MethodInfo("connected", PropertyInfo(Variant::STRING, "session_id")));
	ADD_SIGNAL(MethodInfo("disconnected", PropertyInfo(Variant::STRING, "reason")));
	ADD_SIGNAL(MethodInfo("event_received", PropertyInfo(Variant::STRING, "event_name"), PropertyInfo(Variant::ARRAY, "data")));
}

void SocketIONamespace::emit(const String &p_event, const Array &p_data) {
	ERR_FAIL_NULL(client);
	ERR_FAIL_COND_MSG(state != STATE_CONNECTED, "Cannot emit: namespace not connected");

	client->_send_event(namespace_path, p_event, p_data, -1);
}

void SocketIONamespace::emit_with_ack(const String &p_event, const Array &p_data, const Callable &p_callback, float p_timeout) {
	ERR_FAIL_NULL(client);
	ERR_FAIL_COND_MSG(state != STATE_CONNECTED, "Cannot emit: namespace not connected");

	int ack_id = next_ack_id++;

	// Register callback
	AckCallback ack;
	ack.callback = p_callback;
	ack.timeout_time = Time::get_singleton()->get_unix_time_from_system() + p_timeout;
	ack.ack_id = ack_id;
	pending_acks.push_back(ack);

	client->_send_event(namespace_path, p_event, p_data, ack_id);
}

void SocketIONamespace::on(const String &p_event, const Callable &p_callback) {
	if (!event_listeners.has(p_event)) {
		event_listeners[p_event] = Vector<Callable>();
	}
	event_listeners[p_event].push_back(p_callback);
}

void SocketIONamespace::off(const String &p_event, const Callable &p_callback) {
	if (!event_listeners.has(p_event)) {
		return;
	}

	Vector<Callable> &listeners = event_listeners[p_event];
	for (int i = 0; i < listeners.size(); i++) {
		if (listeners[i] == p_callback) {
			listeners.remove_at(i);
			break;
		}
	}

	if (listeners.is_empty()) {
		event_listeners.erase(p_event);
	}
}

Error SocketIONamespace::connect_to_namespace(const Dictionary &p_auth) {
	ERR_FAIL_NULL_V(client, ERR_UNCONFIGURED);
	ERR_FAIL_COND_V_MSG(state != STATE_DISCONNECTED, ERR_ALREADY_IN_USE, "Namespace already connected or connecting");

	state = STATE_CONNECTING;
	return client->_connect_namespace(namespace_path, p_auth);
}

void SocketIONamespace::disconnect_from_namespace() {
	ERR_FAIL_NULL(client);

	if (state == STATE_DISCONNECTED) {
		return;
	}

	client->_disconnect_namespace(namespace_path);
	state = STATE_DISCONNECTED;
	session_id = "";
}

bool SocketIONamespace::is_namespace_connected() const {
	return state == STATE_CONNECTED;
}

String SocketIONamespace::get_namespace_path() const {
	return namespace_path;
}

SocketIONamespace::ConnectionState SocketIONamespace::get_connection_state() const {
	return state;
}

String SocketIONamespace::get_session_id() const {
	return session_id;
}

void SocketIONamespace::_handle_event(const String &p_event, const Array &p_data) {
	// Call registered listeners
	if (event_listeners.has(p_event)) {
		Vector<Callable> listeners = event_listeners[p_event];
		for (int i = 0; i < listeners.size(); i++) {
			Callable::CallError ce;
			Variant ret;
			Variant data_variant = p_data;
			const Variant *args[1] = { &data_variant };
			listeners[i].callp(args, 1, ret, ce);
			if (ce.error != Callable::CallError::CALL_OK) {
				ERR_PRINT(vformat("Error calling event listener for '%s'", p_event));
			}
		}
	}

	// Emit generic signal
	emit_signal("event_received", p_event, p_data);
}

void SocketIONamespace::_handle_ack(int p_ack_id, const Array &p_data) {
	// Find and call the callback
	for (int i = 0; i < pending_acks.size(); i++) {
		if (pending_acks[i].ack_id == p_ack_id) {
			Callable callback = pending_acks[i].callback;
			pending_acks.remove_at(i);

			// Call the callback
			Callable::CallError ce;
			Variant ret;
			Variant data_variant = p_data;
			const Variant *args[1] = { &data_variant };
			callback.callp(args, 1, ret, ce);
			if (ce.error != Callable::CallError::CALL_OK) {
				ERR_PRINT(vformat("Error calling acknowledgment callback for ack_id %d", p_ack_id));
			}
			return;
		}
	}

	WARN_PRINT(vformat("Received ACK for unknown ack_id: %d", p_ack_id));
}

void SocketIONamespace::_set_connected(const String &p_session_id) {
	state = STATE_CONNECTED;
	session_id = p_session_id;
	emit_signal("connected", session_id);
}

void SocketIONamespace::_set_disconnected(const String &p_reason) {
	state = STATE_DISCONNECTED;
	session_id = "";

	// Clear pending acks with timeout
	for (int i = 0; i < pending_acks.size(); i++) {
		Callable callback = pending_acks[i].callback;
		Array timeout_response;
		timeout_response.push_back("timeout");

		Callable::CallError ce;
		Variant ret;
		Variant response_variant = timeout_response;
		const Variant *args[1] = { &response_variant };
		callback.callp(args, 1, ret, ce);
	}
	pending_acks.clear();

	emit_signal("disconnected", p_reason);
}

void SocketIONamespace::_set_client(SocketIOClient *p_client) {
	client = p_client;
}

int SocketIONamespace::_get_next_ack_id() {
	return next_ack_id++;
}

void SocketIONamespace::_poll(double p_current_time) {
	_process_acks(p_current_time);
}

void SocketIONamespace::_process_acks(double p_current_time) {
	// Check for timed out acknowledgments
	for (int i = pending_acks.size() - 1; i >= 0; i--) {
		if (p_current_time >= pending_acks[i].timeout_time) {
			Callable callback = pending_acks[i].callback;
			int ack_id = pending_acks[i].ack_id;
			pending_acks.remove_at(i);

			// Call callback with timeout error
			Array timeout_response;
			timeout_response.push_back("timeout");

			Callable::CallError ce;
			Variant ret;
			Variant response_variant = timeout_response;
			const Variant *args[1] = { &response_variant };
			callback.callp(args, 1, ret, ce);
			if (ce.error != Callable::CallError::CALL_OK) {
				ERR_PRINT(vformat("Error calling timeout callback for ack_id %d", ack_id));
			}
		}
	}
}

SocketIONamespace::SocketIONamespace() {
}

SocketIONamespace::SocketIONamespace(const String &p_namespace) {
	namespace_path = p_namespace;
}

SocketIONamespace::~SocketIONamespace() {
	if (client && state != STATE_DISCONNECTED) {
		disconnect_from_namespace();
	}
}
