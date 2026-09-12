/**************************************************************************/
/*  protocol.h                                                            */
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

#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

namespace GamesEnetWebrtcProtocol {

enum MessageType {
	MSG_JOIN = 0,
	MSG_ID = 1,
	MSG_PEER_CONNECT = 2,
	MSG_PEER_DISCONNECT = 3,
	MSG_OFFER = 4,
	MSG_ANSWER = 5,
	MSG_CANDIDATE = 6,
	MSG_SEAL = 7,
	MSG_HELLO = 8,
	MSG_PEER_INFO = 9,
	MSG_CREATE = 10,
	MSG_LIST = 11,
	MSG_LEAVE = 12,
	MSG_PEER_READY = 13,
	MSG_USE_RELAY = 14,
	MSG_RELAY_DATAGRAM = 15,
	MSG_ICE_CONFIG = 16,
	MSG_ERROR = 17,
	MSG_PING = 18,
	MSG_KICK = 19,
};

enum {
	PROTOCOL_VERSION = 1,
	MAX_ENVELOPE_BYTES = 16 * 1024,
	MAX_SDP_BYTES = 8 * 1024,
	MAX_CANDIDATE_BYTES = 2 * 1024,
	MAX_RELAY_BYTES = 1400,
	MAX_GAME_ID = 64,
	MAX_ROOM_NAME = 64,
	MAX_PASSWORD = 128,
	MAX_AUTH_TOKEN = 512,
	MAX_TAG_LEN = 32,
	MAX_TAGS = 8,
	ADVERTISED_ROOM_CAP = 8,
	HARD_SIGNAL_CAP = 254,
	MIN_SIGNAL_ID = 1,
	MAX_INBOUND_DATAGRAMS = 256,
};

struct Envelope {
	int v = PROTOCOL_VERSION;
	int type = 0;
	int id = 0;
	Dictionary data;
	bool valid = false;
};

String stringify_envelope(int p_type, int p_id, const Dictionary &p_data);
Envelope parse_envelope(const String &p_text);
bool payload_within_limits(int p_type, const Dictionary &p_data);

bool has_crlf(const String &p_s);
bool is_valid_game_id(const String &p_id);
bool is_valid_signal_url(const String &p_url);
bool is_valid_auth_token(const String &p_token);
bool is_valid_room_code(const String &p_code);
bool is_valid_room_name(const String &p_name);
bool is_valid_password(const String &p_password);
bool is_valid_signal_id(int p_id);
bool is_valid_fake_ip(const String &p_ip);
bool is_valid_fake_port(int p_port);
bool is_valid_sdp(const String &p_sdp);
bool is_valid_candidate(const String &p_candidate);
bool is_valid_relay_payload(const Vector<uint8_t> &p_payload);
bool is_valid_ice_url(const String &p_url);
bool is_valid_turn_username(const String &p_user);
bool is_valid_role(const String &p_role);
bool is_star_pair(int p_a, int p_b);
Array sanitize_ice_servers(const Array &p_servers);
Array sanitize_room_list(const Array &p_rooms);
int clamp_max_clients(int p_max);
String strip_controls(const String &p_s);

} //namespace GamesEnetWebrtcProtocol
