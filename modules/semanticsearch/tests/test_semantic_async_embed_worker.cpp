/**************************************************************************/
/*  test_semantic_async_embed_worker.cpp                                  */
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

#include "test_semantic_async_embed_worker.h"

#include "../semantic_async_embed_worker.h"

#include "core/os/os.h"

void test_semantic_async_embed_worker_enqueue_poll() {
	SemanticAsyncEmbedWorker worker;
	const String job_id = worker.enqueue_refresh(true, false);
	CHECK(!job_id.is_empty());

	bool finished = false;
	for (int i = 0; i < 200 && !finished; i++) {
		const Dictionary poll = worker.poll_refresh(job_id);
		CHECK(poll.get("ok", false));
		finished = bool(poll.get("finished", false));
		if (!finished) {
			OS::get_singleton()->delay_usec(5000);
		}
	}
	CHECK(finished);
	worker.drain_jobs();
}

void test_semantic_async_embed_drain_timeout() {
	SemanticAsyncEmbedWorker worker;
	SemanticAsyncEmbedWorkerTestHooks::set_drain_max_attempts(&worker, 2);
	const uint64_t now = OS::get_singleton()->get_ticks_msec();
	SemanticAsyncEmbedWorkerTestHooks::inject_job(&worker, "stuck-embed", false, now);

	const uint64_t started = OS::get_singleton()->get_ticks_msec();
	worker.drain_jobs();
	const uint64_t elapsed = OS::get_singleton()->get_ticks_msec() - started;

	CHECK(elapsed < 2000);
	CHECK(!SemanticAsyncEmbedWorkerTestHooks::has_job(&worker, "stuck-embed"));
}

void test_semantic_async_embed_ttl_finished_only() {
	SemanticAsyncEmbedWorker worker;

	SemanticAsyncEmbedWorkerTestHooks::set_job_ttl_ms(&worker, 1000);
	SemanticAsyncEmbedWorkerTestHooks::set_drain_max_attempts(&worker, 1);

	while (OS::get_singleton()->get_ticks_msec() < 1100) {
		OS::get_singleton()->delay_usec(50000);
	}

	const uint64_t created_at_ms = 1;
	SemanticAsyncEmbedWorkerTestHooks::inject_job(&worker, "finished-old", true, created_at_ms);
	SemanticAsyncEmbedWorkerTestHooks::inject_job(&worker, "inflight-old", false, created_at_ms);

	SemanticAsyncEmbedWorkerTestHooks::prune_expired(&worker);

	CHECK(!SemanticAsyncEmbedWorkerTestHooks::has_job(&worker, "finished-old"));
	CHECK(SemanticAsyncEmbedWorkerTestHooks::has_job(&worker, "inflight-old"));

	// Finish the injected in-flight job so drain does not warn on timeout.
	SemanticAsyncEmbedWorkerTestHooks::inject_job(&worker, "inflight-old", true, created_at_ms);
	worker.drain_jobs();
}

#endif
