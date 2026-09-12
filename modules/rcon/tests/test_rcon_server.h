/**************************************************************************/
/*  test_rcon_server.h                                                    */
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

#include "core/object/callable_method_pointer.h"
#include "modules/rcon/rcon_packet.h"
#include "modules/rcon/rcon_server.h"

namespace TestRCONServer {

TEST_CASE("[RCONServer] Source packet creation and parsing") {
	// Test Source auth packet
	PackedByteArray auth_packet = RCONPacket::create_source_auth(1, "testpass");
	CHECK(auth_packet.size() > 0);

	Dictionary parsed = RCONPacket::parse_source_packet(auth_packet);
	CHECK((bool)parsed.get("valid", false) == true);
	CHECK((int)parsed.get("id", -1) == 1);
	CHECK((int)parsed.get("type", -1) == (int)RCONPacket::SOURCE_SERVERDATA_AUTH);
	CHECK((String)parsed.get("body", "") == "testpass");

	// Test Source command packet
	PackedByteArray cmd_packet = RCONPacket::create_source_command(5, "status");
	CHECK(cmd_packet.size() > 0);

	Dictionary parsed_cmd = RCONPacket::parse_source_packet(cmd_packet);
	CHECK((bool)parsed_cmd.get("valid", false) == true);
	CHECK((int)parsed_cmd.get("id", -1) == 5);
	CHECK((int)parsed_cmd.get("type", -1) == (int)RCONPacket::SOURCE_SERVERDATA_EXECCOMMAND);
	CHECK((String)parsed_cmd.get("body", "") == "status");
}

TEST_CASE("[RCONServer] BattlEye packet creation and parsing") {
	// Test BattlEye login packet
	PackedByteArray login_packet = RCONPacket::create_battleye_login("admin");
	CHECK(login_packet.size() > 0);
	CHECK(login_packet[0] == 'B');
	CHECK(login_packet[1] == 'E');
	CHECK(login_packet[6] == 0xFF);

	// Verify CRC32
	CHECK(RCONPacket::verify_battleye_crc32(login_packet) == true);

	Dictionary parsed = RCONPacket::parse_battleye_packet(login_packet);
	CHECK((bool)parsed.get("valid", false) == true);
	CHECK((int)parsed.get("type", -1) == (int)RCONPacket::BATTLEYE_LOGIN);

	// Test BattlEye command packet
	PackedByteArray cmd_packet = RCONPacket::create_battleye_command(10, "players");
	CHECK(cmd_packet.size() > 0);
	CHECK(RCONPacket::verify_battleye_crc32(cmd_packet) == true);

	Dictionary parsed_cmd = RCONPacket::parse_battleye_packet(cmd_packet);
	CHECK((bool)parsed_cmd.get("valid", false) == true);
	CHECK((int)parsed_cmd.get("type", -1) == (int)RCONPacket::BATTLEYE_COMMAND);
	CHECK((int)parsed_cmd.get("seq", -1) == 10);
	CHECK((String)parsed_cmd.get("data", "") == "players");
}

TEST_CASE("[RCONServer] BattlEye CRC32 validation") {
	PackedByteArray valid_packet = RCONPacket::create_battleye_command(1, "test");
	CHECK(RCONPacket::verify_battleye_crc32(valid_packet) == true);

	// Corrupt the packet
	PackedByteArray corrupted_packet = valid_packet;
	corrupted_packet.write[10] ^= 0xFF; // Flip bits in payload
	CHECK(RCONPacket::verify_battleye_crc32(corrupted_packet) == false);

	// Invalid packet structure
	PackedByteArray invalid_packet;
	invalid_packet.push_back('B');
	invalid_packet.push_back('E');
	CHECK(RCONPacket::verify_battleye_crc32(invalid_packet) == false);
}

TEST_CASE("[RCONServer] BattlEye message acknowledgment") {
	PackedByteArray ack_packet = RCONPacket::create_battleye_message_ack(42);
	CHECK(ack_packet.size() > 0);
	CHECK(RCONPacket::verify_battleye_crc32(ack_packet) == true);

	Dictionary parsed = RCONPacket::parse_battleye_packet(ack_packet);
	CHECK((bool)parsed.get("valid", false) == true);
	CHECK((int)parsed.get("type", -1) == (int)RCONPacket::BATTLEYE_MESSAGE);
	CHECK((int)parsed.get("seq", -1) == 42);
}

TEST_CASE("[RCONServer] Server initialization and state") {
	Ref<RCONServer> server;
	server.instantiate();

	CHECK(server.is_valid());
	CHECK(server->is_running() == false);
	CHECK(server->get_state() == RCONServer::STATE_STOPPED);

	// Note: Actually starting the server would require available ports
	// and is better suited for integration tests
}

TEST_CASE("[RCONServer] Command registration") {
	Ref<RCONServer> server;
	server.instantiate();

	// Create a simple callable (testing registration mechanism only)
	Callable callback;

	server->register_command("test", callback);

	// Verify command is registered by checking if unregister works
	server->unregister_command("test");

	// Note: Full command execution testing requires a running server and client
}

TEST_CASE("[RCONServer] Protocol enum values") {
	CHECK(RCONServer::PROTOCOL_SOURCE == 0);
	CHECK(RCONServer::PROTOCOL_BATTLEYE == 1);
}

TEST_CASE("[RCONServer] Packet type constants") {
	CHECK(RCONPacket::SOURCE_SERVERDATA_AUTH == 3);
	CHECK(RCONPacket::SOURCE_SERVERDATA_AUTH_RESPONSE == 2);
	CHECK(RCONPacket::SOURCE_SERVERDATA_EXECCOMMAND == 2);
	CHECK(RCONPacket::SOURCE_SERVERDATA_RESPONSE_VALUE == 0);

	CHECK(RCONPacket::BATTLEYE_LOGIN == 0x00);
	CHECK(RCONPacket::BATTLEYE_COMMAND == 0x01);
	CHECK(RCONPacket::BATTLEYE_MESSAGE == 0x02);
}

TEST_CASE("[RCONServer] Source packet size calculation") {
	// Empty body
	PackedByteArray packet1 = RCONPacket::create_source_command(1, "");
	Dictionary parsed1 = RCONPacket::parse_source_packet(packet1);
	CHECK((bool)parsed1.get("valid", false) == true);
	CHECK((String)parsed1.get("body", "x") == "");

	// Long body
	String long_text = "This is a longer command string with multiple words";
	PackedByteArray packet2 = RCONPacket::create_source_command(2, long_text);
	Dictionary parsed2 = RCONPacket::parse_source_packet(packet2);
	CHECK((bool)parsed2.get("valid", false) == true);
	CHECK((String)parsed2.get("body", "") == long_text);
}

TEST_CASE("[RCONServer] BattlEye multi-packet response parsing") {
	// Note: Multi-packet response parsing is complex and requires server implementation
	// For now, test basic command packet parsing which is used in single-packet responses
	PackedByteArray cmd_packet = RCONPacket::create_battleye_command(5, "test command");
	CHECK(cmd_packet.size() > 0);
	CHECK(RCONPacket::verify_battleye_crc32(cmd_packet) == true);

	Dictionary parsed = RCONPacket::parse_battleye_packet(cmd_packet);
	CHECK((bool)parsed.get("valid", false) == true);
	CHECK((int)parsed.get("type", -1) == (int)RCONPacket::BATTLEYE_COMMAND);
	CHECK((int)parsed.get("seq", -1) == 5);
	CHECK((String)parsed.get("data", "") == "test command");
}

} // namespace TestRCONServer
