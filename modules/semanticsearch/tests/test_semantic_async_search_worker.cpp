/**************************************************************************/
/*  test_semantic_async_search_worker.cpp                                 */
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

#include "../semantic_asset_index.h"
#include "../semantic_async_search_worker.h"
#include "../semantic_index_store.h"
#include "../semantic_search_backend_factory.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/os/os.h"
#include "tests/test_macros.h"

void test_semantic_async_search_worker_enqueue_poll() {
	SemanticAsyncSearchWorker worker;
	const String job_id = worker.enqueue_search("hero", 5);
	CHECK(!job_id.is_empty());

	Dictionary poll = worker.poll_search(job_id);
	CHECK(poll.get("job_id", "") == job_id);
	CHECK(worker.cancel_search(job_id));

	Dictionary unknown = worker.poll_search("missing-job");
	CHECK(!unknown.get("ok", true));
}

void test_semantic_async_search_worker_enqueue_filters() {
	SemanticAsyncSearchWorker worker;
	PackedStringArray tags;
	tags.push_back("Gameplay");
	const String job_id = worker.enqueue_search_with_filters("hero", 5, tags, true, "characters/.*", "");
	CHECK(!job_id.is_empty());
	Dictionary poll = worker.poll_search(job_id);
	CHECK(poll.get("job_id", "") == job_id);
	worker.cancel_search(job_id);
}

void test_semantic_async_drain_jobs() {
	SemanticAsyncSearchWorker worker;
	const String job_id = worker.enqueue_search("hero", 5);
	CHECK(!job_id.is_empty());
	worker.drain_jobs();
	CHECK(true);
}

void test_semantic_async_http_provider_rejected() {
	const String prev_provider = String(ProjectSettings::get_singleton()->get_setting("blazium/semanticsearch/embedding_provider"));
	const String prev_backend = String(ProjectSettings::get_singleton()->get_setting("blazium/semanticsearch/backend"));
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/backend", "hybrid");
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/embedding_provider", "http");
	SemanticSearchBackendFactory::invalidate_session_backend();
	SemanticAsyncSearchWorker worker;
	const String job_id = worker.enqueue_search("hero", 5);
	CHECK(!job_id.is_empty());
	for (int i = 0; i < 200; i++) {
		Dictionary poll = worker.poll_search(job_id);
		if (bool(poll.get("finished", false))) {
			CHECK(!poll.get("ok", true));
			CHECK(String(poll.get("error", "")).contains("HTTP embedding provider"));
			ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/embedding_provider", prev_provider);
			ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/backend", prev_backend);
			SemanticSearchBackendFactory::invalidate_session_backend();
			return;
		}
		OS::get_singleton()->delay_usec(10000);
	}
	CHECK_MESSAGE(false, "Async HTTP provider job did not finish.");
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/embedding_provider", prev_provider);
	ProjectSettings::get_singleton()->set_setting("blazium/semanticsearch/backend", prev_backend);
	SemanticSearchBackendFactory::invalidate_session_backend();
}

void test_semantic_async_upsert_concurrency() {
	SemanticAssetIndex index;
	index.clear();
	CHECK(index.upsert_entry("res://async_hero.tscn") == OK);
	SemanticAsyncSearchWorker worker;
	const String job_id = worker.enqueue_search("hero", 5);
	CHECK(!job_id.is_empty());
	for (int i = 0; i < 32; i++) {
		const String path = vformat("res://async_asset_%02d.tscn", i);
		CHECK(index.upsert_entry(path) == OK);
		worker.drain_jobs();
		Dictionary poll = worker.poll_search(job_id);
		if (bool(poll.get("finished", false))) {
			CHECK(poll.get("ok", true));
			return;
		}
		OS::get_singleton()->delay_usec(5000);
	}
	worker.drain_jobs();
	Dictionary final_poll = worker.poll_search(job_id);
	CHECK(bool(final_poll.get("finished", false)));
	worker.cancel_search(job_id);
}

void test_semantic_sidecar_compact_at_64() {
	SemanticIndexStore::set_test_index_dir("res://.blazium/test_semantic_sidecar_compact_64");
	SemanticIndexStore store;
	SemanticAssetEntry seed;
	seed.path = "res://seed.tscn";
	seed.caption = "Seed";
	store.set_entry(seed.path, seed);
	CHECK(store.save() == OK);
	HashMap<String, SemanticAssetEntry> entries(store.get_entries());

	for (int i = 0; i < 64; i++) {
		const String path = vformat("res://semantic_asset_%03d.tscn", i);
		SemanticAssetEntry entry;
		entry.path = path;
		entry.caption = "Bulk";
		entries.insert(path, entry);
		HashSet<String> dirty;
		dirty.insert(path);
		CHECK(SemanticIndexStore::save_dirty_sidecars(dirty, entries));
	}
	Ref<DirAccess> dirty_dir = DirAccess::open(SemanticIndexStore::get_index_dirty_dir());
	int remaining = 0;
	if (dirty_dir.is_valid()) {
		dirty_dir->list_dir_begin();
		String file_name = dirty_dir->get_next();
		while (!file_name.is_empty()) {
			if (!dirty_dir->current_is_dir() && file_name.ends_with(".json")) {
				remaining++;
			}
			file_name = dirty_dir->get_next();
		}
		dirty_dir->list_dir_end();
	}
	CHECK(remaining == 0);
	SemanticIndexStore::clear_test_index_dir();
}

#endif
