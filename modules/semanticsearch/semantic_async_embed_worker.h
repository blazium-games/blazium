/**************************************************************************/
/*  semantic_async_embed_worker.h                                         */
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
#include "core/os/mutex.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"

class SemanticAsyncEmbedWorker;

#ifdef TESTS_ENABLED
class SemanticAsyncEmbedWorkerTestHooks {
public:
	static void set_module_singleton(SemanticAsyncEmbedWorker *p_worker);
	static SemanticAsyncEmbedWorker *get_module_singleton();
	static void set_job_ttl_ms(SemanticAsyncEmbedWorker *p_worker, int p_ttl_ms);
	static void set_drain_max_attempts(SemanticAsyncEmbedWorker *p_worker, int p_attempts);
	static void inject_job(SemanticAsyncEmbedWorker *p_worker, const String &p_job_id, bool p_finished, uint64_t p_created_at_ms);
	static void prune_expired(SemanticAsyncEmbedWorker *p_worker);
	static bool has_job(SemanticAsyncEmbedWorker *p_worker, const String &p_job_id);
};
#endif

class SemanticAsyncEmbedWorker : public RefCounted {
	GDCLASS(SemanticAsyncEmbedWorker, RefCounted);
#ifdef TESTS_ENABLED
	friend class SemanticAsyncEmbedWorkerTestHooks;
#endif

	struct EmbedJob {
		String id;
		bool stale_only = false;
		bool force = false;
		bool cancelled = false;
		bool finished = false;
		String error;
		int refreshed_count = 0;
		uint64_t created_at_ms = 0;
	};

	static SemanticAsyncEmbedWorker *singleton;
	HashMap<String, EmbedJob> jobs;
	Mutex jobs_mutex;
	int job_ttl_ms = 1800000;
	int active_jobs = 0;
	int max_concurrent_jobs = 4;
	int drain_max_attempts = 500;

	static void _embed_worker_trampoline(void *p_userdata);

	void _run_embed_job(const String &p_job_id);
	void _prune_expired_jobs();
	void _notify_job_started();
	void _notify_job_finished();
	int _count_unfinished_jobs_locked() const;

protected:
	static void _bind_methods();

public:
	static SemanticAsyncEmbedWorker *get_singleton();

	String enqueue_refresh(bool p_stale_only = true, bool p_force = false);
	bool cancel_refresh(const String &p_job_id);
	Dictionary poll_refresh(const String &p_job_id);
	void drain_jobs();

	SemanticAsyncEmbedWorker();
	~SemanticAsyncEmbedWorker();
};
