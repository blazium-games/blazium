/**************************************************************************/
/*  semantic_vector_index.h                                               */
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

#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/templates/vector.h"
#include "core/variant/variant.h"

class SemanticVectorIndex {
	struct IndexedVector {
		String path;
		Vector<double> embedding;
	};

	Vector<IndexedVector> vectors;
	HashMap<String, int> path_to_index;

	static const int IVF_THRESHOLD = 512;
	static const int IVF_BUCKET_COUNT = 64;

	bool ivf_enabled = false;
	HashMap<int, Vector<int>> ivf_buckets;

	void _rebuild_ivf();
	void _ensure_ivf_enabled();
	void _add_to_ivf_bucket(int p_index);
	void _remove_from_ivf_buckets(int p_index);
	void _replace_ivf_index(int p_old_index, int p_new_index);
	int _ivf_bucket_for_vector(const Vector<double> &p_embedding) const;
	int _ivf_bucket_for_query(const Vector<double> &p_query) const;
	Vector<int> _ivf_candidate_indices(const Vector<double> &p_query) const;

public:
	void clear();
	void upsert(const String &p_path, const Vector<double> &p_embedding);
	void remove(const String &p_path);
	Array search_top_k(
			const Vector<double> &p_query,
			int p_limit,
			const HashSet<String> &p_exclude_paths = HashSet<String>(),
			const HashSet<String> *p_allowed_paths = nullptr) const;
	int size() const { return vectors.size(); }
};
