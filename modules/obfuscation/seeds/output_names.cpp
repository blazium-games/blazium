/**************************************************************************/
/*  output_names.cpp                                                      */
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

#include "seeds/output_names.h"

#include "core/crypto/crypto.h"
#include "core/crypto/hashing_context.h"

String ObfuscationOutputNames::canonicalize_path(const String &p_path) {
	String s = p_path.strip_edges().replace("\\", "/");
	String prefix;
	String rest = s;
	const int scheme = s.find("://");
	if (scheme >= 0) {
		prefix = s.substr(0, scheme + 3).to_lower();
		rest = s.substr(scheme + 3);
	}
	while (rest.contains("//")) {
		rest = rest.replace("//", "/");
	}
	while (rest.begins_with("./")) {
		rest = rest.substr(2);
	}
	return prefix + rest.to_lower();
}

bool ObfuscationOutputNames::ids_match_folded(const String &p_a, const String &p_b) {
	return p_a.strip_edges().to_lower() == p_b.strip_edges().to_lower();
}

String ObfuscationOutputNames::scramble_output_path(const PackedByteArray &p_hmac_key, const String &p_logical) {
	const String logical = canonicalize_path(p_logical);
	String ext = logical.get_extension();
	if (ext.is_empty()) {
		ext = "bin";
	}
	Ref<Crypto> crypto = Crypto::create();
	PackedByteArray key = p_hmac_key;
	if (key.is_empty()) {
		key.resize(32);
	}
	PackedByteArray msg = String("out:" + logical).to_utf8_buffer();
	PackedByteArray digest;
	if (crypto.is_valid()) {
		digest = crypto->hmac_digest(HashingContext::HASH_SHA256, key, msg);
	}
	if (digest.size() < 16) {
		digest.resize(16);
	}
	const String hex = String::hex_encode_buffer(digest.ptr(), 16);
	return "res://" + hex.substr(0, 8) + "/" + hex.substr(8, 8) + "/" + hex.substr(16, 16) + "." + ext;
}

String ObfuscationOutputNames::scramble_identifier(const PackedByteArray &p_hmac_key, const String &p_name) {
	Ref<Crypto> crypto = Crypto::create();
	PackedByteArray key = p_hmac_key;
	if (key.is_empty()) {
		key.resize(32);
	}
	PackedByteArray msg = String("id:" + p_name).to_utf8_buffer();
	PackedByteArray digest;
	if (crypto.is_valid()) {
		digest = crypto->hmac_digest(HashingContext::HASH_SHA256, key, msg);
	}
	if (digest.size() < 6) {
		digest.resize(6);
	}
	return "_" + String::hex_encode_buffer(digest.ptr(), 6);
}

String ObfuscationOutputNames::to_absolute(const String &p_output_dir, const String &p_res_path) {
	String rest = p_res_path.replace("\\", "/");
	if (rest.begins_with("res://")) {
		rest = rest.substr(6);
	}
	while (rest.begins_with("/")) {
		rest = rest.substr(1);
	}
	return p_output_dir.path_join(rest);
}
