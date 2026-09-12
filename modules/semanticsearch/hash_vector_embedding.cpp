/**************************************************************************/
/*  hash_vector_embedding.cpp                                             */
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

#include "hash_vector_embedding.h"

#include "core/math/math_funcs.h"
#include "core/templates/hashfuncs.h"

Vector<double> HashVectorEmbedding::from_tokens(const Vector<String> &p_tokens, int p_dim) {
	Vector<double> vector;
	vector.resize(p_dim);
	for (int i = 0; i < p_dim; i++) {
		vector.write[i] = 0.0;
	}
	for (int i = 0; i < p_tokens.size(); i++) {
		const uint32_t hash = p_tokens[i].hash();
		const int idx = int(hash % uint32_t(p_dim));
		const double sign = (hash & 1) ? 1.0 : -1.0;
		vector.write[idx] += sign;
	}
	double norm = 0.0;
	for (int i = 0; i < p_dim; i++) {
		norm += vector[i] * vector[i];
	}
	norm = Math::sqrt(norm);
	if (norm > 0.0) {
		for (int i = 0; i < p_dim; i++) {
			vector.write[i] /= norm;
		}
	}
	return vector;
}

double HashVectorEmbedding::cosine_similarity(const Vector<double> &p_left, const Vector<double> &p_right) {
	if (p_left.is_empty() || p_right.is_empty()) {
		return 0.0;
	}
	double dot = 0.0;
	const int count = MIN(p_left.size(), p_right.size());
	for (int i = 0; i < count; i++) {
		dot += p_left[i] * p_right[i];
	}

	return CLAMP(dot, -1.0, 1.0);
}

bool HashVectorEmbedding::is_valid_embedding_dim(int p_size) {
	return p_size == DEFAULT_DIM;
}
