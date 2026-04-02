/**************************************************************************/
/*  test_rcon_client.h                                                    */
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

#include "modules/rcon/rcon_client.h"
#include "modules/rcon/rcon_packet.h"

namespace TestRCONClient {

TEST_CASE("[RCONClient] Client initialization and state") {
	Ref<RCONClient> client;
	client.instantiate();

	CHECK(client.is_valid());
	CHECK(client->is_authenticated() == false);
	CHECK(client->get_state() == RCONClient::STATE_DISCONNECTED);

	// Note: Actually connecting requires a running server
	// and is better suited for integration tests
}

TEST_CASE("[RCONClient] Protocol enum values") {
	CHECK(RCONClient::PROTOCOL_SOURCE == 0);
	CHECK(RCONClient::PROTOCOL_BATTLEYE == 1);
}

TEST_CASE("[RCONClient] State enum values") {
	CHECK(RCONClient::STATE_DISCONNECTED == 0);
	CHECK(RCONClient::STATE_CONNECTING == 1);
	CHECK(RCONClient::STATE_AUTHENTICATING == 2);
	CHECK(RCONClient::STATE_CONNECTED == 3);
	CHECK(RCONClient::STATE_ERROR == 4);
}

TEST_CASE("[RCONClient] Packet parsing with invalid data") {
	// Test Source packet with invalid size
	PackedByteArray invalid_source;
	invalid_source.resize(3); // Too small
	Dictionary parsed_source = RCONPacket::parse_source_packet(invalid_source);
	CHECK((bool)parsed_source.get("valid", true) == false);

	// Test BattlEye packet with invalid magic
	PackedByteArray invalid_battleye;
	invalid_battleye.push_back('X');
	invalid_battleye.push_back('Y');
	invalid_battleye.resize(10);
	Dictionary parsed_battleye = RCONPacket::parse_battleye_packet(invalid_battleye);
	CHECK((bool)parsed_battleye.get("valid", true) == false);
}

TEST_CASE("[RCONClient] Empty command handling") {
	// Test empty Source command
	PackedByteArray empty_cmd = RCONPacket::create_source_command(1, "");
	CHECK(empty_cmd.size() > 0);

	Dictionary parsed = RCONPacket::parse_source_packet(empty_cmd);
	CHECK((bool)parsed.get("valid", false) == true);
	CHECK((String)parsed.get("body", "x") == "");

	// Test empty BattlEye command (keep-alive)
	PackedByteArray keepalive = RCONPacket::create_battleye_command(0, "");
	CHECK(keepalive.size() > 0);
	CHECK(RCONPacket::verify_battleye_crc32(keepalive) == true);

	Dictionary parsed_ka = RCONPacket::parse_battleye_packet(keepalive);
	CHECK((bool)parsed_ka.get("valid", false) == true);
	CHECK((String)parsed_ka.get("data", "x") == "");
}

TEST_CASE("[RCONClient] Special characters in commands") {
	// Test with special characters
	String special_cmd = "say \"Hello, World!\" & test\n";
	PackedByteArray packet = RCONPacket::create_source_command(1, special_cmd);
	Dictionary parsed = RCONPacket::parse_source_packet(packet);

	CHECK((bool)parsed.get("valid", false) == true);
	CHECK((String)parsed.get("body", "") == special_cmd);
}

TEST_CASE("[RCONClient] Unicode password handling") {
	// Test with unicode characters (should work as UTF-8)
	String unicode_pass = "пароль123";
	PackedByteArray auth = RCONPacket::create_source_auth(0, unicode_pass);
	Dictionary parsed = RCONPacket::parse_source_packet(auth);

	CHECK((bool)parsed.get("valid", false) == true);
	CHECK((String)parsed.get("body", "") == unicode_pass);
}

TEST_CASE("[RCONClient] Large command payload") {
	// Test with a large command string
	String large_cmd;
	for (int i = 0; i < 1000; i++) {
		large_cmd += "test ";
	}

	PackedByteArray packet = RCONPacket::create_source_command(1, large_cmd);
	CHECK(packet.size() > 0);

	Dictionary parsed = RCONPacket::parse_source_packet(packet);
	CHECK((bool)parsed.get("valid", false) == true);
	CHECK(((String)parsed.get("body", "")).length() == large_cmd.length());
}

TEST_CASE("[RCONClient] BattlEye sequence number wrapping") {
	// Test sequence numbers at boundary
	PackedByteArray packet_0 = RCONPacket::create_battleye_command(0, "test");
	PackedByteArray packet_255 = RCONPacket::create_battleye_command(255, "test");

	Dictionary parsed_0 = RCONPacket::parse_battleye_packet(packet_0);
	Dictionary parsed_255 = RCONPacket::parse_battleye_packet(packet_255);

	CHECK((int)parsed_0.get("seq", -1) == 0);
	CHECK((int)parsed_255.get("seq", -1) == 255);
}

TEST_CASE("[RCONClient] Source request ID handling") {
	// Test with various request IDs
	PackedByteArray packet_neg = RCONPacket::create_source_command(-1, "test");
	PackedByteArray packet_zero = RCONPacket::create_source_command(0, "test");
	PackedByteArray packet_large = RCONPacket::create_source_command(999999, "test");

	Dictionary parsed_neg = RCONPacket::parse_source_packet(packet_neg);
	Dictionary parsed_zero = RCONPacket::parse_source_packet(packet_zero);
	Dictionary parsed_large = RCONPacket::parse_source_packet(packet_large);

	CHECK((int)parsed_neg.get("id", 0) == -1);
	CHECK((int)parsed_zero.get("id", -1) == 0);
	CHECK((int)parsed_large.get("id", 0) == 999999);
}

TEST_CASE("[RCONClient] BattlEye login response parsing") {
	// Test that we can parse a login packet correctly
	// Note: Actual login response construction requires server-side implementation
	// For now, verify that login packet creation and basic parsing works
	PackedByteArray login_request = RCONPacket::create_battleye_login("testpass");
	CHECK(login_request.size() > 0);
	CHECK(RCONPacket::verify_battleye_crc32(login_request) == true);

	Dictionary parsed = RCONPacket::parse_battleye_packet(login_request);
	CHECK((bool)parsed.get("valid", false) == true);
	CHECK((int)parsed.get("type", -1) == (int)RCONPacket::BATTLEYE_LOGIN);
}

TEST_CASE("[RCONClient] Packet round-trip integrity") {
	// Test that packets can be created and parsed back correctly
	struct TestCase {
		String command;
		int id;
	};

	TestCase cases[] = {
		{ "status", 1 },
		{ "players", 2 },
		{ "say Hello World", 3 },
		{ "", 4 }, // Empty command
		{ "very long command " + String("x").repeat(500), 5 },
	};

	for (const TestCase &test : cases) {
		PackedByteArray packet = RCONPacket::create_source_command(test.id, test.command);
		Dictionary parsed = RCONPacket::parse_source_packet(packet);

		CHECK((bool)parsed.get("valid", false) == true);
		CHECK((int)parsed.get("id", -1) == test.id);
		CHECK((String)parsed.get("body", "") == test.command);
	}
}

} // namespace TestRCONClient
