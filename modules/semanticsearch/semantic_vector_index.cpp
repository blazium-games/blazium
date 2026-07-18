/**************************************************************************/
/*  semantic_vector_index.cpp                                             */
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

#include "semantic_vector_index.h"

#include "hash_vector_embedding.h"
#include "semantic_top_k.h"

void SemanticVectorIndex::_rebuild_ivf() {
	ivf_buckets.clear();
	ivf_enabled = vectors.size() >= IVF_THRESHOLD;
	if (!ivf_enabled) {
		return;
	}
	for (int i = 0; i < vectors.size(); i++) {
		const int bucket = _ivf_bucket_for_vector(vectors[i].embedding);
		if (!ivf_buckets.has(bucket)) {
			ivf_buckets.insert(bucket, Vector<int>());
		}
		ivf_buckets[bucket].push_back(i);
	}
}

void SemanticVectorIndex::_ensure_ivf_enabled() {
	const bool should_enable = vectors.size() >= IVF_THRESHOLD;
	if (should_enable && !ivf_enabled) {
		_rebuild_ivf();
		return;
	}
	if (!should_enable && ivf_enabled) {
		ivf_enabled = false;
		ivf_buckets.clear();
	}
}

void SemanticVectorIndex::_add_to_ivf_bucket(int p_index) {
	if (!ivf_enabled || p_index < 0 || p_index >= vectors.size()) {
		return;
	}
	const int bucket = _ivf_bucket_for_vector(vectors[p_index].embedding);
	if (!ivf_buckets.has(bucket)) {
		ivf_buckets.insert(bucket, Vector<int>());
	}
	ivf_buckets[bucket].push_back(p_index);
}

void SemanticVectorIndex::_remove_from_ivf_buckets(int p_index) {
	if (!ivf_enabled || p_index < 0 || p_index >= vectors.size()) {
		return;
	}
	const int bucket = _ivf_bucket_for_vector(vectors[p_index].embedding);
	if (!ivf_buckets.has(bucket)) {
		return;
	}
	Vector<int> &indices = ivf_buckets[bucket];
	for (int i = 0; i < indices.size(); i++) {
		if (indices[i] == p_index) {
			indices.remove_at(i);
			break;
		}
	}
}

void SemanticVectorIndex::_replace_ivf_index(int p_old_index, int p_new_index) {
	if (!ivf_enabled) {
		return;
	}
	_remove_from_ivf_buckets(p_old_index);
	_add_to_ivf_bucket(p_new_index);
}

int SemanticVectorIndex::_ivf_bucket_for_vector(const Vector<double> &p_embedding) const {
	if (p_embedding.is_empty()) {
		return 0;
	}
	uint32_t hash = 2166136261u;
	const int dims = MIN(p_embedding.size(), 8);
	for (int i = 0; i < dims; i++) {
		const int quantized = p_embedding[i] >= 0.0 ? 1 : 0;
		hash ^= (uint32_t)quantized;
		hash *= 16777619u;
	}
	return int(hash % IVF_BUCKET_COUNT);
}

int SemanticVectorIndex::_ivf_bucket_for_query(const Vector<double> &p_query) const {
	return _ivf_bucket_for_vector(p_query);
}

Vector<int> SemanticVectorIndex::_ivf_candidate_indices(const Vector<double> &p_query) const {
	Vector<int> candidates;
	if (!ivf_enabled) {
		candidates.resize(vectors.size());
		for (int i = 0; i < vectors.size(); i++) {
			candidates.write[i] = i;
		}
		return candidates;
	}

	const int primary = _ivf_bucket_for_query(p_query);
	for (int offset = -1; offset <= 1; offset++) {
		int bucket = primary + offset;
		if (bucket < 0) {
			bucket += IVF_BUCKET_COUNT;
		} else if (bucket >= IVF_BUCKET_COUNT) {
			bucket -= IVF_BUCKET_COUNT;
		}
		if (ivf_buckets.has(bucket)) {
			const Vector<int> &bucket_indices = ivf_buckets[bucket];
			for (int i = 0; i < bucket_indices.size(); i++) {
				candidates.push_back(bucket_indices[i]);
			}
		}
	}
	if (candidates.is_empty()) {
		candidates.resize(vectors.size());
		for (int i = 0; i < vectors.size(); i++) {
			candidates.write[i] = i;
		}
	}
	return candidates;
}

void SemanticVectorIndex::clear() {
	vectors.clear();
	path_to_index.clear();
	ivf_buckets.clear();
	ivf_enabled = false;
}

void SemanticVectorIndex::upsert(const String &p_path, const Vector<double> &p_embedding) {
	if (path_to_index.has(p_path)) {
		const int index = path_to_index[p_path];
		_remove_from_ivf_buckets(index);
		vectors.write[index].embedding = p_embedding;
		_add_to_ivf_bucket(index);
		return;
	}
	IndexedVector entry;
	entry.path = p_path;
	entry.embedding = p_embedding;
	path_to_index[p_path] = vectors.size();
	vectors.push_back(entry);
	_ensure_ivf_enabled();
	if (ivf_enabled) {
		_add_to_ivf_bucket(vectors.size() - 1);
	}
}

void SemanticVectorIndex::remove(const String &p_path) {
	if (!path_to_index.has(p_path)) {
		return;
	}
	const int remove_index = path_to_index[p_path];
	_remove_from_ivf_buckets(remove_index);
	const String moved_path = vectors[vectors.size() - 1].path;
	const int moved_from = vectors.size() - 1;
	vectors.write[remove_index] = vectors[moved_from];
	path_to_index[moved_path] = remove_index;
	vectors.remove_at(vectors.size() - 1);
	path_to_index.erase(p_path);
	if (remove_index != moved_from) {
		_replace_ivf_index(moved_from, remove_index);
	}
	_ensure_ivf_enabled();
}

Array SemanticVectorIndex::search_top_k(
		const Vector<double> &p_query,
		int p_limit,
		const HashSet<String> &p_exclude_paths,
		const HashSet<String> *p_allowed_paths) const {
	struct ScoredPath {
		String path;
		double score = 0.0;
	};

	Vector<ScoredPath> scored;
	const Vector<int> candidates = _ivf_candidate_indices(p_query);
	for (int ci = 0; ci < candidates.size(); ci++) {
		const int i = candidates[ci];
		if (i < 0 || i >= vectors.size()) {
			continue;
		}
		const String path = vectors[i].path;
		if (p_exclude_paths.has(path)) {
			continue;
		}
		if (p_allowed_paths && !p_allowed_paths->is_empty() && !p_allowed_paths->has(path)) {
			continue;
		}
		const double similarity = HashVectorEmbedding::cosine_similarity(p_query, vectors[i].embedding);

		ScoredPath item;
		item.path = path;
		item.score = similarity;
		scored.push_back(item);
	}

	semantic_partial_sort_top_k(scored, MAX(p_limit, 1), [](const ScoredPath &a, const ScoredPath &b) {
		return a.score > b.score;
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
