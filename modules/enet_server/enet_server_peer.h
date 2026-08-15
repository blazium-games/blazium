/**************************************************************************/
/*  enet_server_peer.h                                                    */
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
#include "modules/enet/enet_packet_peer.h"

#include <enet/enet.h>

class ENetServerPeer : public RefCounted {
	GDCLASS(ENetServerPeer, RefCounted);

public:
	enum ConnectionState {
		STATE_CONNECTING,
		STATE_PRELOGIN,
		STATE_AUTHENTICATED,
		STATE_DISCONNECTING,
	};

	enum AuthState {
		AUTH_PENDING,
		AUTH_APPROVED,
		AUTH_REJECTED,
	};

private:
	Ref<ENetPacketPeer> packet_peer;
	int peer_id;
	ConnectionState connection_state;
	AuthState auth_state;
	Dictionary custom_data;
	uint64_t connection_time_ms;
	bool pending_disconnect;
	String disconnect_reason;

protected:
	static void _bind_methods();

public:
	// Getters
	int get_peer_id() const;
	String get_remote_address() const;
	int get_remote_port() const;
	ConnectionState get_connection_state() const;
	AuthState get_auth_state() const;
	Dictionary get_custom_data() const;
	void set_custom_data(const Dictionary &p_data);
	float get_connection_time() const;
	int get_ping() const;

	// Peer operations (called by user)
	Error send_packet(const Variant &p_packet, int p_channel, bool p_reliable);
	Error send_raw_packet(const PackedByteArray &p_data, int p_channel, bool p_reliable);
	void disconnect_peer(const String &p_reason = "");
	void reject(const String &p_reason = "");
	void authenticate();

	// Internal operations (called by ENetServer)
	void _set_packet_peer(const Ref<ENetPacketPeer> &p_peer);
	Ref<ENetPacketPeer> _get_packet_peer() const;
	void _set_peer_id(int p_id);
	void _set_connection_state(ConnectionState p_state);
	void _set_auth_state(AuthState p_state);
	bool _has_pending_disconnect() const;
	String _get_disconnect_reason() const;
	void _clear_pending_disconnect();

	// Statistics
	double get_statistic(ENetPacketPeer::PeerStatistic p_stat);

	ENetServerPeer();
	~ENetServerPeer();
};

VARIANT_ENUM_CAST(ENetServerPeer::ConnectionState);
VARIANT_ENUM_CAST(ENetServerPeer::AuthState);
