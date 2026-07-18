/**************************************************************************/
/*  embedding_backend.cpp                                                 */
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

#include "embedding_backend.h"

#include "lexical_search_engine.h"
#include "lexical_tag_backend.h"
#include "semantic_asset_index.h"
#include "semantic_index_store.h"
#include "semantic_query_embed_cache.h"
#include "semantic_rank_fusion.h"
#include "semantic_search_backend_factory.h"
#include "semantic_search_filters.h"

#include "core/config/project_settings.h"
#include "embedding_provider.h"
#include "hash_vector_embedding.h"

void EmbeddingBackend::_bind_methods() {}

static Vector<double> _token_vector(const Vector<String> &p_tokens) {
	return SemanticQueryEmbedCache::get_or_embed(p_tokens);
}

static Vector<double> _entry_embedding_vector(SemanticAssetIndex *p_index, const String &p_path) {
	Vector<double> entry_vec;
	if (!p_index) {
		return entry_vec;
	}
	const Dictionary entry = p_index->get_asset_entry(p_path);
	if (!entry.get("ok", false)) {
		return entry_vec;
	}
	const String active_provider = SemanticSearchBackendFactory::get_embedding_provider_name();
	const String stored_provider = String(entry.get("embedding_provider", ""));
	const Array stored_embedding = entry.get("embedding_vector", Array());
	if (!stored_embedding.is_empty() && (stored_provider.is_empty() || stored_provider == active_provider)) {
		for (int j = 0; j < stored_embedding.size(); j++) {
			entry_vec.push_back(double(stored_embedding[j]));
		}
		return entry_vec;
	}
	Array tokens_array = entry.get("tokens", Array());
	Vector<String> entry_tokens;
	for (int j = 0; j < tokens_array.size(); j++) {
		entry_tokens.push_back(String(tokens_array[j]));
	}
	return _token_vector(entry_tokens);
}

static Array _vector_ann_search(
		SemanticAssetIndex *p_index,
		const Vector<double> &p_query_vec,
		int p_limit,
		const HashSet<String> &p_allowed_paths,
		const HashSet<String> &p_exclude_paths) {
	if (!p_index || p_query_vec.is_empty()) {
		return Array();
	}
	return p_index->vector_search_top_k(p_query_vec, p_limit * 2, p_allowed_paths, p_exclude_paths);
}

static Array _lexical_leg_search(
		SemanticAssetIndex *p_index,
		const String &p_query,
		int p_limit,
		const HashSet<String> &p_allowed_paths,
		const PackedStringArray &p_tags,
		bool p_require_all,
		const String &p_path_regex,
		const String &p_class_filter) {
	if (!p_index) {
		LexicalTagBackend lexical;
		return lexical.search_with_filters(p_query, p_limit, p_tags, p_require_all, p_path_regex, p_class_filter);
	}
	const HashSet<String> *allowed_ptr = p_allowed_paths.is_empty() ? nullptr : &p_allowed_paths;
	Array lexical_results = p_index->bm25_search(LexicalSearchEngine::tokenize(p_query), p_limit, allowed_ptr);
	if (lexical_results.is_empty()) {
		lexical_results = p_index->search_lexical_with_filters(p_query, p_limit, p_tags, p_require_all, p_path_regex, p_class_filter);
	}
	return lexical_results;
}

Error EmbeddingBackend::rebuild() {
	LexicalTagBackend lexical;
	return lexical.rebuild();
}

Array EmbeddingBackend::search(const String &p_query, int p_limit) const {
	return search_with_filters(p_query, p_limit, PackedStringArray(), false, String(), String());
}

Array EmbeddingBackend::_hybrid_search(const String &p_query, int p_limit, const PackedStringArray &p_tags, bool p_require_all, const String &p_path_regex, const String &p_class_filter) const {
	SemanticAssetIndex *index = SemanticAssetIndex::get_singleton();
	if (index) {
		index->set_last_filter_error(String());
	}
	String filter_error;
	HashSet<String> allowed_paths = SemanticSearchFilters::resolve_allowed_paths(index, p_tags, p_require_all, p_path_regex, p_class_filter, filter_error);
	if (!filter_error.is_empty()) {
		if (index) {
			index->set_last_filter_error(filter_error);
		}
		return Array();
	}
	if ((!p_path_regex.is_empty() || !p_class_filter.is_empty() || p_tags.size() > 0) && allowed_paths.is_empty()) {
		return Array();
	}

	const String mode = SemanticSearchBackendFactory::get_active_backend_name();
	if (mode == "embedding" || mode == "hybrid") {
		if (index && index->has_stale_embeddings_for_active_provider()) {
			if (index) {
				index->set_last_filter_error("Embedding provider mismatch; refresh embeddings before semantic search.");
			}
			return Array();
		}
	}

	if (mode == "embedding") {
		const Vector<double> query_vec = _token_vector(LexicalSearchEngine::tokenize(p_query));
		return _vector_ann_search(index, query_vec, p_limit, allowed_paths, HashSet<String>());
	}

	Array lexical_results = _lexical_leg_search(index, p_query, p_limit, allowed_paths, p_tags, p_require_all, p_path_regex, p_class_filter);
	if (mode != "hybrid") {
		return lexical_results;
	}

	const Vector<double> query_vec = _token_vector(LexicalSearchEngine::tokenize(p_query));
	HashSet<String> lexical_paths;
	for (int i = 0; i < lexical_results.size(); i++) {
		const String path = lexical_results[i].operator Dictionary().get("path", "");
		if (!path.is_empty()) {
			lexical_paths.insert(path);
		}
	}
	Array embedding_results = _vector_ann_search(index, query_vec, p_limit, allowed_paths, lexical_paths);
	return SemanticRankFusion::reciprocal_rank_fusion(lexical_results, embedding_results, p_limit);
}

Array EmbeddingBackend::search_filtered(const String &p_query, int p_limit, const PackedStringArray &p_tags, bool p_require_all) const {
	return search_with_filters(p_query, p_limit, p_tags, p_require_all, String(), String());
}

Array EmbeddingBackend::search_with_filters(const String &p_query, int p_limit, const PackedStringArray &p_tags, bool p_require_all, const String &p_path_regex, const String &p_class_filter) const {
	return _hybrid_search(p_query, p_limit, p_tags, p_require_all, p_path_regex, p_class_filter);
}

Array EmbeddingBackend::find_similar_with_filters(const String &p_path, int p_limit, const String &p_path_regex, const String &p_class_filter) const {
	SemanticAssetIndex *index = SemanticAssetIndex::get_singleton();
	if (!index) {
		LexicalTagBackend lexical;
		return lexical.find_similar_with_filters(p_path, p_limit, p_path_regex, p_class_filter);
	}
	if (index->has_stale_embeddings_for_active_provider()) {
		index->set_last_filter_error("Embedding provider mismatch; refresh embeddings before semantic search.");
		return Array();
	}
	HashSet<String> allowed_paths;
	if (!p_path_regex.is_empty() || !p_class_filter.is_empty()) {
		allowed_paths = index->collect_paths_matching_metadata(p_path_regex, p_class_filter);
		if (allowed_paths.is_empty()) {
			return Array();
		}
	}
	const Vector<double> source_vec = _entry_embedding_vector(index, p_path);
	if (!source_vec.is_empty()) {
		HashSet<String> exclude;
		exclude.insert(p_path);
		return _vector_ann_search(index, source_vec, p_limit, allowed_paths, exclude);
	}
	LexicalTagBackend lexical;
	return lexical.find_similar_with_filters(p_path, p_limit, p_path_regex, p_class_filter);
}

Array EmbeddingBackend::find_similar(const String &p_path, int p_limit) const {
	return find_similar_with_filters(p_path, p_limit, String(), String());
}

Dictionary EmbeddingBackend::get_stats() const {
	LexicalTagBackend lexical;
	Dictionary stats = lexical.get_stats();
	const String mode = SemanticSearchBackendFactory::get_active_backend_name();
	stats["backend"] = SemanticSearchBackendFactory::get_effective_backend_name();
	stats["embedding_provider"] = SemanticSearchBackendFactory::get_embedding_provider_name();
	stats["embedding_mode"] = mode;
	stats["hybrid_fusion"] = mode == "hybrid" ? "reciprocal_rank_fusion" : "disabled";
	stats["lexical_leg"] = mode == "embedding" ? "disabled" : "bm25_with_token_fallback";
	stats["vector_index"] = "persistent_flat_ann";
	stats["embedding_dimensions"] = HashVectorEmbedding::DEFAULT_DIM;
	stats["index_version"] = SemanticIndexStore::get_index_version();
	return stats;
}
