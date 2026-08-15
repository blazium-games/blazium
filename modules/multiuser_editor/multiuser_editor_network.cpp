/**************************************************************************/
/*  multiuser_editor_network.cpp                                          */
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

#ifdef TOOLS_ENABLED

#include "core/object/class_db.h"
#include "multiuser_editor_network.h"

#include "multiuser_editor_constants.h"

#include "core/io/marshalls.h"

PackedByteArray MultiuserEditorNetwork::_serialize_action(const Dictionary &p_action) const {
	PackedByteArray packet;
	int len = 0;
	ERR_FAIL_COND_V(encode_variant(p_action, nullptr, len, false) != OK, packet);
	packet.resize(len);
	ERR_FAIL_COND_V(encode_variant(p_action, packet.ptrw(), len, false) != OK, PackedByteArray());
	return packet;
}

Dictionary MultiuserEditorNetwork::_deserialize_action(const PackedByteArray &p_packet) const {
	Dictionary ret;
	Variant decoded;
	if (decode_variant(decoded, p_packet.ptr(), p_packet.size(), nullptr, false) == OK && decoded.get_type() == Variant::DICTIONARY) {
		ret = decoded;
	}
	return ret;
}

void MultiuserEditorNetwork::_send_raw_packet_to(int p_peer_id, const PackedByteArray &p_packet) {
	if (p_packet.is_empty()) {
		return;
	}
	if (p_packet.size() > max_packet_size) {
		const String msg = vformat("[Multiuser/Network] outgoing packet dropped (size %d > cap %d, target=%d).", p_packet.size(), max_packet_size, p_peer_id);
		print_verbose(msg);
		_security.record(multiuser_editor::kEvtKindInvalidPacket, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatNetwork, msg);
		last_send_truncated_count++;
		return;
	}
	if (!is_connected()) {
		return;
	}
	peer->set_target_peer(p_peer_id);
	peer->set_transfer_mode(MultiplayerPeer::TRANSFER_MODE_RELIABLE);
	peer->put_packet(p_packet.ptr(), p_packet.size());
}

Error MultiuserEditorNetwork::host(int p_port, const String &p_password) {
	stop();
#ifdef MULTIUSER_EDITOR_CLIENT_ONLY
	(void)p_port;
	(void)p_password;
	return ERR_UNAVAILABLE;
#else
	Object *peer_obj = ClassDB::instantiate("ENetMultiplayerPeer");
	peer = Ref<MultiplayerPeer>(Object::cast_to<MultiplayerPeer>(peer_obj));

	const int clamped_max_clients = CLAMP(max_clients_setting, 1, multiuser_editor::kNetworkMaxClientsCeiling);
	Error err = peer.is_valid() ? Error(int(peer->call("create_server", p_port, clamped_max_clients))) : ERR_CANT_CREATE;
	if (err != OK) {
		ERR_PRINT(vformat("Multiuser Editor Network Failed to Host: ENet create_server returned native Error %d", err));
		print_line(vformat("[Multiuser Connect] Failed to open host on port %d (error=%d)", p_port, err));
		peer.unref();
		return err;
	}

	_apply_max_packet_size_to_peer();
	session_password = p_password;
	mode = MODE_HOST;
	print_line(vformat("[Multiuser Connect] Opening host on port %d (max_clients=%d)", p_port, clamped_max_clients));
	return OK;
#endif
}

Error MultiuserEditorNetwork::join(const String &p_host, int p_port, const String &p_password) {
	stop();
	Object *peer_obj = ClassDB::instantiate("ENetMultiplayerPeer");
	peer = Ref<MultiplayerPeer>(Object::cast_to<MultiplayerPeer>(peer_obj));

	String host = p_host.is_empty() ? String("127.0.0.1") : p_host;
	Error err = peer.is_valid() ? Error(int(peer->call("create_client", host, p_port))) : ERR_CANT_CREATE;
	if (err != OK) {
		ERR_PRINT(vformat("Multiuser Editor Network Failed to Join: ENet create_client returned native Error %d", err));
		print_line(vformat("[Multiuser Connect] Failed to open client connection to %s:%d (error=%d)", host, p_port, err));
		peer.unref();
		return err;
	}

	_apply_max_packet_size_to_peer();
	session_password = p_password;
	mode = MODE_JOIN;
	print_line(vformat("[Multiuser Connect] Opening client connection to %s:%d", host, p_port));
	return OK;
}

void MultiuserEditorNetwork::stop() {
	if (peer.is_valid()) {
		peer.unref();
	}
	mode = MODE_OFF;
	clients.clear();
	net_id_to_peer_id.clear();
	net_id_to_role.clear();
	authenticated_peers.clear();
	last_send_truncated_count = 0;
	last_poll_truncated_count = 0;
	last_invalid_size_count = 0;
	last_malformed_packet_count = 0;
}

void MultiuserEditorNetwork::poll(Vector<Packet> &r_packets) {
	if (!peer.is_valid() || peer->get_connection_status() == MultiplayerPeer::CONNECTION_DISCONNECTED) {
		return;
	}
	peer->poll();

	if (!is_connected()) {
		return;
	}
	const int poll_cap = MAX(1, max_packets_per_poll);
	int polled = 0;
	while (peer->get_available_packet_count() > 0) {
		if (polled >= poll_cap) {
			last_poll_truncated_count++;
			const String msg = vformat("[Multiuser/Network] poll cap %d reached, deferring remaining packets.", poll_cap);
			print_verbose(msg);
			_security.record(multiuser_editor::kEvtKindRateLimited, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatNetwork, msg);
			break;
		}

		polled++;
		int sender = peer->get_packet_peer();
		const uint8_t *buffer = nullptr;
		int size = 0;
		if (peer->get_packet(&buffer, size) != OK || !buffer || size <= 0 || size > max_packet_size) {
			last_invalid_size_count++;
			const String msg = vformat("[Multiuser/Network] inbound packet dropped (size=%d, cap=%d, sender=%d).", size, max_packet_size, sender);
			print_verbose(msg);
			_security.record(multiuser_editor::kEvtKindInvalidPacket, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatNetwork, msg);
			continue;
		}

		Packet packet;
		packet.sender_net_id = sender;
		packet.raw_packet.resize(size);
		memcpy(packet.raw_packet.ptrw(), buffer, size);
		packet.action = _deserialize_action(packet.raw_packet);
		if (packet.action.is_empty()) {
			last_malformed_packet_count++;
			const String msg = vformat("[Multiuser/Network] dropped malformed inbound packet from sender=%d size=%d.", sender, size);
			print_verbose(msg);
			_security.record(multiuser_editor::kEvtKindMalformed, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatNetwork, msg);
			continue;
		}
		if (mode == MODE_HOST && !clients.has(sender)) {
			clients.push_back(sender);
		}
		r_packets.push_back(packet);
	}
}

void MultiuserEditorNetwork::send_action(const Dictionary &p_action) {
	if (!is_connected()) {
		return;
	}
	PackedByteArray packet = _serialize_action(p_action);
	_send_raw_packet_to(0, packet);
}

void MultiuserEditorNetwork::send_action_to(int p_net_id, const Dictionary &p_action) {
	_send_raw_packet_to(p_net_id, _serialize_action(p_action));
}

void MultiuserEditorNetwork::relay_packet(int p_sender_net_id, const PackedByteArray &p_packet) {
	if (mode != MODE_HOST) {
		return;
	}
	for (int client_id : clients) {
		if (client_id != p_sender_net_id) {
			_send_raw_packet_to(client_id, p_packet);
		}
	}
}

void MultiuserEditorNetwork::set_max_packet_size_mb(int p_size_mb) {
	max_packet_size = MAX(1, p_size_mb) * 1024 * 1024;

	_apply_max_packet_size_to_peer();
}

void MultiuserEditorNetwork::_apply_max_packet_size_to_peer() {
	if (peer.is_null()) {
		return;
	}
	if (!peer->has_method("set_max_packet_size")) {
		return;
	}
	peer->call("set_max_packet_size", max_packet_size);
}

void MultiuserEditorNetwork::set_max_packets_per_poll(int p_count) {
	max_packets_per_poll = CLAMP(p_count, 1, multiuser_editor::kNetworkPacketsPerPollCeiling);
}

void MultiuserEditorNetwork::set_max_clients(int p_max) {
	max_clients_setting = CLAMP(p_max, 1, multiuser_editor::kNetworkMaxClientsCeiling);
}

int MultiuserEditorNetwork::consume_send_truncated_count() {
	int v = last_send_truncated_count;
	last_send_truncated_count = 0;
	return v;
}

int MultiuserEditorNetwork::consume_poll_truncated_count() {
	int v = last_poll_truncated_count;
	last_poll_truncated_count = 0;
	return v;
}

int MultiuserEditorNetwork::consume_invalid_size_count() {
	int v = last_invalid_size_count;
	last_invalid_size_count = 0;
	return v;
}

int MultiuserEditorNetwork::consume_malformed_packet_count() {
	int v = last_malformed_packet_count;
	last_malformed_packet_count = 0;
	return v;
}

void MultiuserEditorNetwork::disconnect_peer(int p_net_id) {
	if (peer.is_valid() && mode == MODE_HOST) {
		peer->disconnect_peer(p_net_id);
	}
}

void MultiuserEditorNetwork::remember_peer(int p_net_id, const String &p_peer_id) {
	if (!p_peer_id.is_empty()) {
		net_id_to_peer_id[p_net_id] = p_peer_id;
	}
}

void MultiuserEditorNetwork::forget_peer(int p_net_id) {
	net_id_to_peer_id.erase(p_net_id);
	net_id_to_role.erase(p_net_id);
	authenticated_peers.erase(p_net_id);
	clients.erase(p_net_id);
}

void MultiuserEditorNetwork::remember_peer_role(int p_net_id, const String &p_role) {
	if (!p_role.is_empty()) {
		net_id_to_role[p_net_id] = p_role;
	}
}

void MultiuserEditorNetwork::mark_peer_authenticated(int p_net_id) {
	authenticated_peers.insert(p_net_id);
}

bool MultiuserEditorNetwork::is_peer_authenticated(int p_net_id) const {
	if (p_net_id == 1 || p_net_id == 0) {
		return true;
	}
	return authenticated_peers.has(p_net_id);
}

String MultiuserEditorNetwork::get_peer_role(int p_net_id) const {
	if (net_id_to_role.has(p_net_id)) {
		return net_id_to_role[p_net_id];
	}
	return multiuser_editor::kRoleViewer;
}

String MultiuserEditorNetwork::get_peer_id(int p_net_id) const {
	return net_id_to_peer_id.has(p_net_id) ? net_id_to_peer_id[p_net_id] : String();
}

int MultiuserEditorNetwork::get_net_id(const String &p_peer_id) const {
	for (const KeyValue<int, String> &E : net_id_to_peer_id) {
		if (E.value == p_peer_id) {
			return E.key;
		}
	}
	return 0;
}

Vector<String> MultiuserEditorNetwork::get_peer_ids() const {
	Vector<String> peers;
	for (const KeyValue<int, String> &E : net_id_to_peer_id) {
		peers.push_back(E.value);
	}
	return peers;
}

Ref<MultiplayerPeer> MultiuserEditorNetwork::get_peer() const {
	return peer;
}

MultiuserEditorNetwork::SessionMode MultiuserEditorNetwork::get_mode() const {
	return mode;
}

bool MultiuserEditorNetwork::is_connected() const {
	return peer.is_valid() && peer->get_connection_status() == MultiplayerPeer::CONNECTION_CONNECTED;
}

PackedByteArray MultiuserEditorNetwork::test_serialize_action(const Dictionary &p_action) const {
	return _serialize_action(p_action);
}

Dictionary MultiuserEditorNetwork::test_deserialize_action(const PackedByteArray &p_packet) const {
	return _deserialize_action(p_packet);
}

#endif
