/**************************************************************************/
/*  script_seed.cpp                                                       */
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

#include "seeds/script_seed.h"

#include "seeds/source_map.h"

#include "core/crypto/crypto.h"
#include "core/crypto/hashing_context.h"

int64_t ObfuscationScriptSeed::derive(const PackedByteArray &p_hmac_key, const String &p_path) {
	Ref<Crypto> crypto = Crypto::create();
	if (crypto.is_null()) {
		return 0;
	}
	PackedByteArray msg = p_path.to_utf8_buffer();
	PackedByteArray key = p_hmac_key;
	if (key.is_empty()) {
		key.resize(32);
	}
	PackedByteArray d = crypto->hmac_digest(HashingContext::HASH_SHA256, key, msg);
	int64_t v = 0;
	for (int i = 0; i < 8 && i < d.size(); i++) {
		v = (v << 8) | d[i];
	}
	return v & 0x7FFFFFFFFFFFFFFFLL;
}

String ObfuscationScriptSeed::inject(const String &p_source, int64_t p_seed, bool p_luau) {
	const String hex = String::num_uint64((uint64_t)p_seed, 16, true).lpad(16, "0");
	const String line = p_luau ? ("local _CK = 0x" + hex) : ("const _CK := 0x" + hex);
	String src = p_source;
	int existing = src.find("const _CK");
	if (existing < 0) {
		existing = src.find("local _CK");
	}
	if (existing >= 0) {
		int eol = src.find("\n", existing);
		if (eol < 0) {
			eol = src.length();
		}
		src = src.substr(0, existing) + line + src.substr(eol);
	} else {
		src = ObfuscationSourceMap::insert_after_preamble(src, line);
	}
	if (src.contains("_obfuscation_seed_salt") || src.contains("_obfuscation_mix_seed")) {
		return src;
	}
	if (p_luau) {
		src = ObfuscationSourceMap::insert_before_chunk_return(src, "local function _obfuscation_seed_salt()\n\treturn _CK\nend");
	} else if (src.contains("RandomNumberGenerator") && src.contains(".seed")) {
		if (!src.contains("_CK")) {
			src += "\n";
		}
	} else if (src.contains("RandomNumberGenerator")) {
		src += "\nfunc _obfuscation_mix_seed(rng: RandomNumberGenerator) -> void:\n\trng.seed = rng.seed ^ _CK\n";
	} else {
		src += "\nfunc _obfuscation_seed_salt() -> int:\n\treturn _CK\n";
	}
	return src;
}

PackedInt64Array ObfuscationScriptSeed::extract(const String &p_source) {
	PackedInt64Array out;
	int from = 0;
	while (true) {
		int p = p_source.find("const _CK", from);
		if (p < 0) {
			p = p_source.find("local _CK", from);
		}
		if (p < 0) {
			break;
		}
		int hex = p_source.find("0x", p);
		if (hex < 0) {
			break;
		}
		int end = hex + 2;
		while (end < p_source.length()) {
			char32_t c = p_source[end];
			if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
				end++;
			} else {
				break;
			}
		}
		String num = p_source.substr(hex + 2, end - (hex + 2));
		if (!num.is_empty()) {
			out.push_back(num.hex_to_int());
		}
		from = end;
	}
	return out;
}
