/**************************************************************************/
/*  lexical_search_engine.h                                               */
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

#include "semantic_index_store.h"

#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/variant/array.h"

class LexicalSearchEngine {
public:
	static Vector<String> tokenize(const String &p_text);
	static double score_query(const Vector<String> &p_query_tokens, const SemanticAssetEntry &p_entry);
	static Array search(
			const HashMap<String, SemanticAssetEntry> &p_entries,
			const HashMap<String, HashSet<String>> &p_token_index,
			const HashMap<String, HashSet<String>> &p_prefix_index,
			const String &p_query,
			int p_limit,
			const HashSet<String> *p_allowed_paths = nullptr);
	static Array find_similar(
			const HashMap<String, SemanticAssetEntry> &p_entries,
			const HashMap<String, HashSet<String>> &p_token_index,
			const HashMap<String, HashSet<String>> &p_prefix_index,
			const String &p_path,
			int p_limit,
			const HashSet<String> *p_allowed_paths = nullptr);
};
