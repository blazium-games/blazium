/**************************************************************************/
/*  semantic_bm25_index.cpp                                               */
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

#include "semantic_bm25_index.h"

#include "semantic_top_k.h"

#include "core/math/math_funcs.h"

void SemanticBM25Index::clear() {
	document_term_freq.clear();
	document_lengths.clear();
	corpus_term_freq.clear();
	term_postings.clear();
	document_count = 0;
}

void SemanticBM25Index::upsert_document(const String &p_path, const Vector<String> &p_tokens) {
	remove_document(p_path);
	if (p_tokens.is_empty()) {
		return;
	}
	HashMap<String, int> term_freq;
	int length = 0;
	for (int i = 0; i < p_tokens.size(); i++) {
		const String &token = p_tokens[i];
		if (token.is_empty()) {
			continue;
		}
		term_freq[token] = term_freq.has(token) ? term_freq[token] + 1 : 1;
		corpus_term_freq[token] = corpus_term_freq.has(token) ? corpus_term_freq[token] + 1 : 1;
		term_postings[token].insert(p_path);
		length++;
	}
	if (length == 0) {
		return;
	}
	document_term_freq.insert(p_path, term_freq);
	document_lengths.insert(p_path, length);
	document_count++;
}

void SemanticBM25Index::remove_document(const String &p_path) {
	if (!document_term_freq.has(p_path)) {
		return;
	}
	const HashMap<String, int> &term_freq = document_term_freq[p_path];
	for (const KeyValue<String, int> &kv : term_freq) {
		if (corpus_term_freq.has(kv.key)) {
			corpus_term_freq[kv.key] -= kv.value;
			if (corpus_term_freq[kv.key] <= 0) {
				corpus_term_freq.erase(kv.key);
			}
		}
		if (term_postings.has(kv.key)) {
			term_postings[kv.key].erase(p_path);
			if (term_postings[kv.key].is_empty()) {
				term_postings.erase(kv.key);
			}
		}
	}
	document_term_freq.erase(p_path);
	document_lengths.erase(p_path);
	document_count = MAX(0, document_count - 1);
}

Array SemanticBM25Index::search(const Vector<String> &p_query_tokens, int p_limit, const HashSet<String> *p_allowed_paths) const {
	struct ScoredPath {
		String path;
		double score = 0.0;
	};

	const double k1 = 1.2;
	const double b = 0.75;
	double avg_doc_len = 0.0;
	for (const KeyValue<String, int> &kv : document_lengths) {
		avg_doc_len += kv.value;
	}
	if (document_count > 0) {
		avg_doc_len /= document_count;
	}

	HashSet<String> candidate_paths;
	for (int i = 0; i < p_query_tokens.size(); i++) {
		const String &term = p_query_tokens[i];
		if (term.is_empty() || !term_postings.has(term)) {
			continue;
		}
		for (const String &path : term_postings[term]) {
			if (p_allowed_paths && !p_allowed_paths->has(path)) {
				continue;
			}
			candidate_paths.insert(path);
		}
	}

	if (candidate_paths.is_empty()) {
		return Array();
	}

	Vector<ScoredPath> scored;
	scored.resize(candidate_paths.size());
	int write_index = 0;
	for (const String &path : candidate_paths) {
		if (!document_term_freq.has(path)) {
			continue;
		}
		const HashMap<String, int> &doc_terms = document_term_freq[path];
		const int doc_len = document_lengths.has(path) ? document_lengths[path] : 0;
		double score = 0.0;
		for (int i = 0; i < p_query_tokens.size(); i++) {
			const String &term = p_query_tokens[i];
			if (term.is_empty() || !doc_terms.has(term)) {
				continue;
			}
			const int tf = doc_terms[term];
			const int df = term_postings.has(term) ? term_postings[term].size() : 0;
			if (df <= 0) {
				continue;
			}
			const double idf = Math::log((document_count - df + 0.5) / (df + 0.5) + 1.0);
			const double denom = tf + k1 * (1.0 - b + b * (double(doc_len) / MAX(avg_doc_len, 1.0)));
			score += idf * ((tf * (k1 + 1.0)) / MAX(denom, 0.0001));
		}
		if (score > 0.0) {
			ScoredPath item;
			item.path = path;
			item.score = score;
			scored.write[write_index++] = item;
		}
	}
	scored.resize(write_index);

	semantic_partial_sort_top_k(scored, MAX(p_limit, 1), [](const ScoredPath &lhs, const ScoredPath &rhs) {
		return lhs.score > rhs.score;
	});

	Array results;
	for (int i = 0; i < scored.size(); i++) {
		Dictionary item;
		item["path"] = scored[i].path;
		item["score"] = scored[i].score;
		results.push_back(item);
	}
	return results;
}
