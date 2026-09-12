/**************************************************************************/
/*  socketio_packet.h                                                     */
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

#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

class SocketIOPacket {
public:
	enum PacketType {
		PACKET_CONNECT = 0,
		PACKET_DISCONNECT = 1,
		PACKET_EVENT = 2,
		PACKET_ACK = 3,
		PACKET_CONNECT_ERROR = 4,
		PACKET_BINARY_EVENT = 5,
		PACKET_BINARY_ACK = 6
	};

	PacketType type = PACKET_EVENT;
	String namespace_path = "/";
	Array data;
	int ack_id = -1;
	int num_attachments = 0;
	Vector<PackedByteArray> attachments;

	SocketIOPacket() {}
	SocketIOPacket(PacketType p_type) :
			type(p_type) {}

	// Encoding/Decoding
	String encode() const;
	static SocketIOPacket decode(const String &p_packet_str);

	// Binary handling
	bool has_binary() const;
	void extract_binary_attachments();
	void reconstruct_binary_data();

private:
	static bool _contains_binary(const Variant &p_data);
	static Variant _extract_binary_recursive(const Variant &p_data, Vector<PackedByteArray> &r_attachments);
	static Variant _reconstruct_binary_recursive(const Variant &p_data, const Vector<PackedByteArray> &p_attachments);
};
