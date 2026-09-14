/**************************************************************************/
/*  warcry_protocol.cpp                                                   */
/**************************************************************************/

#include "warcry_protocol.h"

#include "core/io/json.h"

#include <cstring>

namespace WarcryProtocol {

String msg_type_name(MsgType p_type) {
	switch (p_type) {
		case MsgType::HELLO:
			return "HELLO";
		case MsgType::HELLO_ACK:
			return "HELLO_ACK";
		case MsgType::JOIN_CHANNEL:
			return "JOIN_CHANNEL";
		case MsgType::LEAVE_CHANNEL:
			return "LEAVE_CHANNEL";
		case MsgType::USER_STATE:
			return "USER_STATE";
		case MsgType::VOICE_FRAME:
			return "VOICE_FRAME";
		case MsgType::PING:
			return "PING";
		case MsgType::PONG:
			return "PONG";
		case MsgType::SERVER_STATE:
			return "SERVER_STATE";
		case MsgType::ERROR_MSG:
			return "ERROR";
		case MsgType::AUTH:
			return "AUTH";
		case MsgType::AUTH_ACK:
			return "AUTH_ACK";
		case MsgType::USER_PREFERENCE:
			return "USER_PREFERENCE";
		default:
			return "INVALID";
	}
}

Vector<uint8_t> serialize_control(MsgType p_type, const Dictionary &p_data) {
	Dictionary msg;
	msg["type"] = msg_type_name(p_type);
	msg["data"] = p_data;
	const String json = JSON::stringify(msg, "", false);
	const CharString utf8 = json.utf8();

	MessageHeader header;
	header.type = p_type;
	header.flags = 0;
	header.payload_size = static_cast<uint32_t>(utf8.length());

	Vector<uint8_t> out;
	out.resize(sizeof(MessageHeader) + utf8.length());
	memcpy(out.ptrw(), &header, sizeof(MessageHeader));
	if (utf8.length() > 0) {
		memcpy(out.ptrw() + sizeof(MessageHeader), utf8.get_data(), utf8.length());
	}
	return out;
}

bool deserialize_control(const uint8_t *p_data, int p_size, MsgType &r_type, Dictionary &r_data) {
	if (!p_data || p_size < (int)sizeof(MessageHeader)) {
		return false;
	}
	MessageHeader header;
	memcpy(&header, p_data, sizeof(MessageHeader));
	if (p_size < (int)(sizeof(MessageHeader) + header.payload_size)) {
		return false;
	}
	r_type = header.type;
	const String json = String::utf8(reinterpret_cast<const char *>(p_data + sizeof(MessageHeader)), header.payload_size);
	Ref<JSON> parser;
	parser.instantiate();
	const Error err = parser->parse(json);
	if (err != OK || parser->get_data().get_type() != Variant::DICTIONARY) {
		return false;
	}
	const Dictionary root = parser->get_data();
	if (root.has("data") && root["data"].get_type() == Variant::DICTIONARY) {
		r_data = root["data"];
	} else {
		r_data = root;
	}
	return true;
}

Vector<uint8_t> serialize_voice(const VoiceFrameHeader &p_header, const uint8_t *p_opus, int p_opus_size) {
	Vector<uint8_t> out;
	out.resize(sizeof(VoiceFrameHeader) + MAX(p_opus_size, 0));
	memcpy(out.ptrw(), &p_header, sizeof(VoiceFrameHeader));
	if (p_opus && p_opus_size > 0) {
		memcpy(out.ptrw() + sizeof(VoiceFrameHeader), p_opus, p_opus_size);
	}
	return out;
}

bool deserialize_voice(const uint8_t *p_data, int p_size, VoiceFrameHeader &r_header, Vector<uint8_t> &r_opus) {
	if (!p_data || p_size < (int)sizeof(VoiceFrameHeader)) {
		return false;
	}
	memcpy(&r_header, p_data, sizeof(VoiceFrameHeader));
	const int opus_size = p_size - (int)sizeof(VoiceFrameHeader);
	r_opus.resize(opus_size);
	if (opus_size > 0) {
		memcpy(r_opus.ptrw(), p_data + sizeof(VoiceFrameHeader), opus_size);
	}
	return true;
}

} // namespace WarcryProtocol
