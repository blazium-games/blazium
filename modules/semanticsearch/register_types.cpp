/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "core/config/project_settings.h"
#include "core/object/class_db.h"
#include "embedding_backend.h"
#include "embedding_provider.h"
#include "lexical_tag_backend.h"
#include "semantic_asset_index.h"
#include "semantic_async_embed_worker.h"
#include "semantic_async_search_worker.h"
#include "semantic_search_backend.h"
#include "semantic_search_backend_factory.h"

#include "modules/modules_enabled.gen.h"

#ifdef MODULE_ASSETTAGS_ENABLED
#include "modules/assettags/asset_tag_registry.h"
#include "semantic_assettags_bridge.h"
#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"
#endif
#endif

#ifdef TOOLS_ENABLED
#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#endif

#ifdef TESTS_ENABLED
#include "tests/test_semantic_asset_index.cpp"
#include "tests/test_semantic_async_embed_worker.cpp"
#include "tests/test_semantic_async_embed_worker.h"
#include "tests/test_semantic_async_search_worker.cpp"
#include "tests/test_semantic_bm25_index.cpp"
#include "tests/test_semantic_filter_snapshot.cpp"
#include "tests/test_semantic_filter_snapshot.h"
#include "tests/test_semantic_query_embed_cache.cpp"
#include "tests/test_semantic_vector_index.cpp"
#include "tests/test_semantic_vector_index.h"
#ifdef TOOLS_ENABLED
#include "tests/test_semantic_assettags_bridge.cpp"
#endif
#endif

static String g_last_semantic_backend;
static String g_last_semantic_provider;
static String g_last_semantic_http_url;
static String g_cached_semantic_backend;
static String g_cached_semantic_provider;
static String g_cached_semantic_http_url;
static int g_cached_semantic_ttl_ms = 1800000;

#ifdef TOOLS_ENABLED
static Callable g_http_embed_unlock_scan_cb;

static void _unlock_http_embedding_deferred() {
	HttpEmbeddingProvider::set_http_embedding_unlocked(true);
}

static void _on_first_scan_unlock_http_embedding(bool p_exist) {
	(void)p_exist;
	if (EditorFileSystem *efs = EditorFileSystem::get_singleton()) {
		if (g_http_embed_unlock_scan_cb.is_valid() && efs->is_connected("sources_changed", g_http_embed_unlock_scan_cb)) {
			efs->disconnect("sources_changed", g_http_embed_unlock_scan_cb);
		}
	}
	g_http_embed_unlock_scan_cb = Callable();
	// Deferred so EditorNode::_sources_changed (layout/scene load) finishes first.
	callable_mp_static(_unlock_http_embedding_deferred).call_deferred();
}

static void _semanticsearch_schedule_http_embedding_unlock() {
	HttpEmbeddingProvider::set_http_embedding_unlocked(false);
	EditorFileSystem *efs = EditorFileSystem::get_singleton();
	if (!efs) {
		callable_mp_static(_unlock_http_embedding_deferred).call_deferred();
		return;
	}
	if (efs->get_filesystem() != nullptr && EditorNode::get_singleton() && EditorNode::get_singleton()->is_editor_ready()) {
		callable_mp_static(_unlock_http_embedding_deferred).call_deferred();
		return;
	}
	g_http_embed_unlock_scan_cb = callable_mp_static(_on_first_scan_unlock_http_embedding);
	if (!efs->is_connected("sources_changed", g_http_embed_unlock_scan_cb)) {
		efs->connect("sources_changed", g_http_embed_unlock_scan_cb);
	}
}
#endif

#if defined(MODULE_ASSETTAGS_ENABLED) && defined(TOOLS_ENABLED)
static void _semanticsearch_attach_assettags_bridge() {
	if (AssetTagRegistry *registry = AssetTagRegistry::get_singleton()) {
		SemanticAssettagsBridge::attach(registry);
	}
}
#endif

static bool _semanticsearch_settings_snapshot_changed() {
	if (!ProjectSettings::get_singleton()) {
		return false;
	}
	const String backend = String(GLOBAL_GET("blazium/semanticsearch/backend"));
	const String provider = String(GLOBAL_GET("blazium/semanticsearch/embedding_provider"));
	const String http_url = String(GLOBAL_GET("blazium/semanticsearch/embedding_http_url"));
	const int ttl_ms = int(GLOBAL_GET("blazium/semanticsearch/async_job_ttl_ms"));
	if (backend == g_cached_semantic_backend &&
			provider == g_cached_semantic_provider &&
			http_url == g_cached_semantic_http_url &&
			ttl_ms == g_cached_semantic_ttl_ms) {
		return false;
	}
	g_cached_semantic_backend = backend;
	g_cached_semantic_provider = provider;
	g_cached_semantic_http_url = http_url;
	g_cached_semantic_ttl_ms = ttl_ms;
	return true;
}

static void _on_semantic_settings_changed() {
	if (!_semanticsearch_settings_snapshot_changed()) {
		return;
	}
	const String backend = SemanticSearchBackendFactory::get_active_backend_name();
	const String provider = SemanticSearchBackendFactory::get_embedding_provider_name();
	const String http_url = String(GLOBAL_GET("blazium/semanticsearch/embedding_http_url"));
	SemanticSearchBackendFactory::invalidate_session_backend();
	const bool provider_changed = backend != g_last_semantic_backend || provider != g_last_semantic_provider;
	const bool http_url_changed = http_url != g_last_semantic_http_url;
	if (!provider_changed && !http_url_changed) {
		return;
	}
	g_last_semantic_backend = backend;
	g_last_semantic_provider = provider;
	g_last_semantic_http_url = http_url;
	if (provider == "http") {
		WARN_PRINT_ONCE("SemanticSearch: skipping sync embedding refresh for HTTP provider (use lexical backend or restart after local provider change).");
		return;
	}
	if (SemanticAssetIndex *index = SemanticAssetIndex::get_singleton()) {
		index->refresh_stale_embeddings_for_active_provider();
	}
}

void initialize_semanticsearch_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		GLOBAL_DEF("blazium/semanticsearch/scan_filesystem", false);
		GLOBAL_DEF("blazium/semanticsearch/backend", "lexical");
		GLOBAL_DEF("blazium/semanticsearch/embedding_provider", "hash_vector");
		GLOBAL_DEF("blazium/semanticsearch/embedding_http_url", "");
		GLOBAL_DEF("blazium/semanticsearch/embedding_http_timeout_ms", 30000);
		GLOBAL_DEF("blazium/semanticsearch/async_job_ttl_ms", 1800000);
		GLOBAL_DEF("blazium/semanticsearch/max_concurrent_async_jobs", 8);
		ProjectSettings::get_singleton()->set_restart_if_changed("blazium/semanticsearch/backend", true);
		ProjectSettings::get_singleton()->set_restart_if_changed("blazium/semanticsearch/embedding_provider", true);
		if (ProjectSettings::get_singleton()) {
			ProjectSettings::get_singleton()->connect("settings_changed", callable_mp_static(&_on_semantic_settings_changed));
		}
		GDREGISTER_ABSTRACT_CLASS(SemanticSearchBackend);
		GDREGISTER_CLASS(LexicalTagBackend);
		GDREGISTER_CLASS(EmbeddingBackend);
		GDREGISTER_CLASS(SemanticAssetIndex);
		GDREGISTER_CLASS(SemanticAsyncSearchWorker);
		GDREGISTER_CLASS(SemanticAsyncEmbedWorker);
		if (!SemanticAssetIndex::get_singleton()) {
			SemanticAssetIndex *index = memnew(SemanticAssetIndex);
#ifdef TESTS_ENABLED
			SemanticAssetIndexTestHooks::set_module_singleton(index);
#endif
			const Error load_err = index->load();
			if (load_err == ERR_PARSE_ERROR || load_err == ERR_UNAVAILABLE) {
				WARN_PRINT("SemanticAssetIndex: index load failed; rebuilding from asset tags.");
				index->rebuild_index();
			} else if (load_err != OK) {
				WARN_PRINT("SemanticAssetIndex: index load returned error; attempting rebuild.");
				index->rebuild_index();
			} else {
				const String provider = SemanticSearchBackendFactory::get_embedding_provider_name();
				if (provider == "http") {
					WARN_PRINT_ONCE("SemanticSearch: skipping sync embedding refresh on init for HTTP provider.");
				} else {
					index->refresh_embeddings_for_active_provider();
				}
			}
			g_last_semantic_backend = SemanticSearchBackendFactory::get_active_backend_name();
			g_last_semantic_provider = SemanticSearchBackendFactory::get_embedding_provider_name();
			g_last_semantic_http_url = String(GLOBAL_GET("blazium/semanticsearch/embedding_http_url"));
			g_cached_semantic_backend = String(GLOBAL_GET("blazium/semanticsearch/backend"));
			g_cached_semantic_provider = String(GLOBAL_GET("blazium/semanticsearch/embedding_provider"));
			g_cached_semantic_http_url = String(GLOBAL_GET("blazium/semanticsearch/embedding_http_url"));
			g_cached_semantic_ttl_ms = int(GLOBAL_GET("blazium/semanticsearch/async_job_ttl_ms"));
		}
#ifdef TESTS_ENABLED
		else if (SemanticAssetIndex *index = SemanticAssetIndex::get_singleton()) {
			SemanticAssetIndexTestHooks::set_module_singleton(index);
		}
#endif
		if (!SemanticAsyncSearchWorker::get_singleton()) {
#ifdef TESTS_ENABLED
			SemanticAsyncSearchWorker *worker = memnew(SemanticAsyncSearchWorker);
			SemanticAsyncSearchWorkerTestHooks::set_module_singleton(worker);
#else
			memnew(SemanticAsyncSearchWorker);
#endif
		}
#ifdef TESTS_ENABLED
		else if (SemanticAsyncSearchWorker *worker = SemanticAsyncSearchWorker::get_singleton()) {
			SemanticAsyncSearchWorkerTestHooks::set_module_singleton(worker);
		}
#endif
		if (!SemanticAsyncEmbedWorker::get_singleton()) {
#ifdef TESTS_ENABLED
			SemanticAsyncEmbedWorker *embed_worker = memnew(SemanticAsyncEmbedWorker);
			SemanticAsyncEmbedWorkerTestHooks::set_module_singleton(embed_worker);
#else
			memnew(SemanticAsyncEmbedWorker);
#endif
		}
#ifdef TESTS_ENABLED
		else if (SemanticAsyncEmbedWorker *embed_worker = SemanticAsyncEmbedWorker::get_singleton()) {
			SemanticAsyncEmbedWorkerTestHooks::set_module_singleton(embed_worker);
		}
#endif
#if defined(MODULE_ASSETTAGS_ENABLED) && defined(TOOLS_ENABLED)
		// Attach after AssetTags' EditorNode init callback creates the registry.
		// Keeping the call site in this module avoids GCC archive link-order failures.
		EditorNode::add_init_callback(_semanticsearch_attach_assettags_bridge);
#endif
#ifdef TOOLS_ENABLED
		EditorNode::add_init_callback(_semanticsearch_schedule_http_embedding_unlock);
#endif
	}
}

void uninitialize_semanticsearch_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
#ifdef TOOLS_ENABLED
		if (EditorFileSystem *efs = EditorFileSystem::get_singleton()) {
			if (g_http_embed_unlock_scan_cb.is_valid() && efs->is_connected("sources_changed", g_http_embed_unlock_scan_cb)) {
				efs->disconnect("sources_changed", g_http_embed_unlock_scan_cb);
			}
		}
		g_http_embed_unlock_scan_cb = Callable();
		HttpEmbeddingProvider::set_http_embedding_unlocked(false);
#endif
#if defined(MODULE_ASSETTAGS_ENABLED) && defined(TOOLS_ENABLED)
		SemanticAssettagsBridge::detach();
#endif
		if (SemanticAsyncSearchWorker *worker = SemanticAsyncSearchWorker::get_singleton()) {
			worker->drain_jobs();
			memdelete(worker);
		}
		if (SemanticAsyncEmbedWorker *embed_worker = SemanticAsyncEmbedWorker::get_singleton()) {
			embed_worker->drain_jobs();
			memdelete(embed_worker);
		}
		if (SemanticAssetIndex *index = SemanticAssetIndex::get_singleton()) {
			index->save();
			memdelete(index);
		}
		SemanticSearchBackendFactory::invalidate_session_backend();
	}
}
