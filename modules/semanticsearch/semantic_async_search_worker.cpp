/**************************************************************************/
/*  semantic_async_search_worker.cpp                                      */
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

#include "core/object/class_db.h"
#include "semantic_async_search_worker.h"

#include "semantic_asset_index.h"
#include "semantic_search_backend.h"
#include "semantic_search_backend_factory.h"
#include "semantic_search_filters.h"

#include "core/config/project_settings.h"
#include "core/object/worker_thread_pool.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/os/thread_safe.h"

struct SemanticSearchWorkerData {
	SemanticAsyncSearchWorker *worker = nullptr;
	String job_id;
};

SemanticAsyncSearchWorker *SemanticAsyncSearchWorker::singleton = nullptr;

void SemanticAsyncSearchWorker::_bind_methods() {
	ClassDB::bind_method(D_METHOD("enqueue_search", "query", "limit"), &SemanticAsyncSearchWorker::enqueue_search, DEFVAL(20));
	ClassDB::bind_method(D_METHOD("cancel_search", "job_id"), &SemanticAsyncSearchWorker::cancel_search);
	ClassDB::bind_method(D_METHOD("poll_search", "job_id"), &SemanticAsyncSearchWorker::poll_search);
}

SemanticAsyncSearchWorker *SemanticAsyncSearchWorker::get_singleton() {
	return singleton;
}

void SemanticAsyncSearchWorker::_search_worker_trampoline(void *p_userdata) {
	SemanticSearchWorkerData *data = static_cast<SemanticSearchWorkerData *>(p_userdata);
	if (!data || !data->worker) {
		if (data) {
			memdelete(data);
		}
		return;
	}
	{
		MutexLock lock(data->worker->jobs_mutex);
		data->worker->_notify_job_started();
	}
	data->worker->_run_search_job(data->job_id);
	{
		MutexLock lock(data->worker->jobs_mutex);
		data->worker->_notify_job_finished();
	}
	memdelete(data);
}

void SemanticAsyncSearchWorker::_run_search_job(const String &p_job_id) {
	SearchJob job;
	{
		MutexLock lock(jobs_mutex);
		if (!jobs.has(p_job_id)) {
			return;
		}
		job = jobs[p_job_id];
		if (job.cancelled) {
			jobs[p_job_id].finished = true;
			return;
		}
	}

	Array results;
	String error;
	if (SemanticAssetIndex *index = SemanticAssetIndex::get_singleton()) {
		const SemanticFilterSnapshot snapshot = index->build_filter_snapshot(job.tags, job.require_all, job.path_regex, job.class_filter);
		if (snapshot.mutation_generation != job.mutation_generation) {
			error = "Semantic index mutated during async search.";
		}
	}
	if (error.is_empty()) {
		const String backend_name = SemanticSearchBackendFactory::get_active_backend_name();
		const String provider_name = SemanticSearchBackendFactory::get_embedding_provider_name();
		if ((backend_name == "embedding" || backend_name == "hybrid") && provider_name == "http" && !Thread::is_main_thread()) {
			error = "HTTP embedding provider cannot be used for async search.";
		} else {
			Ref<SemanticSearchBackend> backend = SemanticSearchBackendFactory::create_active_backend();
			if (backend.is_valid()) {
				results = backend->search_with_filters(job.query, job.limit, job.tags, job.require_all, job.path_regex, job.class_filter);
				if (results.is_empty()) {
					if (SemanticAssetIndex *index = SemanticAssetIndex::get_singleton()) {
						const String filter_error = index->get_last_filter_error();
						if (!filter_error.is_empty()) {
							error = filter_error;
						}
					}
				}
			} else {
				error = "SemanticSearchBackend unavailable.";
			}
		}
	}

	if (error.is_empty()) {
		if (SemanticAssetIndex *index = SemanticAssetIndex::get_singleton()) {
			const SemanticFilterSnapshot after = index->build_filter_snapshot(job.tags, job.require_all, job.path_regex, job.class_filter);
			if (after.mutation_generation != job.mutation_generation) {
				error = "Semantic index mutated during async search.";
				results = Array();
			}
		}
	}

	MutexLock lock(jobs_mutex);
	if (!jobs.has(p_job_id)) {
		return;
	}
	SearchJob &stored = jobs[p_job_id];
	if (stored.cancelled) {
		stored.finished = true;
		return;
	}
	stored.results = results;
	stored.error = error;
	stored.finished = true;
}

void SemanticAsyncSearchWorker::_notify_job_started() {
	active_jobs++;
}

void SemanticAsyncSearchWorker::_notify_job_finished() {
	active_jobs = MAX(active_jobs - 1, 0);
}

int SemanticAsyncSearchWorker::_count_unfinished_jobs_locked() const {
	int count = 0;
	for (const KeyValue<String, SearchJob> &kv : jobs) {
		if (!kv.value.finished) {
			count++;
		}
	}
	return count;
}

void SemanticAsyncSearchWorker::_prune_expired_jobs() {
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/semanticsearch/async_job_ttl_ms")) {
		job_ttl_ms = int(GLOBAL_GET("blazium/semanticsearch/async_job_ttl_ms"));
	}
	const uint64_t now = OS::get_singleton()->get_ticks_msec();
	Vector<String> expired;
	{
		MutexLock lock(jobs_mutex);
		for (const KeyValue<String, SearchJob> &kv : jobs) {
			if (kv.value.finished && (now - kv.value.created_at_ms) > uint64_t(MAX(job_ttl_ms, 1000))) {
				expired.push_back(kv.key);
			}
		}
		for (int i = 0; i < expired.size(); i++) {
			jobs.erase(expired[i]);
		}
	}
}

String SemanticAsyncSearchWorker::enqueue_search(const String &p_query, int p_limit) {
	return enqueue_search_with_filters(p_query, p_limit, PackedStringArray(), false, String(), String());
}

String SemanticAsyncSearchWorker::enqueue_search_with_filters(
		const String &p_query,
		int p_limit,
		const PackedStringArray &p_tags,
		bool p_require_all,
		const String &p_path_regex,
		const String &p_class_filter) {
#ifndef TESTS_ENABLED
	ERR_FAIL_COND_V_MSG(this != get_singleton(), String(), "SemanticAsyncSearchWorker: only the module singleton may enqueue search jobs.");
#endif
	_prune_expired_jobs();
	SearchJob job;
	job.query = p_query;
	job.limit = MAX(p_limit, 1);
	job.tags = p_tags;
	job.require_all = p_require_all;
	job.path_regex = p_path_regex;
	job.class_filter = p_class_filter;
	job.created_at_ms = OS::get_singleton()->get_ticks_msec();
	if (SemanticAssetIndex *index = SemanticAssetIndex::get_singleton()) {
		const SemanticFilterSnapshot snapshot = index->build_filter_snapshot(p_tags, p_require_all, p_path_regex, p_class_filter);
		job.mutation_generation = snapshot.mutation_generation;
	}
	{
		MutexLock lock(jobs_mutex);
		if (_count_unfinished_jobs_locked() >= max_concurrent_jobs) {
			WARN_PRINT_ONCE("SemanticAsyncSearchWorker: concurrent search job cap reached; rejecting enqueue.");
			return String();
		}
		static uint64_t next_job_id = 1;
		job.id = "semantic-search-" + String::num_uint64(next_job_id++) + "-" + String::num_uint64(OS::get_singleton()->get_ticks_usec());
		jobs.insert(job.id, job);
	}

	SemanticSearchWorkerData *data = memnew(SemanticSearchWorkerData);
	data->worker = this;
	data->job_id = job.id;

	if (WorkerThreadPool *pool = WorkerThreadPool::get_singleton()) {
		pool->add_native_task(&SemanticAsyncSearchWorker::_search_worker_trampoline, data, true, "SemanticAsyncSearch");
	} else {
		{
			MutexLock lock(jobs_mutex);
			_notify_job_started();
		}
		_run_search_job(job.id);
		{
			MutexLock lock(jobs_mutex);
			_notify_job_finished();
		}
		memdelete(data);
	}
	return job.id;
}

bool SemanticAsyncSearchWorker::cancel_search(const String &p_job_id) {
	MutexLock lock(jobs_mutex);
	if (!jobs.has(p_job_id)) {
		return false;
	}
	jobs[p_job_id].cancelled = true;
	return true;
}

Dictionary SemanticAsyncSearchWorker::poll_search(const String &p_job_id) {
	_prune_expired_jobs();
	Dictionary result;
	MutexLock lock(jobs_mutex);
	if (!jobs.has(p_job_id)) {
		result["ok"] = false;
		result["error"] = "Unknown search job.";
		return result;
	}
	const SearchJob &job = jobs[p_job_id];
	result["job_id"] = p_job_id;
	result["finished"] = job.finished;
	result["cancelled"] = job.cancelled;
	if (!job.finished) {
		result["ok"] = true;
		return result;
	}
	result["ok"] = job.error.is_empty() && !job.cancelled;
	if (!job.error.is_empty()) {
		result["error"] = job.error;
	} else if (!job.cancelled) {
		result["results"] = job.results;
	}
	return result;
}

void SemanticAsyncSearchWorker::drain_jobs() {
	for (int attempt = 0; attempt < 500; attempt++) {
		bool any_running = false;
		{
			MutexLock lock(jobs_mutex);
			if (active_jobs > 0) {
				any_running = true;
			} else {
				for (const KeyValue<String, SearchJob> &kv : jobs) {
					if (!kv.value.finished) {
						any_running = true;
						break;
					}
				}
			}
		}
		if (!any_running) {
			return;
		}
		OS::get_singleton()->delay_usec(10000);
	}
	WARN_PRINT("SemanticAsyncSearchWorker: drain_jobs timed out while waiting for in-flight search jobs.");
}

SemanticAsyncSearchWorker::SemanticAsyncSearchWorker() {
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/semanticsearch/async_job_ttl_ms")) {
		job_ttl_ms = int(GLOBAL_GET("blazium/semanticsearch/async_job_ttl_ms"));
	}
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/semanticsearch/max_concurrent_async_jobs")) {
		max_concurrent_jobs = MAX(1, int(GLOBAL_GET("blazium/semanticsearch/max_concurrent_async_jobs")));
	}
	if (!singleton) {
		singleton = this;
	}
}

SemanticAsyncSearchWorker::~SemanticAsyncSearchWorker() {
	{
		MutexLock lock(jobs_mutex);
		Vector<String> ids;
		for (const KeyValue<String, SearchJob> &kv : jobs) {
			ids.push_back(kv.key);
		}
		for (int i = 0; i < ids.size(); i++) {
			jobs[ids[i]].cancelled = true;
		}
	}
	drain_jobs();
	if (singleton == this) {
#ifdef TESTS_ENABLED
		singleton = SemanticAsyncSearchWorkerTestHooks::get_module_singleton();
		if (singleton == this) {
			singleton = nullptr;
		}
#else
		singleton = nullptr;
#endif
	}
}

#ifdef TESTS_ENABLED
namespace SemanticAsyncSearchWorkerTestHooks {
static SemanticAsyncSearchWorker *g_module_singleton = nullptr;

void set_module_singleton(SemanticAsyncSearchWorker *p_worker) {
	g_module_singleton = p_worker;
}

SemanticAsyncSearchWorker *get_module_singleton() {
	return g_module_singleton;
}
} //namespace SemanticAsyncSearchWorkerTestHooks
#endif
