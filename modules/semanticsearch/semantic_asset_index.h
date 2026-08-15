/**************************************************************************/
/*  semantic_asset_index.h                                                */
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

#include "semantic_index_store.h"

#include "core/object/object.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"

#include "semantic_bm25_index.h"
#include "semantic_search_filters.h"
#include "semantic_vector_index.h"

#ifdef TESTS_ENABLED
namespace SemanticAssetIndexTestHooks {
extern String g_inject_dirty_path_during_save;
void set_module_singleton(SemanticAssetIndex *p_index);
SemanticAssetIndex *get_module_singleton();
} //namespace SemanticAssetIndexTestHooks
#endif

class SemanticAssetIndex : public Object {
	GDCLASS(SemanticAssetIndex, Object);

	static SemanticAssetIndex *singleton;

	HashMap<String, SemanticAssetEntry> entries;
	HashMap<String, HashSet<String>> token_index;
	HashMap<String, HashSet<String>> prefix_index;
	SemanticVectorIndex vector_index;
	SemanticBM25Index bm25_index;

	int batch_depth = 0;
	bool save_scheduled = false;
	HashSet<String> dirty_paths;
	uint64_t mutation_generation = 0;
	HashMap<String, uint64_t> entry_mutations;
	mutable String last_filter_error;
	mutable Mutex index_mutex;

protected:
	static void _bind_methods();

	void _deferred_save();
	void _schedule_save();
	void _schedule_save_locked();
	Error save_full();
	void _clear_in_memory();
	void _rebuild_derived_indexes();
	void _sync_derived_indexes_for_entry(const String &p_path, const SemanticAssetEntry &p_entry);
	void _remove_derived_indexes_for_entry(const String &p_path);

	void _index_tokens_for_entry(const String &p_path, const Vector<String> &p_tokens);
	void _unindex_tokens_for_entry(const String &p_path, const Vector<String> &p_tokens);
	void _build_entry_from_path(const String &p_path, SemanticAssetEntry &r_entry) const;
	void _scan_filesystem_entries();
	void _upsert_entry_if_changed(const String &p_path);
	bool _validate_path_regex(const String &p_path_regex) const;

public:
	static SemanticAssetIndex *get_singleton();

	Error load();
	Error save();
	void begin_batch();
	void commit_batch();
	Error rebuild_index();
	void refresh_embeddings_for_active_provider(bool p_force = false);
	void refresh_stale_embeddings_for_active_provider();
	void clear();

	Error upsert_entry(const String &p_path);
	Error remove_entry(const String &p_path);
	void upsert_paths(const PackedStringArray &p_paths);
	void remove_paths(const PackedStringArray &p_paths);
	void reconcile_with_registry(class AssetTagRegistry *p_registry);

	Array search(const String &p_query, int p_limit = 20) const;
	Array search_filtered(const String &p_query, int p_limit, const PackedStringArray &p_tags, bool p_require_all = false) const;
	Array search_lexical_with_filters(const String &p_query, int p_limit, const PackedStringArray &p_tags, bool p_require_all, const String &p_path_regex, const String &p_class_filter) const;
	Array search_with_filters(const String &p_query, int p_limit, const PackedStringArray &p_tags, bool p_require_all, const String &p_path_regex, const String &p_class_filter) const;
	Array find_similar(const String &p_path, int p_limit = 10) const;
	Array find_similar_lexical_with_filters(const String &p_path, int p_limit, const String &p_path_regex, const String &p_class_filter) const;
	Array find_similar_with_filters(const String &p_path, int p_limit, const String &p_path_regex, const String &p_class_filter) const;
	HashSet<String> collect_paths_matching_metadata(const String &p_path_regex, const String &p_class_filter) const;
	HashMap<String, SemanticEntryMetadata> build_metadata_snapshot() const;
	SemanticFilterSnapshot build_filter_snapshot(
			const PackedStringArray &p_tags,
			bool p_require_all,
			const String &p_path_regex,
			const String &p_class_filter) const;
	PackedStringArray get_indexed_paths() const;
	Dictionary get_stats() const;
	Dictionary get_asset_entry(const String &p_path) const;
	String get_last_filter_error() const;
	void set_last_filter_error(const String &p_error) const;
	bool has_stale_embeddings_for_active_provider() const;
	int count_stale_embeddings_for_active_provider() const;
	int count_searchable_embeddings_for_active_provider() const;
	Array vector_search_top_k(const Vector<double> &p_query_vec, int p_limit, const HashSet<String> &p_allowed_paths, const HashSet<String> &p_exclude_paths) const;
	Array bm25_search(const Vector<String> &p_query_tokens, int p_limit, const HashSet<String> *p_allowed_paths) const;
	const SemanticVectorIndex &get_vector_index() const { return vector_index; }

#ifdef TESTS_ENABLED
	Error test_save_full() { return save_full(); }
#endif

	SemanticAssetIndex();
	~SemanticAssetIndex();
};
