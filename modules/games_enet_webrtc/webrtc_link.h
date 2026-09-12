/**************************************************************************/
/*  webrtc_link.h                                                         */
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

#include "core/io/ip_address.h"
#include "core/object/ref_counted.h"
#include "core/variant/dictionary.h"
#include "modules/webrtc/webrtc_data_channel.h"
#include "modules/webrtc/webrtc_peer_connection.h"

class FakeAddressMap;

class WebRTCLink : public RefCounted {
	GDCLASS(WebRTCLink, RefCounted);

public:
	enum Mode {
		MODE_P2P,
		MODE_TURN,
		MODE_RELAY,
	};

private:
	int remote_signal_id = 0;
	IPAddress remote_fake_ip;
	uint16_t remote_fake_port = 1;
	bool offerer = false;
	Mode mode = MODE_P2P;
	FakeAddressMap *map = nullptr;

	Ref<WebRTCPeerConnection> pc;
	Ref<WebRTCDataChannel> channel;

	void _on_session_description(const String &p_type, const String &p_sdp);
	void _on_ice_candidate(const String &p_media, int p_index, const String &p_name);
	void _drain_channel();

protected:
	static void _bind_methods();

public:
	void setup(int p_remote_signal_id, const IPAddress &p_fake_ip, uint16_t p_fake_port, FakeAddressMap *p_map, bool p_offerer, const Array &p_ice_servers);
	void set_mode(Mode p_mode);
	Mode get_mode() const { return mode; }

	void set_remote_description(const String &p_type, const String &p_sdp);
	void add_ice_candidate(const String &p_media, int p_index, const String &p_name);

	bool is_open() const;
	bool has_peer_connection() const { return pc.is_valid(); }
	Error send(const uint8_t *p_buffer, int p_len);
	void poll();
	void close();

	int get_remote_signal_id() const { return remote_signal_id; }
	IPAddress get_remote_fake_ip() const { return remote_fake_ip; }
	uint16_t get_remote_fake_port() const { return remote_fake_port; }
};
