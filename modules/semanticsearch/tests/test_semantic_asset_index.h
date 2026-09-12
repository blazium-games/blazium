/**************************************************************************/
/*  test_semantic_asset_index.h                                           */
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

#include "tests/test_macros.h"

void test_semantic_asset_index();
void test_semantic_asset_index_search();
void test_semantic_backend_factory();
void test_semantic_search_tag_filter();
void test_semantic_index_store_roundtrip();
void test_semantic_rebuild_clear_persist();
void test_lexical_search_engine();
void test_semantic_incremental_save();
void test_semantic_save_dirty_roundtrip();
void test_semantic_search_metadata_prefilter();
void test_semantic_sidecar_stale_after_full_save();
void test_semantic_find_similar_excludes_source();
void test_semantic_search_invalid_path_regex();
void test_semantic_rebuild_derived_indexes();
void test_semantic_provider_refresh_embeddings();
void test_semantic_embedding_invalid_path_regex();
void test_semantic_hybrid_search_rrf();
void test_semantic_index_load_wrong_dim_embedding();
void test_semantic_concurrent_save_and_search();
void test_semantic_http_url_change_refresh();
void test_semantic_filter_regex_parity();
void test_semantic_sidecar_wrong_dim_embedding();
void test_semantic_save_preserves_late_dirty_paths();
void test_semantic_provider_mismatch_gate();
void test_semantic_ngram_embedding_normalized();
void test_semantic_http_embed_timeout();
void test_semantic_http_fallback_provider_tag();
void test_semantic_per_entry_stale_search();
void test_semantic_http_embed_provider_live();
void test_semantic_http_embed_index_search();
void test_semantic_http_embed_assettags_filter();

TEST_CASE("[Modules][SemanticSearch] asset index") {
	test_semantic_asset_index();
}

TEST_CASE("[Modules][SemanticSearch] asset index search") {
	test_semantic_asset_index_search();
}

TEST_CASE("[Modules][SemanticSearch] backend factory") {
	test_semantic_backend_factory();
}

TEST_CASE("[Modules][SemanticSearch] search tag filter") {
	test_semantic_search_tag_filter();
}

TEST_CASE("[Modules][SemanticSearch] index store roundtrip") {
	test_semantic_index_store_roundtrip();
}

TEST_CASE("[Modules][SemanticSearch] rebuild clear persist") {
	test_semantic_rebuild_clear_persist();
}

TEST_CASE("[Modules][SemanticSearch] lexical search engine") {
	test_lexical_search_engine();
}

TEST_CASE("[Modules][SemanticSearch] incremental save") {
	test_semantic_incremental_save();
}

TEST_CASE("[Modules][SemanticSearch] save dirty roundtrip") {
	test_semantic_save_dirty_roundtrip();
}

TEST_CASE("[Modules][SemanticSearch] search metadata prefilter") {
	test_semantic_search_metadata_prefilter();
}

TEST_CASE("[Modules][SemanticSearch] sidecar stale after full save") {
	test_semantic_sidecar_stale_after_full_save();
}

TEST_CASE("[Modules][SemanticSearch] find similar excludes source") {
	test_semantic_find_similar_excludes_source();
}

TEST_CASE("[Modules][SemanticSearch] search invalid path regex") {
	test_semantic_search_invalid_path_regex();
}

TEST_CASE("[Modules][SemanticSearch] rebuild derived indexes") {
	test_semantic_rebuild_derived_indexes();
}

TEST_CASE("[Modules][SemanticSearch] provider refresh embeddings") {
	test_semantic_provider_refresh_embeddings();
}

TEST_CASE("[Modules][SemanticSearch] embedding invalid path regex") {
	test_semantic_embedding_invalid_path_regex();
}

TEST_CASE("[Modules][SemanticSearch] hybrid search rrf") {
	test_semantic_hybrid_search_rrf();
}

TEST_CASE("[Modules][SemanticSearch] index load wrong dim embedding") {
	test_semantic_index_load_wrong_dim_embedding();
}

TEST_CASE("[Modules][SemanticSearch] concurrent save and search") {
	test_semantic_concurrent_save_and_search();
}

TEST_CASE("[Modules][SemanticSearch] http url change refresh") {
	test_semantic_http_url_change_refresh();
}

TEST_CASE("[Modules][SemanticSearch] filter regex parity") {
	test_semantic_filter_regex_parity();
}

TEST_CASE("[Modules][SemanticSearch] sidecar wrong dim embedding") {
	test_semantic_sidecar_wrong_dim_embedding();
}

TEST_CASE("[Modules][SemanticSearch] save preserves late dirty paths") {
	test_semantic_save_preserves_late_dirty_paths();
}

TEST_CASE("[Modules][SemanticSearch] provider mismatch gate") {
	test_semantic_provider_mismatch_gate();
}

TEST_CASE("[Modules][SemanticSearch] ngram embedding normalized") {
	test_semantic_ngram_embedding_normalized();
}

TEST_CASE("[Modules][SemanticSearch] http embed timeout") {
	test_semantic_http_embed_timeout();
}

TEST_CASE("[Modules][SemanticSearch] http fallback provider tag") {
	test_semantic_http_fallback_provider_tag();
}

TEST_CASE("[Modules][SemanticSearch] per entry stale search") {
	test_semantic_per_entry_stale_search();
}

TEST_CASE("[Modules][SemanticSearch] http embed provider live") {
	test_semantic_http_embed_provider_live();
}

TEST_CASE("[Modules][SemanticSearch] http embed index search") {
	test_semantic_http_embed_index_search();
}

TEST_CASE("[Modules][SemanticSearch] http embed assettags filter") {
	test_semantic_http_embed_assettags_filter();
}
