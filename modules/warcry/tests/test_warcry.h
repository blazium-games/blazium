/**************************************************************************/
/*  test_warcry.h                                                         */
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

#include "tests/test_macros.h"

#include "core/config/engine.h"
#include "core/math/math_funcs.h"
#include "core/os/os.h"
#include "modules/warcry/src/warcry_opus.h"
#include "modules/warcry/src/warcry_protocol.h"
#include "modules/warcry/warcry_client.h"

#include <cmath>

namespace TestWarcry {

TEST_CASE("[Warcry] singleton available") {
	Engine *engine = Engine::get_singleton();
	REQUIRE_MESSAGE(engine != nullptr, "Engine singleton must exist when running tests.");
	CHECK(engine->has_singleton("Warcry"));
	Object *client = engine->get_singleton_object("Warcry");
	REQUIRE_MESSAGE(client != nullptr, "Engine singleton 'Warcry' should exist.");
	CHECK(client->is_class("WarcryClient"));
}

TEST_CASE("[Warcry] control HELLO roundtrip") {
	Dictionary data;
	data["username"] = "alice";
	const Vector<uint8_t> bytes = WarcryProtocol::serialize_control(WarcryProtocol::MsgType::HELLO, data);
	CHECK(bytes.size() > (int)sizeof(WarcryProtocol::MessageHeader));

	WarcryProtocol::MsgType type = WarcryProtocol::MsgType::INVALID;
	Dictionary parsed;
	CHECK(WarcryProtocol::deserialize_control(bytes.ptr(), bytes.size(), type, parsed));
	CHECK(type == WarcryProtocol::MsgType::HELLO);
	CHECK(String(parsed.get("username", String())) == "alice");
}

TEST_CASE("[Warcry] voice frame header plus opus payload") {
	WarcryProtocol::VoiceFrameHeader header;
	header.channel_id = 3;
	header.sequence = 7;
	header.timestamp = 1234;
	const uint8_t payload[4] = { 0x01, 0x02, 0x03, 0x04 };
	const Vector<uint8_t> bytes = WarcryProtocol::serialize_voice(header, payload, 4);
	CHECK(bytes.size() == 16);

	WarcryProtocol::VoiceFrameHeader out_header;
	Vector<uint8_t> out_opus;
	CHECK(WarcryProtocol::deserialize_voice(bytes.ptr(), bytes.size(), out_header, out_opus));
	CHECK(out_header.channel_id == 3);
	CHECK(out_header.sequence == 7);
	CHECK(out_header.timestamp == 1234);
	CHECK(out_opus.size() == 4);
	CHECK(out_opus[0] == 0x01);
}

TEST_CASE("[Warcry] opus sine encode/decode") {
	WarcryOpusCodec codec;
	REQUIRE(codec.init());

	Vector<int16_t> sine;
	sine.resize(WarcryOpusCodec::FRAME_SAMPLES);
	for (int i = 0; i < sine.size(); i++) {
		sine.ptrw()[i] = (int16_t)(sinf((float)i * 0.1f) * 16000.0f);
	}

	Vector<uint8_t> opus;
	REQUIRE(codec.encode_frame(sine.ptr(), sine.size(), opus));
	CHECK(opus.size() > 0);

	Vector<int16_t> decoded;
	REQUIRE(codec.decode_frame(opus.ptr(), opus.size(), decoded));
	CHECK(decoded.size() == WarcryOpusCodec::FRAME_SAMPLES);
}

TEST_CASE("[Warcry] optional live connection") {
	OS *os = OS::get_singleton();
	REQUIRE(os);
	const String host = os->get_environment("WARCRY_TEST_HOST");
	if (host.is_empty()) {
		INFO("Warcry live connection test skipped: set WARCRY_TEST_HOST to enable.");
		return;
	}

	int port = 27015;
	const String port_env = os->get_environment("WARCRY_TEST_PORT");
	if (!port_env.is_empty()) {
		port = port_env.to_int();
	}
	String username = os->get_environment("WARCRY_TEST_USER");
	if (username.is_empty()) {
		username = "warcry_test";
	}

	WarcryClient *client = WarcryClient::get_singleton();
	REQUIRE(client);
	CHECK(client->connect_to_server(host, port, username));

	const uint64_t start = os->get_ticks_msec();
	while (!client->is_connected() && os->get_ticks_msec() - start < 2000) {
		client->poll();
		os->delay_usec(10000);
	}
	CHECK_MESSAGE(client->is_connected(), "WarcryClient failed to connect to politeia_server.");
	client->disconnect_from_server();
}

} // namespace TestWarcry
