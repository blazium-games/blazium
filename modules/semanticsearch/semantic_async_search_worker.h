/**************************************************************************/
/*  semantic_async_search_worker.h                                        */
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
#include "core/variant/variant.h"

class SemanticAsyncSearchWorker : public RefCounted {
	GDCLASS(SemanticAsyncSearchWorker, RefCounted);

	struct SearchJob {
		String id;
		String query;
		int limit = 20;
		PackedStringArray tags;
		bool require_all = false;
		String path_regex;
		String class_filter;
		bool cancelled = false;
		Array results;
		String error;
		bool finished = false;
		uint64_t created_at_ms = 0;
		uint64_t mutation_generation = 0;
	};

	static SemanticAsyncSearchWorker *singleton;
	HashMap<String, SearchJob> jobs;
	Mutex jobs_mutex;
	int job_ttl_ms = 1800000;
	int active_jobs = 0;
	int max_concurrent_jobs = 8;

	static void _search_worker_trampoline(void *p_userdata);

	void _run_search_job(const String &p_job_id);
	void _prune_expired_jobs();
	void _notify_job_started();
	void _notify_job_finished();
	int _count_unfinished_jobs_locked() const;

protected:
	static void _bind_methods();

public:
	static SemanticAsyncSearchWorker *get_singleton();

	String enqueue_search(const String &p_query, int p_limit = 20);
	String enqueue_search_with_filters(
			const String &p_query,
			int p_limit,
			const PackedStringArray &p_tags,
			bool p_require_all,
			const String &p_path_regex,
			const String &p_class_filter);
	bool cancel_search(const String &p_job_id);
	Dictionary poll_search(const String &p_job_id);
	void drain_jobs();

	SemanticAsyncSearchWorker();
	~SemanticAsyncSearchWorker();
};

#ifdef TESTS_ENABLED
namespace SemanticAsyncSearchWorkerTestHooks {
void set_module_singleton(SemanticAsyncSearchWorker *p_worker);
SemanticAsyncSearchWorker *get_module_singleton();
} //namespace SemanticAsyncSearchWorkerTestHooks
#endif
