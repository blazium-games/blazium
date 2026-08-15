/**************************************************************************/
/*  semantic_asset_index_query.cpp                                        */
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

#include "semantic_asset_index.h"
#include "semantic_asset_index_helpers.h"

#include "hash_vector_embedding.h"
#include "lexical_search_engine.h"
#include "modules/modules_enabled.gen.h"
#include "semantic_search_backend.h"
#include "semantic_search_backend_factory.h"
#include "semantic_search_filters.h"

#ifdef TOOLS_ENABLED
#include "editor/file_system/editor_file_system.h"
#endif

Array SemanticAssetIndex::search_lexical_with_filters(const String &p_query, int p_limit, const PackedStringArray &p_tags, bool p_require_all, const String &p_path_regex, const String &p_class_filter) const {
	const SemanticFilterSnapshot filter_snapshot = build_filter_snapshot(p_tags, p_require_all, p_path_regex, p_class_filter);
	if (!filter_snapshot.filter_error.is_empty()) {
		MutexLock lock(index_mutex);
		last_filter_error = filter_snapshot.filter_error;
		return Array();
	}
	if (filter_snapshot.has_tag_filter && filter_snapshot.allowed_paths.is_empty()) {
		MutexLock lock(index_mutex);
		last_filter_error = String();
		return Array();
	}
	const HashSet<String> *allowed_ptr = filter_snapshot.allowed_paths.is_empty() ? nullptr : &filter_snapshot.allowed_paths;
	HashMap<String, SemanticAssetEntry> entries_snapshot;
	HashMap<String, HashSet<String>> token_index_snapshot;
	HashMap<String, HashSet<String>> prefix_index_snapshot;
	{
		MutexLock lock(index_mutex);
		last_filter_error = String();
		entries_snapshot = entries;
		token_index_snapshot = token_index;
		prefix_index_snapshot = prefix_index;
	}
	return LexicalSearchEngine::search(entries_snapshot, token_index_snapshot, prefix_index_snapshot, p_query, p_limit, allowed_ptr);
}

Array SemanticAssetIndex::search_with_filters(const String &p_query, int p_limit, const PackedStringArray &p_tags, bool p_require_all, const String &p_path_regex, const String &p_class_filter) const {
	const String configured_backend = SemanticSearchBackendFactory::get_active_backend_name();
	if (configured_backend == "embedding" || configured_backend == "hybrid") {
		Ref<SemanticSearchBackend> backend = SemanticSearchBackendFactory::create_active_backend();
		if (backend.is_valid()) {
			return backend->search_with_filters(p_query, p_limit, p_tags, p_require_all, p_path_regex, p_class_filter);
		}
	}
	return search_lexical_with_filters(p_query, p_limit, p_tags, p_require_all, p_path_regex, p_class_filter);
}

Array SemanticAssetIndex::search_filtered(const String &p_query, int p_limit, const PackedStringArray &p_tags, bool p_require_all) const {
	return search_with_filters(p_query, p_limit, p_tags, p_require_all, String(), String());
}

Array SemanticAssetIndex::find_similar_lexical_with_filters(const String &p_path, int p_limit, const String &p_path_regex, const String &p_class_filter) const {
	const SemanticFilterSnapshot filter_snapshot = build_filter_snapshot(PackedStringArray(), false, p_path_regex, p_class_filter);
	if (!filter_snapshot.filter_error.is_empty()) {
		MutexLock lock(index_mutex);
		last_filter_error = filter_snapshot.filter_error;
		return Array();
	}
	if (filter_snapshot.has_metadata_filter && filter_snapshot.allowed_paths.is_empty()) {
		return Array();
	}
	const HashSet<String> *allowed_ptr = filter_snapshot.has_metadata_filter ? &filter_snapshot.allowed_paths : nullptr;
	HashMap<String, SemanticAssetEntry> entries_snapshot;
	HashMap<String, HashSet<String>> token_index_snapshot;
	HashMap<String, HashSet<String>> prefix_index_snapshot;
	{
		MutexLock lock(index_mutex);
		last_filter_error = String();
		entries_snapshot = entries;
		token_index_snapshot = token_index;
		prefix_index_snapshot = prefix_index;
	}
	return LexicalSearchEngine::find_similar(entries_snapshot, token_index_snapshot, prefix_index_snapshot, p_path, p_limit, allowed_ptr);
}

Array SemanticAssetIndex::find_similar_with_filters(const String &p_path, int p_limit, const String &p_path_regex, const String &p_class_filter) const {
	const String configured_backend = SemanticSearchBackendFactory::get_active_backend_name();
	if (configured_backend == "embedding" || configured_backend == "hybrid") {
		Ref<SemanticSearchBackend> backend = SemanticSearchBackendFactory::create_active_backend();
		if (backend.is_valid()) {
			return backend->find_similar_with_filters(p_path, p_limit, p_path_regex, p_class_filter);
		}
	}
	return find_similar_lexical_with_filters(p_path, p_limit, p_path_regex, p_class_filter);
}

Array SemanticAssetIndex::find_similar(const String &p_path, int p_limit) const {
	return find_similar_with_filters(p_path, p_limit, String(), String());
}

Array SemanticAssetIndex::search(const String &p_query, int p_limit) const {
	return search_filtered(p_query, p_limit, PackedStringArray(), false);
}

PackedStringArray SemanticAssetIndex::get_indexed_paths() const {
	MutexLock lock(index_mutex);
	PackedStringArray paths;
	for (const KeyValue<String, SemanticAssetEntry> &kv : entries) {
		paths.push_back(kv.key);
	}
	return paths;
}

Dictionary SemanticAssetIndex::get_stats() const {
	MutexLock lock(index_mutex);
	Dictionary stats;
	stats["asset_count"] = entries.size();
	stats["token_count"] = token_index.size();
	stats["prefix_count"] = prefix_index.size();
	stats["dirty_count"] = dirty_paths.size();
	stats["index_path"] = "res://.blazium/semantic_index/index.json";
	stats["configured_backend"] = SemanticSearchBackendFactory::get_active_backend_name();
	stats["effective_backend"] = SemanticSearchBackendFactory::get_effective_backend_name();
	const String configured_backend = SemanticSearchBackendFactory::get_active_backend_name();
	stats["embedding_ready"] = configured_backend == "embedding" || configured_backend == "hybrid";
	stats["embedding_mode"] = SemanticSearchBackendFactory::get_embedding_provider_name();
	stats["vector_index_entries"] = vector_index.size();
	stats["bm25_documents"] = bm25_index.size();
	stats["index_version"] = SemanticIndexStore::get_index_version();
	const String active_provider = SemanticSearchBackendFactory::get_embedding_provider_name();
	int stale_count = 0;
	for (const KeyValue<String, SemanticAssetEntry> &kv : entries) {
		if (semantic_is_embedding_stale(kv.value, active_provider)) {
			stale_count++;
		}
	}
	stats["stale_embedding_count"] = stale_count;
	return stats;
}

Dictionary SemanticAssetIndex::get_asset_entry(const String &p_path) const {
	MutexLock lock(index_mutex);
	Dictionary result;
	if (!entries.has(p_path)) {
		result["ok"] = false;
		result["error"] = "Asset not indexed: " + p_path;
		return result;
	}
	const SemanticAssetEntry &entry = entries[p_path];
	result["ok"] = true;
	result["path"] = entry.path;
	result["caption"] = entry.caption;
	Array tokens;
	for (int i = 0; i < entry.tokens.size(); i++) {
		tokens.push_back(entry.tokens[i]);
	}
	result["tokens"] = tokens;
	if (!entry.asset_class.is_empty()) {
		result["asset_class"] = entry.asset_class;
	}
	if (!entry.path_segments.is_empty()) {
		result["path_segments"] = entry.path_segments;
	}
	if (!entry.embedding_vector.is_empty()) {
		Array embedding;
		for (int i = 0; i < entry.embedding_vector.size(); i++) {
			embedding.push_back(entry.embedding_vector[i]);
		}
		result["embedding_vector"] = embedding;
	}
	if (!entry.embedding_provider.is_empty()) {
		result["embedding_provider"] = entry.embedding_provider;
	}
	return result;
}

String SemanticAssetIndex::get_last_filter_error() const {
	MutexLock lock(index_mutex);
	return last_filter_error;
}

void SemanticAssetIndex::set_last_filter_error(const String &p_error) const {
	MutexLock lock(index_mutex);
	last_filter_error = p_error;
}

int SemanticAssetIndex::count_searchable_embeddings_for_active_provider() const {
	MutexLock lock(index_mutex);
	int count = 0;
	for (const KeyValue<String, SemanticAssetEntry> &kv : entries) {
		if (HashVectorEmbedding::is_valid_embedding_dim(kv.value.embedding_vector.size())) {
			count++;
		}
	}
	return count;
}

int SemanticAssetIndex::count_stale_embeddings_for_active_provider() const {
	const String active_provider = SemanticSearchBackendFactory::get_embedding_provider_name();
	MutexLock lock(index_mutex);
	int count = 0;
	for (const KeyValue<String, SemanticAssetEntry> &kv : entries) {
		if (semantic_is_embedding_stale(kv.value, active_provider)) {
			count++;
		}
	}
	return count;
}

bool SemanticAssetIndex::has_stale_embeddings_for_active_provider() const {
	const int searchable = count_searchable_embeddings_for_active_provider();
	if (searchable <= 0) {
		return false;
	}
	return count_stale_embeddings_for_active_provider() >= searchable;
}

Array SemanticAssetIndex::vector_search_top_k(const Vector<double> &p_query_vec, int p_limit, const HashSet<String> &p_allowed_paths, const HashSet<String> &p_exclude_paths) const {
	Array results;
	if (p_query_vec.is_empty() || !HashVectorEmbedding::is_valid_embedding_dim(p_query_vec.size())) {
		return results;
	}
	SemanticVectorIndex vectors_snapshot;
	HashMap<String, SemanticAssetEntry> entries_snapshot;
	String active_provider;
	{
		MutexLock lock(index_mutex);
		active_provider = SemanticSearchBackendFactory::get_embedding_provider_name();
		vectors_snapshot = vector_index;
		entries_snapshot = entries;
	}
	const HashSet<String> *allowed_ptr = p_allowed_paths.is_empty() ? nullptr : &p_allowed_paths;
	int stale_count = 0;
	for (const KeyValue<String, SemanticAssetEntry> &kv : entries_snapshot) {
		if (semantic_is_embedding_stale(kv.value, active_provider)) {
			stale_count++;
		}
	}
	const int fetch_limit = MAX(p_limit * MAX(4, stale_count + 1), p_limit + stale_count);
	Array ann_results = vectors_snapshot.search_top_k(p_query_vec, fetch_limit, p_exclude_paths, allowed_ptr);
	for (int i = 0; i < ann_results.size(); i++) {
		Dictionary item = ann_results[i];
		const String path = item.get("path", "");
		if (path.is_empty() || !entries_snapshot.has(path)) {
			continue;
		}
		if (semantic_is_embedding_stale(entries_snapshot[path], active_provider)) {
			continue;
		}
		item["caption"] = entries_snapshot[path].caption;
		results.push_back(item);
		if (results.size() >= p_limit) {
			break;
		}
	}
	return results;
}

Array SemanticAssetIndex::bm25_search(const Vector<String> &p_query_tokens, int p_limit, const HashSet<String> *p_allowed_paths) const {
	SemanticBM25Index bm25_snapshot;
	{
		MutexLock lock(index_mutex);
		bm25_snapshot = bm25_index;
	}
	return bm25_snapshot.search(p_query_tokens, p_limit, p_allowed_paths);
}
