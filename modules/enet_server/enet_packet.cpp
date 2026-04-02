/**************************************************************************/
/*  enet_packet.cpp                                                       */
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

#include "enet_packet.h"

#include "core/io/json.h"
#include "core/io/marshalls.h"
#include "core/variant/variant.h"
#include "scene/2d/node_2d.h"
#include "scene/3d/node_3d.h"

ENetPacketUtils *ENetPacketUtils::singleton = nullptr;

ENetPacketUtils *ENetPacketUtils::get_singleton() {
	return singleton;
}

void ENetPacketUtils::_bind_methods() {
	// Encoding methods
	ClassDB::bind_method(D_METHOD("encode_variant", "variant"), &ENetPacketUtils::encode_variant);
	ClassDB::bind_method(D_METHOD("encode_dictionary", "dictionary"), &ENetPacketUtils::encode_dictionary);
	ClassDB::bind_method(D_METHOD("encode_json", "json"), &ENetPacketUtils::encode_json);
	ClassDB::bind_method(D_METHOD("encode_string", "string"), &ENetPacketUtils::encode_string);
	ClassDB::bind_method(D_METHOD("encode_node", "node", "flags"), &ENetPacketUtils::encode_node);

	// Decoding methods
	ClassDB::bind_method(D_METHOD("decode_variant", "data"), &ENetPacketUtils::decode_variant);
	ClassDB::bind_method(D_METHOD("decode_dictionary", "data"), &ENetPacketUtils::decode_dictionary);
	ClassDB::bind_method(D_METHOD("decode_json", "data"), &ENetPacketUtils::decode_json);
	ClassDB::bind_method(D_METHOD("decode_string", "data"), &ENetPacketUtils::decode_string);
	ClassDB::bind_method(D_METHOD("decode_node_metadata", "data"), &ENetPacketUtils::decode_node_metadata);

	// Auto encoding/decoding
	ClassDB::bind_method(D_METHOD("encode_auto", "variant"), &ENetPacketUtils::encode_auto);
	ClassDB::bind_method(D_METHOD("decode_auto", "data"), &ENetPacketUtils::decode_auto);

	// Enums
	BIND_ENUM_CONSTANT(PACKET_TYPE_RAW);
	BIND_ENUM_CONSTANT(PACKET_TYPE_VARIANT);
	BIND_ENUM_CONSTANT(PACKET_TYPE_DICTIONARY);
	BIND_ENUM_CONSTANT(PACKET_TYPE_JSON);
	BIND_ENUM_CONSTANT(PACKET_TYPE_STRING);
	BIND_ENUM_CONSTANT(PACKET_TYPE_NODE);

	BIND_ENUM_CONSTANT(SYNC_PROPERTIES);
	BIND_ENUM_CONSTANT(SYNC_METADATA_ONLY);
	BIND_ENUM_CONSTANT(SYNC_TRANSFORM_ONLY);
	BIND_ENUM_CONSTANT(SYNC_CUSTOM);
}

PackedByteArray ENetPacketUtils::encode_variant(const Variant &p_variant) {
	PackedByteArray data;
	int len = 0;
	Error err = ::encode_variant(p_variant, nullptr, len, false);
	ERR_FAIL_COND_V(err != OK, data);

	data.resize(len + 1);
	data.write[0] = PACKET_TYPE_VARIANT;
	::encode_variant(p_variant, data.ptrw() + 1, len, false);

	return data;
}

PackedByteArray ENetPacketUtils::encode_dictionary(const Dictionary &p_dict) {
	return encode_variant(p_dict);
}

PackedByteArray ENetPacketUtils::encode_json(const String &p_json) {
	PackedByteArray data;
	CharString utf8 = p_json.utf8();
	int len = utf8.length();

	data.resize(len + 5);
	data.write[0] = PACKET_TYPE_JSON;
	encode_uint32(len, data.ptrw() + 1);
	memcpy(data.ptrw() + 5, utf8.get_data(), len);

	return data;
}

PackedByteArray ENetPacketUtils::encode_string(const String &p_string) {
	PackedByteArray data;
	CharString utf8 = p_string.utf8();
	int len = utf8.length();

	data.resize(len + 5);
	data.write[0] = PACKET_TYPE_STRING;
	encode_uint32(len, data.ptrw() + 1);
	memcpy(data.ptrw() + 5, utf8.get_data(), len);

	return data;
}

PackedByteArray ENetPacketUtils::encode_node(Node *p_node, int p_flags) {
	ERR_FAIL_NULL_V(p_node, PackedByteArray());

	Dictionary node_data;

	if (p_flags & SYNC_CUSTOM) {
		node_data = _serialize_node_custom(p_node);
	} else if (p_flags & SYNC_PROPERTIES) {
		node_data = _serialize_node_properties(p_node);
	} else if (p_flags & SYNC_TRANSFORM_ONLY) {
		node_data = _serialize_node_transform(p_node);
	} else if (p_flags & SYNC_METADATA_ONLY) {
		node_data = _serialize_node_metadata(p_node);
	} else {
		node_data = _serialize_node_metadata(p_node);
	}

	PackedByteArray data;
	int len = 0;
	Error err = ::encode_variant(node_data, nullptr, len, false);
	ERR_FAIL_COND_V(err != OK, data);

	data.resize(len + 1);
	data.write[0] = PACKET_TYPE_NODE;
	::encode_variant(node_data, data.ptrw() + 1, len, false);

	return data;
}

Variant ENetPacketUtils::decode_variant(const PackedByteArray &p_data) {
	ERR_FAIL_COND_V(p_data.size() < 2, Variant());
	ERR_FAIL_COND_V(p_data[0] != PACKET_TYPE_VARIANT, Variant());

	int len = p_data.size() - 1;
	Variant variant;
	Error err = ::decode_variant(variant, p_data.ptr() + 1, len, nullptr, false);
	ERR_FAIL_COND_V(err != OK, Variant());

	return variant;
}

Dictionary ENetPacketUtils::decode_dictionary(const PackedByteArray &p_data) {
	Variant variant = decode_variant(p_data);
	if (variant.get_type() == Variant::DICTIONARY) {
		return variant;
	}
	return Dictionary();
}

String ENetPacketUtils::decode_json(const PackedByteArray &p_data) {
	ERR_FAIL_COND_V(p_data.size() < 5, String());
	ERR_FAIL_COND_V(p_data[0] != PACKET_TYPE_JSON, String());

	uint32_t len = decode_uint32(p_data.ptr() + 1);
	ERR_FAIL_COND_V(p_data.size() < (int)(len + 5), String());

	String json;
	json.parse_utf8((const char *)(p_data.ptr() + 5), len);
	return json;
}

String ENetPacketUtils::decode_string(const PackedByteArray &p_data) {
	ERR_FAIL_COND_V(p_data.size() < 5, String());
	ERR_FAIL_COND_V(p_data[0] != PACKET_TYPE_STRING, String());

	uint32_t len = decode_uint32(p_data.ptr() + 1);
	ERR_FAIL_COND_V(p_data.size() < (int)(len + 5), String());

	String str;
	str.parse_utf8((const char *)(p_data.ptr() + 5), len);
	return str;
}

Dictionary ENetPacketUtils::decode_node_metadata(const PackedByteArray &p_data) {
	ERR_FAIL_COND_V(p_data.size() < 2, Dictionary());
	ERR_FAIL_COND_V(p_data[0] != PACKET_TYPE_NODE, Dictionary());

	int len = p_data.size() - 1;
	Variant variant;
	Error err = ::decode_variant(variant, p_data.ptr() + 1, len, nullptr, false);
	ERR_FAIL_COND_V(err != OK, Dictionary());

	if (variant.get_type() == Variant::DICTIONARY) {
		return variant;
	}
	return Dictionary();
}

PackedByteArray ENetPacketUtils::encode_auto(const Variant &p_variant) {
	switch (p_variant.get_type()) {
		case Variant::PACKED_BYTE_ARRAY: {
			// Raw data - just return as-is with type header
			PackedByteArray raw = p_variant;
			PackedByteArray data;
			data.resize(raw.size() + 1);
			data.write[0] = PACKET_TYPE_RAW;
			memcpy(data.ptrw() + 1, raw.ptr(), raw.size());
			return data;
		}
		case Variant::STRING: {
			return encode_string(p_variant);
		}
		case Variant::DICTIONARY: {
			return encode_dictionary(p_variant);
		}
		case Variant::OBJECT: {
			Object *obj = p_variant;
			Node *node = Object::cast_to<Node>(obj);
			if (node) {
				return encode_node(node, SYNC_PROPERTIES);
			}
			// Try to call _to_packet() if it exists
			if (obj && obj->has_method("_to_packet")) {
				Variant result = obj->call("_to_packet");
				if (result.get_type() == Variant::PACKED_BYTE_ARRAY) {
					return result;
				}
			}
			// Fall through to variant encoding
			return encode_variant(p_variant);
		}
		default: {
			return encode_variant(p_variant);
		}
	}
}

Variant ENetPacketUtils::decode_auto(const PackedByteArray &p_data) {
	ERR_FAIL_COND_V(p_data.size() < 1, Variant());

	PacketType type = (PacketType)p_data[0];

	switch (type) {
		case PACKET_TYPE_RAW: {
			PackedByteArray raw;
			raw.resize(p_data.size() - 1);
			memcpy(raw.ptrw(), p_data.ptr() + 1, p_data.size() - 1);
			return raw;
		}
		case PACKET_TYPE_VARIANT:
		case PACKET_TYPE_DICTIONARY: {
			return decode_variant(p_data);
		}
		case PACKET_TYPE_JSON: {
			String json = decode_json(p_data);
			JSON parser;
			Error err = parser.parse(json);
			if (err == OK) {
				return parser.get_data();
			}
			return json;
		}
		case PACKET_TYPE_STRING: {
			return decode_string(p_data);
		}
		case PACKET_TYPE_NODE: {
			return decode_node_metadata(p_data);
		}
		default: {
			// Try to decode as variant anyway
			return decode_variant(p_data);
		}
	}
}

Dictionary ENetPacketUtils::_serialize_node_properties(Node *p_node) {
	ERR_FAIL_NULL_V(p_node, Dictionary());

	Dictionary data;
	data["_type"] = "node";
	data["_class"] = p_node->get_class();
	data["_path"] = p_node->get_path();
	data["_name"] = p_node->get_name();

	// Get all properties
	List<PropertyInfo> properties;
	p_node->get_property_list(&properties);

	Dictionary props;
	for (const PropertyInfo &prop : properties) {
		// Only serialize exported properties and some basic properties
		if ((prop.usage & PROPERTY_USAGE_STORAGE) && !(prop.usage & PROPERTY_USAGE_INTERNAL)) {
			props[prop.name] = p_node->get(prop.name);
		}
	}
	data["properties"] = props;

	// Add transform data if applicable
	Node2D *node2d = Object::cast_to<Node2D>(p_node);
	if (node2d) {
		data["position"] = node2d->get_position();
		data["rotation"] = node2d->get_rotation();
		data["scale"] = node2d->get_scale();
	}

	Node3D *node3d = Object::cast_to<Node3D>(p_node);
	if (node3d) {
		data["transform"] = node3d->get_transform();
		data["position"] = node3d->get_position();
		data["rotation"] = node3d->get_rotation();
		data["scale"] = node3d->get_scale();
	}

	return data;
}

Dictionary ENetPacketUtils::_serialize_node_metadata(Node *p_node) {
	ERR_FAIL_NULL_V(p_node, Dictionary());

	Dictionary data;
	data["_type"] = "node_meta";
	data["_class"] = p_node->get_class();
	data["_path"] = p_node->get_path();
	data["_name"] = p_node->get_name();

	return data;
}

Dictionary ENetPacketUtils::_serialize_node_transform(Node *p_node) {
	ERR_FAIL_NULL_V(p_node, Dictionary());

	Dictionary data;
	data["_type"] = "node_transform";
	data["_class"] = p_node->get_class();
	data["_path"] = p_node->get_path();
	data["_name"] = p_node->get_name();

	Node2D *node2d = Object::cast_to<Node2D>(p_node);
	if (node2d) {
		data["position"] = node2d->get_position();
		data["rotation"] = node2d->get_rotation();
		data["scale"] = node2d->get_scale();
	}

	Node3D *node3d = Object::cast_to<Node3D>(p_node);
	if (node3d) {
		data["transform"] = node3d->get_transform();
		data["position"] = node3d->get_position();
		data["rotation"] = node3d->get_rotation();
		data["scale"] = node3d->get_scale();
	}

	return data;
}

Dictionary ENetPacketUtils::_serialize_node_custom(Node *p_node) {
	ERR_FAIL_NULL_V(p_node, Dictionary());

	Dictionary data;
	data["_type"] = "node_custom";
	data["_class"] = p_node->get_class();
	data["_path"] = p_node->get_path();
	data["_name"] = p_node->get_name();

	// Call custom serialization method if it exists
	if (p_node->has_method("_serialize_for_network")) {
		Variant custom_data = p_node->call("_serialize_for_network");
		if (custom_data.get_type() == Variant::DICTIONARY) {
			data["custom"] = custom_data;
		} else if (custom_data.get_type() == Variant::PACKED_BYTE_ARRAY) {
			data["custom_bytes"] = custom_data;
		}
	}

	return data;
}

ENetPacketUtils::ENetPacketUtils() {
	singleton = this;
}

ENetPacketUtils::~ENetPacketUtils() {
	singleton = nullptr;
}
