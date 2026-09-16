/**************************************************************************/
/*  copyright_lattice.cpp                                                 */
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

#include "seeds/copyright_lattice.h"
#include "seeds/source_map.h"

#include "core/crypto/crypto_core.h"

String ObfuscationCopyrightLattice::canonical(const String &p_author, const String &p_license, const String &p_text) {
	PackedStringArray parts;
	if (!p_author.is_empty()) {
		parts.push_back(p_author);
	}
	if (!p_license.is_empty()) {
		parts.push_back(p_license);
	}
	if (!p_text.is_empty()) {
		parts.push_back(p_text);
	}
	return String("|").join(parts);
}

String ObfuscationCopyrightLattice::to_base64(const String &p_canonical) {
	PackedByteArray utf = p_canonical.to_utf8_buffer();
	if (utf.is_empty()) {
		return String();
	}
	return CryptoCore::b64_encode_str(utf.ptr(), utf.size());
}

PackedStringArray ObfuscationCopyrightLattice::split_shards(const String &p_b64, int p_count) {
	PackedStringArray out;
	int n = MAX(1, p_count);
	String src = p_b64;
	if (src.is_empty()) {
		src = "AA";
	}
	while (src.length() < n) {
		src += src;
	}
	int base = src.length() / n;
	int extra = src.length() % n;
	int pos = 0;
	for (int i = 0; i < n; i++) {
		int take = base + (i < extra ? 1 : 0);
		if (take < 1) {
			take = 1;
		}
		out.push_back(src.substr(pos, take));
		pos += take;
		if (pos >= src.length()) {
			pos = 0;
		}
	}
	return out;
}

String ObfuscationCopyrightLattice::inject(const String &p_source, int p_index, const String &p_shard, bool p_luau) {
	String ci = p_luau ? vformat("local _CI = %d", p_index) : vformat("const _CI := %d", p_index);
	String cr = p_luau ? vformat("local _CR = \"%s\"", p_shard.c_escape()) : vformat("const _CR := \"%s\"", p_shard.c_escape());
	String src = p_source;
	int p = src.find("const _CI");
	if (p < 0) {
		p = src.find("local _CI");
	}
	if (p >= 0) {
		int eol = src.find("\n", p);
		if (eol < 0) {
			eol = src.length();
		}
		src = src.substr(0, p) + ci + src.substr(eol);
	} else {
		src = ObfuscationSourceMap::insert_after_preamble(src, ci);
	}
	p = src.find("const _CR");
	if (p < 0) {
		p = src.find("local _CR");
	}
	if (p >= 0) {
		int eol = src.find("\n", p);
		if (eol < 0) {
			eol = src.length();
		}
		src = src.substr(0, p) + cr + src.substr(eol);
	} else {
		src = ObfuscationSourceMap::insert_after_preamble(src, cr);
	}
	if (!src.contains("_obfuscation_cr_mix")) {
		if (p_luau) {
			src = ObfuscationSourceMap::insert_before_chunk_return(src, "local function _obfuscation_cr_mix(v)\n\treturn v\nend");
		} else {
			src += "\nfunc _obfuscation_cr_mix(v: int) -> int:\n\treturn v ^ _CR.hash() ^ _CI\n";
		}
	}
	return src;
}

Dictionary ObfuscationCopyrightLattice::collect(const String &p_source) {
	Dictionary d;
	int ci = -1;
	int p = p_source.find("const _CI");
	if (p < 0) {
		p = p_source.find("local _CI");
	}
	if (p >= 0) {
		int eq = p_source.find(":=", p);
		if (eq < 0 || eq > p + 16) {
			eq = p_source.find("=", p);
		}
		if (eq >= 0) {
			int start = eq + (p_source.substr(eq, 2) == ":=" ? 2 : 1);
			ci = p_source.substr(start, 12).strip_edges().to_int();
		}
	}
	String shard;
	p = p_source.find("const _CR");
	if (p < 0) {
		p = p_source.find("local _CR");
	}
	if (p >= 0) {
		int q1 = p_source.find("\"", p);
		int q2 = q1 >= 0 ? p_source.find("\"", q1 + 1) : -1;
		if (q1 >= 0 && q2 > q1) {
			shard = p_source.substr(q1 + 1, q2 - q1 - 1);
		}
	}
	if (ci >= 0 && !shard.is_empty()) {
		d["index"] = ci;
		d["shard"] = shard;
	}
	return d;
}

String ObfuscationCopyrightLattice::reconstruct(const Dictionary &p_index_to_shard, int p_count) {
	String out;
	for (int i = 0; i < p_count; i++) {
		if (p_index_to_shard.has(i)) {
			out += String(p_index_to_shard[i]);
		}
	}
	return out;
}
