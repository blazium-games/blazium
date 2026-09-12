/**************************************************************************/
/*  signal_client.cpp                                                     */
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

#include "signal_client.h"

#include "protocol.h"

#include "core/io/json.h"

void SignalClient::_bind_methods() {
	ClassDB::bind_method(D_METHOD("configure", "url", "game_id", "auth_token"), &SignalClient::configure);
	ClassDB::bind_method(D_METHOD("connect_to_signal"), &SignalClient::connect_to_signal);
	ClassDB::bind_method(D_METHOD("close"), &SignalClient::close);
	ClassDB::bind_method(D_METHOD("poll"), &SignalClient::poll);
	ClassDB::bind_method(D_METHOD("send_message", "type", "data", "id"), &SignalClient::send_message, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("is_open"), &SignalClient::is_open);

	ADD_SIGNAL(MethodInfo("connected"));
	ADD_SIGNAL(MethodInfo("disconnected"));
	ADD_SIGNAL(MethodInfo("message_received", PropertyInfo(Variant::INT, "type"), PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::DICTIONARY, "data")));
}

Error SignalClient::configure(const String &p_url, const String &p_game_id, const String &p_auth_token) {
	if (!GamesEnetWebrtcProtocol::is_valid_signal_url(p_url) || !GamesEnetWebrtcProtocol::is_valid_game_id(p_game_id) || !GamesEnetWebrtcProtocol::is_valid_auth_token(p_auth_token)) {
		url = String();
		game_id = String();
		auth_token = String();
		return ERR_INVALID_PARAMETER;
	}
	url = p_url;
	game_id = p_game_id;
	auth_token = p_auth_token;
	return OK;
}

Error SignalClient::connect_to_signal() {
	ERR_FAIL_COND_V(!GamesEnetWebrtcProtocol::is_valid_signal_url(url) || !GamesEnetWebrtcProtocol::is_valid_game_id(game_id), ERR_UNCONFIGURED);
	close();
	ws = Ref<WebSocketPeer>(WebSocketPeer::create());
	ERR_FAIL_COND_V(ws.is_null(), ERR_CANT_CREATE);
	ws->set_inbound_buffer_size(GamesEnetWebrtcProtocol::MAX_ENVELOPE_BYTES + 1024);
	ws->set_max_queued_packets(64);

	PackedStringArray headers;
	headers.push_back("X-Game-Id: " + game_id);
	if (!auth_token.is_empty()) {
		headers.push_back("Authorization: Bearer " + auth_token);
	}
	ws->set_handshake_headers(headers);

	String connect_url = url;
	if (connect_url.contains("?")) {
		connect_url += "&game_id=" + game_id.uri_encode();
	} else {
		connect_url += "?game_id=" + game_id.uri_encode();
	}
	announced_open = false;
	return ws->connect_to_url(connect_url);
}

void SignalClient::close() {
	if (ws.is_valid()) {
		ws->close();
		ws.unref();
	}
	announced_open = false;
}

void SignalClient::poll() {
	if (ws.is_null()) {
		return;
	}
	ws->poll();
	const WebSocketPeer::State st = ws->get_ready_state();
	if (st == WebSocketPeer::STATE_OPEN) {
		if (!announced_open) {
			announced_open = true;
			emit_signal("connected");
		}
		while (ws->get_available_packet_count() > 0) {
			Vector<uint8_t> pkt;
			ws->get_packet_buffer(pkt);
			const String text = String::utf8((const char *)pkt.ptr(), pkt.size());
			GamesEnetWebrtcProtocol::Envelope env = GamesEnetWebrtcProtocol::parse_envelope(text);
			if (env.valid) {
				emit_signal("message_received", env.type, env.id, env.data);
			}
		}
	} else if (st == WebSocketPeer::STATE_CLOSED && announced_open) {
		announced_open = false;
		emit_signal("disconnected");
	}
}

Error SignalClient::send_message(int p_type, const Dictionary &p_data, int p_id) {
	ERR_FAIL_COND_V(ws.is_null() || ws->get_ready_state() != WebSocketPeer::STATE_OPEN, ERR_UNAVAILABLE);
	if (p_id != 0 && !GamesEnetWebrtcProtocol::is_valid_signal_id(p_id)) {
		return ERR_INVALID_PARAMETER;
	}
	if (!GamesEnetWebrtcProtocol::payload_within_limits(p_type, p_data)) {
		return ERR_INVALID_PARAMETER;
	}
	const String text = GamesEnetWebrtcProtocol::stringify_envelope(p_type, p_id, p_data);
	return ws->send_text(text);
}

bool SignalClient::is_open() const {
	return ws.is_valid() && ws->get_ready_state() == WebSocketPeer::STATE_OPEN;
}
