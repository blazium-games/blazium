/**************************************************************************/
/*  test_games_enet_webrtc.cpp                                            */
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

#include "test_games_enet_webrtc.h"

#include "../enet_webrtc_socket.h"
#include "../enet_webrtc_socket_factory.h"
#include "../relay_client.h"
#include "../signal_client.h"
#include "../webrtc_enet_session.h"
#include "../webrtc_link.h"

#include "core/crypto/crypto_core.h"

namespace TestGamesEnetWebrtc {

void test_enet_webrtc_socket_queue() {
	FakeAddressMap map;
	ENetWebRTCSocket sock(&map, nullptr);
	CHECK(sock.bind(FakeAddressMap::make_fake_ip(1), 1) == OK);
	IPAddress ip;
	uint16_t port = 0;
	int read = 0;
	uint8_t buf[16];
	CHECK(sock.recvfrom(buf, 16, read, ip, port) == ERR_BUSY);
	CHECK(read == 0);

	Vector<uint8_t> payload;
	payload.resize(4);
	payload.ptrw()[0] = 1;
	payload.ptrw()[1] = 2;
	payload.ptrw()[2] = 3;
	payload.ptrw()[3] = 4;
	map.push_inbound(payload, FakeAddressMap::make_fake_ip(2), 1);
	CHECK(sock.recvfrom(buf, 16, read, ip, port) == OK);
	CHECK(read == 4);
	CHECK(buf[0] == 1);
	CHECK(buf[3] == 4);
	CHECK(port == 1);

	int sent = 0;
	CHECK(sock.sendto(buf, 4, sent, FakeAddressMap::make_fake_ip(2), 1) == ERR_UNCONFIGURED);

	WebRTCEnetSession session;
	ENetWebRTCSocket mapped(&map, &session);
	CHECK(mapped.bind(FakeAddressMap::make_fake_ip(1), 1) == OK);
	int mapped_sent = 0;
	CHECK(mapped.sendto(buf, 4, mapped_sent, FakeAddressMap::make_fake_ip(9), 1) == ERR_BUSY);
	CHECK(session.send_datagram(FakeAddressMap::make_fake_ip(9), 1, buf, 4) == ERR_BUSY);

	FakeAddressMap q;
	Vector<uint8_t> one;
	one.resize(1);
	one.ptrw()[0] = 1;
	for (int i = 0; i < GamesEnetWebrtcProtocol::MAX_INBOUND_DATAGRAMS + 8; i++) {
		one.ptrw()[0] = (uint8_t)(i & 0xff);
		q.push_inbound(one, FakeAddressMap::make_fake_ip(2), 1);
	}
	CHECK(q.inbound_size() == GamesEnetWebrtcProtocol::MAX_INBOUND_DATAGRAMS);

	Vector<uint8_t> huge;
	huge.resize(GamesEnetWebrtcProtocol::MAX_RELAY_BYTES + 8);
	const int before = q.inbound_size();
	q.push_inbound(huge, FakeAddressMap::make_fake_ip(2), 1);
	CHECK(q.inbound_size() == before);

	int neg_sent = 0;
	CHECK(mapped.sendto(nullptr, 4, neg_sent, FakeAddressMap::make_fake_ip(9), 1) == ERR_INVALID_PARAMETER);
	CHECK(mapped.sendto(buf, 0, neg_sent, FakeAddressMap::make_fake_ip(9), 1) == ERR_INVALID_PARAMETER);
	CHECK(mapped.sendto(buf, -1, neg_sent, FakeAddressMap::make_fake_ip(9), 1) == ERR_INVALID_PARAMETER);
	int bad_read = 0;
	IPAddress bad_ip;
	uint16_t bad_port = 0;
	CHECK(mapped.recvfrom(nullptr, 16, bad_read, bad_ip, bad_port) == ERR_INVALID_PARAMETER);

	Ref<SignalClient> sc;
	sc.instantiate();
	CHECK(sc->configure("http://evil.example/v1", "ok-game", "") == ERR_INVALID_PARAMETER);
	CHECK(sc->configure("ws://127.0.0.1:8080/v1/signal", "bad id", "") == ERR_INVALID_PARAMETER);
	CHECK(sc->configure("ws://127.0.0.1:8080/v1/signal", "ok-game", "tok\nCRLF") == ERR_INVALID_PARAMETER);
	CHECK(sc->configure("ws://127.0.0.1:8080/v1/signal", "ok-game", "") == OK);
	CHECK(sc->get_url() == "ws://127.0.0.1:8080/v1/signal");
	CHECK(sc->get_game_id() == "ok-game");
	CHECK_FALSE(sc->is_open());
	Ref<SignalClient> sc_unconf;
	sc_unconf.instantiate();
	CHECK(sc_unconf->connect_to_signal() == ERR_UNCONFIGURED);

	Dictionary relay_ok;
	relay_ok["from"] = 2;
	uint8_t raw[3] = { 9, 8, 7 };
	relay_ok["payload"] = CryptoCore::b64_encode_str(raw, 3);
	int from = 0;
	Vector<uint8_t> decoded;
	CHECK(RelayClient::decode_datagram(relay_ok, from, decoded));
	CHECK(from == 2);
	CHECK(decoded.size() == 3);
	CHECK(decoded[0] == 9);
	Dictionary relay_empty;
	relay_empty["from"] = 2;
	relay_empty["payload"] = "";
	CHECK_FALSE(RelayClient::decode_datagram(relay_empty, from, decoded));
	Dictionary relay_from0;
	relay_from0["from"] = 0;
	relay_from0["payload"] = relay_ok["payload"];
	CHECK_FALSE(RelayClient::decode_datagram(relay_from0, from, decoded));
	Dictionary relay_from_id;
	relay_from_id["id"] = 3;
	relay_from_id["payload"] = relay_ok["payload"];
	CHECK(RelayClient::decode_datagram(relay_from_id, from, decoded));
	CHECK(from == 3);

	WebRTCEnetSession sess;
	CHECK(sess.configure("http://evil.example/v1", "ok-game", "") == ERR_INVALID_PARAMETER);
	CHECK(sess.configure("ws://127.0.0.1:8080/v1/signal", "ok-game", "") == OK);
	CHECK_FALSE(sess.is_in_room());
	CHECK(sess.get_peer().is_null());
	sess.set_ice_timeout_msec(99999);
	CHECK(sess.get_ice_timeout_msec() == 60000);
	sess.set_ice_timeout_msec(-5);
	CHECK(sess.get_ice_timeout_msec() == 0);
	sess.set_ice_timeout_msec(0);
	CHECK(sess.get_ice_timeout_msec() == 0);
	CHECK(sess.get_room_code().is_empty());
	sess.set_force_relay(true);
	CHECK(sess.get_force_relay());
	CHECK(sess.leave() == ERR_UNCONFIGURED);
	CHECK(sess.kick(2) == ERR_UNCONFIGURED);
	CHECK(sess.kick(0) == ERR_UNCONFIGURED);
	CHECK(sess.seal() == ERR_UNCONFIGURED);
	CHECK(sess.get_local_fake_port() == 1);

	IPAddress bound;
	uint16_t bound_port = 0;
	ENetWebRTCSocket unbound(&map, &session);
	IPAddress unbound_ip;
	uint16_t unbound_port = 0;
	CHECK(unbound.get_socket_address(&unbound_ip, &unbound_port) == ERR_UNCONFIGURED);
	int oversize_sent = 0;
	Vector<uint8_t> oversize;
	oversize.resize(GamesEnetWebrtcProtocol::MAX_RELAY_BYTES + 1);
	CHECK(mapped.sendto(oversize.ptr(), oversize.size(), oversize_sent, FakeAddressMap::make_fake_ip(2), 1) == ERR_INVALID_PARAMETER);
	int zero_read = 0;
	CHECK(mapped.recvfrom(buf, 0, zero_read, ip, port) == ERR_INVALID_PARAMETER);
	CHECK(mapped.get_socket_address(&bound, &bound_port) == OK);
	CHECK(bound_port == 1);
	CHECK(mapped.set_option(0, 1) == 0);
	CHECK_FALSE(mapped.can_upgrade());
	Vector<uint8_t> eight;
	eight.resize(8);
	for (int i = 0; i < 8; i++) {
		eight.ptrw()[i] = (uint8_t)(i + 1);
	}
	map.push_inbound(eight, FakeAddressMap::make_fake_ip(2), 1);
	uint8_t trunc_buf[4];
	int trunc_read = 0;
	IPAddress trunc_ip;
	uint16_t trunc_port = 0;
	CHECK(mapped.recvfrom(trunc_buf, 4, trunc_read, trunc_ip, trunc_port) == ERR_OUT_OF_MEMORY);
	CHECK(trunc_read == 4);
	CHECK(trunc_buf[0] == 1);
	mapped.close();
	int after_close = 0;
	CHECK(mapped.recvfrom(buf, 16, after_close, ip, port) == ERR_BUSY);

	Ref<WebRTCLink> link;
	link.instantiate();
	link->set_mode(WebRTCLink::MODE_RELAY);
	CHECK(link->get_mode() == WebRTCLink::MODE_RELAY);
	CHECK(link->is_open());
	link->poll();
	int dummy_sent = 0;
	(void)dummy_sent;
	CHECK(link->send(nullptr, 4) == ERR_INVALID_PARAMETER);
	uint8_t tiny[2] = { 1, 2 };
	CHECK(link->send(tiny, 2) == ERR_UNAVAILABLE);
	Vector<uint8_t> too_big;
	too_big.resize(GamesEnetWebrtcProtocol::MAX_RELAY_BYTES + 1);
	CHECK(link->send(too_big.ptr(), too_big.size()) == ERR_INVALID_PARAMETER);
	link->close();

	RelayClient rc;
	CHECK(rc.send_datagram(2, tiny, 2) == ERR_UNCONFIGURED);
	Ref<SignalClient> sc2;
	sc2.instantiate();
	sc2->configure("ws://127.0.0.1:8080/v1/signal", "ok-game", "");
	rc.set_signal_client(sc2.ptr());
	CHECK(rc.send_datagram(0, tiny, 2) == ERR_INVALID_PARAMETER);
	CHECK(rc.send_datagram(255, tiny, 2) == ERR_INVALID_PARAMETER);
	CHECK(rc.send_datagram(2, tiny, 0) == ERR_INVALID_PARAMETER);
	Dictionary empty_msg;
	CHECK(sc2->send_message(GamesEnetWebrtcProtocol::MSG_PING, empty_msg) == ERR_UNAVAILABLE);

	SIGNAL_WATCH(&sess, "failed");
	sess.create_room("");
	Array fail_args;
	Array fail_one;
	fail_one.push_back("invalid room fields");
	fail_args.push_back(fail_one);
	SIGNAL_CHECK("failed", fail_args);
	sess.join_room("!!!!!!");
	Array join_fail;
	Array join_one;
	join_one.push_back("invalid join fields");
	join_fail.push_back(join_one);
	SIGNAL_CHECK("failed", join_fail);
	sess.create_room("pending-one");
	sess.create_room("pending-two");
	CHECK_FALSE(sess.is_in_room());
	SIGNAL_UNWATCH(&sess, "failed");

	{
		ENetWebRTCSocketFactory::Guard guard(&map, &sess);
		ENetSocketCreateFn fn = enet_get_socket_create_fn();
		CHECK(fn != nullptr);
		ENetGodotSocket *created = fn();
		CHECK(created != nullptr);
		memdelete(created);
	}
	CHECK(enet_get_socket_create_fn() == nullptr);
}

} //namespace TestGamesEnetWebrtc
