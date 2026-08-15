/**************************************************************************/
/*  test_enet_packet_utils.h                                              */
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

// Only include the packet utils - no ENet headers transitively included
#include "modules/enet_server/enet_packet.h"

namespace TestENetPacketUtils {

// ============================================================================
// STAGE 1: ENetPacketUtils - Basic Encoding/Decoding
// ============================================================================

TEST_CASE("[ENetServer][Stage1] encode_decode_variant") {
	ENetPacketUtils *utils = ENetPacketUtils::get_singleton();
	REQUIRE(utils != nullptr);

	// Test integer
	Variant original_int = 42;
	PackedByteArray encoded_int = utils->encode_variant(original_int);
	CHECK(encoded_int.size() > 0);
	Variant decoded_int = utils->decode_variant(encoded_int);
	CHECK((int)decoded_int == 42);

	// Test string
	String original_string = "Hello, World!";
	PackedByteArray encoded_string = utils->encode_variant(original_string);
	Variant decoded_string = utils->decode_variant(encoded_string);
	CHECK((String)decoded_string == original_string);

	// Test float
	Variant original_float = 3.14f;
	PackedByteArray encoded_float = utils->encode_variant(original_float);
	Variant decoded_float = utils->decode_variant(encoded_float);
	CHECK((float)decoded_float == 3.14f);
}

TEST_CASE("[ENetServer][Stage1] encode_decode_dictionary") {
	ENetPacketUtils *utils = ENetPacketUtils::get_singleton();
	REQUIRE(utils != nullptr);

	Dictionary original;
	original["name"] = "Player1";
	original["score"] = 100;
	original["alive"] = true;

	PackedByteArray encoded = utils->encode_dictionary(original);
	CHECK(encoded.size() > 0);

	Dictionary decoded = utils->decode_dictionary(encoded);
	CHECK(decoded.has("name"));
	CHECK((String)decoded["name"] == "Player1");
	CHECK((int)decoded["score"] == 100);
	CHECK((bool)decoded["alive"] == true);
}

TEST_CASE("[ENetServer][Stage1] encode_decode_string") {
	ENetPacketUtils *utils = ENetPacketUtils::get_singleton();
	REQUIRE(utils != nullptr);

	String original = "Test String with Special Characters: !@#$%";
	PackedByteArray encoded = utils->encode_string(original);
	CHECK(encoded.size() > 0);

	String decoded = utils->decode_string(encoded);
	CHECK(decoded == original);
}

TEST_CASE("[ENetServer][Stage1] encode_decode_json") {
	ENetPacketUtils *utils = ENetPacketUtils::get_singleton();
	REQUIRE(utils != nullptr);

	String json_string = "{\"key\":\"value\",\"number\":42}";
	PackedByteArray encoded = utils->encode_json(json_string);
	CHECK(encoded.size() > 0);

	String decoded = utils->decode_json(encoded);
	CHECK(decoded == json_string);
}

TEST_CASE("[ENetServer][Stage1] auto_encode_decode") {
	ENetPacketUtils *utils = ENetPacketUtils::get_singleton();
	REQUIRE(utils != nullptr);

	// Test auto-detection with Variant
	Variant test_int = 42;
	PackedByteArray encoded_int = utils->encode_auto(test_int);
	Variant decoded_int = utils->decode_auto(encoded_int);
	CHECK((int)decoded_int == 42);

	// Test auto-detection with String
	String test_string = "Auto String";
	PackedByteArray encoded_str = utils->encode_auto(test_string);
	Variant decoded_str = utils->decode_auto(encoded_str);
	CHECK(decoded_str.get_type() == Variant::STRING);
	CHECK((String)decoded_str == test_string);

	// Test auto-detection with Dictionary
	Dictionary test_dict;
	test_dict["key"] = "value";
	PackedByteArray encoded_dict = utils->encode_auto(test_dict);
	Variant decoded_dict = utils->decode_auto(encoded_dict);
	CHECK(decoded_dict.get_type() == Variant::DICTIONARY);
	Dictionary result_dict = decoded_dict;
	String key_value = result_dict["key"];
	CHECK(key_value == "value");
}

} // namespace TestENetPacketUtils
