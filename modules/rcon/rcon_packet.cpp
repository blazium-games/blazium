/**************************************************************************/
/*  rcon_packet.cpp                                                       */
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

#include "rcon_packet.h"

// CRC32 lookup table for BattlEye
static uint32_t crc32_table[256];
static bool crc32_table_initialized = false;

void RCONPacket::_bind_methods() {
	// Source RCON methods
	ClassDB::bind_static_method("RCONPacket", D_METHOD("create_source_auth", "id", "password"), &RCONPacket::create_source_auth);
	ClassDB::bind_static_method("RCONPacket", D_METHOD("create_source_command", "id", "command"), &RCONPacket::create_source_command);
	ClassDB::bind_static_method("RCONPacket", D_METHOD("parse_source_packet", "data"), &RCONPacket::parse_source_packet);

	// BattlEye RCON methods
	ClassDB::bind_static_method("RCONPacket", D_METHOD("create_battleye_login", "password"), &RCONPacket::create_battleye_login);
	ClassDB::bind_static_method("RCONPacket", D_METHOD("create_battleye_command", "seq", "command"), &RCONPacket::create_battleye_command);
	ClassDB::bind_static_method("RCONPacket", D_METHOD("create_battleye_message_ack", "seq"), &RCONPacket::create_battleye_message_ack);
	ClassDB::bind_static_method("RCONPacket", D_METHOD("parse_battleye_packet", "data"), &RCONPacket::parse_battleye_packet);
	ClassDB::bind_static_method("RCONPacket", D_METHOD("verify_battleye_crc32", "packet"), &RCONPacket::verify_battleye_crc32);

	// Enums
	BIND_ENUM_CONSTANT(SOURCE_SERVERDATA_AUTH);
	BIND_ENUM_CONSTANT(SOURCE_SERVERDATA_AUTH_RESPONSE);
	BIND_ENUM_CONSTANT(SOURCE_SERVERDATA_EXECCOMMAND);
	BIND_ENUM_CONSTANT(SOURCE_SERVERDATA_RESPONSE_VALUE);

	BIND_ENUM_CONSTANT(BATTLEYE_LOGIN);
	BIND_ENUM_CONSTANT(BATTLEYE_COMMAND);
	BIND_ENUM_CONSTANT(BATTLEYE_MESSAGE);
}

void RCONPacket::_write_int32_le(PackedByteArray &p_array, int32_t p_value) {
	p_array.push_back((p_value) & 0xFF);
	p_array.push_back((p_value >> 8) & 0xFF);
	p_array.push_back((p_value >> 16) & 0xFF);
	p_array.push_back((p_value >> 24) & 0xFF);
}

int32_t RCONPacket::_read_int32_le(const PackedByteArray &p_array, int p_offset) {
	if (p_offset + 4 > p_array.size()) {
		return 0;
	}
	return (int32_t)(p_array[p_offset] |
			(p_array[p_offset + 1] << 8) |
			(p_array[p_offset + 2] << 16) |
			(p_array[p_offset + 3] << 24));
}

uint32_t RCONPacket::_calculate_crc32(const PackedByteArray &p_data) {
	// Initialize CRC32 table if needed
	if (!crc32_table_initialized) {
		for (uint32_t i = 0; i < 256; i++) {
			uint32_t crc = i;
			for (int j = 0; j < 8; j++) {
				crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
			}
			crc32_table[i] = crc;
		}
		crc32_table_initialized = true;
	}

	uint32_t crc = 0xFFFFFFFF;
	for (int i = 0; i < p_data.size(); i++) {
		crc = (crc >> 8) ^ crc32_table[(crc ^ p_data[i]) & 0xFF];
	}
	return ~crc;
}

// ========== Source RCON Implementation ==========

PackedByteArray RCONPacket::create_source_auth(int p_id, const String &p_password) {
	PackedByteArray packet;
	CharString password_utf8 = p_password.utf8();

	// Body: password + null terminator
	int body_size = password_utf8.length() + 1;

	// Size = 4 (ID) + 4 (Type) + body_size + 1 (null terminator)
	int32_t size = 4 + 4 + body_size + 1;

	_write_int32_le(packet, size);
	_write_int32_le(packet, p_id);
	_write_int32_le(packet, SOURCE_SERVERDATA_AUTH);

	// Body
	for (int i = 0; i < password_utf8.length(); i++) {
		packet.push_back(password_utf8[i]);
	}
	packet.push_back(0x00); // Null terminator

	// Packet terminator
	packet.push_back(0x00);

	return packet;
}

PackedByteArray RCONPacket::create_source_command(int p_id, const String &p_command) {
	PackedByteArray packet;
	CharString command_utf8 = p_command.utf8();

	// Body: command + null terminator
	int body_size = command_utf8.length() + 1;

	// Size = 4 (ID) + 4 (Type) + body_size + 1 (null terminator)
	int32_t size = 4 + 4 + body_size + 1;

	_write_int32_le(packet, size);
	_write_int32_le(packet, p_id);
	_write_int32_le(packet, SOURCE_SERVERDATA_EXECCOMMAND);

	// Body
	for (int i = 0; i < command_utf8.length(); i++) {
		packet.push_back(command_utf8[i]);
	}
	packet.push_back(0x00); // Null terminator

	// Packet terminator
	packet.push_back(0x00);

	return packet;
}

Dictionary RCONPacket::parse_source_packet(const PackedByteArray &p_data) {
	Dictionary result;

	if (p_data.size() < 14) { // Minimum packet size
		result["valid"] = false;
		result["error"] = "Packet too small";
		return result;
	}

	int32_t size = _read_int32_le(p_data, 0);
	int32_t id = _read_int32_le(p_data, 4);
	int32_t type = _read_int32_le(p_data, 8);

	// Validate size
	if (size != p_data.size() - 4) {
		result["valid"] = false;
		result["error"] = "Invalid packet size";
		return result;
	}

	// Extract body (everything between type and last 2 bytes)
	String body;
	int body_start = 12;
	int body_end = p_data.size() - 2;

	if (body_end > body_start) {
		PackedByteArray body_data = p_data.slice(body_start, body_end);
		// Remove null terminators
		while (body_data.size() > 0 && body_data[body_data.size() - 1] == 0x00) {
			body_data.resize(body_data.size() - 1);
		}
		body = String::utf8((const char *)body_data.ptr(), body_data.size());
	}

	result["valid"] = true;
	result["size"] = size;
	result["id"] = id;
	result["type"] = type;
	result["body"] = body;

	return result;
}

// ========== BattlEye RCON Implementation ==========

PackedByteArray RCONPacket::create_battleye_login(const String &p_password) {
	PackedByteArray payload;
	CharString password_utf8 = p_password.utf8();

	// Payload: type + password
	payload.push_back(BATTLEYE_LOGIN);
	for (int i = 0; i < password_utf8.length(); i++) {
		payload.push_back(password_utf8[i]);
	}

	// Calculate CRC32 of payload
	uint32_t crc = _calculate_crc32(payload);

	// Build packet: header + payload
	PackedByteArray packet;
	packet.push_back('B');
	packet.push_back('E');

	// CRC32 (little-endian)
	_write_int32_le(packet, crc);

	// Marker
	packet.push_back(0xFF);

	// Payload
	packet.append_array(payload);

	return packet;
}

PackedByteArray RCONPacket::create_battleye_command(int p_seq, const String &p_command) {
	PackedByteArray payload;
	CharString command_utf8 = p_command.utf8();

	// Payload: type + seq + command
	payload.push_back(BATTLEYE_COMMAND);
	payload.push_back((uint8_t)p_seq);

	for (int i = 0; i < command_utf8.length(); i++) {
		payload.push_back(command_utf8[i]);
	}

	// Calculate CRC32 of payload
	uint32_t crc = _calculate_crc32(payload);

	// Build packet: header + payload
	PackedByteArray packet;
	packet.push_back('B');
	packet.push_back('E');

	// CRC32 (little-endian)
	_write_int32_le(packet, crc);

	// Marker
	packet.push_back(0xFF);

	// Payload
	packet.append_array(payload);

	return packet;
}

PackedByteArray RCONPacket::create_battleye_message_ack(int p_seq) {
	PackedByteArray payload;

	// Payload: type + seq
	payload.push_back(BATTLEYE_MESSAGE);
	payload.push_back((uint8_t)p_seq);

	// Calculate CRC32 of payload
	uint32_t crc = _calculate_crc32(payload);

	// Build packet: header + payload
	PackedByteArray packet;
	packet.push_back('B');
	packet.push_back('E');

	// CRC32 (little-endian)
	_write_int32_le(packet, crc);

	// Marker
	packet.push_back(0xFF);

	// Payload
	packet.append_array(payload);

	return packet;
}

Dictionary RCONPacket::parse_battleye_packet(const PackedByteArray &p_data) {
	Dictionary result;

	if (p_data.size() < 7) {
		result["valid"] = false;
		result["error"] = "Packet too small";
		return result;
	}

	// Check magic bytes
	if (p_data[0] != 'B' || p_data[1] != 'E') {
		result["valid"] = false;
		result["error"] = "Invalid magic bytes";
		return result;
	}

	// Check marker
	if (p_data[6] != 0xFF) {
		result["valid"] = false;
		result["error"] = "Invalid marker byte";
		return result;
	}

	// Extract CRC32
	uint32_t received_crc = (uint32_t)_read_int32_le(p_data, 2);

	// Extract payload
	PackedByteArray payload = p_data.slice(7);

	// Verify CRC32
	uint32_t calculated_crc = _calculate_crc32(payload);
	if (received_crc != calculated_crc) {
		result["valid"] = false;
		result["error"] = "CRC32 mismatch";
		return result;
	}

	if (payload.size() < 1) {
		result["valid"] = false;
		result["error"] = "Empty payload";
		return result;
	}

	// Parse payload
	uint8_t type = payload[0];
	result["valid"] = true;
	result["type"] = type;

	if (type == BATTLEYE_LOGIN) {
		// Login response: type + result
		if (payload.size() >= 2) {
			result["success"] = (payload[1] == 0x01);
		}
	} else if (type == BATTLEYE_COMMAND) {
		// Command response: type + seq + [multi-packet header] + data
		if (payload.size() >= 2) {
			result["seq"] = payload[1];

			if (payload.size() > 2) {
				// Check for multi-packet header
				if (payload.size() >= 5 && payload[2] == 0x00) {
					result["multi_packet"] = true;
					result["total_packets"] = payload[3];
					result["packet_index"] = payload[4];
					if (payload.size() > 5) {
						PackedByteArray data = payload.slice(5);
						result["data"] = String::utf8((const char *)data.ptr(), data.size());
					} else {
						result["data"] = "";
					}
				} else {
					result["multi_packet"] = false;
					PackedByteArray data = payload.slice(2);
					result["data"] = String::utf8((const char *)data.ptr(), data.size());
				}
			} else {
				result["multi_packet"] = false;
				result["data"] = "";
			}
		}
	} else if (type == BATTLEYE_MESSAGE) {
		// Server message: type + seq + message
		if (payload.size() >= 2) {
			result["seq"] = payload[1];
			if (payload.size() > 2) {
				PackedByteArray message_data = payload.slice(2);
				result["message"] = String::utf8((const char *)message_data.ptr(), message_data.size());
			} else {
				result["message"] = "";
			}
		}
	}

	return result;
}

bool RCONPacket::verify_battleye_crc32(const PackedByteArray &p_packet) {
	if (p_packet.size() < 7) {
		return false;
	}

	if (p_packet[0] != 'B' || p_packet[1] != 'E' || p_packet[6] != 0xFF) {
		return false;
	}

	uint32_t received_crc = (uint32_t)_read_int32_le(p_packet, 2);
	PackedByteArray payload = p_packet.slice(7);
	uint32_t calculated_crc = _calculate_crc32(payload);

	return received_crc == calculated_crc;
}

RCONPacket::RCONPacket() {
}

RCONPacket::~RCONPacket() {
}
