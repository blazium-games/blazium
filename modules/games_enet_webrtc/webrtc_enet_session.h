/**************************************************************************/
/*  webrtc_enet_session.h                                                 */
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

#include "fake_address_map.h"
#include "relay_client.h"
#include "signal_client.h"
#include "webrtc_link.h"

#include "core/templates/hash_set.h"
#include "modules/enet/enet_multiplayer_peer.h"
#include "scene/main/node.h"

class WebRTCEnetSession : public Node {
	GDCLASS(WebRTCEnetSession, Node);

	enum PendingOp {
		OP_NONE,
		OP_CREATE,
		OP_JOIN,
		OP_LIST,
	};

	Ref<SignalClient> signal_client;
	RelayClient relay;
	FakeAddressMap map;
	HashMap<int, Ref<WebRTCLink>> links;
	Ref<ENetMultiplayerPeer> peer;

	String signal_url;
	String game_id;
	String auth_token;
	String room_code;
	String local_fake_ip;
	int local_fake_port = 1;
	int local_signal_id = 0;
	bool is_host = false;
	bool force_relay = false;
	int ice_timeout_msec = 8000;
	uint64_t ice_deadline_msec = 0;
	PendingOp pending = OP_NONE;
	Array ice_servers;
	Array cached_rooms;
	bool session_emitted = false;
	bool host_transport_ready = false;
	uint64_t last_ping_msec = 0;

	void _on_signal_connected();
	void _on_signal_disconnected();
	void _on_signal_message(int p_type, int p_id, Dictionary p_data);
	void _on_local_description(const String &p_type, const String &p_sdp, int p_signal_id);
	void _on_local_candidate(const String &p_media, int p_index, const String &p_name, int p_signal_id);
	void _on_channel_open(int p_signal_id);
	void _on_using_turn(int p_signal_id);
	HashSet<int> notified_open;

	void _ensure_process();
	void _poll();
	void _fail(const String &p_reason);
	void _handle_hello(const Dictionary &p_data);
	void _handle_peer_info(const Dictionary &p_data);
	void _maybe_create_enet();
	void _maybe_emit_ready();
	Ref<WebRTCLink> _ensure_link(int p_signal_id, const IPAddress &p_ip, uint16_t p_port, bool p_offerer);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	Error configure(const String &p_signal_url, const String &p_game_id, const String &p_auth_token);
	Signal create_room(const String &p_name, int p_max_clients = 8, const String &p_password = String(), bool p_hidden = false, const PackedStringArray &p_tags = PackedStringArray());
	Signal join_room(const String &p_room_code, const String &p_password = String(), const String &p_display_name = String());
	Signal list_rooms();
	Error leave();
	Error seal();
	Error kick(int p_signal_id);
	Ref<ENetMultiplayerPeer> get_peer() const;
	void close();
	bool is_in_room() const;

	void set_force_relay(bool p_enable);
	bool get_force_relay() const { return force_relay; }
	void set_ice_timeout_msec(int p_msec);
	int get_ice_timeout_msec() const { return ice_timeout_msec; }

	String get_room_code() const { return room_code; }
	String get_local_fake_ip() const { return local_fake_ip; }
	int get_local_fake_port() const { return local_fake_port; }
	int get_local_signal_id() const { return local_signal_id; }

	Error send_datagram(const IPAddress &p_ip, uint16_t p_port, const uint8_t *p_buffer, int p_len);

	WebRTCEnetSession();
	~WebRTCEnetSession();
};
