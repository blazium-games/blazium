/**************************************************************************/
/*  socketio_packet.cpp                                                   */
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

#include "socketio_packet.h"

#include "core/io/json.h"

String SocketIOPacket::encode() const {
	String result;

	// 1. Packet type
	result += String::num_int64(type);

	// 2. Number of attachments (for binary packets)
	if (type == PACKET_BINARY_EVENT || type == PACKET_BINARY_ACK) {
		result += String::num_int64(num_attachments) + "-";
	}

	// 3. Namespace (only if not main namespace)
	if (namespace_path != "/") {
		result += namespace_path + ",";
	}

	// 4. Acknowledgment ID (optional)
	if (ack_id >= 0) {
		result += String::num_int64(ack_id);
	}

	// 5. Data payload (JSON)
	if (type == PACKET_CONNECT || type == PACKET_CONNECT_ERROR || type == PACKET_EVENT ||
			type == PACKET_ACK || type == PACKET_BINARY_EVENT || type == PACKET_BINARY_ACK) {
		if (!data.is_empty() || type == PACKET_CONNECT || type == PACKET_CONNECT_ERROR) {
			Variant payload;
			if (data.size() == 1 && (type == PACKET_CONNECT || type == PACKET_CONNECT_ERROR)) {
				// For CONNECT/CONNECT_ERROR, use object directly
				payload = data[0];
			} else {
				// For events/acks, use array
				payload = data;
			}
			result += JSON::stringify(payload);
		}
	}

	return result;
}

SocketIOPacket SocketIOPacket::decode(const String &p_packet_str) {
	SocketIOPacket packet;

	if (p_packet_str.is_empty()) {
		ERR_FAIL_V_MSG(packet, "Cannot decode empty packet string");
	}

	int pos = 0;

	// 1. Parse packet type
	char32_t c = p_packet_str[pos];
	if (c < '0' || c > '9') {
		ERR_FAIL_V_MSG(packet, "Invalid packet: missing packet type");
	}
	packet.type = (PacketType)(c - '0');
	pos++;

	// 2. Parse number of attachments (for binary packets)
	if (packet.type == PACKET_BINARY_EVENT || packet.type == PACKET_BINARY_ACK) {
		String num_str;
		while (pos < p_packet_str.length()) {
			char32_t ch = p_packet_str[pos];
			if (ch < '0' || ch > '9') {
				break;
			}
			num_str += p_packet_str[pos];
			pos++;
		}
		if (pos < p_packet_str.length() && p_packet_str[pos] == '-') {
			packet.num_attachments = num_str.to_int();
			pos++; // Skip '-'
		}
	}

	// 3. Parse namespace (if present)
	if (pos < p_packet_str.length() && p_packet_str[pos] == '/') {
		int comma_pos = p_packet_str.find(",", pos);
		if (comma_pos != -1) {
			packet.namespace_path = p_packet_str.substr(pos, comma_pos - pos);
			pos = comma_pos + 1; // Skip ','
		} else {
			// Namespace without comma - rest might be just namespace or namespace + JSON
			// Check if there's JSON after the namespace
			int json_start = pos;
			while (json_start < p_packet_str.length() && p_packet_str[json_start] != '{' && p_packet_str[json_start] != '[') {
				char32_t ch = p_packet_str[json_start];
				if (ch >= '0' && ch <= '9') {
					break;
				}
				json_start++;
			}
			if (json_start > pos) {
				packet.namespace_path = p_packet_str.substr(pos, json_start - pos);
				// Remove trailing comma if present
				if (packet.namespace_path.ends_with(",")) {
					packet.namespace_path = packet.namespace_path.substr(0, packet.namespace_path.length() - 1);
				}
				pos = json_start;
			}
		}
	}

	// 4. Parse acknowledgment ID (if present - digits before JSON)
	if (pos < p_packet_str.length()) {
		char32_t ch = p_packet_str[pos];
		if (ch >= '0' && ch <= '9') {
			String ack_str;
			while (pos < p_packet_str.length()) {
				char32_t ch2 = p_packet_str[pos];
				if (ch2 < '0' || ch2 > '9') {
					break;
				}
				ack_str += p_packet_str[pos];
				pos++;
			}
			packet.ack_id = ack_str.to_int();
		}
	}

	// 5. Parse data payload (JSON)
	if (pos < p_packet_str.length()) {
		String json_str = p_packet_str.substr(pos);
		Ref<JSON> json;
		json.instantiate();
		Error err = json->parse(json_str);
		if (err == OK) {
			Variant parsed = json->get_data();
			if (parsed.get_type() == Variant::ARRAY) {
				packet.data = parsed;
			} else {
				// For non-array types (DICTIONARY, etc.), wrap in array
				packet.data.push_back(parsed);
			}
		} else {
			ERR_PRINT("Failed to parse JSON payload: " + json_str);
		}
	}

	return packet;
}

bool SocketIOPacket::has_binary() const {
	for (int i = 0; i < data.size(); i++) {
		if (_contains_binary(data[i])) {
			return true;
		}
	}
	return false;
}

void SocketIOPacket::extract_binary_attachments() {
	attachments.clear();
	num_attachments = 0;

	// Extract binary and replace with placeholders
	Array new_data;
	for (int i = 0; i < data.size(); i++) {
		new_data.push_back(_extract_binary_recursive(data[i], attachments));
	}
	data = new_data;

	num_attachments = attachments.size();

	// Update packet type if we found binary
	if (num_attachments > 0) {
		if (type == PACKET_EVENT) {
			type = PACKET_BINARY_EVENT;
		} else if (type == PACKET_ACK) {
			type = PACKET_BINARY_ACK;
		}
	}
}

void SocketIOPacket::reconstruct_binary_data() {
	if (num_attachments == 0 || attachments.size() == 0) {
		return;
	}

	Array new_data;
	for (int i = 0; i < data.size(); i++) {
		new_data.push_back(_reconstruct_binary_recursive(data[i], attachments));
	}
	data = new_data;
}

bool SocketIOPacket::_contains_binary(const Variant &p_data) {
	switch (p_data.get_type()) {
		case Variant::PACKED_BYTE_ARRAY:
			return true;
		case Variant::ARRAY: {
			Array arr = p_data;
			for (int i = 0; i < arr.size(); i++) {
				if (_contains_binary(arr[i])) {
					return true;
				}
			}
			return false;
		}
		case Variant::DICTIONARY: {
			Dictionary dict = p_data;
			Array keys = dict.keys();
			for (int i = 0; i < keys.size(); i++) {
				if (_contains_binary(dict[keys[i]])) {
					return true;
				}
			}
			return false;
		}
		default:
			return false;
	}
}

Variant SocketIOPacket::_extract_binary_recursive(const Variant &p_data, Vector<PackedByteArray> &r_attachments) {
	switch (p_data.get_type()) {
		case Variant::PACKED_BYTE_ARRAY: {
			// Replace with placeholder
			Dictionary placeholder;
			placeholder["_placeholder"] = true;
			placeholder["num"] = r_attachments.size();
			r_attachments.push_back(p_data);
			return placeholder;
		}
		case Variant::ARRAY: {
			Array arr = p_data;
			Array new_arr;
			for (int i = 0; i < arr.size(); i++) {
				new_arr.push_back(_extract_binary_recursive(arr[i], r_attachments));
			}
			return new_arr;
		}
		case Variant::DICTIONARY: {
			Dictionary dict = p_data;
			Dictionary new_dict;
			Array keys = dict.keys();
			for (int i = 0; i < keys.size(); i++) {
				new_dict[keys[i]] = _extract_binary_recursive(dict[keys[i]], r_attachments);
			}
			return new_dict;
		}
		default:
			return p_data;
	}
}

Variant SocketIOPacket::_reconstruct_binary_recursive(const Variant &p_data, const Vector<PackedByteArray> &p_attachments) {
	if (p_data.get_type() == Variant::DICTIONARY) {
		Dictionary dict = p_data;
		// Check if this is a placeholder
		if (dict.has("_placeholder") && dict.has("num")) {
			bool is_placeholder = dict["_placeholder"];
			if (is_placeholder) {
				int index = dict["num"];
				if (index >= 0 && index < p_attachments.size()) {
					return p_attachments[index];
				}
			}
		}

		// Not a placeholder, recurse into dictionary
		Dictionary new_dict;
		Array keys = dict.keys();
		for (int i = 0; i < keys.size(); i++) {
			new_dict[keys[i]] = _reconstruct_binary_recursive(dict[keys[i]], p_attachments);
		}
		return new_dict;
	} else if (p_data.get_type() == Variant::ARRAY) {
		Array arr = p_data;
		Array new_arr;
		for (int i = 0; i < arr.size(); i++) {
			new_arr.push_back(_reconstruct_binary_recursive(arr[i], p_attachments));
		}
		return new_arr;
	}

	return p_data;
}
