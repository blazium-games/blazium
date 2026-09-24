/**************************************************************************/
/*  source_map.h                                                          */
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

#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/variant/variant.h"

class ObfuscationSourceMap {
public:
	enum ScriptLang {
		SCRIPT_GDSCRIPT,
		SCRIPT_LUAU,
	};

	static bool is_reserved(const String &p_name);
	static bool is_script_ext(const String &p_ext);
	static ScriptLang script_lang_from_path(const String &p_path);
	static bool should_collect_name(const String &p_name);
	static void collect_identifiers(const String &p_source, HashSet<String> &r_out);
	static void collect_split(const String &p_source, HashSet<String> &r_funcs, HashSet<String> &r_vars);
	static void collect_split(const String &p_source, HashSet<String> &r_funcs, HashSet<String> &r_vars, ScriptLang p_lang);
	static void collect_scene_names(const String &p_text, HashSet<String> &r_out);
	static HashMap<String, String> map_identifiers(const PackedByteArray &p_hmac_key, const HashSet<String> &p_names);
	static String rewrite_node_path(const String &p_path, const HashMap<String, String> &p_idents);
	static String rewrite_script(const String &p_source, const HashMap<String, String> &p_idents, const HashSet<String> &p_funcs, const PackedByteArray &p_hmac_key, bool p_scramble_paths);
	static String rewrite_script(const String &p_source, const HashMap<String, String> &p_idents, const HashSet<String> &p_funcs, const HashSet<String> &p_scene_names, const PackedByteArray &p_hmac_key, bool p_scramble_paths, ScriptLang p_lang = SCRIPT_GDSCRIPT);
	static String rewrite_paths(const String &p_text, const PackedByteArray &p_hmac_key);
	static String rewrite_scene(const String &p_text, const HashMap<String, String> &p_idents, const PackedByteArray &p_hmac_key);
	static bool keep_original_path(const String &p_logical);
	static bool skip_relative_path(const String &p_rel);
	static void list_files(const String &p_dir, Vector<String> &r_files);
	static int gdscript_preamble_end(const String &p_source);
	static String insert_after_preamble(const String &p_source, const String &p_block);
	static String insert_before_chunk_return(const String &p_source, const String &p_block);
	static String strip_minify(const String &p_source, ScriptLang p_lang = SCRIPT_GDSCRIPT);
};
