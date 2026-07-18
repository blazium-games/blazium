/**************************************************************************/
/*  semantic_query_embed_cache.cpp                                        */
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

#include "semantic_query_embed_cache.h"

#include "embedding_provider.h"
#include "semantic_search_backend_factory.h"

#include "core/os/time.h"

HashMap<uint64_t, SemanticQueryEmbedCache::CacheEntry> SemanticQueryEmbedCache::cache;
Vector<uint64_t> SemanticQueryEmbedCache::lru_order;
String SemanticQueryEmbedCache::active_provider;
Mutex SemanticQueryEmbedCache::cache_mutex;

static uint64_t _hash_query_tokens(const Vector<String> &p_tokens) {
	uint64_t hash = 5381;
	for (int i = 0; i < p_tokens.size(); i++) {
		hash = hash * 33 + (uint64_t)p_tokens[i].hash();
	}
	return hash;
}

void SemanticQueryEmbedCache::_touch_lru(uint64_t p_key) {
	for (int i = 0; i < lru_order.size(); i++) {
		if (lru_order[i] == p_key) {
			lru_order.remove_at(i);
			break;
		}
	}
	lru_order.push_back(p_key);
}

void SemanticQueryEmbedCache::_evict_expired_and_overflow() {
	const uint64_t now_usec = Time::get_singleton()->get_ticks_usec();
	Vector<uint64_t> expired;
	for (const KeyValue<uint64_t, CacheEntry> &kv : cache) {
		if (kv.value.last_access_usec > 0 && now_usec - kv.value.last_access_usec > TTL_USEC) {
			expired.push_back(kv.key);
		}
	}
	for (int i = 0; i < expired.size(); i++) {
		cache.erase(expired[i]);
		for (int j = 0; j < lru_order.size(); j++) {
			if (lru_order[j] == expired[i]) {
				lru_order.remove_at(j);
				break;
			}
		}
	}
	while (cache.size() > MAX_ENTRIES && !lru_order.is_empty()) {
		const uint64_t oldest = lru_order[0];
		lru_order.remove_at(0);
		cache.erase(oldest);
	}
}

void SemanticQueryEmbedCache::invalidate() {
	MutexLock lock(cache_mutex);
	cache.clear();
	lru_order.clear();
	active_provider = String();
}

void SemanticQueryEmbedCache::set_active_provider(const String &p_provider) {
	if (active_provider != p_provider) {
		cache.clear();
		lru_order.clear();
		active_provider = p_provider;
	}
}

Vector<double> SemanticQueryEmbedCache::get_or_embed(const Vector<String> &p_tokens) {
	const String provider = SemanticSearchBackendFactory::get_embedding_provider_name();
	const uint64_t token_hash = _hash_query_tokens(p_tokens);
	{
		MutexLock lock(cache_mutex);
		set_active_provider(provider);
		_evict_expired_and_overflow();
		if (cache.has(token_hash)) {
			CacheEntry &cached = cache[token_hash];
			if (cached.provider == provider && cached.token_hash == token_hash) {
				cached.last_access_usec = Time::get_singleton()->get_ticks_usec();
				_touch_lru(token_hash);
				return cached.vector;
			}
		}
	}
	const Ref<EmbeddingProvider> embed_provider = SemanticSearchBackendFactory::create_embedding_provider();
	const Vector<double> vector = embed_provider->embed_tokens_result(p_tokens).vector;
	CacheEntry entry;
	entry.vector = vector;
	entry.token_hash = token_hash;
	entry.provider = provider;
	entry.last_access_usec = Time::get_singleton()->get_ticks_usec();
	{
		MutexLock lock(cache_mutex);
		set_active_provider(provider);
		cache.insert(token_hash, entry);
		_touch_lru(token_hash);
		_evict_expired_and_overflow();
	}
	return vector;
}
