/**************************************************************************/
/*  semantic_asset_index_mutations.cpp                                    */
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

#include "core/config/project_settings.h"
#include "core/io/file_access.h"
#include "hash_vector_embedding.h"
#include "lexical_index_builder.h"
#include "modules/modules_enabled.gen.h"
#include "semantic_embedding_pipeline.h"
#include "semantic_index_store.h"
#include "semantic_search_backend_factory.h"

#ifdef MODULE_ASSETTAGS_ENABLED
#include "modules/assettags/asset_tag_registry.h"
#include "modules/assettags/asset_tag_storage.h"
#endif

#ifdef TOOLS_ENABLED
#include "editor/file_system/editor_file_system.h"
#endif

Error SemanticAssetIndex::rebuild_index() {
#ifndef TESTS_ENABLED
	ERR_FAIL_COND_V_MSG(this != get_singleton(), ERR_UNCONFIGURED, "SemanticAssetIndex: only the module singleton may rebuild the index.");
#endif
#ifndef MODULE_ASSETTAGS_ENABLED
	return ERR_UNCONFIGURED;
#else
	AssetTagRegistry *registry = AssetTagRegistry::get_singleton();
	if (!registry) {
		return ERR_UNCONFIGURED;
	}
	Vector<String> paths_to_build;
	bool defer_save = false;
	{
		MutexLock lock(index_mutex);
		HashSet<String> tombstones;
		for (const KeyValue<String, SemanticAssetEntry> &kv : entries) {
			tombstones.insert(kv.key);
		}
		_clear_in_memory();
		for (const String &path : tombstones) {
			dirty_paths.insert(path);
		}
		const PackedStringArray paths = registry->get_indexed_paths();
		for (int i = 0; i < paths.size(); i++) {
			paths_to_build.push_back(paths[i]);
		}
		defer_save = batch_depth > 0;
	}
	for (int i = 0; i < paths_to_build.size(); i++) {
		SemanticAssetEntry entry;
		SemanticEmbeddingPipeline::build_entry_from_path(paths_to_build[i], entry);
		MutexLock lock(index_mutex);
		entries.insert(entry.path, entry);
		_index_tokens_for_entry(entry.path, entry.tokens);
		_sync_derived_indexes_for_entry(entry.path, entry);
		dirty_paths.insert(entry.path);
	}
	_scan_filesystem_entries();
	if (defer_save) {
		MutexLock lock(index_mutex);
		save_scheduled = true;
		return OK;
	}
	return save_full();
#endif
}

void SemanticAssetIndex::refresh_embeddings_for_active_provider(bool p_force) {
	const String configured_backend = SemanticSearchBackendFactory::get_active_backend_name();
	if (configured_backend != "embedding" && configured_backend != "hybrid") {
		return;
	}
	const String active_provider = SemanticSearchBackendFactory::get_embedding_provider_name();
	Vector<String> paths;
	HashMap<String, uint64_t> mutation_snapshot;
	{
		MutexLock lock(index_mutex);
		for (const KeyValue<String, SemanticAssetEntry> &kv : entries) {
			paths.push_back(kv.key);
			if (entry_mutations.has(kv.key)) {
				mutation_snapshot.insert(kv.key, entry_mutations[kv.key]);
			}
		}
	}
	HashMap<String, SemanticAssetEntry> refreshed;
	for (int i = 0; i < paths.size(); i++) {
		const String &path = paths[i];
		if (!p_force) {
			bool skip = false;
			{
				MutexLock lock(index_mutex);
				if (!entries.has(path)) {
					continue;
				}
				const SemanticAssetEntry &existing = entries[path];
				if (existing.embedding_provider == active_provider &&
						HashVectorEmbedding::is_valid_embedding_dim(existing.embedding_vector.size())) {
					skip = true;
				}
			}
			if (skip) {
				continue;
			}
		}
		SemanticAssetEntry entry;
		SemanticEmbeddingPipeline::build_entry_from_path(path, entry);
		refreshed.insert(path, entry);
	}
	if (refreshed.is_empty()) {
		return;
	}
	bool should_save = false;
	{
		MutexLock lock(index_mutex);
		for (const KeyValue<String, SemanticAssetEntry> &kv : refreshed) {
			const String &path = kv.key;
			if (!entries.has(path)) {
				continue;
			}
			if (mutation_snapshot.has(path) && entry_mutations.get(path) != mutation_snapshot[path]) {
				continue;
			}
			_unindex_tokens_for_entry(path, entries[path].tokens);
			entries.insert(path, kv.value);
			_index_tokens_for_entry(path, kv.value.tokens);
			_sync_derived_indexes_for_entry(path, kv.value);
			dirty_paths.insert(path);
			entry_mutations.insert(path, ++mutation_generation);
		}
		should_save = batch_depth == 0;
		if (!should_save) {
			save_scheduled = true;
		}
	}
	if (should_save) {
		save();
	}
}

void SemanticAssetIndex::refresh_stale_embeddings_for_active_provider() {
	const String configured_backend = SemanticSearchBackendFactory::get_active_backend_name();
	if (configured_backend != "embedding" && configured_backend != "hybrid") {
		return;
	}
	const String active_provider = SemanticSearchBackendFactory::get_embedding_provider_name();
	Vector<String> stale_paths;
	HashMap<String, uint64_t> mutation_snapshot;
	{
		MutexLock lock(index_mutex);
		for (const KeyValue<String, SemanticAssetEntry> &kv : entries) {
			if (semantic_is_embedding_stale(kv.value, active_provider)) {
				stale_paths.push_back(kv.key);
				if (entry_mutations.has(kv.key)) {
					mutation_snapshot.insert(kv.key, entry_mutations[kv.key]);
				}
			}
		}
	}
	if (stale_paths.is_empty()) {
		return;
	}
	HashMap<String, SemanticAssetEntry> refreshed;
	for (int i = 0; i < stale_paths.size(); i++) {
		const String &path = stale_paths[i];
		SemanticAssetEntry entry;
		SemanticEmbeddingPipeline::build_entry_from_path(path, entry);
		refreshed.insert(path, entry);
	}
	bool should_save = false;
	{
		MutexLock lock(index_mutex);
		for (const KeyValue<String, SemanticAssetEntry> &kv : refreshed) {
			const String &path = kv.key;
			if (!entries.has(path)) {
				continue;
			}
			if (mutation_snapshot.has(path) && entry_mutations.get(path) != mutation_snapshot[path]) {
				continue;
			}
			_unindex_tokens_for_entry(path, entries[path].tokens);
			entries.insert(path, kv.value);
			_index_tokens_for_entry(path, kv.value.tokens);
			_sync_derived_indexes_for_entry(path, kv.value);
			dirty_paths.insert(path);
			entry_mutations.insert(path, ++mutation_generation);
		}
		should_save = batch_depth == 0;
		if (!should_save) {
			save_scheduled = true;
		}
	}
	if (should_save) {
		save();
	}
}

void SemanticAssetIndex::clear() {
#ifndef TESTS_ENABLED
	ERR_FAIL_COND_MSG(this != get_singleton(), "SemanticAssetIndex: only the module singleton may clear the index.");
#endif
	HashSet<String> tombstones;
	bool should_save = false;
	{
		MutexLock lock(index_mutex);
		for (const KeyValue<String, SemanticAssetEntry> &kv : entries) {
			tombstones.insert(kv.key);
		}
		_clear_in_memory();
		for (const String &path : tombstones) {
			dirty_paths.insert(path);
		}
		should_save = batch_depth == 0;
		if (!should_save) {
			save_scheduled = true;
		}
	}
	if (should_save) {
		save_full();
	}
}

Error SemanticAssetIndex::upsert_entry(const String &p_path) {
	if (!p_path.begins_with("res://")) {
		return ERR_INVALID_PARAMETER;
	}
#ifdef MODULE_ASSETTAGS_ENABLED
	if (!AssetTagStorage::is_taggable_extension(p_path)) {
		return ERR_INVALID_PARAMETER;
	}
#endif
	SemanticAssetEntry entry;
	_build_entry_from_path(p_path, entry);
	MutexLock lock(index_mutex);
	if (entries.has(p_path)) {
		_unindex_tokens_for_entry(p_path, entries[p_path].tokens);
		_remove_derived_indexes_for_entry(p_path);
	}
	entries.insert(p_path, entry);
	_index_tokens_for_entry(p_path, entry.tokens);
	_sync_derived_indexes_for_entry(p_path, entry);
	dirty_paths.insert(p_path);
	entry_mutations.insert(p_path, ++mutation_generation);
	_schedule_save_locked();
	return OK;
}

Error SemanticAssetIndex::remove_entry(const String &p_path) {
	MutexLock lock(index_mutex);
	if (!entries.has(p_path)) {
		return OK;
	}
	_unindex_tokens_for_entry(p_path, entries[p_path].tokens);
	_remove_derived_indexes_for_entry(p_path);
	entries.erase(p_path);
	dirty_paths.insert(p_path);
	entry_mutations.erase(p_path);
	++mutation_generation;
	_schedule_save_locked();
	return OK;
}

void SemanticAssetIndex::upsert_paths(const PackedStringArray &p_paths) {
	begin_batch();
	for (int i = 0; i < p_paths.size(); i++) {
		const String &path = p_paths[i];
		Error err = OK;
		if (FileAccess::exists(path)) {
			err = upsert_entry(path);
		} else {
			err = remove_entry(path);
		}
		if (err != OK) {
			WARN_PRINT(vformat("SemanticAssetIndex: failed to upsert path %s (error %d)", path, err));
		}
	}
	commit_batch();
}

void SemanticAssetIndex::remove_paths(const PackedStringArray &p_paths) {
	begin_batch();
	for (int i = 0; i < p_paths.size(); i++) {
		remove_entry(p_paths[i]);
	}
	commit_batch();
}

void SemanticAssetIndex::reconcile_with_registry(AssetTagRegistry *p_registry) {
#ifndef MODULE_ASSETTAGS_ENABLED
	(void)p_registry;
	return;
#else
	if (!p_registry) {
		return;
	}
	const PackedStringArray registry_paths = p_registry->get_indexed_paths();
	HashSet<String> active_paths;
	for (int i = 0; i < registry_paths.size(); i++) {
		active_paths.insert(registry_paths[i]);
	}

	begin_batch();
	Vector<String> stale_paths;
	{
		MutexLock lock(index_mutex);
		for (const KeyValue<String, SemanticAssetEntry> &kv : entries) {
			if (!active_paths.has(kv.key)) {
				stale_paths.push_back(kv.key);
			}
		}
	}
	for (int i = 0; i < stale_paths.size(); i++) {
		remove_entry(stale_paths[i]);
	}
	for (int i = 0; i < registry_paths.size(); i++) {
		_upsert_entry_if_changed(registry_paths[i]);
	}
#ifdef TOOLS_ENABLED
	if (ProjectSettings::get_singleton() && GLOBAL_GET("blazium/semanticsearch/scan_filesystem")) {
		_scan_filesystem_entries();
	}
#endif
	commit_batch();
#endif
}
