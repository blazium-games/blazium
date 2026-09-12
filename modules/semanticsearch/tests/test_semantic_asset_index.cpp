/**************************************************************************/
/*  test_semantic_asset_index.cpp                                         */
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

#include "../embedding_provider.h"
#include "../hash_vector_embedding.h"
#include "../lexical_search_engine.h"
#include "../semantic_asset_index.h"
#include "../semantic_index_store.h"
#include "../semantic_query_embed_cache.h"
#include "../semantic_search_backend.h"
#include "../semantic_search_backend_factory.h"
#include "modules/modules_enabled.gen.h"
#ifdef MODULE_ASSETTAGS_ENABLED
#include "modules/assettags/asset_tag_manager.h"
#include "modules/assettags/asset_tag_registry.h"
#include "modules/assettags/asset_tag_storage.h"
#endif
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/http_client.h"
#include "core/math/math_funcs.h"
#include "core/os/os.h"
#include "tests/test_macros.h"

static SemanticAssetIndex *semantic_test_index() {
	SemanticAssetIndex *index = SemanticAssetIndex::get_singleton();
	CHECK(index != nullptr);
	return index;
}

void test_semantic_asset_index() {
	SemanticAssetIndex *previous_singleton = SemanticAssetIndex::get_singleton();
	SemanticAssetIndex index;
	if (previous_singleton) {
		CHECK(SemanticAssetIndex::get_singleton() == previous_singleton);
	} else {
		CHECK(SemanticAssetIndex::get_singleton() == &index);
	}
	index.clear();
	CHECK(index.search("", 10).is_empty());
	CHECK(index.search("   ", 10).is_empty());

	index.rebuild_index();
	Dictionary stats = index.get_stats();
	CHECK(int(stats.get("asset_count", -1)) >= 0);

	Array similar = index.find_similar("res://missing.tscn", 5);
	CHECK(similar.is_empty());

	index.begin_batch();
	index.commit_batch();
}

void test_semantic_asset_index_search() {
	SemanticAssetIndex index;
	index.clear();
	CHECK(index.upsert_entry("res://hero.tscn") == OK);
	Array results = index.search("hero", 5);
	CHECK(results.size() >= 1);
}

void test_semantic_backend_factory() {
	const String configured = SemanticSearchBackendFactory::get_active_backend_name();
	CHECK(!configured.is_empty());
	CHECK(SemanticSearchBackendFactory::get_effective_backend_name() == "lexical");
	Ref<SemanticSearchBackend> backend = SemanticSearchBackendFactory::create_active_backend();
	CHECK(backend.is_valid());
	Ref<SemanticSearchBackend> cached = SemanticSearchBackendFactory::create_active_backend();
	CHECK(cached.ptr() == backend.ptr());
}

void test_semantic_search_tag_filter() {
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/backend", "lexical");
	SemanticSearchBackendFactory::invalidate_session_backend();
	SemanticAssetIndex index;
	index.clear();
	CHECK(index.upsert_entry("res://hero.tscn") == OK);
	CHECK(index.upsert_entry("res://tree.tscn") == OK);
	PackedStringArray tags;
	tags.push_back("__semantic_test_nonexistent_tag__");
	Array filtered = index.search_lexical_with_filters("hero", 5, tags, true, String(), String());
	CHECK(filtered.is_empty());
	Array unfiltered = index.search("hero", 5);
	CHECK(unfiltered.size() >= 1);
}

void test_semantic_search_metadata_prefilter() {
	SemanticAssetIndex index;
	index.clear();
	index.upsert_entry("res://characters/hero.tscn");
	index.upsert_entry("res://props/tree.tscn");
	Array hero_only = index.search_with_filters("hero", 5, PackedStringArray(), false, "characters/.*", "");
	CHECK(hero_only.size() == 1);
	CHECK(String(Dictionary(hero_only[0]).get("path", "")) == "res://characters/hero.tscn");
	Array limited = index.search_with_filters("tscn", 1, PackedStringArray(), false, "characters/.*", "");
	CHECK(limited.size() == 1);
}

void test_semantic_index_store_roundtrip() {
	SemanticIndexStore store;
	SemanticAssetEntry entry;
	entry.path = "res://sample.tscn";
	entry.caption = "Sample caption";
	entry.tokens.push_back("sample");
	store.set_entry(entry.path, entry);
	CHECK(store.has_entry("res://sample.tscn"));
	const SemanticAssetEntry *loaded = store.get_entry("res://sample.tscn");
	CHECK(loaded != nullptr);
	CHECK(loaded->caption == "Sample caption");
}

void test_semantic_rebuild_clear_persist() {
	SemanticIndexStore::set_test_index_dir("res://.blazium/test_isolated_semantic_index");
	SemanticAssetIndex index;
	index.clear();
	index.upsert_entry("res://hero.tscn");
	CHECK(index.search("hero", 5).size() >= 1);
	CHECK(index.save() == OK);

	index.clear();
	CHECK(int(index.get_stats().get("asset_count", -1)) == 0);

	SemanticIndexStore store;
	CHECK(store.load() == OK);
	CHECK(!store.has_entry("res://hero.tscn"));

	index.upsert_entry("res://tree.tscn");
	CHECK(index.save() == OK);
	SemanticAssetIndex reloaded;
	CHECK(reloaded.load() == OK);
	CHECK(reloaded.search("tree", 5).size() >= 1);

	SemanticIndexStore::clear_test_index_dir();
}

void test_lexical_search_engine() {
	HashMap<String, SemanticAssetEntry> entries;
	SemanticAssetEntry entry;
	entry.path = "res://hero.tscn";
	entry.caption = "Hero character";
	entry.tokens = LexicalSearchEngine::tokenize(entry.caption);
	entries.insert(entry.path, entry);
	HashMap<String, HashSet<String>> token_index;
	token_index["hero"].insert("res://hero.tscn");
	Array results = LexicalSearchEngine::search(entries, token_index, HashMap<String, HashSet<String>>(), "hero", 5, nullptr);
	CHECK(results.size() >= 1);

	SemanticAssetEntry duplicate_entry;
	duplicate_entry.path = "res://dup_asset.tscn";
	duplicate_entry.caption = "duplicate token";
	duplicate_entry.tokens.push_back("duplicate");
	entries.insert(duplicate_entry.path, duplicate_entry);
	Vector<String> query_tokens;
	query_tokens.push_back("duplicate");
	const double dedup_score = LexicalSearchEngine::score_query(query_tokens, duplicate_entry);
	CHECK(dedup_score >= 4.0);
	CHECK(dedup_score <= 8.0);
}

void test_semantic_incremental_save() {
	SemanticIndexStore::set_test_index_dir("res://.blazium/test_isolated_semantic_incremental");
	SemanticAssetIndex index;
	index.clear();
	index.upsert_entry("res://alpha.tscn");
	CHECK(index.save() == OK);
	index.upsert_entry("res://beta.tscn");
	CHECK(index.save() == OK);

	SemanticAssetIndex reloaded;
	CHECK(reloaded.load() == OK);
	CHECK(reloaded.search("alpha", 5).size() >= 1);
	CHECK(reloaded.search("beta", 5).size() >= 1);
	SemanticIndexStore::clear_test_index_dir();
}

void test_semantic_save_dirty_roundtrip() {
	SemanticIndexStore::set_test_index_dir("res://.blazium/test_isolated_semantic_save_dirty");
	SemanticAssetIndex index;
	index.clear();
	index.upsert_entry("res://persist.tscn");
	CHECK(index.save() == OK);

	index.upsert_entry("res://second.tscn");
	CHECK(index.save() == OK);

	SemanticAssetIndex reloaded;
	CHECK(reloaded.load() == OK);
	CHECK(reloaded.search("persist", 5).size() >= 1);
	CHECK(reloaded.search("second", 5).size() >= 1);

	index.remove_entry("res://persist.tscn");
	CHECK(index.save() == OK);
	SemanticAssetIndex pruned;
	CHECK(pruned.load() == OK);
	CHECK(pruned.search("persist", 5).is_empty());
	CHECK(pruned.search("second", 5).size() >= 1);
	CHECK(SemanticIndexStore::get_index_version() == 2);

	SemanticIndexStore::set_test_index_dir("res://.blazium/test_isolated_semantic_future_version");
	const String future_index_path = SemanticIndexStore::test_get_index_file_path();
	Ref<DirAccess> future_dir = DirAccess::create(DirAccess::ACCESS_USERDATA);
	if (future_dir.is_valid()) {
		future_dir->make_dir_recursive(future_index_path.get_base_dir());
	}
	Ref<FileAccess> future_file = FileAccess::open(future_index_path, FileAccess::WRITE);
	if (future_file.is_valid()) {
		future_file->store_string("{\"version\":99,\"entries\":[]}");
		future_file.unref();
	}
	SemanticIndexStore future_store;
	CHECK(future_store.load() == ERR_UNAVAILABLE);
	SemanticIndexStore::clear_test_index_dir();
}

void test_semantic_sidecar_stale_after_full_save() {
	SemanticIndexStore::set_test_index_dir("res://.blazium/test_isolated_semantic_sidecar_stale");
	SemanticAssetIndex index;
	index.clear();
	index.upsert_entry("res://alpha.tscn");
	CHECK(index.save() == OK);
	index.remove_entry("res://alpha.tscn");
	CHECK(index.save() == OK);
	index.upsert_entry("res://beta.tscn");
	CHECK(index.test_save_full() == OK);

	SemanticAssetIndex reloaded;
	CHECK(reloaded.load() == OK);
	CHECK(reloaded.search("alpha", 5).is_empty());
	CHECK(reloaded.search("beta", 5).size() >= 1);
	SemanticIndexStore::clear_test_index_dir();
}

void test_semantic_find_similar_excludes_source() {
	SemanticAssetIndex index;
	index.clear();
	index.upsert_entry("res://hero.tscn");
	index.upsert_entry("res://hero_copy.tscn");
	Array similar = index.find_similar("res://hero.tscn", 10);
	for (int i = 0; i < similar.size(); i++) {
		CHECK(String(Dictionary(similar[i]).get("path", "")) != "res://hero.tscn");
	}
}

void test_semantic_search_invalid_path_regex() {
	SemanticAssetIndex index;
	index.clear();
	index.upsert_entry("res://characters/hero.tscn");
	Array results = index.search_with_filters("hero", 5, PackedStringArray(), false, "[invalid", "");
	CHECK(results.is_empty());
	CHECK(!index.get_last_filter_error().is_empty());
}

void test_semantic_rebuild_derived_indexes() {
#ifdef MODULE_ASSETTAGS_ENABLED
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_semantic_rebuild_derived");
	AssetTagManager manager;
	AssetTagRegistry registry;
	PackedStringArray hero_tags;
	hero_tags.push_back("Character.Hero");
	CHECK(registry.set_tags_for_asset("res://hero.tscn", hero_tags) == OK);
	PackedStringArray tree_tags;
	tree_tags.push_back("Environment.Tree");
	CHECK(registry.set_tags_for_asset("res://tree.tscn", tree_tags) == OK);
#endif
	SemanticAssetIndex *index = semantic_test_index();
#ifdef MODULE_ASSETTAGS_ENABLED
	CHECK(index->rebuild_index() == OK);
#else
	index->clear();
	CHECK(index->upsert_entry("res://hero.tscn") == OK);
	CHECK(index->upsert_entry("res://tree.tscn") == OK);
#endif
	CHECK(index->search("hero", 5).size() >= 1);
	CHECK(index->search("tree", 5).size() >= 1);
#ifdef MODULE_ASSETTAGS_ENABLED
	AssetTagStorage::clear_test_storage_dir();
#endif
}

void test_semantic_provider_refresh_embeddings() {
	SemanticAssetIndex index;
	index.clear();
	index.upsert_entry("res://hero.tscn");
	index.refresh_embeddings_for_active_provider();
	CHECK(index.search("hero", 5).size() >= 1);
}

void test_semantic_embedding_invalid_path_regex() {
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/backend", "hybrid");
	SemanticSearchBackendFactory::invalidate_session_backend();
	SemanticAssetIndex *index = SemanticAssetIndex::get_singleton();
	CHECK(index != nullptr);
	index->clear();
	index->upsert_entry("res://characters/hero.tscn");
	Ref<SemanticSearchBackend> backend = SemanticSearchBackendFactory::create_active_backend();
	CHECK(backend.is_valid());
	Array results = backend->search_with_filters("hero", 5, PackedStringArray(), false, "[invalid", "");
	CHECK(results.is_empty());
	CHECK(!index->get_last_filter_error().is_empty());
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/backend", "lexical");
	SemanticSearchBackendFactory::invalidate_session_backend();
}

void test_semantic_hybrid_search_rrf() {
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/backend", "hybrid");
	SemanticSearchBackendFactory::invalidate_session_backend();
	SemanticAssetIndex *index = semantic_test_index();
	index->clear();
	CHECK(index->upsert_entry("res://hero.tscn") == OK);
	CHECK(index->upsert_entry("res://tree.tscn") == OK);
	Ref<SemanticSearchBackend> backend = SemanticSearchBackendFactory::create_active_backend();
	CHECK(backend.is_valid());
	Array results = backend->search_with_filters("hero", 5, PackedStringArray(), false, "", "");
	CHECK(results.size() >= 1);
	Dictionary stats = backend->get_stats();
	CHECK(String(stats.get("hybrid_fusion", "")) == "reciprocal_rank_fusion");
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/backend", "lexical");
	SemanticSearchBackendFactory::invalidate_session_backend();
}

void test_semantic_index_load_wrong_dim_embedding() {
	SemanticIndexStore::set_test_index_dir("res://.blazium/test_wrong_dim_load");
	const String index_path = SemanticIndexStore::test_get_index_file_path();
	Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_USERDATA);
	if (dir.is_valid()) {
		dir->make_dir_recursive(index_path.get_base_dir());
	}
	const String json = "{\"version\":2,\"entries\":[{\"path\":\"res://hero.tscn\",\"caption\":\"hero\",\"tokens\":[\"hero\"],\"embedding_vector\":[0.1,0.2,0.3],\"embedding_provider\":\"hash_vector\"}]}";
	Ref<FileAccess> file = FileAccess::open(index_path, FileAccess::WRITE);
	if (file.is_valid()) {
		file->store_string(json);
		file.unref();
	}
	SemanticAssetIndex *index = semantic_test_index();
	CHECK(index->load() == OK);
	Dictionary entry = index->get_asset_entry("res://hero.tscn");
	CHECK(!entry.is_empty());
	Array embedding = entry.get("embedding_vector", Array());
	CHECK(embedding.is_empty());
	SemanticIndexStore::clear_test_index_dir();
}

void test_semantic_concurrent_save_and_search() {
	SemanticAssetIndex index;
	index.clear();
	index.begin_batch();
	CHECK(index.upsert_entry("res://hero.tscn") == OK);
	CHECK(index.upsert_entry("res://tree.tscn") == OK);
	CHECK(index.search("hero", 5).size() >= 1);
	CHECK(index.save() == OK);
	index.commit_batch();
	CHECK(index.get_indexed_paths().size() >= 2);
}

void test_semantic_http_url_change_refresh() {
	SemanticAssetIndex index;
	index.clear();
	CHECK(index.upsert_entry("res://hero.tscn") == OK);
	const String prev_url = String(ProjectSettings::get_singleton()->get_setting("blazium/semanticsearch/embedding_http_url"));
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/embedding_http_url", "http://localhost:9999/embed");
	index.refresh_embeddings_for_active_provider(true);
	Dictionary entry = index.get_asset_entry("res://hero.tscn");
	CHECK(!entry.is_empty());
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/embedding_http_url", prev_url);
}

void test_semantic_filter_regex_parity() {
	SemanticAssetIndex *index = semantic_test_index();
	index->clear();
	CHECK(index->upsert_entry("res://characters/hero.tscn") == OK);
	CHECK(index->upsert_entry("res://props/tree.tscn") == OK);
	const HashSet<String> filtered = index->collect_paths_matching_metadata("characters/.*", "");
	CHECK(filtered.has("res://characters/hero.tscn"));
	CHECK(!filtered.has("res://props/tree.tscn"));
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/backend", "hybrid");
	SemanticSearchBackendFactory::invalidate_session_backend();
	Ref<SemanticSearchBackend> backend = SemanticSearchBackendFactory::create_active_backend();
	CHECK(backend.is_valid());
	Array results = backend->search_with_filters("tscn", 10, PackedStringArray(), false, "characters/.*", "");
	CHECK(results.size() == 1);
	CHECK(String(Dictionary(results[0]).get("path", "")) == "res://characters/hero.tscn");
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/backend", "lexical");
	SemanticSearchBackendFactory::invalidate_session_backend();
}

void test_semantic_sidecar_wrong_dim_embedding() {
	SemanticIndexStore::set_test_index_dir("res://.blazium/test_sidecar_wrong_dim");
	SemanticAssetEntry entry;
	entry.path = "res://hero.tscn";
	entry.caption = "hero";
	entry.tokens.push_back("hero");
	entry.embedding_vector.push_back(0.1);
	entry.embedding_vector.push_back(0.2);
	entry.embedding_vector.push_back(0.3);
	entry.embedding_provider = "hash_vector";
	HashMap<String, SemanticAssetEntry> entries;
	entries.insert(entry.path, entry);
	HashSet<String> dirty;
	dirty.insert(entry.path);
	CHECK(SemanticIndexStore::save_dirty_sidecars(dirty, entries));
	SemanticAssetIndex index;
	CHECK(index.load() == OK);
	Dictionary loaded = index.get_asset_entry("res://hero.tscn");
	Array embedding = loaded.get("embedding_vector", Array());
	CHECK(embedding.is_empty());
	SemanticIndexStore::clear_test_index_dir();
}

void test_semantic_save_preserves_late_dirty_paths() {
	SemanticAssetIndex index;
	index.clear();
	CHECK(index.upsert_entry("res://early.tscn") == OK);
	SemanticAssetIndexTestHooks::g_inject_dirty_path_during_save = "res://late.tscn";
	CHECK(index.save() == OK);
	SemanticAssetIndexTestHooks::g_inject_dirty_path_during_save = String();
	CHECK(index.upsert_entry("res://late.tscn") == OK);
	CHECK(index.save() == OK);
	CHECK(index.get_indexed_paths().has("res://early.tscn"));
	CHECK(index.get_indexed_paths().has("res://late.tscn"));
}

void test_semantic_provider_mismatch_gate() {
	SemanticAssetIndex *index = semantic_test_index();
	index->clear();
	CHECK(index->upsert_entry("res://hero.tscn") == OK);
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/backend", "hybrid");
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/embedding_provider", "ngram");
	SemanticSearchBackendFactory::invalidate_session_backend();
	index->refresh_embeddings_for_active_provider(true);
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/embedding_provider", "hash_vector");
	SemanticSearchBackendFactory::invalidate_session_backend();
	Ref<SemanticSearchBackend> backend = SemanticSearchBackendFactory::create_active_backend();
	CHECK(backend.is_valid());
	CHECK(index->has_stale_embeddings_for_active_provider());
	Array results = backend->search_with_filters("hero", 5, PackedStringArray(), false, String(), String());
	CHECK(results.is_empty());
	CHECK(!index->get_last_filter_error().is_empty());
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/backend", "lexical");
	SemanticSearchBackendFactory::invalidate_session_backend();
}

void test_semantic_ngram_embedding_normalized() {
	NgramEmbeddingProvider provider;
	Vector<String> tokens;
	tokens.push_back("hero");
	tokens.push_back("tree");
	const Vector<double> embedding = provider.embed_tokens(tokens);
	double norm = 0.0;
	for (int i = 0; i < embedding.size(); i++) {
		norm += embedding[i] * embedding[i];
	}
	CHECK(Math::is_equal_approx(Math::sqrt(norm), 1.0));
}

void test_semantic_http_embed_timeout() {
	const bool prev_unlocked = HttpEmbeddingProvider::is_http_embedding_unlocked();
	HttpEmbeddingProvider::set_http_embedding_unlocked(true);
	const String prev_provider = String(ProjectSettings::get_singleton()->get_setting("blazium/semanticsearch/embedding_provider"));
	const String prev_url = String(ProjectSettings::get_singleton()->get_setting("blazium/semanticsearch/embedding_http_url"));
	const int prev_timeout = int(ProjectSettings::get_singleton()->get_setting("blazium/semanticsearch/embedding_http_timeout_ms"));
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/embedding_provider", "http");
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/embedding_http_url", "http://127.0.0.1:1/embeddings");
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/embedding_http_timeout_ms", 500);
	SemanticSearchBackendFactory::invalidate_embedding_provider_cache();
	HttpEmbeddingProvider provider;
	Vector<String> tokens;
	tokens.push_back("timeout");
	const uint64_t start = OS::get_singleton()->get_ticks_msec();
	const EmbeddingResult result = provider.embed_tokens_result(tokens);
	CHECK(int(OS::get_singleton()->get_ticks_msec() - start) < 5000);
	CHECK(!result.vector.is_empty());
	CHECK(result.effective_provider == "hash_vector");
	CHECK(result.used_fallback);
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/embedding_provider", prev_provider);
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/embedding_http_url", prev_url);
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/embedding_http_timeout_ms", prev_timeout);
	SemanticSearchBackendFactory::invalidate_embedding_provider_cache();
	HttpEmbeddingProvider::set_http_embedding_unlocked(prev_unlocked);
}

void test_semantic_http_fallback_provider_tag() {
	const String prev_provider = String(ProjectSettings::get_singleton()->get_setting("blazium/semanticsearch/embedding_provider"));
	const String prev_url = String(ProjectSettings::get_singleton()->get_setting("blazium/semanticsearch/embedding_http_url"));
	const String prev_backend = String(ProjectSettings::get_singleton()->get_setting("blazium/semanticsearch/backend"));
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/backend", "hybrid");
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/embedding_provider", "http");
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/embedding_http_url", "http://127.0.0.1:1/embeddings");
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/embedding_http_timeout_ms", 500);
	SemanticSearchBackendFactory::invalidate_session_backend();
	SemanticSearchBackendFactory::invalidate_embedding_provider_cache();
	SemanticAssetIndex index;
	index.clear();
	CHECK(index.upsert_entry("res://hero.tscn") == OK);
	const Dictionary entry = index.get_asset_entry("res://hero.tscn");
	CHECK(entry.get("ok", false));
	CHECK(String(entry.get("embedding_provider", "")) == "hash_vector");
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/embedding_provider", prev_provider);
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/embedding_http_url", prev_url);
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/backend", prev_backend);
	SemanticSearchBackendFactory::invalidate_session_backend();
	SemanticSearchBackendFactory::invalidate_embedding_provider_cache();
}

void test_semantic_per_entry_stale_search() {
	const String prev_provider = String(ProjectSettings::get_singleton()->get_setting("blazium/semanticsearch/embedding_provider"));
	const String prev_backend = String(ProjectSettings::get_singleton()->get_setting("blazium/semanticsearch/backend"));
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	project_settings->set_block_signals(true);
	project_settings->set_setting("blazium/semanticsearch/backend", "embedding");
	project_settings->set_setting("blazium/semanticsearch/embedding_provider", "ngram");
	project_settings->set_block_signals(false);
	SemanticSearchBackendFactory::invalidate_session_backend();
	SemanticSearchBackendFactory::invalidate_embedding_provider_cache();
	SemanticAssetIndex *index = semantic_test_index();
	index->clear();
	CHECK(index->upsert_entry("res://fresh.tscn") == OK);
	project_settings->set_block_signals(true);
	project_settings->set_setting("blazium/semanticsearch/embedding_provider", "hash_vector");
	project_settings->set_block_signals(false);
	SemanticSearchBackendFactory::invalidate_session_backend();
	SemanticSearchBackendFactory::invalidate_embedding_provider_cache();
	CHECK(index->upsert_entry("res://stale.tscn") == OK);
	project_settings->set_block_signals(true);
	project_settings->set_setting("blazium/semanticsearch/embedding_provider", "ngram");
	project_settings->set_block_signals(false);
	SemanticSearchBackendFactory::invalidate_session_backend();
	SemanticSearchBackendFactory::invalidate_embedding_provider_cache();
	CHECK(index->count_stale_embeddings_for_active_provider() == 1);
	CHECK(!index->has_stale_embeddings_for_active_provider());
	Ref<SemanticSearchBackend> backend = SemanticSearchBackendFactory::create_active_backend();
	Array results = backend->search_with_filters("fresh.tscn", 5, PackedStringArray(), false, String(), String());
	bool found_fresh = false;
	bool found_stale = false;
	for (int i = 0; i < results.size(); i++) {
		const String path = String(results[i].operator Dictionary().get("path", ""));
		if (path == "res://fresh.tscn") {
			found_fresh = true;
		}
		if (path == "res://stale.tscn") {
			found_stale = true;
		}
	}
	CHECK(found_fresh);
	CHECK(!found_stale);
	project_settings->set_block_signals(true);
	project_settings->set_setting("blazium/semanticsearch/embedding_provider", prev_provider);
	project_settings->set_setting("blazium/semanticsearch/backend", prev_backend);
	project_settings->set_block_signals(false);
	SemanticSearchBackendFactory::invalidate_session_backend();
	SemanticSearchBackendFactory::invalidate_embedding_provider_cache();
}

static const char *HTTP_EMBED_MOCK_URL = "http://127.0.0.1:8091/embeddings";

static bool _http_embed_mock_reachable() {
	Ref<HTTPClient> client = HTTPClient::create();
	if (client->connect_to_host("127.0.0.1", 8091) != OK) {
		return false;
	}
	const uint64_t start = OS::get_singleton()->get_ticks_msec();
	while (client->get_status() == HTTPClient::STATUS_CONNECTING || client->get_status() == HTTPClient::STATUS_RESOLVING) {
		if (int(OS::get_singleton()->get_ticks_msec() - start) > 2000) {
			return false;
		}
		client->poll();
		OS::get_singleton()->delay_usec(1000);
	}
	if (client->get_status() != HTTPClient::STATUS_CONNECTED) {
		return false;
	}
	Vector<String> headers;
	headers.push_back("Accept: application/json");
	if (client->request(HTTPClient::METHOD_GET, "/health", headers, nullptr, 0) != OK) {
		return false;
	}
	while (client->get_status() == HTTPClient::STATUS_REQUESTING) {
		if (int(OS::get_singleton()->get_ticks_msec() - start) > 2000) {
			return false;
		}
		client->poll();
		OS::get_singleton()->delay_usec(1000);
	}
	return client->has_response() && client->get_response_code() >= 200 && client->get_response_code() < 300;
}

static void _restore_semantic_http_settings(const String &p_provider, const String &p_url, const String &p_backend, int p_timeout) {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	ps->set_block_signals(true);
	ps->set_setting("blazium/semanticsearch/embedding_provider", p_provider);
	ps->set_setting("blazium/semanticsearch/embedding_http_url", p_url);
	ps->set_setting("blazium/semanticsearch/backend", p_backend);
	ps->set_setting("blazium/semanticsearch/embedding_http_timeout_ms", p_timeout);
	ps->set_block_signals(false);
	SemanticSearchBackendFactory::invalidate_session_backend();
	SemanticSearchBackendFactory::invalidate_embedding_provider_cache();
	SemanticQueryEmbedCache::invalidate();
}

void test_semantic_http_embed_provider_live() {
	if (!_http_embed_mock_reachable()) {
		MESSAGE("Skipping: HTTP embedding mock not available at http://127.0.0.1:8091");
		return;
	}

	const String prev_provider = String(ProjectSettings::get_singleton()->get_setting("blazium/semanticsearch/embedding_provider"));
	const String prev_url = String(ProjectSettings::get_singleton()->get_setting("blazium/semanticsearch/embedding_http_url"));
	const String prev_backend = String(ProjectSettings::get_singleton()->get_setting("blazium/semanticsearch/backend"));
	const int prev_timeout = int(ProjectSettings::get_singleton()->get_setting("blazium/semanticsearch/embedding_http_timeout_ms"));

	ProjectSettings::get_singleton()->set_block_signals(true);
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/embedding_provider", "http");
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/embedding_http_url", HTTP_EMBED_MOCK_URL);
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/embedding_http_timeout_ms", 5000);
	ProjectSettings::get_singleton()->set_block_signals(false);
	SemanticSearchBackendFactory::invalidate_embedding_provider_cache();

	const bool prev_unlocked = HttpEmbeddingProvider::is_http_embedding_unlocked();
	HttpEmbeddingProvider::set_http_embedding_unlocked(true);

	HttpEmbeddingProvider provider;
	Vector<String> tokens;
	tokens.push_back("hero");
	tokens.push_back("crate");
	const EmbeddingResult result = provider.embed_tokens_result(tokens);
	CHECK_MESSAGE(!result.used_fallback, "HttpEmbeddingProvider must not fall back when mock is healthy");
	CHECK(result.effective_provider == "http");
	CHECK(result.vector.size() == HashVectorEmbedding::DEFAULT_DIM);
	CHECK(HashVectorEmbedding::is_valid_embedding_dim(result.vector.size()));

	double norm = 0.0;
	for (int i = 0; i < result.vector.size(); i++) {
		norm += result.vector[i] * result.vector[i];
	}
	CHECK(Math::is_equal_approx(Math::sqrt(norm), 1.0));

	_restore_semantic_http_settings(prev_provider, prev_url, prev_backend, prev_timeout);
	HttpEmbeddingProvider::set_http_embedding_unlocked(prev_unlocked);
}

void test_semantic_http_embed_index_search() {
	if (!_http_embed_mock_reachable()) {
		MESSAGE("Skipping: HTTP embedding mock not available at http://127.0.0.1:8091");
		return;
	}

	const String prev_provider = String(ProjectSettings::get_singleton()->get_setting("blazium/semanticsearch/embedding_provider"));
	const String prev_url = String(ProjectSettings::get_singleton()->get_setting("blazium/semanticsearch/embedding_http_url"));
	const String prev_backend = String(ProjectSettings::get_singleton()->get_setting("blazium/semanticsearch/backend"));
	const int prev_timeout = int(ProjectSettings::get_singleton()->get_setting("blazium/semanticsearch/embedding_http_timeout_ms"));
	const bool prev_unlocked = HttpEmbeddingProvider::is_http_embedding_unlocked();
	HttpEmbeddingProvider::set_http_embedding_unlocked(true);

	ProjectSettings::get_singleton()->set_block_signals(true);
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/backend", "embedding");
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/embedding_provider", "http");
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/embedding_http_url", HTTP_EMBED_MOCK_URL);
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/embedding_http_timeout_ms", 5000);
	ProjectSettings::get_singleton()->set_block_signals(false);
	SemanticSearchBackendFactory::invalidate_session_backend();
	SemanticSearchBackendFactory::invalidate_embedding_provider_cache();
	SemanticQueryEmbedCache::invalidate();

	SemanticAssetIndex *index = semantic_test_index();
	index->clear();
	CHECK(index->upsert_entry("res://http_hero_character.tscn") == OK);
	CHECK(index->upsert_entry("res://http_unrelated_prop.tscn") == OK);

	const Dictionary hero = index->get_asset_entry("res://http_hero_character.tscn");
	CHECK(hero.get("ok", false));
	CHECK(String(hero.get("embedding_provider", "")) == "http");
	CHECK(Array(hero.get("embedding_vector", Array())).size() == HashVectorEmbedding::DEFAULT_DIM);

	const Dictionary prop = index->get_asset_entry("res://http_unrelated_prop.tscn");
	CHECK(prop.get("ok", false));
	CHECK(String(prop.get("embedding_provider", "")) == "http");

	CHECK(!index->has_stale_embeddings_for_active_provider());
	CHECK(index->count_searchable_embeddings_for_active_provider() >= 2);

	Ref<SemanticSearchBackend> backend = SemanticSearchBackendFactory::create_active_backend();
	CHECK(backend.is_valid());

	HttpEmbeddingProvider provider;
	Vector<String> hero_tokens;
	{
		const Array token_array = hero.get("tokens", Array());
		for (int i = 0; i < token_array.size(); i++) {
			hero_tokens.push_back(String(token_array[i]));
		}
	}
	CHECK(!hero_tokens.is_empty());
	const EmbeddingResult query_embed = provider.embed_tokens_result(hero_tokens);
	CHECK_MESSAGE(!query_embed.used_fallback, "Query embed over HTTP must succeed");
	CHECK(query_embed.effective_provider == "http");
	CHECK(query_embed.vector.size() == HashVectorEmbedding::DEFAULT_DIM);

	Array ann = index->vector_search_top_k(query_embed.vector, 5, HashSet<String>(), HashSet<String>());
	CHECK(ann.size() >= 1);
	CHECK(String(Dictionary(ann[0]).get("path", "")) == "res://http_hero_character.tscn");
	CHECK(double(Dictionary(ann[0]).get("score", -1.0)) > 0.99);

	Array results = backend->search_with_filters("http_hero_character.tscn", 5, PackedStringArray(), false, String(), String());
	CHECK_MESSAGE(results.size() >= 1, vformat("backend search empty; filter_error=%s", index->get_last_filter_error()));
	CHECK(String(Dictionary(results[0]).get("path", "")) == "res://http_hero_character.tscn");

	Array similar = backend->find_similar("res://http_hero_character.tscn", 5);
	CHECK(similar.size() >= 1);
	CHECK(String(Dictionary(similar[0]).get("path", "")) == "res://http_unrelated_prop.tscn");

	Dictionary stats = index->get_stats();
	CHECK(int(stats.get("vector_index_entries", 0)) >= 2);
	CHECK(String(stats.get("embedding_mode", "")) == "http");

	index->clear();
	_restore_semantic_http_settings(prev_provider, prev_url, prev_backend, prev_timeout);
	HttpEmbeddingProvider::set_http_embedding_unlocked(prev_unlocked);
}

void test_semantic_http_embed_assettags_filter() {
	if (!_http_embed_mock_reachable()) {
		MESSAGE("Skipping: HTTP embedding mock not available at http://127.0.0.1:8091");
		return;
	}

#ifdef MODULE_ASSETTAGS_ENABLED
	const String prev_provider = String(ProjectSettings::get_singleton()->get_setting("blazium/semanticsearch/embedding_provider"));
	const String prev_url = String(ProjectSettings::get_singleton()->get_setting("blazium/semanticsearch/embedding_http_url"));
	const String prev_backend = String(ProjectSettings::get_singleton()->get_setting("blazium/semanticsearch/backend"));
	const int prev_timeout = int(ProjectSettings::get_singleton()->get_setting("blazium/semanticsearch/embedding_http_timeout_ms"));
	const bool prev_unlocked = HttpEmbeddingProvider::is_http_embedding_unlocked();
	HttpEmbeddingProvider::set_http_embedding_unlocked(true);

	AssetTagStorage::set_test_storage_dir("res://.blazium/test_http_embed_assettags");
	AssetTagManager manager;
	AssetTagRegistry registry;
	CHECK(manager.add_tag("Http.Embed.Hero") == OK);

	ProjectSettings::get_singleton()->set_block_signals(true);
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/backend", "hybrid");
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/embedding_provider", "http");
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/embedding_http_url", HTTP_EMBED_MOCK_URL);
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/embedding_http_timeout_ms", 5000);
	ProjectSettings::get_singleton()->set_block_signals(false);
	SemanticSearchBackendFactory::invalidate_session_backend();
	SemanticSearchBackendFactory::invalidate_embedding_provider_cache();
	SemanticQueryEmbedCache::invalidate();

	SemanticAssetIndex *index = semantic_test_index();
	index->clear();

	PackedStringArray hero_tags;
	hero_tags.push_back("Http.Embed.Hero");
	CHECK(registry.set_tags_for_asset("res://http_tagged_hero.tscn", hero_tags) == OK);
	CHECK(registry.commit_batch() == OK);
	CHECK(registry.get_tags_for_asset("res://http_tagged_hero.tscn").has("Http.Embed.Hero"));

	CHECK(index->upsert_entry("res://http_tagged_hero.tscn") == OK);
	CHECK(index->upsert_entry("res://http_untagged_tree.tscn") == OK);

	const Dictionary tagged = index->get_asset_entry("res://http_tagged_hero.tscn");
	CHECK(String(tagged.get("embedding_provider", "")) == "http");
	const String tagged_caption = String(tagged.get("caption", ""));
	const bool caption_has_tag = tagged_caption.containsn("Http.Embed.Hero");
	CHECK_MESSAGE(caption_has_tag, vformat("Expected tag in caption, got: %s", tagged_caption));

	Ref<SemanticSearchBackend> backend = SemanticSearchBackendFactory::create_active_backend();
	PackedStringArray filter_tags;
	filter_tags.push_back("Http.Embed.Hero");
	Array filtered = backend->search_with_filters("http_tagged_hero.tscn", 10, filter_tags, true, String(), String());
	CHECK_MESSAGE(filtered.size() >= 1, vformat("tagged search empty; filter_error=%s", index->get_last_filter_error()));
	bool saw_tagged = false;
	bool saw_untagged = false;
	for (int i = 0; i < filtered.size(); i++) {
		const String path = String(Dictionary(filtered[i]).get("path", ""));
		if (path == "res://http_tagged_hero.tscn") {
			saw_tagged = true;
		}
		if (path == "res://http_untagged_tree.tscn") {
			saw_untagged = true;
		}
	}
	CHECK(saw_tagged);
	CHECK(!saw_untagged);

	index->clear();
	AssetTagStorage::clear_test_storage_dir();
	_restore_semantic_http_settings(prev_provider, prev_url, prev_backend, prev_timeout);
	HttpEmbeddingProvider::set_http_embedding_unlocked(prev_unlocked);
#else
	REQUIRE_MESSAGE(false, "MODULE_ASSETTAGS_ENABLED required to validate HTTP embeddings with AssetTags");
#endif
}

#endif
