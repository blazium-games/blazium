/**************************************************************************/
/*  embedding_provider.cpp                                                */
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

#include "embedding_provider.h"

#include "hash_vector_embedding.h"
#include "lexical_search_engine.h"

#include "core/config/project_settings.h"
#include "core/io/http_client.h"
#include "core/io/json.h"
#include "core/math/math_funcs.h"
#include "core/os/os.h"
#include "core/os/thread.h"

namespace {
#ifdef TOOLS_ENABLED
// Locked until EditorNode finishes first-scan project load (see register_types).
bool g_http_embedding_unlocked = false;
#else
bool g_http_embedding_unlocked = true;
#endif
} //namespace

void EmbeddingProvider::_bind_methods() {}

EmbeddingResult EmbeddingProvider::embed_tokens_result(const Vector<String> &p_tokens) const {
	EmbeddingResult result;
	result.vector = embed_tokens(p_tokens);
	result.effective_provider = get_provider_name();
	result.used_fallback = false;
	return result;
}

void HashVectorEmbeddingProvider::_bind_methods() {}

void NgramEmbeddingProvider::_bind_methods() {}

void HttpEmbeddingProvider::_bind_methods() {}

bool HttpEmbeddingProvider::is_http_embedding_unlocked() {
	return g_http_embedding_unlocked;
}

void HttpEmbeddingProvider::set_http_embedding_unlocked(bool p_unlocked) {
	g_http_embedding_unlocked = p_unlocked;
}

Vector<double> HashVectorEmbeddingProvider::embed_tokens(const Vector<String> &p_tokens) const {
	return embed_tokens_result(p_tokens).vector;
}

EmbeddingResult HashVectorEmbeddingProvider::embed_tokens_result(const Vector<String> &p_tokens) const {
	EmbeddingResult result;
	result.vector = HashVectorEmbedding::from_tokens(p_tokens);
	result.effective_provider = get_provider_name();
	result.used_fallback = false;
	return result;
}

Vector<double> NgramEmbeddingProvider::embed_tokens(const Vector<String> &p_tokens) const {
	return embed_tokens_result(p_tokens).vector;
}

EmbeddingResult NgramEmbeddingProvider::embed_tokens_result(const Vector<String> &p_tokens) const {
	EmbeddingResult result;
	Vector<double> base = HashVectorEmbedding::from_tokens(p_tokens);
	for (int i = 0; i + 1 < p_tokens.size(); i++) {
		const String bigram = p_tokens[i] + "_" + p_tokens[i + 1];
		Vector<String> bigram_tokens;
		bigram_tokens.push_back(bigram);
		const Vector<double> bigram_vec = HashVectorEmbedding::from_tokens(bigram_tokens);
		for (int j = 0; j < base.size() && j < bigram_vec.size(); j++) {
			base.write[j] += bigram_vec[j] * 0.35;
		}
	}
	double norm = 0.0;
	for (int i = 0; i < base.size(); i++) {
		norm += base[i] * base[i];
	}
	norm = Math::sqrt(norm);
	if (norm > 0.0) {
		for (int i = 0; i < base.size(); i++) {
			base.write[i] /= norm;
		}
	}
	result.vector = base;
	result.effective_provider = get_provider_name();
	result.used_fallback = false;
	return result;
}

static Vector<double> _parse_embedding_response(const String &p_body) {
	Vector<double> result;
	Ref<JSON> json;
	json.instantiate();
	if (json->parse(p_body) != OK) {
		return result;
	}
	Variant parsed = json->get_data();
	Array embedding_array;
	if (parsed.get_type() == Variant::ARRAY) {
		embedding_array = parsed;
	} else if (parsed.get_type() == Variant::DICTIONARY) {
		Dictionary root = parsed;
		if (root.has("embedding") && root["embedding"].get_type() == Variant::ARRAY) {
			embedding_array = root["embedding"];
		} else if (root.has("data") && root["data"].get_type() == Variant::ARRAY && root["data"].operator Array().size() > 0) {
			const Variant first = root["data"].operator Array()[0];
			if (first.get_type() == Variant::DICTIONARY) {
				const Dictionary item = first;
				if (item.has("embedding") && item["embedding"].get_type() == Variant::ARRAY) {
					embedding_array = item["embedding"];
				}
			}
		}
	}
	for (int i = 0; i < embedding_array.size(); i++) {
		result.push_back(double(embedding_array[i]));
	}
	return result;
}

static int _http_embedding_timeout_ms() {
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/semanticsearch/embedding_http_timeout_ms")) {
		return MAX(int(GLOBAL_GET("blazium/semanticsearch/embedding_http_timeout_ms")), 1000);
	}
	return 30000;
}

static bool _http_poll_timed_out(uint64_t p_start_msec, int p_timeout_ms) {
	return int(OS::get_singleton()->get_ticks_msec() - p_start_msec) >= p_timeout_ms;
}

static EmbeddingResult _http_hash_fallback(const Vector<String> &p_tokens, const char *p_reason) {
	WARN_PRINT_ONCE(p_reason);
	EmbeddingResult result;
	result.vector = HashVectorEmbedding::from_tokens(p_tokens);
	result.effective_provider = "hash_vector";
	result.used_fallback = true;
	return result;
}

Vector<double> HttpEmbeddingProvider::embed_tokens(const Vector<String> &p_tokens) const {
	return embed_tokens_result(p_tokens).vector;
}

EmbeddingResult HttpEmbeddingProvider::embed_tokens_result(const Vector<String> &p_tokens) const {
	if (!is_http_embedding_unlocked()) {
		return _http_hash_fallback(p_tokens, "HttpEmbeddingProvider: deferred until editor project load completes; using hash_vector fallback.");
	}
	if (!Thread::is_main_thread()) {
		return _http_hash_fallback(p_tokens, "HttpEmbeddingProvider: HTTP embedding is main-thread only; using hash_vector fallback.");
	}
	if (!ProjectSettings::get_singleton() || !ProjectSettings::get_singleton()->has_setting("blazium/semanticsearch/embedding_http_url")) {
		return _http_hash_fallback(p_tokens, "HttpEmbeddingProvider: embedding_http_url not configured; using hash_vector fallback.");
	}
	const String url = String(GLOBAL_GET("blazium/semanticsearch/embedding_http_url")).strip_edges();
	if (url.is_empty()) {
		return _http_hash_fallback(p_tokens, "HttpEmbeddingProvider: embedding_http_url is empty; using hash_vector fallback.");
	}

	String scheme;
	String host;
	int port = 80;
	String path;
	String fragment;
	if (url.parse_url(scheme, host, port, path, fragment) != OK || host.is_empty()) {
		return _http_hash_fallback(p_tokens, "HttpEmbeddingProvider: invalid embedding_http_url; using hash_vector fallback.");
	}
	if (path.is_empty()) {
		path = "/embeddings";
	}
	if (port < 0) {
		port = scheme == "https" ? 443 : 80;
	}

	String joined;
	for (int i = 0; i < p_tokens.size(); i++) {
		if (i > 0) {
			joined += " ";
		}
		joined += p_tokens[i];
	}
	Dictionary payload;
	payload["input"] = joined;
	const String body = JSON::stringify(payload);

	const int timeout_ms = _http_embedding_timeout_ms();
	const uint64_t start_msec = OS::get_singleton()->get_ticks_msec();

	Ref<HTTPClient> client = HTTPClient::create();
	Ref<TLSOptions> tls;
	if (scheme == "https") {
		tls = TLSOptions::client();
	}
	if (client->connect_to_host(host, port, tls) != OK) {
		return _http_hash_fallback(p_tokens, "HttpEmbeddingProvider: failed to connect; using hash_vector fallback.");
	}

	while (client->get_status() == HTTPClient::STATUS_CONNECTING || client->get_status() == HTTPClient::STATUS_RESOLVING) {
		if (_http_poll_timed_out(start_msec, timeout_ms)) {
			return _http_hash_fallback(p_tokens, "HttpEmbeddingProvider: connect timed out; using hash_vector fallback.");
		}
		client->poll();
		OS::get_singleton()->delay_usec(1000);
	}
	if (client->get_status() != HTTPClient::STATUS_CONNECTED) {
		return _http_hash_fallback(p_tokens, "HttpEmbeddingProvider: connection failed; using hash_vector fallback.");
	}

	Vector<String> headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back("Accept: application/json");
	const PackedByteArray body_bytes = body.to_utf8_buffer();
	Error req_err = client->request(HTTPClient::METHOD_POST, path, headers, body_bytes.ptr(), body_bytes.size());
	if (req_err != OK) {
		return _http_hash_fallback(p_tokens, "HttpEmbeddingProvider: request failed; using hash_vector fallback.");
	}

	while (client->get_status() == HTTPClient::STATUS_REQUESTING) {
		if (_http_poll_timed_out(start_msec, timeout_ms)) {
			return _http_hash_fallback(p_tokens, "HttpEmbeddingProvider: request timed out; using hash_vector fallback.");
		}
		client->poll();
		OS::get_singleton()->delay_usec(1000);
	}
	if (client->get_status() != HTTPClient::STATUS_BODY && client->get_status() != HTTPClient::STATUS_CONNECTED) {
		return _http_hash_fallback(p_tokens, "HttpEmbeddingProvider: bad HTTP status; using hash_vector fallback.");
	}

	if (!client->has_response()) {
		return _http_hash_fallback(p_tokens, "HttpEmbeddingProvider: empty HTTP response; using hash_vector fallback.");
	}
	if (client->get_response_code() < 200 || client->get_response_code() >= 300) {
		return _http_hash_fallback(p_tokens, "HttpEmbeddingProvider: non-2xx HTTP response; using hash_vector fallback.");
	}

	PackedByteArray response_bytes;
	while (client->get_status() == HTTPClient::STATUS_BODY) {
		if (_http_poll_timed_out(start_msec, timeout_ms)) {
			return _http_hash_fallback(p_tokens, "HttpEmbeddingProvider: response body timed out; using hash_vector fallback.");
		}
		client->poll();
		response_bytes.append_array(client->read_response_body_chunk());
		OS::get_singleton()->delay_usec(1000);
	}

	String response_text;
	response_text.parse_utf8(reinterpret_cast<const char *>(response_bytes.ptr()), response_bytes.size());
	Vector<double> embedding = _parse_embedding_response(response_text);
	if (embedding.is_empty()) {
		return _http_hash_fallback(p_tokens, "HttpEmbeddingProvider: could not parse embedding response; using hash_vector fallback.");
	}
	if (!HashVectorEmbedding::is_valid_embedding_dim(embedding.size())) {
		WARN_PRINT_ONCE(vformat("HttpEmbeddingProvider: embedding dimension %d does not match expected %d.", embedding.size(), HashVectorEmbedding::DEFAULT_DIM));
		EmbeddingResult result;
		result.vector.clear();
		result.effective_provider = String();
		result.used_fallback = true;
		return result;
	}
	EmbeddingResult result;
	result.vector = embedding;
	result.effective_provider = get_provider_name();
	result.used_fallback = false;
	return result;
}
