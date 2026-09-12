/**************************************************************************/
/*  test_semantic_async_search_worker.h                                   */
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

#include "tests/test_macros.h"

void test_semantic_async_search_worker_enqueue_poll();
void test_semantic_async_search_worker_enqueue_filters();
void test_semantic_async_drain_jobs();
void test_semantic_async_http_provider_rejected();
void test_semantic_async_upsert_concurrency();
void test_semantic_sidecar_compact_at_64();

TEST_CASE("[Modules][SemanticSearch] async upsert concurrency") {
	test_semantic_async_upsert_concurrency();
}

TEST_CASE("[Modules][SemanticSearch] semantic sidecar compact at 64") {
	test_semantic_sidecar_compact_at_64();
}

TEST_CASE("[Modules][SemanticSearch] async search worker enqueue poll") {
	test_semantic_async_search_worker_enqueue_poll();
}

TEST_CASE("[Modules][SemanticSearch] async search worker enqueue filters") {
	test_semantic_async_search_worker_enqueue_filters();
}

TEST_CASE("[Modules][SemanticSearch] async drain jobs") {
	test_semantic_async_drain_jobs();
}

TEST_CASE("[Modules][SemanticSearch] async http provider rejected") {
	test_semantic_async_http_provider_rejected();
}
