/**************************************************************************/
/*  semantic_asset_index_persistence.cpp                                  */
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

#include "semantic_asset_index.h"

#include "lexical_index_builder.h"
#include "modules/modules_enabled.gen.h"
#include "semantic_embedding_pipeline.h"
#include "semantic_index_store.h"

#ifdef MODULE_ASSETTAGS_ENABLED
#include "modules/assettags/asset_tag_storage.h"
#endif

void SemanticAssetIndex::begin_batch() {
	MutexLock lock(index_mutex);
	batch_depth++;
}

void SemanticAssetIndex::commit_batch() {
	bool should_save = false;
	{
		MutexLock lock(index_mutex);
		if (batch_depth <= 0) {
			return;
		}
		batch_depth--;
		should_save = batch_depth == 0 && save_scheduled;
		if (should_save) {
			save_scheduled = false;
		}
	}
	if (should_save) {
		save();
	}
}

void SemanticAssetIndex::_deferred_save() {
	bool should_save = false;
	{
		MutexLock lock(index_mutex);
		save_scheduled = false;
		should_save = batch_depth == 0;
		if (!should_save) {
			save_scheduled = true;
		}
	}
	if (should_save) {
		save();
	}
}

void SemanticAssetIndex::_schedule_save_locked() {
	if (batch_depth > 0) {
		save_scheduled = true;
		return;
	}
	if (!save_scheduled) {
		save_scheduled = true;
		call_deferred(SNAME("_deferred_save"));
	}
}

void SemanticAssetIndex::_schedule_save() {
	MutexLock lock(index_mutex);
	_schedule_save_locked();
}

Error SemanticAssetIndex::load() {
#ifndef TESTS_ENABLED
	ERR_FAIL_COND_V_MSG(this != get_singleton(), ERR_UNCONFIGURED, "SemanticAssetIndex: only the module singleton may load the index.");
#endif
	MutexLock lock(index_mutex);
	SemanticIndexStore store;
	const Error err = store.load();
	if (err != OK) {
		return err;
	}
	entries.clear();
	token_index.clear();
	prefix_index.clear();
	for (const KeyValue<String, SemanticAssetEntry> &kv : store.get_entries()) {
		entries.insert(kv.value.path, kv.value);
		_index_tokens_for_entry(kv.value.path, kv.value.tokens);
	}
	_rebuild_derived_indexes();
	return OK;
}

Error SemanticAssetIndex::save_full() {
	HashMap<String, SemanticAssetEntry> entries_snapshot;
	{
		MutexLock lock(index_mutex);
		entries_snapshot = entries;
	}
	SemanticIndexStore store;
	for (const KeyValue<String, SemanticAssetEntry> &kv : entries_snapshot) {
		store.set_entry(kv.key, kv.value);
	}
	const Error err = store.save();
	if (err == OK) {
		MutexLock lock(index_mutex);
		SemanticIndexStore::clear_index_dirty_sidecars();
		dirty_paths.clear();
	}
	return err;
}

#ifdef TESTS_ENABLED
namespace SemanticAssetIndexTestHooks {
String g_inject_dirty_path_during_save;
static SemanticAssetIndex *g_module_singleton = nullptr;

void set_module_singleton(SemanticAssetIndex *p_index) {
	g_module_singleton = p_index;
}

SemanticAssetIndex *get_module_singleton() {
	return g_module_singleton;
}
} //namespace SemanticAssetIndexTestHooks
#endif

Error SemanticAssetIndex::save() {
#ifndef TESTS_ENABLED
	ERR_FAIL_COND_V_MSG(this != get_singleton(), ERR_UNCONFIGURED, "SemanticAssetIndex: only the module singleton may save the index.");
#endif
	HashSet<String> paths_snapshot;
	HashMap<String, SemanticAssetEntry> entries_snapshot;
	{
		MutexLock lock(index_mutex);
		if (dirty_paths.is_empty()) {
			return OK;
		}
		paths_snapshot = dirty_paths;
		entries_snapshot = entries;
#ifdef TESTS_ENABLED
		if (!SemanticAssetIndexTestHooks::g_inject_dirty_path_during_save.is_empty()) {
			dirty_paths.insert(SemanticAssetIndexTestHooks::g_inject_dirty_path_during_save);
		}
#endif
	}
	SemanticIndexStore store;
	const Error err = SemanticIndexStore::save_dirty(paths_snapshot, entries_snapshot);
	if (err == OK) {
		MutexLock lock(index_mutex);
		for (const String &path : paths_snapshot) {
			dirty_paths.erase(path);
		}
	}
	return err;
}
