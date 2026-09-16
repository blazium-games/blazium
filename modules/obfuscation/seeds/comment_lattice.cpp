/**************************************************************************/
/*  comment_lattice.cpp                                                   */
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

#include "seeds/comment_lattice.h"
#include "seeds/script_seed.h"

#include "core/crypto/crypto.h"
#include "core/crypto/hashing_context.h"
#include "core/math/random_pcg.h"
#include "core/templates/vector.h"

static const char32_t kTagSep = 0x1f;

bool ObfuscationCommentLattice::is_lattice_line(const String &p_line) {
	const String t = p_line.strip_edges();
	return t.begins_with("# ~ ") || t.begins_with("-- ~ ");
}

String ObfuscationCommentLattice::prefix(ObfuscationSourceMap::ScriptLang p_lang) {
	return p_lang == ObfuscationSourceMap::SCRIPT_LUAU ? String("-- ~ ") : String("# ~ ");
}

static PackedByteArray _hmac_sha256(const PackedByteArray &p_key, const PackedByteArray &p_msg) {
	Ref<Crypto> crypto = Crypto::create();
	PackedByteArray key = p_key;
	if (key.is_empty()) {
		key.resize(32);
	}
	if (crypto.is_null()) {
		PackedByteArray empty;
		empty.resize(32);
		return empty;
	}
	return crypto->hmac_digest(HashingContext::HASH_SHA256, key, p_msg);
}

String ObfuscationCommentLattice::mac8(const PackedByteArray &p_hmac_key, const String &p_packed_path, const String &p_slot, const String &p_payload) {
	const PackedByteArray digest = _hmac_sha256(p_hmac_key, String(p_packed_path + ":" + p_slot + ":" + p_payload).to_utf8_buffer());
	if (digest.size() < 4) {
		return "00000000";
	}
	return String::hex_encode_buffer(digest.ptr(), 4);
}

String ObfuscationCommentLattice::encode_payload(char32_t p_tag, const String &p_value) {
	PackedByteArray raw;
	raw.push_back((uint8_t)p_tag);
	raw.push_back((uint8_t)kTagSep);
	const PackedByteArray utf = p_value.to_utf8_buffer();
	raw.append_array(utf);
	return String::hex_encode_buffer(raw.ptr(), raw.size());
}

bool ObfuscationCommentLattice::decode_payload(const String &p_payload, char32_t &r_tag, String &r_value) {
	const String hex = p_payload.strip_edges();
	if (hex.is_empty() || (hex.length() % 2) != 0) {
		return false;
	}
	const Vector<uint8_t> raw = hex.hex_decode();
	if (raw.size() < 2 || raw[1] != (uint8_t)kTagSep) {
		return false;
	}
	r_tag = (char32_t)raw[0];
	r_value = String::utf8((const char *)raw.ptr() + 2, raw.size() - 2);
	return true;
}

String ObfuscationCommentLattice::encode_line(const PackedByteArray &p_hmac_key, const String &p_packed_path, const String &p_slot, char32_t p_tag, const String &p_value, ObfuscationSourceMap::ScriptLang p_lang) {
	const String payload = encode_payload(p_tag, p_value);
	return prefix(p_lang) + p_slot + " " + mac8(p_hmac_key, p_packed_path, p_slot, payload) + payload;
}

static bool _parse_lattice_rest(const String &p_rest, String &r_slot, String &r_mac, String &r_payload) {
	const int sp = p_rest.find(" ");
	if (sp <= 0) {
		return false;
	}
	r_slot = p_rest.substr(0, sp);
	const String macpay = p_rest.substr(sp + 1).strip_edges();
	if (macpay.length() < 8) {
		return false;
	}
	r_mac = macpay.substr(0, 8).to_lower();
	r_payload = macpay.substr(8);
	return true;
}

static Variant _value_from_tag(char32_t p_tag, const String &p_value) {
	if (p_tag == 'k' || p_tag == 'i') {
		return p_value.is_valid_int() ? Variant(p_value.to_int()) : Variant(p_value);
	}
	return Variant(p_value);
}

Dictionary ObfuscationCommentLattice::parse(const String &p_source, const String &p_packed_path, const PackedByteArray &p_hmac_key) {
	Dictionary out;
	const PackedStringArray lines = p_source.split("\n");
	for (int i = 0; i < lines.size(); i++) {
		String t = lines[i].strip_edges();
		if (t.ends_with("\r")) {
			t = t.substr(0, t.length() - 1);
		}
		String rest;
		if (t.begins_with("# ~ ")) {
			rest = t.substr(4);
		} else if (t.begins_with("-- ~ ")) {
			rest = t.substr(5);
		} else {
			continue;
		}
		String slot, mac, payload;
		if (!_parse_lattice_rest(rest, slot, mac, payload)) {
			continue;
		}
		if (mac8(p_hmac_key, p_packed_path, slot, payload).to_lower() != mac) {
			continue;
		}
		char32_t tag = 0;
		String value;
		if (!decode_payload(payload, tag, value)) {
			continue;
		}
		out[slot] = _value_from_tag(tag, value);
	}
	return out;
}

Dictionary ObfuscationCommentLattice::decode_unverified(const String &p_source) {
	Dictionary out;
	const PackedStringArray lines = p_source.split("\n");
	for (int i = 0; i < lines.size(); i++) {
		String t = lines[i].strip_edges();
		String rest;
		if (t.begins_with("# ~ ")) {
			rest = t.substr(4);
		} else if (t.begins_with("-- ~ ")) {
			rest = t.substr(5);
		} else {
			continue;
		}
		String slot, mac, payload;
		if (!_parse_lattice_rest(rest, slot, mac, payload)) {
			continue;
		}
		char32_t tag = 0;
		String value;
		if (!decode_payload(payload, tag, value)) {
			continue;
		}
		if (slot == "k" || slot == "i" || slot == "r") {
			out[slot] = _value_from_tag(tag, value);
		}
	}
	return out;
}

Vector<String> ObfuscationCommentLattice::packed_path_candidates(const String &p_path) {
	Vector<String> out;
	String n = p_path.replace("\\", "/");
	while (n.ends_with("/")) {
		n = n.substr(0, n.length() - 1);
	}
	out.push_back(n);
	if (!n.begins_with("res://")) {
		out.push_back("res://" + n);
		PackedStringArray parts = n.split("/");
		Vector<String> segs;
		for (int i = 0; i < parts.size(); i++) {
			if (!parts[i].is_empty() && parts[i] != "." && parts[i] != "..") {
				segs.push_back(parts[i]);
			}
		}
		if (segs.size() >= 3) {
			out.push_back("res://" + segs[segs.size() - 3] + "/" + segs[segs.size() - 2] + "/" + segs[segs.size() - 1]);
		}
		if (!segs.is_empty()) {
			out.push_back("res://" + segs[segs.size() - 1]);
		}
	}
	return out;
}

Dictionary ObfuscationCommentLattice::parse_with_fallbacks(const String &p_source, const String &p_path, const PackedByteArray &p_hmac_key) {
	Dictionary best;
	const Vector<String> cands = packed_path_candidates(p_path);
	for (int i = 0; i < cands.size(); i++) {
		Dictionary d = parse(p_source, cands[i], p_hmac_key);
		if (d.size() > best.size()) {
			best = d;
		}
	}
	return best;
}

Variant ObfuscationCommentLattice::comment_ref(const String &p_source, const String &p_packed_path, const String &p_slot, const PackedByteArray &p_hmac_key) {
	Dictionary d = parse_with_fallbacks(p_source, p_packed_path, p_hmac_key);
	if (!d.has(p_slot)) {
		return Variant();
	}
	return d[p_slot];
}

static bool _has_helper_def(const String &p_src, const String &p_name) {
	return p_src.contains("func " + p_name) || p_src.contains("function " + p_name);
}

String ObfuscationCommentLattice::inject_helpers(const String &p_source, ObfuscationSourceMap::ScriptLang p_lang, bool p_seed, bool p_copyright) {
	String src = p_source;
	if (p_lang == ObfuscationSourceMap::SCRIPT_LUAU) {
		if (!_has_helper_def(src, "_obfuscation_cref")) {
			src = ObfuscationSourceMap::insert_before_chunk_return(src,
					"local function _obfuscation_cref(self, slot)\n"
					"\tif not Obfuscation:has_identity() then\n"
					"\t\tObfuscation:load_identity()\n"
					"\tend\n"
					"\tlocal sc = nil\n"
					"\tif type(self) == \"table\" and self.get_script then\n"
					"\t\tsc = self:get_script()\n"
					"\tend\n"
					"\tif sc == nil then\n"
					"\t\treturn \"\"\n"
					"\tend\n"
					"\treturn Obfuscation:comment_ref(sc.source_code, sc.resource_path, slot)\n"
					"end");
		}
		if (p_seed && !_has_helper_def(src, "_obfuscation_seed_salt")) {
			src = ObfuscationSourceMap::insert_before_chunk_return(src,
					"local function _obfuscation_seed_salt(self)\n"
					"\treturn tonumber(_obfuscation_cref(self, \"k\")) or 0\n"
					"end");
		}
		if (p_copyright && !_has_helper_def(src, "_obfuscation_cr_mix")) {
			src = ObfuscationSourceMap::insert_before_chunk_return(src,
					"local function _obfuscation_cr_mix(self, v)\n"
					"\treturn v\n"
					"end");
		}
		return src;
	}
	if (!_has_helper_def(src, "_obfuscation_cref")) {
		src += "\nfunc _obfuscation_cref(_s: String) -> Variant:\n"
			   "\tif not Obfuscation.has_identity():\n"
			   "\t\tObfuscation.load_identity()\n"
			   "\tvar _sc = get_script()\n"
			   "\tif _sc == null:\n"
			   "\t\treturn \"\"\n"
			   "\treturn Obfuscation.comment_ref(_sc.source_code, _sc.resource_path, _s)\n";
	}
	if (p_seed && !_has_helper_def(src, "_obfuscation_seed_salt")) {
		src += "\nfunc _obfuscation_seed_salt() -> int:\n"
			   "\treturn int(_obfuscation_cref(\"k\"))\n";
	}
	if (p_seed && src.contains("RandomNumberGenerator") && !_has_helper_def(src, "_obfuscation_mix_seed")) {
		src += "\nfunc _obfuscation_mix_seed(rng: RandomNumberGenerator) -> void:\n"
			   "\trng.seed = rng.seed ^ int(_obfuscation_cref(\"k\"))\n";
	}
	if (p_copyright && !_has_helper_def(src, "_obfuscation_cr_mix")) {
		src += "\nfunc _obfuscation_cr_mix(v: int) -> int:\n"
			   "\treturn v ^ str(_obfuscation_cref(\"r\")).hash() ^ int(_obfuscation_cref(\"i\"))\n";
	}
	return src;
}

static bool _is_ident_start_cl(char32_t c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool _is_ident_cont_cl(char32_t c) {
	return _is_ident_start_cl(c) || (c >= '0' && c <= '9');
}

static int _skip_string_cl(const String &p_src, int p_i) {
	const int n = p_src.length();
	if (p_i >= n) {
		return p_i;
	}
	const char32_t q = p_src[p_i];
	bool triple = false;
	int i = p_i + 1;
	if ((q == '"' || q == '\'') && i + 1 < n && p_src[i] == q && p_src[i + 1] == q) {
		triple = true;
		i += 2;
	}
	while (i < n) {
		if (p_src[i] == '\\' && i + 1 < n) {
			i += 2;
			continue;
		}
		if (triple) {
			if (i + 2 < n && p_src[i] == q && p_src[i + 1] == q && p_src[i + 2] == q) {
				return i + 3;
			}
		} else if (p_src[i] == q) {
			return i + 1;
		} else if (p_src[i] == '\n' && !triple) {
			return i;
		}
		i++;
	}
	return n;
}

static int _skip_luau_long_cl(const String &p_src, int p_i) {
	const int n = p_src.length();
	if (p_i >= n || p_src[p_i] != '[') {
		return p_i;
	}
	int i = p_i + 1;
	int eq = 0;
	while (i < n && p_src[i] == '=') {
		eq++;
		i++;
	}
	if (i >= n || p_src[i] != '[') {
		return p_i + 1;
	}
	i++;
	while (i < n) {
		if (p_src[i] == ']') {
			int j = i + 1;
			int e = 0;
			while (j < n && p_src[j] == '=') {
				e++;
				j++;
			}
			if (e == eq && j < n && p_src[j] == ']') {
				return j + 1;
			}
		}
		i++;
	}
	return n;
}

static int _skip_luau_comment_cl(const String &p_src, int p_i) {
	const int n = p_src.length();
	if (p_i + 1 >= n || p_src[p_i] != '-' || p_src[p_i + 1] != '-') {
		return p_i;
	}
	if (p_i + 3 < n && p_src[p_i + 2] == '[' && (p_src[p_i + 3] == '[' || p_src[p_i + 3] == '=')) {
		return _skip_luau_long_cl(p_src, p_i + 2);
	}
	int i = p_i + 2;
	while (i < n && p_src[i] != '\n') {
		i++;
	}
	return i;
}

static String _read_ident_cl(const String &p_src, int &p_i) {
	const int n = p_src.length();
	const int start = p_i;
	p_i++;
	while (p_i < n && _is_ident_cont_cl(p_src[p_i])) {
		p_i++;
	}
	return p_src.substr(start, p_i - start);
}

static String _slot_for(const PackedByteArray &p_hmac_key, const String &p_packed_path, const String &p_value) {
	const PackedByteArray digest = _hmac_sha256(p_hmac_key, String("slot:" + p_packed_path + ":" + p_value).to_utf8_buffer());
	String hex = String::hex_encode_buffer(digest.ptr(), MIN(digest.size(), 4));
	if (hex.length() < 4) {
		hex = hex.lpad(4, "0");
	}
	String slot = hex.substr(0, 4);
	if (slot == "k" || slot == "i" || slot == "r") {
		slot = "a" + slot.substr(0, 3);
	}
	return slot;
}

static bool _looks_scrambled_res(const String &p_inner) {
	return p_inner.begins_with("res://") && p_inner.find("uid://") < 0;
}

static bool _path_has_target(const String &p_inner, const HashSet<String> &p_targets) {
	PackedStringArray segs = p_inner.replace("\\", "/").split("/");
	for (int i = 0; i < segs.size(); i++) {
		if (p_targets.has(segs[i])) {
			return true;
		}
	}
	return false;
}

String ObfuscationCommentLattice::apply_cref(const String &p_source, const PackedByteArray &p_hmac_key, const String &p_packed_path, ObfuscationSourceMap::ScriptLang p_lang, const HashMap<String, String> &p_idents, const HashSet<String> &p_funcs, const HashSet<String> &p_scene_names, Vector<String> &r_lines) {
	HashSet<String> targets;
	HashSet<String> scene_vals;
	for (const KeyValue<String, String> &E : p_idents) {
		if (p_funcs.has(E.key) || p_scene_names.has(E.key)) {
			targets.insert(E.value);
		}
		if (p_scene_names.has(E.key)) {
			scene_vals.insert(E.value);
		}
	}
	HashMap<String, String> slot_for_value;
	HashSet<String> used_slots;
	used_slots.insert("k");
	used_slots.insert("i");
	used_slots.insert("r");
	auto cref_for = [&](char32_t tag, const String &value) -> String {
		String slot;
		if (slot_for_value.has(value)) {
			slot = slot_for_value[value];
		} else {
			slot = _slot_for(p_hmac_key, p_packed_path, value);
			int n = 0;
			while (used_slots.has(slot)) {
				slot = _slot_for(p_hmac_key, p_packed_path, value + ":" + String::num_int64(n++));
				if (n > 16) {
					slot += String::num_int64(n);
					break;
				}
			}
			used_slots.insert(slot);
			slot_for_value[value] = slot;
			r_lines.push_back(encode_line(p_hmac_key, p_packed_path, slot, tag, value, p_lang));
		}
		if (p_lang == ObfuscationSourceMap::SCRIPT_LUAU) {
			return "_obfuscation_cref(self, \"" + slot + "\")";
		}
		return "_obfuscation_cref(\"" + slot + "\")";
	};

	String out;
	const int n = p_source.length();
	int i = 0;
	while (i < n) {
		const char32_t c = p_source[i];
		if (p_lang == ObfuscationSourceMap::SCRIPT_LUAU && c == '-' && i + 1 < n && p_source[i + 1] == '-') {
			const int start = i;
			i = _skip_luau_comment_cl(p_source, i);
			out += p_source.substr(start, i - start);
			continue;
		}
		if (p_lang != ObfuscationSourceMap::SCRIPT_LUAU && c == '#') {
			const int start = i;
			while (i < n && p_source[i] != '\n') {
				i++;
			}
			out += p_source.substr(start, i - start);
			continue;
		}
		if (p_lang == ObfuscationSourceMap::SCRIPT_LUAU && c == '[' && i + 1 < n && (p_source[i + 1] == '[' || p_source[i + 1] == '=')) {
			const int start = i;
			i = _skip_luau_long_cl(p_source, i);
			out += p_source.substr(start, i - start);
			continue;
		}
		if (p_lang != ObfuscationSourceMap::SCRIPT_LUAU && c == '$' && i + 1 < n && _is_ident_start_cl(p_source[i + 1])) {
			i++;
			const String id = _read_ident_cl(p_source, i);
			if (scene_vals.has(id)) {
				out += "get_node(" + cref_for('n', id) + ")";
			} else {
				out += "$" + id;
			}
			continue;
		}
		if (c == '"' || c == '\'') {
			const int start = i;
			i = _skip_string_cl(p_source, i);
			const String lit = p_source.substr(start, i - start);
			String inner;
			if (lit.length() >= 6 && lit.substr(0, 3) == lit.substr(lit.length() - 3, 3) && (lit[0] == '"' || lit[0] == '\'')) {
				inner = lit.substr(3, lit.length() - 6);
			} else if (lit.length() >= 2) {
				inner = lit.substr(1, lit.length() - 2);
			} else {
				out += lit;
				continue;
			}
			char32_t tag = 0;
			if (targets.has(inner)) {
				tag = 'n';
			} else if (_looks_scrambled_res(inner)) {
				tag = 'p';
			} else if (inner.find("/") >= 0 && _path_has_target(inner, targets)) {
				tag = 'n';
			}
			if (tag != 0) {
				String call = cref_for(tag, inner);
				if (out.ends_with("preload(") || out.ends_with("preload (")) {
					const int plen = out.ends_with("preload (") ? 9 : 8;
					out = out.substr(0, out.length() - plen) + "load(" + call;
				} else {
					out += call;
				}
			} else {
				out += lit;
			}
			continue;
		}
		if (_is_ident_start_cl(c)) {
			out += _read_ident_cl(p_source, i);
			continue;
		}
		out += String::chr(c);
		i++;
	}
	return out;
}

static String _rand_hex(RandomPCG &p_rng, int p_n) {
	static const char *digits = "0123456789abcdef";
	String s;
	for (int i = 0; i < p_n; i++) {
		s += String::chr(digits[p_rng.rand(16)]);
	}
	return s;
}

String ObfuscationCommentLattice::make_decoy(uint64_t p_seed, int p_index, ObfuscationSourceMap::ScriptLang p_lang) {
	RandomPCG rng;
	rng.seed(p_seed ^ (uint64_t(p_index + 1) * 0x9E3779B97F4A7C15ULL));
	String slot = _rand_hex(rng, 4);
	while (slot == "k" || slot == "i" || slot == "r") {
		slot = _rand_hex(rng, 4);
	}
	return prefix(p_lang) + slot + " " + _rand_hex(rng, 8) + _rand_hex(rng, 16 + (int)rng.rand(12) * 2);
}

String ObfuscationCommentLattice::append_lines(const String &p_source, const Vector<String> &p_lines) {
	if (p_lines.is_empty()) {
		return p_source;
	}
	String src = p_source;
	if (!src.is_empty() && !src.ends_with("\n")) {
		src += "\n";
	}
	for (int i = 0; i < p_lines.size(); i++) {
		src += p_lines[i];
		if (!p_lines[i].ends_with("\n")) {
			src += "\n";
		}
	}
	return src;
}

void ObfuscationCommentLattice::collect_lines(const String &p_source, Vector<String> &r_lines) {
	const PackedStringArray lines = p_source.split("\n");
	for (int i = 0; i < lines.size(); i++) {
		if (is_lattice_line(lines[i])) {
			r_lines.push_back(lines[i].strip_edges());
		}
	}
}

void ObfuscationCommentLattice::scatter_copies(HashMap<String, String> &r_packed_to_source, const PackedByteArray &p_hmac_key, int p_copies) {
	if (p_copies <= 0 || r_packed_to_source.is_empty()) {
		return;
	}
	Vector<String> gd_pool;
	Vector<String> luau_pool;
	Vector<String> keys;
	for (const KeyValue<String, String> &E : r_packed_to_source) {
		keys.push_back(E.key);
		Vector<String> lines;
		collect_lines(E.value, lines);
		const bool luau = ObfuscationSourceMap::script_lang_from_path(E.key) == ObfuscationSourceMap::SCRIPT_LUAU;
		for (int i = 0; i < lines.size(); i++) {
			if (luau) {
				luau_pool.push_back(lines[i]);
			} else {
				gd_pool.push_back(lines[i]);
			}
		}
	}
	for (int i = 0; i < keys.size(); i++) {
		const String packed = keys[i];
		const bool luau = ObfuscationSourceMap::script_lang_from_path(packed) == ObfuscationSourceMap::SCRIPT_LUAU;
		const Vector<String> &pool = luau ? luau_pool : gd_pool;
		if (pool.is_empty()) {
			continue;
		}
		RandomPCG rng;
		rng.seed((uint64_t)ObfuscationScriptSeed::derive(p_hmac_key, packed) ^ 0xC0FFEEULL);
		Vector<String> extra;
		for (int n = 0; n < p_copies; n++) {
			extra.push_back(pool[rng.rand(pool.size())]);
		}
		r_packed_to_source[packed] = append_lines(r_packed_to_source[packed], extra);
	}
}
