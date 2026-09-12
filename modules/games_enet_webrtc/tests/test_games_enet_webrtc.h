/**************************************************************************/
/*  test_games_enet_webrtc.h                                              */
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

#include "../fake_address_map.h"
#include "../protocol.h"

#include "tests/test_macros.h"

namespace TestGamesEnetWebrtc {

TEST_CASE("[Modules][GamesEnetWebrtc] protocol envelope roundtrip") {
	Dictionary data;
	data["room_code"] = "ABC123";
	data["signal_id"] = 1;
	const String json = GamesEnetWebrtcProtocol::stringify_envelope(GamesEnetWebrtcProtocol::MSG_HELLO, 0, data);
	GamesEnetWebrtcProtocol::Envelope env = GamesEnetWebrtcProtocol::parse_envelope(json);
	CHECK(env.valid);
	CHECK(env.v == GamesEnetWebrtcProtocol::PROTOCOL_VERSION);
	CHECK(env.type == GamesEnetWebrtcProtocol::MSG_HELLO);
	CHECK(String(env.data["room_code"]) == "ABC123");
}

TEST_CASE("[Modules][GamesEnetWebrtc] protocol rejects invalid json") {
	GamesEnetWebrtcProtocol::Envelope env = GamesEnetWebrtcProtocol::parse_envelope("not-json");
	CHECK_FALSE(env.valid);
}

TEST_CASE("[Modules][GamesEnetWebrtc] SDP size limit") {
	Dictionary data;
	const String huge = String("a").repeat(GamesEnetWebrtcProtocol::MAX_SDP_BYTES + 8);
	data["sdp"] = huge;
	CHECK_FALSE(GamesEnetWebrtcProtocol::payload_within_limits(GamesEnetWebrtcProtocol::MSG_OFFER, data));
	data["sdp"] = "v=0";
	CHECK(GamesEnetWebrtcProtocol::payload_within_limits(GamesEnetWebrtcProtocol::MSG_OFFER, data));
	data["sdp"] = "o=not-sdp";
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_sdp("o=not-sdp"));
}

TEST_CASE("[Modules][GamesEnetWebrtc] envelope oversize and type bounds") {
	const String huge = String("x").repeat(GamesEnetWebrtcProtocol::MAX_ENVELOPE_BYTES + 8);
	CHECK_FALSE(GamesEnetWebrtcProtocol::parse_envelope(huge).valid);
	Dictionary data;
	data["room_code"] = "ABC123";
	String bad = GamesEnetWebrtcProtocol::stringify_envelope(20, 0, data);
	CHECK_FALSE(GamesEnetWebrtcProtocol::parse_envelope(bad).valid);
	bad = GamesEnetWebrtcProtocol::stringify_envelope(-1, 0, data);
	CHECK_FALSE(GamesEnetWebrtcProtocol::parse_envelope("{\"v\":1,\"type\":-1,\"id\":0,\"data\":{}}").valid);
	CHECK_FALSE(GamesEnetWebrtcProtocol::parse_envelope("{\"v\":1,\"type\":18,\"id\":255,\"data\":{}}").valid);
	CHECK_FALSE(GamesEnetWebrtcProtocol::parse_envelope("{\"v\":1,\"type\":18,\"id\":-1,\"data\":{}}").valid);
	CHECK(GamesEnetWebrtcProtocol::parse_envelope("{\"v\":1,\"type\":18,\"id\":0,\"data\":{}}").valid);
	Dictionary ping;
	ping["t"] = "ping";
	const String ping_json = GamesEnetWebrtcProtocol::stringify_envelope(GamesEnetWebrtcProtocol::MSG_PING, 2, ping);
	GamesEnetWebrtcProtocol::Envelope ping_env = GamesEnetWebrtcProtocol::parse_envelope(ping_json);
	CHECK(ping_env.valid);
	CHECK(ping_env.id == 2);
	CHECK(ping_env.type == GamesEnetWebrtcProtocol::MSG_PING);
	Dictionary empty;
	CHECK(GamesEnetWebrtcProtocol::payload_within_limits(GamesEnetWebrtcProtocol::MSG_LIST, empty));
	CHECK(GamesEnetWebrtcProtocol::payload_within_limits(GamesEnetWebrtcProtocol::MSG_LEAVE, empty));
	CHECK(GamesEnetWebrtcProtocol::payload_within_limits(GamesEnetWebrtcProtocol::MSG_PING, empty));
	CHECK(GamesEnetWebrtcProtocol::payload_within_limits(GamesEnetWebrtcProtocol::MSG_SEAL, empty));
	Dictionary relay_bad_to;
	relay_bad_to["to"] = 255;
	relay_bad_to["payload"] = "AQID";
	CHECK_FALSE(GamesEnetWebrtcProtocol::payload_within_limits(GamesEnetWebrtcProtocol::MSG_RELAY_DATAGRAM, relay_bad_to));
	Dictionary relay_ok;
	relay_ok["to"] = 2;
	relay_ok["payload"] = "AQID";
	CHECK(GamesEnetWebrtcProtocol::payload_within_limits(GamesEnetWebrtcProtocol::MSG_RELAY_DATAGRAM, relay_ok));
}

TEST_CASE("[Modules][GamesEnetWebrtc] game_id and url sanitization") {
	CHECK(GamesEnetWebrtcProtocol::is_valid_game_id("webrtc-enet-module-test"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_game_id("bad\r\nid"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_game_id("has space"));
	CHECK(GamesEnetWebrtcProtocol::is_valid_signal_url("ws://127.0.0.1:8080/v1/signal"));
	CHECK(GamesEnetWebrtcProtocol::is_valid_signal_url("wss://signal.example/v1"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_signal_url("http://evil"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_signal_url("ws://x y"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_signal_url("ws://evil\r\nHost: h"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_auth_token("tok\nCRLF"));
	Dictionary join_bad;
	CHECK_FALSE(GamesEnetWebrtcProtocol::payload_within_limits(GamesEnetWebrtcProtocol::MSG_JOIN, join_bad));
	join_bad["room_code"] = "ABC234";
	CHECK(GamesEnetWebrtcProtocol::payload_within_limits(GamesEnetWebrtcProtocol::MSG_JOIN, join_bad));
}

TEST_CASE("[Modules][GamesEnetWebrtc] fake ip sanitization") {
	CHECK(GamesEnetWebrtcProtocol::is_valid_fake_ip("10.66.0.1"));
	CHECK(GamesEnetWebrtcProtocol::is_valid_fake_ip("10.66.0.254"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_fake_ip("8.8.8.8"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_fake_ip("10.66.0.0"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_fake_ip("10.66.1.1"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_fake_ip("10.66.0.01"));
	CHECK(GamesEnetWebrtcProtocol::is_valid_fake_port(1));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_fake_port(0));
}

TEST_CASE("[Modules][GamesEnetWebrtc] candidate and relay limits") {
	CHECK(GamesEnetWebrtcProtocol::is_valid_candidate("candidate:1 1 UDP 1 1.2.3.4 9 typ host"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_candidate(String("a").repeat(GamesEnetWebrtcProtocol::MAX_CANDIDATE_BYTES + 2)));
	Vector<uint8_t> ok;
	ok.resize(16);
	CHECK(GamesEnetWebrtcProtocol::is_valid_relay_payload(ok));
	Vector<uint8_t> huge;
	huge.resize(GamesEnetWebrtcProtocol::MAX_RELAY_BYTES + 1);
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_relay_payload(huge));
}

TEST_CASE("[Modules][GamesEnetWebrtc] ICE url sanitization") {
	CHECK(GamesEnetWebrtcProtocol::is_valid_ice_url("stun:127.0.0.1:3478"));
	CHECK(GamesEnetWebrtcProtocol::is_valid_ice_url("STUN:127.0.0.1:3478"));
	CHECK(GamesEnetWebrtcProtocol::is_valid_ice_url("turn:example.com:3478?transport=udp"));
	CHECK(GamesEnetWebrtcProtocol::is_valid_ice_url("turns:example.com:5349"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_ice_url("http://evil"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_ice_url("stun:x\r\nHost: h"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_ice_url("stun://example.com:3478"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_ice_url("stun:has space"));
	CHECK(GamesEnetWebrtcProtocol::is_valid_ice_url("stun:127.0.0.1:3478"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_ice_url("stun:169.254.169.254:80"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_ice_url("stun:0.0.0.0:3478"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_signal_url("ws://169.254.169.254/v1"));
	CHECK(GamesEnetWebrtcProtocol::clamp_max_clients(999) == 8);
	CHECK(GamesEnetWebrtcProtocol::clamp_max_clients(0) == 1);
	Array servers;
	Dictionary bad;
	bad["urls"] = "http://no";
	servers.push_back(bad);
	Dictionary good;
	Array urls;
	urls.push_back("stun:127.0.0.1:3478");
	good["urls"] = urls;
	servers.push_back(good);
	Array kept = GamesEnetWebrtcProtocol::sanitize_ice_servers(servers);
	CHECK(kept.size() == 1);
	Array many;
	for (int i = 0; i < 12; i++) {
		Dictionary d;
		Array u;
		u.push_back("stun:127.0.0.1:3478");
		d["urls"] = u;
		many.push_back(d);
	}
	CHECK(GamesEnetWebrtcProtocol::sanitize_ice_servers(many).size() == 8);
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_ice_url(""));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_ice_url(String("stun:") + String("x").repeat(300)));
	Dictionary crlf_user;
	Array stun_urls;
	stun_urls.push_back("stun:127.0.0.1:3478");
	crlf_user["urls"] = stun_urls;
	crlf_user["username"] = "bad\nuser";
	crlf_user["credential"] = "ok";
	Array mixed;
	mixed.push_back(crlf_user);
	Array sanitized = GamesEnetWebrtcProtocol::sanitize_ice_servers(mixed);
	CHECK(sanitized.size() == 1);
	Dictionary out0 = sanitized[0];
	CHECK_FALSE(out0.has("username"));
	CHECK_FALSE(out0.has("credential"));
	CHECK(GamesEnetWebrtcProtocol::is_valid_turn_username("1700000000:ABC234:2"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_turn_username("not-a-user"));
	CHECK(GamesEnetWebrtcProtocol::is_star_pair(1, 2));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_star_pair(2, 3));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_star_pair(1, 1));
	Dictionary create_bad;
	create_bad["name"] = "";
	CHECK_FALSE(GamesEnetWebrtcProtocol::payload_within_limits(GamesEnetWebrtcProtocol::MSG_CREATE, create_bad));
	Dictionary create_ok;
	create_ok["name"] = "ok";
	CHECK(GamesEnetWebrtcProtocol::payload_within_limits(GamesEnetWebrtcProtocol::MSG_CREATE, create_ok));
	Dictionary kick_bad;
	kick_bad["signal_id"] = 0;
	CHECK_FALSE(GamesEnetWebrtcProtocol::payload_within_limits(GamesEnetWebrtcProtocol::MSG_KICK, kick_bad));
	kick_bad["signal_id"] = 255;
	CHECK_FALSE(GamesEnetWebrtcProtocol::payload_within_limits(GamesEnetWebrtcProtocol::MSG_KICK, kick_bad));
	kick_bad["signal_id"] = 2;
	CHECK(GamesEnetWebrtcProtocol::payload_within_limits(GamesEnetWebrtcProtocol::MSG_KICK, kick_bad));
	Array listed;
	Dictionary row;
	row["room_code"] = "ABC234";
	row["name"] = "  ok  ";
	row["max"] = 99;
	row["members"] = 2;
	row["password"] = "secret";
	Array row_tags;
	row_tags.push_back("modtest");
	row_tags.push_back("bad tag");
	row["tags"] = row_tags;
	listed.push_back(row);
	Dictionary junk;
	junk["room_code"] = "!!!!!!";
	listed.push_back(junk);
	Array listed_clean = GamesEnetWebrtcProtocol::sanitize_room_list(listed);
	CHECK(listed_clean.size() == 1);
	Dictionary listed_row = listed_clean[0];
	CHECK(String(listed_row["room_code"]) == "ABC234");
	CHECK_FALSE(listed_row.has("password"));
	CHECK(listed_row.has("tags"));
	Array kept_tags = listed_row["tags"];
	CHECK(kept_tags.size() == 1);
	CHECK(String(kept_tags[0]) == "modtest");
}

TEST_CASE("[Modules][GamesEnetWebrtc] valid HELLO-shaped dict") {
	Dictionary hello;
	hello["signal_id"] = 1;
	hello["fake_ip"] = "10.66.0.1";
	hello["fake_port"] = 1;
	hello["room_code"] = "ABC234";
	CHECK(GamesEnetWebrtcProtocol::is_valid_signal_id(int(hello["signal_id"])));
	CHECK(GamesEnetWebrtcProtocol::is_valid_fake_ip(String(hello["fake_ip"])));
	CHECK(GamesEnetWebrtcProtocol::is_valid_room_code(String(hello["room_code"])));
}

TEST_CASE("[Modules][GamesEnetWebrtc] fake address map") {
	FakeAddressMap map;
	FakePeerEntry host;
	host.signal_id = 1;
	host.fake_ip = FakeAddressMap::make_fake_ip(1);
	host.fake_port = 1;
	host.role = "host";
	map.add_peer(host);

	FakePeerEntry joiner;
	joiner.signal_id = 2;
	joiner.fake_ip = FakeAddressMap::make_fake_ip(2);
	joiner.fake_port = 1;
	map.add_peer(joiner);

	CHECK(FakeAddressMap::is_fake_ip(host.fake_ip));
	CHECK_FALSE(FakeAddressMap::is_fake_ip(IPAddress(10, 66, 1, 1)));
	CHECK_FALSE(FakeAddressMap::is_fake_ip(IPAddress(10, 66, 0, 0)));
	CHECK_FALSE(FakeAddressMap::is_fake_ip(IPAddress(10, 66, 0, 255)));
	FakePeerEntry found;
	CHECK(map.lookup_signal(1, found));
	CHECK(found.signal_id == 1);
	CHECK(map.lookup_addr(joiner.fake_ip, 1, found));
	CHECK(found.signal_id == 2);
	CHECK_FALSE(map.lookup_addr(joiner.fake_ip, 9, found));
	map.remove_signal(2);
	CHECK_FALSE(map.lookup_signal(2, found));
	Vector<uint8_t> empty_in;
	const int inbound_before = map.inbound_size();
	map.push_inbound(empty_in, host.fake_ip, 1);
	CHECK(map.inbound_size() == inbound_before);
	map.set_relay(1, true);
	CHECK(map.lookup_signal(1, found));
	CHECK(found.relay);
	map.set_link(1, nullptr);
	CHECK(map.lookup_signal(1, found));
	CHECK(found.link == nullptr);
	Vector<uint8_t> inbound;
	inbound.resize(2);
	inbound.ptrw()[0] = 9;
	inbound.ptrw()[1] = 8;
	map.push_inbound(inbound, joiner.fake_ip, 1);
	InboundDatagram popped;
	CHECK(map.pop_inbound(popped));
	CHECK(popped.data.size() == 2);
	CHECK(popped.data[0] == 9);
	map.clear();
	CHECK_FALSE(map.lookup_signal(1, found));
	CHECK_FALSE(map.pop_inbound(popped));
}

TEST_CASE("[Modules][GamesEnetWebrtc] remaining protocol helpers") {
	CHECK(GamesEnetWebrtcProtocol::is_valid_room_code("ABC234"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_room_code("ILOUxx"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_room_code("ABC23"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_room_code("ABC2345"));
	String ctrl = "  a";
	ctrl += String::chr(1);
	ctrl += "b  ";
	CHECK(GamesEnetWebrtcProtocol::strip_controls(ctrl) == "ab");
	CHECK(GamesEnetWebrtcProtocol::has_crlf("x\ny"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::has_crlf("ok"));
	CHECK(GamesEnetWebrtcProtocol::is_valid_role("host"));
	CHECK(GamesEnetWebrtcProtocol::is_valid_role("joiner"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_role("evil"));
	CHECK(GamesEnetWebrtcProtocol::is_valid_password(""));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_password(String("p").repeat(GamesEnetWebrtcProtocol::MAX_PASSWORD + 1)));
	CHECK(GamesEnetWebrtcProtocol::is_valid_auth_token(""));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_game_id(String("a").repeat(GamesEnetWebrtcProtocol::MAX_GAME_ID + 1)));
	CHECK(GamesEnetWebrtcProtocol::is_valid_ice_url("stun:[::1]:3478"));
	CHECK(GamesEnetWebrtcProtocol::is_valid_signal_url("ws://[::1]/v1"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_ice_url("stun:::3478"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_ice_url("stun:[fe80::1]:3478"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_ice_url("stun:[ff02::1]:3478"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_ice_url("stun:224.0.0.1:3478"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_ice_url("stun:100.100.100.200:3478"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_signal_url("ws://224.0.0.1/v1"));
	Dictionary cred_ok;
	Array cred_urls;
	cred_urls.push_back("turn:example.com:3478?transport=udp");
	cred_ok["urls"] = cred_urls;
	cred_ok["username"] = "1700000000:ABC234:2";
	cred_ok["credential"] = "secret";
	Array cred_in;
	cred_in.push_back(cred_ok);
	Array cred_out = GamesEnetWebrtcProtocol::sanitize_ice_servers(cred_in);
	CHECK(cred_out.size() == 1);
	Dictionary cred_row = cred_out[0];
	CHECK(String(cred_row["username"]) == "1700000000:ABC234:2");
	CHECK(String(cred_row["credential"]) == "secret");
	CHECK_FALSE(GamesEnetWebrtcProtocol::parse_envelope("{\"v\":1,\"type\":18,\"id\":0,\"data\":[]}").valid);
	CHECK_FALSE(GamesEnetWebrtcProtocol::parse_envelope("{\"v\":1,\"type\":18,\"id\":0}").valid);
	Dictionary answer;
	answer["sdp"] = "v=0";
	CHECK(GamesEnetWebrtcProtocol::payload_within_limits(GamesEnetWebrtcProtocol::MSG_ANSWER, answer));
	Dictionary cand;
	cand["candidate"] = "candidate:1 1 UDP 1 1.2.3.4 9 typ host";
	CHECK(GamesEnetWebrtcProtocol::payload_within_limits(GamesEnetWebrtcProtocol::MSG_CANDIDATE, cand));
	Dictionary relay_b64;
	relay_b64["to"] = 2;
	relay_b64["payload"] = "!!!not-b64!!!";
	CHECK_FALSE(GamesEnetWebrtcProtocol::payload_within_limits(GamesEnetWebrtcProtocol::MSG_RELAY_DATAGRAM, relay_b64));
	Array many_tags;
	Dictionary tag_row;
	tag_row["room_code"] = "ABC234";
	tag_row["members"] = 999;
	Array tags;
	for (int i = 0; i < 12; i++) {
		tags.push_back("t" + String::num_int64(i));
	}
	tag_row["tags"] = tags;
	many_tags.push_back(tag_row);
	Array cleaned_tags = GamesEnetWebrtcProtocol::sanitize_room_list(many_tags);
	CHECK(cleaned_tags.size() == 1);
	Dictionary cleaned_row = cleaned_tags[0];
	CHECK(int(cleaned_row["members"]) == GamesEnetWebrtcProtocol::HARD_SIGNAL_CAP);
	Array kept_tags = cleaned_row["tags"];
	CHECK(kept_tags.size() == GamesEnetWebrtcProtocol::MAX_TAGS);
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_turn_username("nocolon"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_turn_username("x:ABC234:2"));
	CHECK(GamesEnetWebrtcProtocol::is_star_pair(1, 254));
	CHECK(GamesEnetWebrtcProtocol::is_star_pair(254, 1));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_star_pair(254, 253));
	CHECK(GamesEnetWebrtcProtocol::clamp_max_clients(1) == 1);
	CHECK(GamesEnetWebrtcProtocol::clamp_max_clients(8) == 8);
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_auth_token(String("t").repeat(GamesEnetWebrtcProtocol::MAX_AUTH_TOKEN + 1)));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_auth_token("tok\x01"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_password("pw\r\n"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_sdp(""));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_candidate("cand\nidate"));
	CHECK_FALSE(GamesEnetWebrtcProtocol::is_valid_ice_url("stun:[::]:3478"));
	CHECK(GamesEnetWebrtcProtocol::is_valid_signal_url("wss://signal.example/v1"));
	CHECK_FALSE(FakeAddressMap::is_fake_ip(FakeAddressMap::make_fake_ip(0)));
}

void test_enet_webrtc_socket_queue();

TEST_CASE("[Modules][GamesEnetWebrtc] socket recvfrom empty and queued") {
	test_enet_webrtc_socket_queue();
}

} //namespace TestGamesEnetWebrtc
