/**************************************************************************/
/*  lexical_search_engine.cpp                                             */
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

#include "lexical_search_engine.h"

#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

static const int LEXICAL_PREFIX_MAX_LEN = 16;

static double _score_token_against_entry(const String &p_query, const SemanticAssetEntry &p_entry) {
	double score = 0.0;
	if (p_entry.path.containsn(p_query)) {
		score += 2.0;
	}
	if (p_entry.caption.containsn(p_query)) {
		score += 3.0;
	}
	bool exact_token_match = false;
	for (int j = 0; j < p_entry.tokens.size(); j++) {
		if (p_entry.tokens[j] == p_query) {
			score += 4.0;
			exact_token_match = true;
			break;
		}
	}
	if (!exact_token_match) {
		for (int j = 0; j < p_entry.tokens.size(); j++) {
			if (p_entry.tokens[j].containsn(p_query)) {
				score += 2.0;
				break;
			}
		}
	}
	return score;
}

Vector<String> LexicalSearchEngine::tokenize(const String &p_text) {
	Vector<String> tokens;
	const PackedStringArray parts = p_text.to_lower().split_spaces();
	for (int i = 0; i < parts.size(); i++) {
		const String part = parts[i].strip_edges();
		if (!part.is_empty()) {
			tokens.push_back(part);
		}
	}
	return tokens;
}

double LexicalSearchEngine::score_query(const Vector<String> &p_query_tokens, const SemanticAssetEntry &p_entry) {
	if (p_query_tokens.is_empty()) {
		return 0.0;
	}
	double score = 0.0;
	for (int i = 0; i < p_query_tokens.size(); i++) {
		score += _score_token_against_entry(p_query_tokens[i], p_entry);
	}
	return score;
}

Array LexicalSearchEngine::search(
		const HashMap<String, SemanticAssetEntry> &p_entries,
		const HashMap<String, HashSet<String>> &p_token_index,
		const HashMap<String, HashSet<String>> &p_prefix_index,
		const String &p_query,
		int p_limit,
		const HashSet<String> *p_allowed_paths) {
	const Vector<String> query_tokens = tokenize(p_query);
	if (query_tokens.is_empty()) {
		return Array();
	}

	struct ScoredEntry {
		String path;
		double score;
		bool operator<(const ScoredEntry &p_other) const {
			return score > p_other.score;
		}
	};

	HashSet<String> candidate_paths;
	for (int i = 0; i < query_tokens.size(); i++) {
		if (p_token_index.has(query_tokens[i])) {
			for (const String &path : p_token_index[query_tokens[i]]) {
				candidate_paths.insert(path);
			}
		}
		const int prefix_cap = MIN(query_tokens[i].length(), LEXICAL_PREFIX_MAX_LEN);
		for (int len = prefix_cap; len >= 3; len--) {
			const String prefix = query_tokens[i].substr(0, len);
			if (p_prefix_index.has(prefix)) {
				for (const String &path : p_prefix_index[prefix]) {
					candidate_paths.insert(path);
				}
			}
		}
	}
	if (candidate_paths.is_empty()) {
		Vector<String> sorted_paths;
		for (const KeyValue<String, SemanticAssetEntry> &kv : p_entries) {
			sorted_paths.push_back(kv.key);
		}
		sorted_paths.sort();
		const int fallback_cap = 500;
		for (int i = 0; i < sorted_paths.size() && i < fallback_cap; i++) {
			candidate_paths.insert(sorted_paths[i]);
		}
	}

	Vector<ScoredEntry> scored;
	for (const String &path : candidate_paths) {
		if (p_allowed_paths && !p_allowed_paths->has(path)) {
			continue;
		}
		if (!p_entries.has(path)) {
			continue;
		}
		const double score = score_query(query_tokens, p_entries[path]);
		if (score > 0.0) {
			ScoredEntry item;
			item.path = path;
			item.score = score;
			scored.push_back(item);
		}
	}

	const int limit = MAX(p_limit, 1);
	if (scored.size() > (uint32_t)limit) {
		scored.sort();
		scored.resize(limit);
	} else {
		scored.sort();
	}

	Array results;
	for (int i = 0; i < scored.size(); i++) {
		Dictionary item;
		item["path"] = scored[i].path;
		item["score"] = scored[i].score;
		if (p_entries.has(scored[i].path)) {
			item["caption"] = p_entries[scored[i].path].caption;
		}
		results.push_back(item);
	}
	return results;
}

Array LexicalSearchEngine::find_similar(
		const HashMap<String, SemanticAssetEntry> &p_entries,
		const HashMap<String, HashSet<String>> &p_token_index,
		const HashMap<String, HashSet<String>> &p_prefix_index,
		const String &p_path,
		int p_limit,
		const HashSet<String> *p_allowed_paths) {
	if (!p_entries.has(p_path)) {
		return Array();
	}
	const SemanticAssetEntry &source = p_entries[p_path];
	const Vector<String> query_tokens = source.tokens;
	if (query_tokens.is_empty()) {
		return Array();
	}

	struct ScoredEntry {
		String path;
		double score;
		bool operator<(const ScoredEntry &p_other) const {
			return score > p_other.score;
		}
	};

	HashSet<String> candidate_paths;
	for (int i = 0; i < query_tokens.size(); i++) {
		if (p_token_index.has(query_tokens[i])) {
			for (const String &path : p_token_index[query_tokens[i]]) {
				if (path != p_path) {
					candidate_paths.insert(path);
				}
			}
		}
		const int prefix_cap = MIN(query_tokens[i].length(), LEXICAL_PREFIX_MAX_LEN);
		for (int len = prefix_cap; len >= 3; len--) {
			const String prefix = query_tokens[i].substr(0, len);
			if (p_prefix_index.has(prefix)) {
				for (const String &path : p_prefix_index[prefix]) {
					if (path != p_path) {
						candidate_paths.insert(path);
					}
				}
			}
		}
	}
	if (candidate_paths.is_empty()) {
		Vector<String> sorted_paths;
		for (const KeyValue<String, SemanticAssetEntry> &kv : p_entries) {
			if (kv.key != p_path) {
				sorted_paths.push_back(kv.key);
			}
		}
		sorted_paths.sort();
		const int fallback_cap = 500;
		for (int i = 0; i < sorted_paths.size() && i < fallback_cap; i++) {
			candidate_paths.insert(sorted_paths[i]);
		}
	}

	Vector<ScoredEntry> scored;
	for (const String &path : candidate_paths) {
		if (path == p_path) {
			continue;
		}
		if (p_allowed_paths && !p_allowed_paths->has(path)) {
			continue;
		}
		if (!p_entries.has(path)) {
			continue;
		}
		const double score = score_query(query_tokens, p_entries[path]);
		if (score > 0.0) {
			ScoredEntry item;
			item.path = path;
			item.score = score;
			scored.push_back(item);
		}
	}
	scored.sort();

	Array results;
	const int limit = MAX(p_limit, 1);
	for (int i = 0; i < scored.size() && i < limit; i++) {
		if (scored[i].path == p_path) {
			continue;
		}
		Dictionary item;
		item["path"] = scored[i].path;
		item["score"] = scored[i].score;
		item["caption"] = p_entries[scored[i].path].caption;
		results.push_back(item);
	}
	return results;
}
