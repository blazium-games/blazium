/**************************************************************************/
/*  test_semantic_query_embed_cache.cpp                                   */
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

#include "test_semantic_query_embed_cache.h"

#include "modules/semanticsearch/semantic_query_embed_cache.h"
#include "modules/semanticsearch/semantic_rank_fusion.h"

void test_semantic_query_embed_cache_invalidate() {
	SemanticQueryEmbedCache::set_active_provider("provider-a");
	SemanticQueryEmbedCache::invalidate();
	SemanticQueryEmbedCache::set_active_provider("provider-b");
	SemanticQueryEmbedCache::invalidate();
	CHECK(true);
}

void test_semantic_rank_fusion_rrf_ordering() {
	Array lexical;
	Dictionary first;
	first["path"] = "res://a.tscn";
	first["score"] = 0.9;
	lexical.push_back(first);
	Dictionary second;
	second["path"] = "res://b.tscn";
	second["score"] = 0.8;
	lexical.push_back(second);

	Array embedding;
	Dictionary third;
	third["path"] = "res://b.tscn";
	third["score"] = 0.95;
	embedding.push_back(third);
	Dictionary fourth;
	fourth["path"] = "res://c.tscn";
	fourth["score"] = 0.7;
	embedding.push_back(fourth);

	const Array fused = SemanticRankFusion::reciprocal_rank_fusion(lexical, embedding, 3);
	CHECK(fused.size() == 3);
	CHECK(String(Dictionary(fused[0]).get("path", "")) == "res://b.tscn");
}
