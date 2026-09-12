/**************************************************************************/
/*  protocol.cpp                                                          */
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

#include "protocol.h"

#include "core/crypto/crypto_core.h"
#include "core/io/ip_address.h"
#include "core/io/json.h"
#include "core/string/ustring.h"

namespace GamesEnetWebrtcProtocol {

static bool _charset_id(char32_t c) {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
}

static bool _crockford(char32_t c) {
	if (c >= 'a' && c <= 'z') {
		c = c - 'a' + 'A';
	}
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'H') || (c >= 'J' && c <= 'K') || (c >= 'M' && c <= 'N') || (c >= 'P' && c <= 'T') || (c >= 'V' && c <= 'Z');
}

bool has_crlf(const String &p_s) {
	return p_s.contains("\r") || p_s.contains("\n");
}

String strip_controls(const String &p_s) {
	String out;
	for (int i = 0; i < p_s.length(); i++) {
		const char32_t c = p_s[i];
		if (c >= 32 && c != 127) {
			out += c;
		}
	}
	return out.strip_edges();
}

bool is_valid_game_id(const String &p_id) {
	if (p_id.is_empty() || p_id.length() > MAX_GAME_ID || has_crlf(p_id)) {
		return false;
	}
	for (int i = 0; i < p_id.length(); i++) {
		if (!_charset_id(p_id[i])) {
			return false;
		}
	}
	return true;
}

static String _host_from_hostport(const String &p_hp) {
	String hp = p_hp;
	const int q = hp.find("?");
	if (q >= 0) {
		hp = hp.substr(0, q);
	}
	if (hp.begins_with("[")) {
		const int br = hp.find("]");
		if (br <= 1) {
			return String();
		}
		return hp.substr(1, br - 1);
	}
	const int colon = hp.rfind(":");
	if (colon >= 0) {
		return hp.substr(0, colon);
	}
	return hp;
}

static bool _looks_like_ip(const String &p_host) {
	if (p_host.contains(":")) {
		return true;
	}
	const PackedStringArray parts = p_host.split(".");
	if (parts.size() != 4) {
		return false;
	}
	for (int i = 0; i < 4; i++) {
		if (parts[i].is_empty() || !parts[i].is_valid_int()) {
			return false;
		}
	}
	return true;
}

static bool _is_denied_url_host(const String &p_host) {
	if (p_host.is_empty()) {
		return true;
	}
	if (!_looks_like_ip(p_host)) {
		return false;
	}
	const IPAddress ip(p_host);
	if (!ip.is_valid()) {
		return true;
	}
	if (ip.is_ipv4()) {
		const uint8_t *b = ip.get_ipv4();
		if (b[0] == 0 && b[1] == 0 && b[2] == 0 && b[3] == 0) {
			return true;
		}
		if (b[0] == 127) {
			return false;
		}
		if (b[0] == 169 && b[1] == 254) {
			return true;
		}
		if (b[0] == 100 && b[1] == 100 && b[2] == 100 && b[3] == 200) {
			return true;
		}
		if (b[0] >= 224) {
			return true;
		}
		return false;
	}
	const uint8_t *v6 = ip.get_ipv6();
	bool unspecified = true;
	for (int i = 0; i < 16; i++) {
		if (v6[i] != 0) {
			unspecified = false;
			break;
		}
	}
	if (unspecified) {
		return true;
	}
	bool loopback = true;
	for (int i = 0; i < 15; i++) {
		if (v6[i] != 0) {
			loopback = false;
			break;
		}
	}
	if (loopback && v6[15] == 1) {
		return false;
	}
	if (v6[0] == 0xfe && (v6[1] & 0xc0) == 0x80) {
		return true;
	}
	if (v6[0] == 0xff) {
		return true;
	}
	return false;
}

bool is_valid_signal_url(const String &p_url) {
	if (has_crlf(p_url) || p_url.length() > 1024 || p_url.contains(" ") || p_url.contains("\t")) {
		return false;
	}
	int start = 0;
	if (p_url.begins_with("wss://")) {
		start = 6;
	} else if (p_url.begins_with("ws://")) {
		start = 5;
	} else {
		return false;
	}
	String rest = p_url.substr(start);
	const int slash = rest.find("/");
	if (slash >= 0) {
		rest = rest.substr(0, slash);
	}
	return !_is_denied_url_host(_host_from_hostport(rest));
}

bool is_valid_auth_token(const String &p_token) {
	if (p_token.is_empty()) {
		return true;
	}
	if (p_token.length() > MAX_AUTH_TOKEN || has_crlf(p_token)) {
		return false;
	}
	for (int i = 0; i < p_token.length(); i++) {
		if (p_token[i] < 32 || p_token[i] == 127) {
			return false;
		}
	}
	return true;
}

bool is_valid_room_code(const String &p_code) {
	if (p_code.length() != 6) {
		return false;
	}
	for (int i = 0; i < p_code.length(); i++) {
		if (!_crockford(p_code[i])) {
			return false;
		}
	}
	return true;
}

bool is_valid_room_name(const String &p_name) {
	const String s = strip_controls(p_name);
	return !s.is_empty() && s.length() <= MAX_ROOM_NAME && !has_crlf(s);
}

bool is_valid_password(const String &p_password) {
	return p_password.length() <= MAX_PASSWORD && !has_crlf(p_password);
}

bool is_valid_signal_id(int p_id) {
	return p_id >= MIN_SIGNAL_ID && p_id <= HARD_SIGNAL_CAP;
}

bool is_valid_fake_ip(const String &p_ip) {
	const PackedStringArray parts = p_ip.split(".");
	if (parts.size() != 4) {
		return false;
	}
	if (parts[0] != "10" || parts[1] != "66" || parts[2] != "0") {
		return false;
	}
	const int n = parts[3].to_int();
	return n >= 1 && n <= HARD_SIGNAL_CAP && String::num_int64(n) == parts[3];
}

bool is_valid_fake_port(int p_port) {
	return p_port == 1;
}

bool is_valid_sdp(const String &p_sdp) {
	if (p_sdp.is_empty() || p_sdp.utf8().length() > MAX_SDP_BYTES) {
		return false;
	}
	return p_sdp.begins_with("v=");
}

bool is_valid_candidate(const String &p_candidate) {
	if (p_candidate.is_empty() || p_candidate.utf8().length() > MAX_CANDIDATE_BYTES) {
		return false;
	}
	for (int i = 0; i < p_candidate.length(); i++) {
		const char32_t c = p_candidate[i];
		if (c < 32 || c > 126) {
			return false;
		}
	}
	return true;
}

bool is_valid_relay_payload(const Vector<uint8_t> &p_payload) {
	return p_payload.size() > 0 && p_payload.size() <= MAX_RELAY_BYTES;
}

int clamp_max_clients(int p_max) {
	if (p_max < 1) {
		return 1;
	}
	if (p_max > ADVERTISED_ROOM_CAP) {
		return ADVERTISED_ROOM_CAP;
	}
	return p_max;
}

String stringify_envelope(int p_type, int p_id, const Dictionary &p_data) {
	Dictionary env;
	env["v"] = PROTOCOL_VERSION;
	env["type"] = p_type;
	env["id"] = p_id;
	env["data"] = p_data;
	return JSON::stringify(env);
}

Envelope parse_envelope(const String &p_text) {
	Envelope out;
	if (p_text.utf8().length() > MAX_ENVELOPE_BYTES) {
		return out;
	}
	Variant parsed = JSON::parse_string(p_text);
	if (parsed.get_type() != Variant::DICTIONARY) {
		return out;
	}
	Dictionary d = parsed;
	out.v = int(d.get("v", 0));
	out.type = int(d.get("type", -1));
	out.id = int(d.get("id", 0));
	if (!d.has("data") || d["data"].get_type() != Variant::DICTIONARY) {
		return out;
	}
	out.data = d["data"];
	out.valid = out.v == PROTOCOL_VERSION && out.type >= MSG_JOIN && out.type <= MSG_KICK;
	if (out.valid && out.id != 0 && !is_valid_signal_id(out.id)) {
		out.valid = false;
	}
	return out;
}

bool payload_within_limits(int p_type, const Dictionary &p_data) {
	if (p_type == MSG_OFFER || p_type == MSG_ANSWER) {
		return is_valid_sdp(String(p_data.get("sdp", "")));
	}
	if (p_type == MSG_CANDIDATE) {
		return is_valid_candidate(String(p_data.get("candidate", "")));
	}
	if (p_type == MSG_RELAY_DATAGRAM) {
		const String b64 = String(p_data.get("payload", ""));
		if (b64.is_empty() || b64.length() > MAX_RELAY_BYTES * 2) {
			return false;
		}
		const int to = int(p_data.get("to", 0));
		if (to != 0 && !is_valid_signal_id(to)) {
			return false;
		}
		const CharString cs = b64.ascii();
		size_t decoded = 0;
		Vector<uint8_t> buf;
		buf.resize(cs.length());
		if (CryptoCore::b64_decode(buf.ptrw(), buf.size(), &decoded, (const uint8_t *)cs.get_data(), cs.length()) != OK) {
			return false;
		}
		buf.resize((int)decoded);
		return is_valid_relay_payload(buf);
	}
	if (p_type == MSG_CREATE) {
		return is_valid_room_name(String(p_data.get("name", ""))) && is_valid_password(String(p_data.get("password", "")));
	}
	if (p_type == MSG_JOIN) {
		return is_valid_room_code(String(p_data.get("room_code", ""))) && is_valid_password(String(p_data.get("password", "")));
	}
	if (p_type == MSG_KICK) {
		return is_valid_signal_id(int(p_data.get("signal_id", 0)));
	}
	return true;
}

bool is_valid_ice_url(const String &p_url) {
	if (p_url.is_empty() || p_url.length() > 256 || has_crlf(p_url) || p_url.contains(" ") || p_url.contains("\t")) {
		return false;
	}
	const String lower = p_url.to_lower();
	String rest;
	if (lower.begins_with("turns:")) {
		rest = p_url.substr(6);
	} else if (lower.begins_with("turn:")) {
		rest = p_url.substr(5);
	} else if (lower.begins_with("stun:")) {
		rest = p_url.substr(5);
	} else {
		return false;
	}
	if (rest.is_empty() || rest.begins_with("//")) {
		return false;
	}
	return !_is_denied_url_host(_host_from_hostport(rest));
}

bool is_valid_turn_username(const String &p_user) {
	if (p_user.is_empty() || has_crlf(p_user) || p_user.length() > 256) {
		return false;
	}
	const PackedStringArray parts = p_user.split(":");
	if (parts.size() != 3) {
		return false;
	}
	if (!parts[0].is_valid_int()) {
		return false;
	}
	if (parts[1].is_empty() || parts[1].length() > 64) {
		return false;
	}
	for (int i = 0; i < parts[1].length(); i++) {
		if (!_charset_id(parts[1][i])) {
			return false;
		}
	}
	return is_valid_signal_id(parts[2].to_int()) && String::num_int64(parts[2].to_int()) == parts[2];
}

bool is_valid_role(const String &p_role) {
	return p_role == "host" || p_role == "joiner";
}

bool is_star_pair(int p_a, int p_b) {
	return (p_a == 1 || p_b == 1) && p_a != p_b && is_valid_signal_id(p_a) && is_valid_signal_id(p_b);
}

Array sanitize_ice_servers(const Array &p_servers) {
	Array out;
	for (int i = 0; i < p_servers.size() && out.size() < 8; i++) {
		if (p_servers[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary src = p_servers[i];
		Array urls;
		const Variant uv = src.get("urls", Variant());
		if (uv.get_type() == Variant::ARRAY) {
			urls = uv;
		} else if (uv.get_type() == Variant::STRING && is_valid_ice_url(String(uv))) {
			urls.push_back(String(uv));
		}
		Array kept;
		for (int u = 0; u < urls.size(); u++) {
			const String url = String(urls[u]);
			if (is_valid_ice_url(url)) {
				kept.push_back(url);
			}
		}
		if (kept.is_empty()) {
			continue;
		}
		Dictionary dst;
		dst["urls"] = kept;
		const String user = String(src.get("username", ""));
		const String cred = String(src.get("credential", ""));
		if (is_valid_turn_username(user) && !has_crlf(cred) && cred.length() <= 256) {
			dst["username"] = user;
			if (!cred.is_empty()) {
				dst["credential"] = cred;
			}
		}
		out.push_back(dst);
	}
	return out;
}

Array sanitize_room_list(const Array &p_rooms) {
	Array out;
	for (int i = 0; i < p_rooms.size() && out.size() < 100; i++) {
		if (p_rooms[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary src = p_rooms[i];
		const String code = String(src.get("room_code", ""));
		if (!is_valid_room_code(code)) {
			continue;
		}
		Dictionary dst;
		dst["room_code"] = code;
		const String name = strip_controls(String(src.get("name", "")));
		if (is_valid_room_name(name)) {
			dst["name"] = name;
		}
		dst["max"] = clamp_max_clients(int(src.get("max", ADVERTISED_ROOM_CAP)));
		int members = int(src.get("members", 0));
		if (members < 0) {
			members = 0;
		}
		if (members > HARD_SIGNAL_CAP) {
			members = HARD_SIGNAL_CAP;
		}
		dst["members"] = members;
		Variant tags_v = src.get("tags", Array());
		Array src_tags;
		if (tags_v.get_type() == Variant::PACKED_STRING_ARRAY) {
			PackedStringArray psa = tags_v;
			for (int t = 0; t < psa.size(); t++) {
				src_tags.push_back(psa[t]);
			}
		} else if (tags_v.get_type() == Variant::ARRAY) {
			src_tags = tags_v;
		}
		Array tags_out;
		for (int t = 0; t < src_tags.size() && tags_out.size() < MAX_TAGS; t++) {
			const String tag = strip_controls(String(src_tags[t]));
			if (is_valid_game_id(tag) && tag.length() <= MAX_TAG_LEN) {
				tags_out.push_back(tag);
			}
		}
		if (!tags_out.is_empty()) {
			dst["tags"] = tags_out;
		}
		out.push_back(dst);
	}
	return out;
}

} //namespace GamesEnetWebrtcProtocol
