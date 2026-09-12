/**************************************************************************/
/*  enet_server_peer.cpp                                                  */
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

#include "enet_server_peer.h"

#include "core/os/time.h"
#include "enet_packet.h"
#include "enet_server.h"

void ENetServerPeer::_bind_methods() {
	// Getters
	ClassDB::bind_method(D_METHOD("get_peer_id"), &ENetServerPeer::get_peer_id);
	ClassDB::bind_method(D_METHOD("get_remote_address"), &ENetServerPeer::get_remote_address);
	ClassDB::bind_method(D_METHOD("get_remote_port"), &ENetServerPeer::get_remote_port);
	ClassDB::bind_method(D_METHOD("get_connection_state"), &ENetServerPeer::get_connection_state);
	ClassDB::bind_method(D_METHOD("get_auth_state"), &ENetServerPeer::get_auth_state);
	ClassDB::bind_method(D_METHOD("get_custom_data"), &ENetServerPeer::get_custom_data);
	ClassDB::bind_method(D_METHOD("set_custom_data", "data"), &ENetServerPeer::set_custom_data);
	ClassDB::bind_method(D_METHOD("get_connection_time"), &ENetServerPeer::get_connection_time);
	ClassDB::bind_method(D_METHOD("get_ping"), &ENetServerPeer::get_ping);

	// Operations
	ClassDB::bind_method(D_METHOD("send_packet", "packet", "channel", "reliable"), &ENetServerPeer::send_packet);
	ClassDB::bind_method(D_METHOD("send_raw_packet", "data", "channel", "reliable"), &ENetServerPeer::send_raw_packet);
	ClassDB::bind_method(D_METHOD("disconnect_peer", "reason"), &ENetServerPeer::disconnect_peer, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("reject", "reason"), &ENetServerPeer::reject, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("authenticate"), &ENetServerPeer::authenticate);
	ClassDB::bind_method(D_METHOD("get_statistic", "statistic"), &ENetServerPeer::get_statistic);

	// Properties
	ADD_PROPERTY(PropertyInfo(Variant::INT, "peer_id"), "", "get_peer_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "remote_address"), "", "get_remote_address");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "remote_port"), "", "get_remote_port");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "connection_state"), "", "get_connection_state");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "auth_state"), "", "get_auth_state");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "custom_data"), "set_custom_data", "get_custom_data");

	// Enums
	BIND_ENUM_CONSTANT(STATE_CONNECTING);
	BIND_ENUM_CONSTANT(STATE_PRELOGIN);
	BIND_ENUM_CONSTANT(STATE_AUTHENTICATED);
	BIND_ENUM_CONSTANT(STATE_DISCONNECTING);

	BIND_ENUM_CONSTANT(AUTH_PENDING);
	BIND_ENUM_CONSTANT(AUTH_APPROVED);
	BIND_ENUM_CONSTANT(AUTH_REJECTED);
}

int ENetServerPeer::get_peer_id() const {
	return peer_id;
}

String ENetServerPeer::get_remote_address() const {
	if (packet_peer.is_valid()) {
		return packet_peer->get_remote_address();
	}
	return "";
}

int ENetServerPeer::get_remote_port() const {
	if (packet_peer.is_valid()) {
		return packet_peer->get_remote_port();
	}
	return 0;
}

ENetServerPeer::ConnectionState ENetServerPeer::get_connection_state() const {
	return connection_state;
}

ENetServerPeer::AuthState ENetServerPeer::get_auth_state() const {
	return auth_state;
}

Dictionary ENetServerPeer::get_custom_data() const {
	return custom_data;
}

void ENetServerPeer::set_custom_data(const Dictionary &p_data) {
	custom_data = p_data;
}

float ENetServerPeer::get_connection_time() const {
	uint64_t current_time = Time::get_singleton()->get_ticks_msec();
	return (current_time - connection_time_ms) / 1000.0f;
}

int ENetServerPeer::get_ping() const {
	if (packet_peer.is_valid()) {
		return packet_peer->get_statistic(ENetPacketPeer::PEER_ROUND_TRIP_TIME);
	}
	return 0;
}

Error ENetServerPeer::send_packet(const Variant &p_packet, int p_channel, bool p_reliable) {
	ERR_FAIL_COND_V(!packet_peer.is_valid(), ERR_UNCONFIGURED);

	// Encode packet using auto-detection
	PackedByteArray data = ENetPacketUtils::get_singleton()->encode_auto(p_packet);
	ERR_FAIL_COND_V(data.size() == 0, ERR_INVALID_DATA);

	// Send via ENet using public put_packet method
	int flags = p_reliable ? ENetPacketPeer::FLAG_RELIABLE : ENetPacketPeer::FLAG_UNSEQUENCED;

	// Create ENet packet
	ENetPacket *enet_packet = enet_packet_create(data.ptr(), data.size(), flags);
	ERR_FAIL_NULL_V(enet_packet, ERR_CANT_CREATE);

	return (Error)packet_peer->send(p_channel, enet_packet);
}

Error ENetServerPeer::send_raw_packet(const PackedByteArray &p_data, int p_channel, bool p_reliable) {
	ERR_FAIL_COND_V(!packet_peer.is_valid(), ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(p_data.size() == 0, ERR_INVALID_DATA);

	int flags = p_reliable ? ENetPacketPeer::FLAG_RELIABLE : ENetPacketPeer::FLAG_UNSEQUENCED;
	ENetPacket *enet_packet = enet_packet_create(p_data.ptr(), p_data.size(), flags);
	ERR_FAIL_NULL_V(enet_packet, ERR_CANT_CREATE);

	return (Error)packet_peer->send(p_channel, enet_packet);
}

void ENetServerPeer::disconnect_peer(const String &p_reason) {
	pending_disconnect = true;
	disconnect_reason = p_reason;
	connection_state = STATE_DISCONNECTING;
}

void ENetServerPeer::reject(const String &p_reason) {
	ERR_FAIL_COND(connection_state != STATE_CONNECTING && connection_state != STATE_PRELOGIN);
	auth_state = AUTH_REJECTED;
	disconnect_peer(p_reason);
}

void ENetServerPeer::authenticate() {
	ERR_FAIL_COND(connection_state != STATE_PRELOGIN);
	auth_state = AUTH_APPROVED;
	connection_state = STATE_AUTHENTICATED;

	if (ENetServer::get_singleton()) {
		ENetServer::get_singleton()->queue_peer_authenticated(peer_id);
	}
}

void ENetServerPeer::_set_packet_peer(const Ref<ENetPacketPeer> &p_peer) {
	packet_peer = p_peer;
}

Ref<ENetPacketPeer> ENetServerPeer::_get_packet_peer() const {
	return packet_peer;
}

void ENetServerPeer::_set_peer_id(int p_id) {
	peer_id = p_id;
}

void ENetServerPeer::_set_connection_state(ConnectionState p_state) {
	connection_state = p_state;
}

void ENetServerPeer::_set_auth_state(AuthState p_state) {
	auth_state = p_state;
}

bool ENetServerPeer::_has_pending_disconnect() const {
	return pending_disconnect;
}

String ENetServerPeer::_get_disconnect_reason() const {
	return disconnect_reason;
}

void ENetServerPeer::_clear_pending_disconnect() {
	pending_disconnect = false;
	disconnect_reason = "";
}

double ENetServerPeer::get_statistic(ENetPacketPeer::PeerStatistic p_stat) {
	if (packet_peer.is_valid()) {
		return packet_peer->get_statistic(p_stat);
	}
	return 0.0;
}

ENetServerPeer::ENetServerPeer() {
	peer_id = 0;
	connection_state = STATE_CONNECTING;
	auth_state = AUTH_PENDING;
	connection_time_ms = Time::get_singleton()->get_ticks_msec();
	pending_disconnect = false;
}

ENetServerPeer::~ENetServerPeer() {
}
