/**************************************************************************/
/*  embedding_provider.h                                                  */
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

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"

struct EmbeddingResult {
	Vector<double> vector;
	String effective_provider;
	bool used_fallback = false;
};

class EmbeddingProvider : public RefCounted {
	GDCLASS(EmbeddingProvider, RefCounted);

protected:
	static void _bind_methods();

public:
	virtual Vector<double> embed_tokens(const Vector<String> &p_tokens) const = 0;
	virtual EmbeddingResult embed_tokens_result(const Vector<String> &p_tokens) const;
	virtual String get_provider_name() const = 0;
};

class HashVectorEmbeddingProvider : public EmbeddingProvider {
	GDCLASS(HashVectorEmbeddingProvider, EmbeddingProvider);

protected:
	static void _bind_methods();

public:
	Vector<double> embed_tokens(const Vector<String> &p_tokens) const override;
	EmbeddingResult embed_tokens_result(const Vector<String> &p_tokens) const override;
	String get_provider_name() const override { return "hash_vector"; }
};

class NgramEmbeddingProvider : public EmbeddingProvider {
	GDCLASS(NgramEmbeddingProvider, EmbeddingProvider);

protected:
	static void _bind_methods();

public:
	Vector<double> embed_tokens(const Vector<String> &p_tokens) const override;
	EmbeddingResult embed_tokens_result(const Vector<String> &p_tokens) const override;
	String get_provider_name() const override { return "ngram"; }
};

class HttpEmbeddingProvider : public EmbeddingProvider {
	GDCLASS(HttpEmbeddingProvider, EmbeddingProvider);

protected:
	static void _bind_methods();

public:
	// HTTP embeds block the main thread; keep them disabled until the editor has
	// finished first-scan project load (or when no EditorNode is present, e.g. tests).
	static bool is_http_embedding_unlocked();
	static void set_http_embedding_unlocked(bool p_unlocked);

	Vector<double> embed_tokens(const Vector<String> &p_tokens) const override;
	EmbeddingResult embed_tokens_result(const Vector<String> &p_tokens) const override;
	String get_provider_name() const override { return "http"; }
};
