/**************************************************************************/
/*  semantic_async_embed_worker.cpp                                       */
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

#include "semantic_async_embed_worker.h"

#include "core/object/worker_thread_pool.h"
#include "semantic_asset_index.h"
#include "semantic_search_backend_factory.h"

#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/os/thread_safe.h"

struct SemanticEmbedWorkerData {
	SemanticAsyncEmbedWorker *worker = nullptr;
	String job_id;
};

SemanticAsyncEmbedWorker *SemanticAsyncEmbedWorker::singleton = nullptr;

void SemanticAsyncEmbedWorker::_bind_methods() {
	ClassDB::bind_method(D_METHOD("enqueue_refresh", "stale_only", "force"), &SemanticAsyncEmbedWorker::enqueue_refresh, DEFVAL(true), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("cancel_refresh", "job_id"), &SemanticAsyncEmbedWorker::cancel_refresh);
	ClassDB::bind_method(D_METHOD("poll_refresh", "job_id"), &SemanticAsyncEmbedWorker::poll_refresh);
}

SemanticAsyncEmbedWorker *SemanticAsyncEmbedWorker::get_singleton() {
	return singleton;
}

void SemanticAsyncEmbedWorker::_embed_worker_trampoline(void *p_userdata) {
	SemanticEmbedWorkerData *data = static_cast<SemanticEmbedWorkerData *>(p_userdata);
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
	data->worker->_run_embed_job(data->job_id);
	{
		MutexLock lock(data->worker->jobs_mutex);
		data->worker->_notify_job_finished();
	}
	memdelete(data);
}

void SemanticAsyncEmbedWorker::_notify_job_started() {
	active_jobs++;
}

void SemanticAsyncEmbedWorker::_notify_job_finished() {
	active_jobs = MAX(active_jobs - 1, 0);
}

void SemanticAsyncEmbedWorker::_prune_expired_jobs() {
	const uint64_t now = OS::get_singleton()->get_ticks_msec();
	Vector<String> to_erase;
	for (const KeyValue<String, EmbedJob> &kv : jobs) {
		if (!kv.value.finished) {
			continue;
		}
		if (job_ttl_ms > 0 && kv.value.created_at_ms > 0 && now - kv.value.created_at_ms > (uint64_t)MAX(job_ttl_ms, 1000)) {
			to_erase.push_back(kv.key);
		}
	}
	for (int i = 0; i < to_erase.size(); i++) {
		jobs.erase(to_erase[i]);
	}
}

int SemanticAsyncEmbedWorker::_count_unfinished_jobs_locked() const {
	int count = 0;
	for (const KeyValue<String, EmbedJob> &kv : jobs) {
		if (!kv.value.finished) {
			count++;
		}
	}
	return count;
}

void SemanticAsyncEmbedWorker::_run_embed_job(const String &p_job_id) {
	bool stale_only = true;
	bool force = false;
	{
		MutexLock lock(jobs_mutex);
		if (!jobs.has(p_job_id)) {
			return;
		}
		if (jobs[p_job_id].cancelled) {
			jobs[p_job_id].finished = true;
			return;
		}
		stale_only = jobs[p_job_id].stale_only;
		force = jobs[p_job_id].force;
	}

	SemanticAssetIndex *index = SemanticAssetIndex::get_singleton();
	if (!index) {
		MutexLock lock(jobs_mutex);
		if (jobs.has(p_job_id)) {
			jobs[p_job_id].error = "Semantic asset index unavailable.";
			jobs[p_job_id].finished = true;
		}
		return;
	}

	const String provider_name = SemanticSearchBackendFactory::get_embedding_provider_name();
	if (provider_name == "http" && !Thread::is_main_thread()) {
		MutexLock lock(jobs_mutex);
		if (jobs.has(p_job_id)) {
			jobs[p_job_id].error = "HTTP embedding provider cannot be used for async embed refresh.";
			jobs[p_job_id].finished = true;
		}
		return;
	}

	const int before = index->count_searchable_embeddings_for_active_provider();
	if (stale_only) {
		index->refresh_stale_embeddings_for_active_provider();
	} else {
		index->refresh_embeddings_for_active_provider(force);
	}
	const int after = index->count_searchable_embeddings_for_active_provider();

	MutexLock lock(jobs_mutex);
	if (!jobs.has(p_job_id)) {
		return;
	}
	if (jobs[p_job_id].cancelled) {
		jobs[p_job_id].finished = true;
		return;
	}
	jobs[p_job_id].refreshed_count = MAX(after - before, 0);
	jobs[p_job_id].finished = true;
}

String SemanticAsyncEmbedWorker::enqueue_refresh(bool p_stale_only, bool p_force) {
#ifndef TESTS_ENABLED
	ERR_FAIL_COND_V_MSG(this != get_singleton(), String(), "SemanticAsyncEmbedWorker: only the module singleton may enqueue embed jobs.");
#endif
	const String provider_name = SemanticSearchBackendFactory::get_embedding_provider_name();
	if (provider_name == "http") {
		WARN_PRINT_ONCE("SemanticAsyncEmbedWorker: HTTP embedding provider cannot be used for async embed refresh.");
		return String();
	}

	MutexLock lock(jobs_mutex);
	_prune_expired_jobs();
	if (_count_unfinished_jobs_locked() >= max_concurrent_jobs) {
		WARN_PRINT_ONCE("SemanticAsyncEmbedWorker: concurrent embed job cap reached; rejecting enqueue.");
		return String();
	}

	uint8_t bytes[8];
	CryptoCore::RandomGenerator rng;
	String job_id;
	if (rng.init() == OK && rng.get_random_bytes(bytes, 8) == OK) {
		String hex;
		for (int i = 0; i < 8; i++) {
			hex += vformat("%02x", bytes[i]);
		}
		job_id = hex;
	} else {
		job_id = vformat("embed-%d", OS::get_singleton()->get_ticks_usec());
	}

	EmbedJob job;
	job.id = job_id;
	job.stale_only = p_stale_only;
	job.force = p_force;
	job.created_at_ms = OS::get_singleton()->get_ticks_msec();
	jobs.insert(job_id, job);

	SemanticEmbedWorkerData *data = memnew(SemanticEmbedWorkerData);
	data->worker = this;
	data->job_id = job_id;
	WorkerThreadPool::get_singleton()->add_native_task(_embed_worker_trampoline, data, false, vformat("SemanticEmbed:%s", job_id));
	return job_id;
}

bool SemanticAsyncEmbedWorker::cancel_refresh(const String &p_job_id) {
	MutexLock lock(jobs_mutex);
	if (!jobs.has(p_job_id)) {
		return false;
	}
	jobs[p_job_id].cancelled = true;
	return true;
}

Dictionary SemanticAsyncEmbedWorker::poll_refresh(const String &p_job_id) {
	MutexLock lock(jobs_mutex);
	_prune_expired_jobs();
	Dictionary result;
	if (!jobs.has(p_job_id)) {
		result["ok"] = false;
		result["error"] = "Unknown embed job.";
		return result;
	}
	const EmbedJob &job = jobs[p_job_id];
	result["ok"] = true;
	result["finished"] = job.finished;
	result["cancelled"] = job.cancelled;
	result["refreshed_count"] = job.refreshed_count;
	if (!job.error.is_empty()) {
		result["error"] = job.error;
	}
	return result;
}

void SemanticAsyncEmbedWorker::drain_jobs() {
	const int max_attempts = MAX(1, drain_max_attempts);
	for (int attempt = 0; attempt < max_attempts; attempt++) {
		bool any_running = false;
		{
			MutexLock lock(jobs_mutex);
			if (active_jobs > 0 || _count_unfinished_jobs_locked() > 0) {
				any_running = true;
			} else {
				jobs.clear();
				return;
			}
		}
		if (!any_running) {
			return;
		}
		OS::get_singleton()->delay_usec(10000);
	}
	WARN_PRINT("SemanticAsyncEmbedWorker: drain_jobs timed out while waiting for in-flight embed jobs.");
	MutexLock lock(jobs_mutex);
	jobs.clear();
	active_jobs = 0;
}

SemanticAsyncEmbedWorker::SemanticAsyncEmbedWorker() {
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/semanticsearch/async_job_ttl_ms")) {
		job_ttl_ms = int(GLOBAL_GET("blazium/semanticsearch/async_job_ttl_ms"));
	}
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/semanticsearch/max_concurrent_async_jobs")) {
		max_concurrent_jobs = MAX(1, int(GLOBAL_GET("blazium/semanticsearch/max_concurrent_async_jobs")) / 2);
	}
	if (!singleton) {
		singleton = this;
	}
}

SemanticAsyncEmbedWorker::~SemanticAsyncEmbedWorker() {
	drain_jobs();
	if (singleton == this) {
#ifdef TESTS_ENABLED
		singleton = SemanticAsyncEmbedWorkerTestHooks::get_module_singleton();
		if (singleton == this) {
			singleton = nullptr;
		}
#else
		singleton = nullptr;
#endif
	}
}

#ifdef TESTS_ENABLED
static SemanticAsyncEmbedWorker *g_module_embed_singleton = nullptr;

void SemanticAsyncEmbedWorkerTestHooks::set_module_singleton(SemanticAsyncEmbedWorker *p_worker) {
	g_module_embed_singleton = p_worker;
}

SemanticAsyncEmbedWorker *SemanticAsyncEmbedWorkerTestHooks::get_module_singleton() {
	return g_module_embed_singleton;
}

void SemanticAsyncEmbedWorkerTestHooks::set_job_ttl_ms(SemanticAsyncEmbedWorker *p_worker, int p_ttl_ms) {
	ERR_FAIL_NULL(p_worker);
	MutexLock lock(p_worker->jobs_mutex);
	p_worker->job_ttl_ms = p_ttl_ms;
}

void SemanticAsyncEmbedWorkerTestHooks::set_drain_max_attempts(SemanticAsyncEmbedWorker *p_worker, int p_attempts) {
	ERR_FAIL_NULL(p_worker);
	p_worker->drain_max_attempts = MAX(1, p_attempts);
}

void SemanticAsyncEmbedWorkerTestHooks::inject_job(SemanticAsyncEmbedWorker *p_worker, const String &p_job_id, bool p_finished, uint64_t p_created_at_ms) {
	ERR_FAIL_NULL(p_worker);
	MutexLock lock(p_worker->jobs_mutex);
	SemanticAsyncEmbedWorker::EmbedJob job;
	job.id = p_job_id;
	job.finished = p_finished;
	job.created_at_ms = p_created_at_ms;
	p_worker->jobs[p_job_id] = job;
	int unfinished = 0;
	for (const KeyValue<String, SemanticAsyncEmbedWorker::EmbedJob> &kv : p_worker->jobs) {
		if (!kv.value.finished) {
			unfinished++;
		}
	}
	p_worker->active_jobs = unfinished;
}

void SemanticAsyncEmbedWorkerTestHooks::prune_expired(SemanticAsyncEmbedWorker *p_worker) {
	ERR_FAIL_NULL(p_worker);
	MutexLock lock(p_worker->jobs_mutex);
	p_worker->_prune_expired_jobs();
}

bool SemanticAsyncEmbedWorkerTestHooks::has_job(SemanticAsyncEmbedWorker *p_worker, const String &p_job_id) {
	ERR_FAIL_NULL_V(p_worker, false);
	MutexLock lock(p_worker->jobs_mutex);
	return p_worker->jobs.has(p_job_id);
}
#endif
