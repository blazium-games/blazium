/**************************************************************************/
/*  warcry_protocol.h                                                     */
/**************************************************************************/

#pragma once

#include "core/io/json.h"
#include "core/templates/vector.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

#include <cstdint>

namespace WarcryProtocol {

enum class MsgType : uint8_t {
	HELLO = 0,
	HELLO_ACK = 1,
	JOIN_CHANNEL = 2,
	LEAVE_CHANNEL = 3,
	USER_STATE = 4,
	VOICE_FRAME = 5,
	PING = 6,
	PONG = 7,
	SERVER_STATE = 8,
	ERROR_MSG = 9,
	AUTH = 10,
	AUTH_ACK = 11,
	USER_PREFERENCE = 12,
	INVALID = 255
};

#pragma pack(push, 1)
struct MessageHeader {
	MsgType type;
	uint8_t flags;
	uint32_t payload_size;
};

struct VoiceFrameHeader {
	uint32_t channel_id;
	uint32_t sequence;
	uint32_t timestamp;
};
#pragma pack(pop)

static_assert(sizeof(MessageHeader) == 6, "control header must be 6 bytes");
static_assert(sizeof(VoiceFrameHeader) == 12, "voice header must be 12 bytes");

String msg_type_name(MsgType p_type);
Vector<uint8_t> serialize_control(MsgType p_type, const Dictionary &p_data);
bool deserialize_control(const uint8_t *p_data, int p_size, MsgType &r_type, Dictionary &r_data);
Vector<uint8_t> serialize_voice(const VoiceFrameHeader &p_header, const uint8_t *p_opus, int p_opus_size);
bool deserialize_voice(const uint8_t *p_data, int p_size, VoiceFrameHeader &r_header, Vector<uint8_t> &r_opus);

} // namespace WarcryProtocol
