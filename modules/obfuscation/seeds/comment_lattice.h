/**************************************************************************/
/*  comment_lattice.h                                                     */
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

#include "seeds/source_map.h"

#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/variant/dictionary.h"

class ObfuscationCommentLattice {
public:
	static bool is_lattice_line(const String &p_line);
	static String prefix(ObfuscationSourceMap::ScriptLang p_lang);
	static String mac8(const PackedByteArray &p_hmac_key, const String &p_packed_path, const String &p_slot, const String &p_payload);
	static String encode_payload(char32_t p_tag, const String &p_value);
	static bool decode_payload(const String &p_payload, char32_t &r_tag, String &r_value);
	static String encode_line(const PackedByteArray &p_hmac_key, const String &p_packed_path, const String &p_slot, char32_t p_tag, const String &p_value, ObfuscationSourceMap::ScriptLang p_lang);
	static Dictionary parse(const String &p_source, const String &p_packed_path, const PackedByteArray &p_hmac_key);
	static Dictionary decode_unverified(const String &p_source);
	static Vector<String> packed_path_candidates(const String &p_path);
	static Dictionary parse_with_fallbacks(const String &p_source, const String &p_path, const PackedByteArray &p_hmac_key);
	static Variant comment_ref(const String &p_source, const String &p_packed_path, const String &p_slot, const PackedByteArray &p_hmac_key);
	static String inject_helpers(const String &p_source, ObfuscationSourceMap::ScriptLang p_lang, bool p_seed, bool p_copyright);
	static String apply_cref(const String &p_source, const PackedByteArray &p_hmac_key, const String &p_packed_path, ObfuscationSourceMap::ScriptLang p_lang, const HashMap<String, String> &p_idents, const HashSet<String> &p_funcs, const HashSet<String> &p_scene_names, Vector<String> &r_lines);
	static String make_decoy(uint64_t p_seed, int p_index, ObfuscationSourceMap::ScriptLang p_lang);
	static String append_lines(const String &p_source, const Vector<String> &p_lines);
	static void collect_lines(const String &p_source, Vector<String> &r_lines);
	static void scatter_copies(HashMap<String, String> &r_packed_to_source, const PackedByteArray &p_hmac_key, int p_copies);
};
