/**************************************************************************/
/*  webrtc_link.cpp                                                       */
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

#include "webrtc_link.h"

#include "fake_address_map.h"
#include "protocol.h"

#include <cstring>

void WebRTCLink::_bind_methods() {
	ADD_SIGNAL(MethodInfo("local_description", PropertyInfo(Variant::STRING, "type"), PropertyInfo(Variant::STRING, "sdp")));
	ADD_SIGNAL(MethodInfo("local_candidate", PropertyInfo(Variant::STRING, "media"), PropertyInfo(Variant::INT, "index"), PropertyInfo(Variant::STRING, "name")));
	ADD_SIGNAL(MethodInfo("channel_open"));
	ADD_SIGNAL(MethodInfo("using_turn"));
}

void WebRTCLink::setup(int p_remote_signal_id, const IPAddress &p_fake_ip, uint16_t p_fake_port, FakeAddressMap *p_map, bool p_offerer, const Array &p_ice_servers) {
	remote_signal_id = p_remote_signal_id;
	remote_fake_ip = p_fake_ip;
	remote_fake_port = p_fake_port;
	map = p_map;
	offerer = p_offerer;

	pc = Ref<WebRTCPeerConnection>(WebRTCPeerConnection::create());
	ERR_FAIL_COND(pc.is_null());

	pc->connect("session_description_created", callable_mp(this, &WebRTCLink::_on_session_description));
	pc->connect("ice_candidate_created", callable_mp(this, &WebRTCLink::_on_ice_candidate));

	Dictionary config;
	if (!p_ice_servers.is_empty()) {
		config["iceServers"] = p_ice_servers;
	}
	pc->initialize(config);

	Dictionary opts;
	opts["negotiated"] = true;
	opts["id"] = 1;
	opts["ordered"] = false;
	opts["maxRetransmits"] = 0;
	channel = pc->create_data_channel("enet", opts);
	if (channel.is_valid()) {
		channel->set_write_mode(WebRTCDataChannel::WRITE_MODE_BINARY);
	}

	if (offerer) {
		pc->create_offer();
	}
}

void WebRTCLink::set_mode(Mode p_mode) {
	mode = p_mode;
}

void WebRTCLink::_on_session_description(const String &p_type, const String &p_sdp) {
	if (pc.is_valid()) {
		pc->set_local_description(p_type, p_sdp);
	}
	emit_signal("local_description", p_type, p_sdp);
}

void WebRTCLink::_on_ice_candidate(const String &p_media, int p_index, const String &p_name) {
	if (p_name.contains("typ relay") && mode != MODE_RELAY) {
		mode = MODE_TURN;
		emit_signal("using_turn");
	}
	emit_signal("local_candidate", p_media, p_index, p_name);
}

void WebRTCLink::set_remote_description(const String &p_type, const String &p_sdp) {
	ERR_FAIL_COND(pc.is_null());
	pc->set_remote_description(p_type, p_sdp);
}

void WebRTCLink::add_ice_candidate(const String &p_media, int p_index, const String &p_name) {
	ERR_FAIL_COND(pc.is_null());
	pc->add_ice_candidate(p_media, p_index, p_name);
}

bool WebRTCLink::is_open() const {
	if (mode == MODE_RELAY) {
		return true;
	}
	return channel.is_valid() && channel->get_ready_state() == WebRTCDataChannel::STATE_OPEN;
}

Error WebRTCLink::send(const uint8_t *p_buffer, int p_len) {
	ERR_FAIL_NULL_V(p_buffer, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_len <= 0 || p_len > GamesEnetWebrtcProtocol::MAX_RELAY_BYTES, ERR_INVALID_PARAMETER);
	if (mode == MODE_RELAY || channel.is_null() || channel->get_ready_state() != WebRTCDataChannel::STATE_OPEN) {
		return ERR_UNAVAILABLE;
	}
	return channel->put_packet(p_buffer, p_len);
}

void WebRTCLink::_drain_channel() {
	if (channel.is_null() || map == nullptr) {
		return;
	}
	channel->poll();
	int n = 0;
	while (channel->get_available_packet_count() > 0 && n < 64) {
		n++;
		const uint8_t *buf = nullptr;
		int size = 0;
		if (channel->get_packet(&buf, size) != OK || buf == nullptr || size <= 0) {
			break;
		}
		if (size > GamesEnetWebrtcProtocol::MAX_RELAY_BYTES) {
			continue;
		}
		Vector<uint8_t> data;
		data.resize(size);
		memcpy(data.ptrw(), buf, size);
		map->push_inbound(data, remote_fake_ip, remote_fake_port);
	}
}

void WebRTCLink::poll() {
	if (pc.is_valid()) {
		pc->poll();
	}
	if (channel.is_valid()) {
		const WebRTCDataChannel::ChannelState st = channel->get_ready_state();
		if (st == WebRTCDataChannel::STATE_OPEN) {
			_drain_channel();
		}
	}
}

void WebRTCLink::close() {
	if (channel.is_valid()) {
		channel->close();
		channel.unref();
	}
	if (pc.is_valid()) {
		pc->close();
		pc.unref();
	}
}
