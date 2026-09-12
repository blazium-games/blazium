/**************************************************************************/
/*  semantic_search_backend_factory.cpp                                   */
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

#include "semantic_search_backend_factory.h"

#include "embedding_backend.h"
#include "embedding_provider.h"
#include "lexical_tag_backend.h"
#include "semantic_query_embed_cache.h"

#include "core/config/project_settings.h"
#include "core/os/mutex.h"

static Ref<SemanticSearchBackend> g_session_backend;
static Ref<EmbeddingProvider> g_embedding_provider;
static Mutex g_factory_mutex;

Ref<SemanticSearchBackend> SemanticSearchBackendFactory::create_active_backend() {
	MutexLock lock(g_factory_mutex);
	if (g_session_backend.is_null()) {
		const String configured = get_active_backend_name();
		if (configured == "embedding" || configured == "hybrid") {
			g_session_backend = Ref<EmbeddingBackend>(memnew(EmbeddingBackend));
		} else {
			g_session_backend = Ref<LexicalTagBackend>(memnew(LexicalTagBackend));
		}
	}
	return g_session_backend;
}

void SemanticSearchBackendFactory::invalidate_session_backend() {
	MutexLock lock(g_factory_mutex);
	g_session_backend.unref();
	g_embedding_provider.unref();
	SemanticQueryEmbedCache::invalidate();
}

void SemanticSearchBackendFactory::invalidate_embedding_provider_cache() {
	MutexLock lock(g_factory_mutex);
	g_embedding_provider.unref();
	SemanticQueryEmbedCache::invalidate();
}

String SemanticSearchBackendFactory::get_active_backend_name() {
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/semanticsearch/backend")) {
		return String(GLOBAL_GET("blazium/semanticsearch/backend")).strip_edges().to_lower();
	}
	return "lexical";
}

String SemanticSearchBackendFactory::get_embedding_provider_name() {
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/semanticsearch/embedding_provider")) {
		return String(GLOBAL_GET("blazium/semanticsearch/embedding_provider")).strip_edges().to_lower();
	}
	return "hash_vector";
}

String SemanticSearchBackendFactory::get_effective_backend_name() {
	const String configured = get_active_backend_name();
	const String provider = get_embedding_provider_name();
	if (configured == "embedding") {
		return provider == "hash_vector" ? "embedding_v3_hash_vector" : "embedding_v3_" + provider;
	}
	if (configured == "hybrid") {
		return provider == "hash_vector" ? "hybrid_v3_hash_vector" : "hybrid_v3_" + provider;
	}
	return configured;
}

Ref<EmbeddingProvider> SemanticSearchBackendFactory::create_embedding_provider() {
	MutexLock lock(g_factory_mutex);
	if (g_embedding_provider.is_null()) {
		const String provider = get_embedding_provider_name();
		if (provider == "ngram") {
			g_embedding_provider = Ref<NgramEmbeddingProvider>(memnew(NgramEmbeddingProvider));
		} else if (provider == "http") {
			g_embedding_provider = Ref<HttpEmbeddingProvider>(memnew(HttpEmbeddingProvider));
		} else {
			g_embedding_provider = Ref<HashVectorEmbeddingProvider>(memnew(HashVectorEmbeddingProvider));
		}
	}
	return g_embedding_provider;
}
