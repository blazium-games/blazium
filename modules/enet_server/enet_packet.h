/**************************************************************************/
/*  enet_packet.h                                                         */
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

#include "core/object/object.h"
#include "core/variant/variant.h"
#include "scene/main/node.h"

class ENetPacketUtils : public Object {
	GDCLASS(ENetPacketUtils, Object);

public:
	enum PacketType {
		PACKET_TYPE_RAW = 0,
		PACKET_TYPE_VARIANT = 1,
		PACKET_TYPE_DICTIONARY = 2,
		PACKET_TYPE_JSON = 3,
		PACKET_TYPE_STRING = 4,
		PACKET_TYPE_NODE = 5,
	};

	enum NodeSyncFlags {
		SYNC_PROPERTIES = 1 << 0,
		SYNC_METADATA_ONLY = 1 << 1,
		SYNC_TRANSFORM_ONLY = 1 << 2,
		SYNC_CUSTOM = 1 << 3,
	};

private:
	static ENetPacketUtils *singleton;

protected:
	static void _bind_methods();

public:
	static ENetPacketUtils *get_singleton();

	// Encoding methods
	PackedByteArray encode_variant(const Variant &p_variant);
	PackedByteArray encode_dictionary(const Dictionary &p_dict);
	PackedByteArray encode_json(const String &p_json);
	PackedByteArray encode_string(const String &p_string);
	PackedByteArray encode_node(Node *p_node, int p_flags);

	// Decoding methods
	Variant decode_variant(const PackedByteArray &p_data);
	Dictionary decode_dictionary(const PackedByteArray &p_data);
	String decode_json(const PackedByteArray &p_data);
	String decode_string(const PackedByteArray &p_data);
	Dictionary decode_node_metadata(const PackedByteArray &p_data);

	// Auto-detection encoding (used internally by server/client)
	PackedByteArray encode_auto(const Variant &p_variant);
	Variant decode_auto(const PackedByteArray &p_data);

	// Node serialization helpers
	static Dictionary _serialize_node_properties(Node *p_node);
	static Dictionary _serialize_node_metadata(Node *p_node);
	static Dictionary _serialize_node_transform(Node *p_node);
	static Dictionary _serialize_node_custom(Node *p_node);

	ENetPacketUtils();
	~ENetPacketUtils();
};

VARIANT_ENUM_CAST(ENetPacketUtils::PacketType);
VARIANT_ENUM_CAST(ENetPacketUtils::NodeSyncFlags);
