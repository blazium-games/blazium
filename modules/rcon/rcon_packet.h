/**************************************************************************/
/*  rcon_packet.h                                                         */
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

#include "core/object/class_db.h"
#include "core/object/ref_counted.h"

class RCONPacket : public RefCounted {
	GDCLASS(RCONPacket, RefCounted);

public:
	// Source RCON packet types
	enum SourcePacketType {
		SOURCE_SERVERDATA_AUTH = 3,
		SOURCE_SERVERDATA_AUTH_RESPONSE = 2,
		SOURCE_SERVERDATA_EXECCOMMAND = 2,
		SOURCE_SERVERDATA_RESPONSE_VALUE = 0,
	};

	// BattlEye RCON packet types
	enum BattlEyePacketType {
		BATTLEYE_LOGIN = 0x00,
		BATTLEYE_COMMAND = 0x01,
		BATTLEYE_MESSAGE = 0x02,
	};

protected:
	static void _bind_methods();

private:
	// Helper methods for byte manipulation
	static void _write_int32_le(PackedByteArray &p_array, int32_t p_value);
	static int32_t _read_int32_le(const PackedByteArray &p_array, int p_offset);

public:
	static uint32_t _calculate_crc32(const PackedByteArray &p_data);
	// Source RCON packet creation
	static PackedByteArray create_source_auth(int p_id, const String &p_password);
	static PackedByteArray create_source_command(int p_id, const String &p_command);
	static Dictionary parse_source_packet(const PackedByteArray &p_data);

	// BattlEye RCON packet creation
	static PackedByteArray create_battleye_login(const String &p_password);
	static PackedByteArray create_battleye_command(int p_seq, const String &p_command);
	static PackedByteArray create_battleye_message_ack(int p_seq);
	static Dictionary parse_battleye_packet(const PackedByteArray &p_data);
	static bool verify_battleye_crc32(const PackedByteArray &p_packet);

	RCONPacket();
	~RCONPacket();
};

VARIANT_ENUM_CAST(RCONPacket::SourcePacketType);
VARIANT_ENUM_CAST(RCONPacket::BattlEyePacketType);
