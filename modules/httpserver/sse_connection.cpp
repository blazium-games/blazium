/**************************************************************************/
/*  sse_connection.cpp                                                    */
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

#include "sse_connection.h"

void SSEConnection::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_connection_id", "id"), &SSEConnection::set_connection_id);
	ClassDB::bind_method(D_METHOD("get_connection_id"), &SSEConnection::get_connection_id);
	ClassDB::bind_method(D_METHOD("set_path", "path"), &SSEConnection::set_path);
	ClassDB::bind_method(D_METHOD("get_path"), &SSEConnection::get_path);
	ClassDB::bind_method(D_METHOD("send_event", "event_type", "data", "event_id"), &SSEConnection::send_event, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("send_data", "data"), &SSEConnection::send_data);
	ClassDB::bind_method(D_METHOD("close_connection"), &SSEConnection::close_connection);
	ClassDB::bind_method(D_METHOD("is_connection_active"), &SSEConnection::is_connection_active);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "connection_id"), "set_connection_id", "get_connection_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "path"), "set_path", "get_path");
}

void SSEConnection::set_peer(const Ref<StreamPeer> &p_peer) {
	peer = p_peer;
}

Ref<StreamPeer> SSEConnection::get_peer() const {
	return peer;
}

void SSEConnection::set_connection_id(int p_id) {
	connection_id = p_id;
}

int SSEConnection::get_connection_id() const {
	return connection_id;
}

void SSEConnection::set_path(const String &p_path) {
	path = p_path;
}

String SSEConnection::get_path() const {
	return path;
}

Error SSEConnection::send_event(const String &p_event_type, const String &p_data, const String &p_event_id) {
	if (!connected || peer.is_null()) {
		return ERR_UNCONFIGURED;
	}

	String message;

	// Add event ID if provided
	if (!p_event_id.is_empty()) {
		message += "id: " + p_event_id + "\r\n";
	}

	// Add event type if provided
	if (!p_event_type.is_empty()) {
		message += "event: " + p_event_type + "\r\n";
	}

	// Add data (handle multi-line data)
	Vector<String> lines = p_data.split("\n");
	for (int i = 0; i < lines.size(); i++) {
		message += "data: " + lines[i] + "\r\n";
	}

	// Empty line signals end of event
	message += "\r\n";

	CharString cs = message.utf8();
	Error err = peer->put_data((const uint8_t *)cs.get_data(), cs.size() - 1);

	if (err != OK) {
		connected = false;
	}

	return err;
}

Error SSEConnection::send_data(const String &p_data) {
	return send_event("", p_data, "");
}

void SSEConnection::close_connection() {
	connected = false;
	peer.unref();
}

bool SSEConnection::is_connection_active() const {
	return connected && peer.is_valid();
}

SSEConnection::SSEConnection() {
}

SSEConnection::~SSEConnection() {
	close_connection();
}
