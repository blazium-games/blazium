/**************************************************************************/
/*  test_semantic_bm25_index.cpp                                          */
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

#ifdef TESTS_ENABLED

#include "../semantic_bm25_index.h"
#include "tests/test_macros.h"

void test_semantic_bm25_index_idf() {
	SemanticBM25Index index;
	Vector<String> hero_tokens;
	hero_tokens.push_back("hero");
	hero_tokens.push_back("hero");
	index.upsert_document("res://hero.tscn", hero_tokens);

	Vector<String> tree_tokens;
	tree_tokens.push_back("tree");
	index.upsert_document("res://tree.tscn", tree_tokens);

	Vector<String> query;
	query.push_back("hero");
	Array results = index.search(query, 5);
	CHECK(results.size() >= 1);
	CHECK(String(Dictionary(results[0]).get("path", "")) == "res://hero.tscn");
	CHECK(double(Dictionary(results[0]).get("score", 0.0)) > 0.0);
}

void test_semantic_bm25_index_search_ranking() {
	SemanticBM25Index index;
	Vector<String> alpha_tokens;
	alpha_tokens.push_back("alpha");
	alpha_tokens.push_back("beta");
	index.upsert_document("res://alpha.tscn", alpha_tokens);

	Vector<String> beta_tokens;
	beta_tokens.push_back("beta");
	index.upsert_document("res://beta.tscn", beta_tokens);

	Vector<String> query;
	query.push_back("beta");
	Array results = index.search(query, 5);
	CHECK(results.size() >= 2);
	CHECK(String(Dictionary(results[0]).get("path", "")) == "res://beta.tscn");
}

#endif
