/**************************************************************************/
/*  multiuser_editor_network.h                                            */
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

#ifdef TOOLS_ENABLED

#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/templates/vector.h"
#include "core/variant/variant.h"
#include "multiuser_editor_security_sink.h"
#include "scene/main/multiplayer_peer.h"

class MultiuserEditorNetwork {
public:
	enum SessionMode {
		MODE_OFF,
		MODE_HOST,
		MODE_JOIN,
	};

	struct Packet {
		int sender_net_id = 0;
		Dictionary action;
		PackedByteArray raw_packet;
	};

private:
	Ref<MultiplayerPeer> peer;
	SessionMode mode = MODE_OFF;
	Vector<int> clients;
	HashMap<int, String> net_id_to_peer_id;
	HashMap<int, String> net_id_to_role;
	HashSet<int> authenticated_peers;
	String session_password;
	int max_packet_size = 8 * 1024 * 1024;
	int max_packets_per_poll = 256;
	int max_clients_setting = 32;
	int last_send_truncated_count = 0;
	int last_poll_truncated_count = 0;
	int last_invalid_size_count = 0;
	int last_malformed_packet_count = 0;

	multiuser_editor::SecuritySink _security;

	PackedByteArray _serialize_action(const Dictionary &p_action) const;
	Dictionary _deserialize_action(const PackedByteArray &p_packet) const;
	void _send_raw_packet_to(int p_peer_id, const PackedByteArray &p_packet);

	void _apply_max_packet_size_to_peer();

public:
	Error host(int p_port, const String &p_password = "");
	Error join(const String &p_host, int p_port, const String &p_password = "");
	void stop();
	void poll(Vector<Packet> &r_packets);
	void send_action(const Dictionary &p_action);
	void send_action_to(int p_net_id, const Dictionary &p_action);
	void relay_packet(int p_sender_net_id, const PackedByteArray &p_packet);
	void set_max_packet_size_mb(int p_size_mb);
	void set_max_packets_per_poll(int p_count);
	int get_max_packets_per_poll() const { return max_packets_per_poll; }
	void set_max_clients(int p_max);
	int get_max_clients() const { return max_clients_setting; }
	int consume_send_truncated_count();
	int consume_poll_truncated_count();
	int consume_invalid_size_count();
	int consume_malformed_packet_count();
	void set_security_sink(const multiuser_editor::SecuritySink &p_sink) { _security = p_sink; }
	void disconnect_peer(int p_net_id);
	void remember_peer(int p_net_id, const String &p_peer_id);
	void forget_peer(int p_net_id);
	void remember_peer_role(int p_net_id, const String &p_role);
	void mark_peer_authenticated(int p_net_id);
	bool is_peer_authenticated(int p_net_id) const;
	String get_peer_role(int p_net_id) const;
	String get_peer_id(int p_net_id) const;
	int get_net_id(const String &p_peer_id) const;

	Vector<String> get_peer_ids() const;
	String get_session_password() const { return session_password; }
	Ref<MultiplayerPeer> get_peer() const;

	PackedByteArray test_serialize_action(const Dictionary &p_action) const;
	Dictionary test_deserialize_action(const PackedByteArray &p_packet) const;
	void test_send_raw_packet_to(int p_peer_id, const PackedByteArray &p_packet) { _send_raw_packet_to(p_peer_id, p_packet); }
	int get_max_packet_size_bytes() const { return max_packet_size; }
	void test_set_max_packet_size_bytes(int p_bytes) { max_packet_size = MAX(1, p_bytes); }
	SessionMode get_mode() const;
	bool is_connected() const;
};

#endif
