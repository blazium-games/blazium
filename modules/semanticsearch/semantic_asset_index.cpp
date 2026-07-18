/**************************************************************************/
/*  semantic_asset_index.cpp                                              */
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
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/math/math_funcs.h"
#include "hash_vector_embedding.h"
#include "lexical_index_builder.h"
#include "lexical_search_engine.h"
#include "modules/modules_enabled.gen.h"
#include "modules/regex/regex.h"
#include "semantic_embedding_pipeline.h"
#include "semantic_index_store.h"
#include "semantic_search_backend.h"
#include "semantic_search_backend_factory.h"
#include "semantic_search_filters.h"

#ifdef MODULE_ASSETTAGS_ENABLED
#include "modules/assettags/asset_tag_manager.h"
#include "modules/assettags/asset_tag_registry.h"
#include "modules/assettags/asset_tag_storage.h"
#endif

#ifdef TOOLS_ENABLED
#include "editor/editor_file_system.h"
#endif

SemanticAssetIndex *SemanticAssetIndex::singleton = nullptr;

void SemanticAssetIndex::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load"), &SemanticAssetIndex::load);
	ClassDB::bind_method(D_METHOD("save"), &SemanticAssetIndex::save);
	ClassDB::bind_method(D_METHOD("begin_batch"), &SemanticAssetIndex::begin_batch);
	ClassDB::bind_method(D_METHOD("commit_batch"), &SemanticAssetIndex::commit_batch);
	ClassDB::bind_method(D_METHOD("rebuild_index"), &SemanticAssetIndex::rebuild_index);
	ClassDB::bind_method(D_METHOD("clear"), &SemanticAssetIndex::clear);
	ClassDB::bind_method(D_METHOD("upsert_entry", "path"), &SemanticAssetIndex::upsert_entry);
	ClassDB::bind_method(D_METHOD("remove_entry", "path"), &SemanticAssetIndex::remove_entry);
	ClassDB::bind_method(D_METHOD("upsert_paths", "paths"), &SemanticAssetIndex::upsert_paths);
	ClassDB::bind_method(D_METHOD("remove_paths", "paths"), &SemanticAssetIndex::remove_paths);
	ClassDB::bind_method(D_METHOD("search", "query", "limit"), &SemanticAssetIndex::search, DEFVAL(20));
	ClassDB::bind_method(D_METHOD("search_filtered", "query", "limit", "tags", "require_all"), &SemanticAssetIndex::search_filtered, DEFVAL(20), DEFVAL(PackedStringArray()), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("search_with_filters", "query", "limit", "tags", "require_all", "path_regex", "class_filter"), &SemanticAssetIndex::search_with_filters, DEFVAL(20), DEFVAL(PackedStringArray()), DEFVAL(false), DEFVAL(String()), DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("find_similar", "path", "limit"), &SemanticAssetIndex::find_similar, DEFVAL(10));
	ClassDB::bind_method(D_METHOD("find_similar_with_filters", "path", "limit", "path_regex", "class_filter"), &SemanticAssetIndex::find_similar_with_filters, DEFVAL(10), DEFVAL(String()), DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_stats"), &SemanticAssetIndex::get_stats);
	ClassDB::bind_method(D_METHOD("get_asset_entry", "path"), &SemanticAssetIndex::get_asset_entry);
	ClassDB::bind_method(D_METHOD("_deferred_save"), &SemanticAssetIndex::_deferred_save);
}

SemanticAssetIndex *SemanticAssetIndex::get_singleton() {
	return singleton;
}

void SemanticAssetIndex::_index_tokens_for_entry(const String &p_path, const Vector<String> &p_tokens) {
	LexicalIndexBuilder::index_tokens_for_entry(p_path, p_tokens, token_index, prefix_index);
}

void SemanticAssetIndex::_unindex_tokens_for_entry(const String &p_path, const Vector<String> &p_tokens) {
	LexicalIndexBuilder::unindex_tokens_for_entry(p_path, p_tokens, token_index, prefix_index);
}

void SemanticAssetIndex::_build_entry_from_path(const String &p_path, SemanticAssetEntry &r_entry) const {
	SemanticEmbeddingPipeline::build_entry_from_path(p_path, r_entry);
}

void SemanticAssetIndex::_upsert_entry_if_changed(const String &p_path) {
	// Cheap caption fingerprint (tags/comments/path/class) before tokenize/embed.
	const String new_caption = SemanticEmbeddingPipeline::build_caption_from_path(p_path);
	{
		MutexLock lock(index_mutex);
		if (entries.has(p_path) && entries[p_path].caption == new_caption) {
			return;
		}
	}
	upsert_entry(p_path);
}

void SemanticAssetIndex::_scan_filesystem_entries() {
#ifdef TOOLS_ENABLED
	if (!ProjectSettings::get_singleton() || !GLOBAL_GET("blazium/semanticsearch/scan_filesystem")) {
		return;
	}
	if (!EditorFileSystem::get_singleton()) {
		return;
	}
	EditorFileSystemDirectory *root = EditorFileSystem::get_singleton()->get_filesystem();
	if (!root) {
		return;
	}
	begin_batch();
	Vector<EditorFileSystemDirectory *> stack;
	stack.push_back(root);
	while (!stack.is_empty()) {
		EditorFileSystemDirectory *dir = stack[stack.size() - 1];
		stack.remove_at(stack.size() - 1);
		for (int i = 0; i < dir->get_subdir_count(); i++) {
			stack.push_back(dir->get_subdir(i));
		}
		for (int i = 0; i < dir->get_file_count(); i++) {
			const String path = dir->get_file_path(i);
#ifdef MODULE_ASSETTAGS_ENABLED
			if (!AssetTagStorage::is_taggable_extension(path)) {
				continue;
			}
#endif
			bool needs_upsert = false;
			{
				MutexLock lock(index_mutex);
				needs_upsert = !entries.has(path);
			}
			if (needs_upsert) {
				upsert_entry(path);
			}
		}
	}
	commit_batch();
#endif
}

bool SemanticAssetIndex::_validate_path_regex(const String &p_path_regex) const {
	if (p_path_regex.is_empty()) {
		return true;
	}
	Ref<RegEx> path_re;
	path_re.instantiate();
	return path_re->compile(p_path_regex) == OK;
}

HashMap<String, SemanticEntryMetadata> SemanticAssetIndex::build_metadata_snapshot() const {
	MutexLock lock(index_mutex);
	HashMap<String, SemanticEntryMetadata> snapshot;
	for (const KeyValue<String, SemanticAssetEntry> &kv : entries) {
		SemanticEntryMetadata meta;
		meta.asset_class = kv.value.asset_class;
#ifdef TOOLS_ENABLED
		if (meta.asset_class.is_empty() && EditorFileSystem::get_singleton()) {
			meta.asset_class = EditorFileSystem::get_singleton()->get_file_type(kv.key);
		}
#endif
		meta.path_segments = kv.value.path_segments;
		snapshot.insert(kv.key, meta);
	}
	return snapshot;
}

SemanticFilterSnapshot SemanticAssetIndex::build_filter_snapshot(
		const PackedStringArray &p_tags,
		bool p_require_all,
		const String &p_path_regex,
		const String &p_class_filter) const {
	HashMap<String, SemanticEntryMetadata> metadata_snapshot;
	uint64_t generation = 0;
	{
		MutexLock lock(index_mutex);
		generation = mutation_generation;
		for (const KeyValue<String, SemanticAssetEntry> &kv : entries) {
			SemanticEntryMetadata meta;
			meta.asset_class = kv.value.asset_class;
#ifdef TOOLS_ENABLED
			if (meta.asset_class.is_empty() && EditorFileSystem::get_singleton()) {
				meta.asset_class = EditorFileSystem::get_singleton()->get_file_type(kv.key);
			}
#endif
			meta.path_segments = kv.value.path_segments;
			metadata_snapshot.insert(kv.key, meta);
		}
	}

	HashSet<String> tag_allowed_paths;
#ifdef MODULE_ASSETTAGS_ENABLED
	if (p_tags.size() > 0) {
		if (AssetTagRegistry *registry = AssetTagRegistry::get_singleton()) {
			tag_allowed_paths = registry->collect_paths_for_tag_filter(p_tags, p_require_all);
		}
	}
#endif
	const HashSet<String> *tag_paths_ptr = p_tags.size() > 0 ? &tag_allowed_paths : nullptr;
	return SemanticSearchFilters::build_filter_snapshot(metadata_snapshot, p_tags, p_require_all, p_path_regex, p_class_filter, tag_paths_ptr, generation);
}

HashSet<String> SemanticAssetIndex::collect_paths_matching_metadata(const String &p_path_regex, const String &p_class_filter) const {
	String filter_error;
	return SemanticSearchFilters::collect_paths_matching_metadata_snapshot(
			build_metadata_snapshot(), p_path_regex, p_class_filter, filter_error);
}

void SemanticAssetIndex::_clear_in_memory() {
	for (const KeyValue<String, SemanticAssetEntry> &kv : entries) {
		_unindex_tokens_for_entry(kv.key, kv.value.tokens);
	}
	entries.clear();
	token_index.clear();
	prefix_index.clear();
	vector_index.clear();
	bm25_index.clear();
}

void SemanticAssetIndex::_sync_derived_indexes_for_entry(const String &p_path, const SemanticAssetEntry &p_entry) {
	if (!p_entry.embedding_vector.is_empty() && HashVectorEmbedding::is_valid_embedding_dim(p_entry.embedding_vector.size())) {
		vector_index.upsert(p_path, p_entry.embedding_vector);
	} else {
		vector_index.remove(p_path);
	}
	bm25_index.upsert_document(p_path, p_entry.tokens);
}

void SemanticAssetIndex::_remove_derived_indexes_for_entry(const String &p_path) {
	vector_index.remove(p_path);
	bm25_index.remove_document(p_path);
}

void SemanticAssetIndex::_rebuild_derived_indexes() {
	vector_index.clear();
	bm25_index.clear();
	for (const KeyValue<String, SemanticAssetEntry> &kv : entries) {
		_sync_derived_indexes_for_entry(kv.key, kv.value);
	}
}

SemanticAssetIndex::SemanticAssetIndex() {
	if (!singleton) {
		singleton = this;
	}
}

SemanticAssetIndex::~SemanticAssetIndex() {
	if (singleton == this) {
#ifdef TESTS_ENABLED
		singleton = SemanticAssetIndexTestHooks::get_module_singleton();
		if (singleton == this) {
			singleton = nullptr;
		}
#else
		singleton = nullptr;
#endif
	}
}
