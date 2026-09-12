/**************************************************************************/
/*  test_semantic_vector_index.cpp                                        */
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

#include "test_semantic_vector_index.h"

#include "../hash_vector_embedding.h"
#include "../semantic_vector_index.h"

void test_semantic_vector_index_incremental_ivf() {
	SemanticVectorIndex index;
	Vector<String> tokens;
	tokens.push_back("alpha");
	const Vector<double> embedding_a = HashVectorEmbedding::from_tokens(tokens);

	for (int i = 0; i < 520; i++) {
		Vector<String> item_tokens;
		item_tokens.push_back("item-" + String::num_int64(i));
		index.upsert("res://items/item_" + String::num_int64(i) + ".tscn", HashVectorEmbedding::from_tokens(item_tokens));
	}

	Array results = index.search_top_k(embedding_a, 5);
	CHECK(results.size() > 0);

	index.upsert("res://items/item_0.tscn", embedding_a);
	index.remove("res://items/item_1.tscn");

	Array after_mutation = index.search_top_k(embedding_a, 5);
	CHECK(after_mutation.size() > 0);
}
