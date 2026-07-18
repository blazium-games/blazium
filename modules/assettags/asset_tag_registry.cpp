/**************************************************************************/
/*  asset_tag_registry.cpp                                                */
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

#include "asset_tag_registry.h"

#include "asset_tag_hierarchy.h"
#include "asset_tag_manager.h"
#include "asset_tag_query.h"
#include "asset_tag_registry_commit_policy.h"
#include "asset_tag_runtime.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/templates/hash_set.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_file_system.h"
#endif

AssetTagRegistry *AssetTagRegistry::singleton = nullptr;

void AssetTagRegistry::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load"), &AssetTagRegistry::load);
	ClassDB::bind_method(D_METHOD("save"), &AssetTagRegistry::save);
	ClassDB::bind_method(D_METHOD("get_tags_for_asset", "path"), &AssetTagRegistry::get_tags_for_asset);
	ClassDB::bind_method(D_METHOD("set_tags_for_asset", "path", "tags"), &AssetTagRegistry::set_tags_for_asset);
	ClassDB::bind_method(D_METHOD("add_tags_to_asset", "path", "tags"), &AssetTagRegistry::add_tags_to_asset);
	ClassDB::bind_method(D_METHOD("remove_tags_from_asset", "path", "tags"), &AssetTagRegistry::remove_tags_from_asset);
	ClassDB::bind_method(D_METHOD("find_assets_by_tag", "tag", "match_parent"), &AssetTagRegistry::find_assets_by_tag, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("search_assets", "tags", "type_filter", "path_glob", "path_regex", "require_all"), &AssetTagRegistry::search_assets, DEFVAL(String()), DEFVAL(String()), DEFVAL(String()), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("get_unused_tags"), &AssetTagRegistry::get_unused_tags);
	ClassDB::bind_method(D_METHOD("rescan"), &AssetTagRegistry::rescan);
	ClassDB::bind_method(D_METHOD("get_indexed_paths"), &AssetTagRegistry::get_indexed_paths);
	ClassDB::bind_method(D_METHOD("rename_asset_path", "old_path", "new_path"), &AssetTagRegistry::rename_asset_path);
	ClassDB::bind_method(D_METHOD("begin_batch"), &AssetTagRegistry::begin_batch);
	ClassDB::bind_method(D_METHOD("commit_batch"), &AssetTagRegistry::commit_batch);
	ClassDB::bind_method(D_METHOD("abort_batch"), &AssetTagRegistry::abort_batch);
	ClassDB::bind_method(D_METHOD("cancel_opened_batch"), &AssetTagRegistry::cancel_opened_batch);
	ClassDB::bind_method(D_METHOD("schedule_prune_removed_paths"), &AssetTagRegistry::schedule_prune_removed_paths);
	ClassDB::bind_method(D_METHOD("prune_removed_paths"), &AssetTagRegistry::prune_removed_paths);
	ClassDB::bind_method(D_METHOD("_debounced_prune_removed_paths"), &AssetTagRegistry::_debounced_prune_removed_paths);
	ClassDB::bind_method(D_METHOD("_on_prune_debounce_timeout"), &AssetTagRegistry::_on_prune_debounce_timeout);
	ClassDB::bind_method(D_METHOD("_deferred_save"), &AssetTagRegistry::_deferred_save);
	ClassDB::bind_method(D_METHOD("asset_matches_tag_filter", "path", "tag_tokens", "require_all"), &AssetTagRegistry::asset_matches_tag_filter, DEFVAL(true));

	ADD_SIGNAL(MethodInfo("asset_tags_changed", PropertyInfo(Variant::STRING, "path")));
	ADD_SIGNAL(MethodInfo("asset_tags_batch_changed", PropertyInfo(Variant::PACKED_STRING_ARRAY, "paths")));
	ADD_SIGNAL(MethodInfo("index_reloaded"));
}

String AssetTagRegistry::remap_tag_for_rename(const String &p_tag, const String &p_old_name, const String &p_new_name) {
	return AssetTagHierarchy::remap_tag_for_rename(p_tag, p_old_name, p_new_name);
}

bool AssetTagRegistry::tag_matches_prefix(const String &p_tag, const String &p_prefix) {
	return AssetTagHierarchy::tag_matches_prefix(p_tag, p_prefix);
}

AssetTagRegistry *AssetTagRegistry::get_singleton() {
	return singleton;
}

void AssetTagRegistry::begin_batch() {
	ERR_FAIL_COND_MSG(this != singleton, "AssetTagRegistry: only the module singleton may begin a batch.");
	if (batch_depth == 0) {
		if (AssetTagManager *manager = AssetTagManager::get_singleton()) {
			manager->begin_batch();
		}
	}
	batch_depth++;
}

void AssetTagRegistry::_notify_asset_tags_changed(const String &p_path) {
	if (batch_depth > 0) {
		pending_changed_paths.insert(p_path);
		return;
	}
	emit_signal(SNAME("asset_tags_changed"), p_path);
}

void AssetTagRegistry::_emit_batch_changed(const HashSet<String> &p_paths) {
	if (p_paths.is_empty()) {
		return;
	}
	PackedStringArray paths;
	for (const String &path : p_paths) {
		paths.push_back(path);
	}
	paths.sort();
	emit_signal(SNAME("asset_tags_batch_changed"), paths);
}

void AssetTagRegistry::_restore_index_state(const HashMap<String, Vector<String>> &p_index, const HashMap<String, HashSet<String>> &p_reverse) {
	asset_index = p_index;
	reverse_lookup = p_reverse;
}

Error AssetTagRegistry::_persist_and_notify(const HashSet<String> &p_paths) {
	if (batch_depth > 0) {
		for (const String &path : p_paths) {
			pending_changed_paths.insert(path);
			dirty_index_paths.insert(path);
		}
		index_dirty = true;
		return OK;
	}
	for (const String &path : p_paths) {
		dirty_index_paths.insert(path);
	}
	index_dirty = true;
	const Error err = _save_index_incremental();
	if (err != OK) {
		return err;
	}
	for (const String &path : p_paths) {
		_notify_asset_tags_changed(path);
	}
	return OK;
}

Error AssetTagRegistry::_save_index_incremental() {
	if (AssetTagStorage::is_index_write_blocked()) {
		WARN_PRINT_ONCE("Asset tag index writes are blocked until load recovery completes.");
		return ERR_UNAVAILABLE;
	}
	if (!index_dirty && dirty_index_paths.is_empty()) {
		return OK;
	}
	HashSet<String> paths_to_merge = dirty_index_paths;
	if (paths_to_merge.is_empty()) {
		for (const KeyValue<String, Vector<String>> &kv : asset_index) {
			paths_to_merge.insert(kv.key);
		}
	}
	if (!AssetTagStorage::save_index_merge(asset_index, paths_to_merge)) {
		return ERR_CANT_CREATE;
	}
	index_dirty = false;
	dirty_index_paths.clear();
	save_scheduled = false;
	AssetTagRuntime::invalidate_cache();
	return OK;
}

void AssetTagRegistry::_deferred_save() {
	save_scheduled = false;
	if (shutting_down) {
		return;
	}
	_save_index_incremental();
}

void AssetTagRegistry::_schedule_save() {
	if (shutting_down) {
		return;
	}
	if (batch_depth > 0) {
		index_dirty = true;
		return;
	}
	if (!save_scheduled) {
		save_scheduled = true;
		call_deferred(SNAME("_deferred_save"));
	}
}

Error AssetTagRegistry::save() {
	ERR_FAIL_COND_V_MSG(this != singleton, ERR_UNCONFIGURED, "AssetTagRegistry: only the module singleton may save the asset index.");
	if (batch_depth > 0) {
		index_dirty = true;
		return OK;
	}
	_schedule_save();
	return OK;
}

Error AssetTagRegistry::commit_batch() {
	if (batch_depth <= 0) {
		return OK;
	}
	if (batch_depth == 1 && AssetTagStorage::is_index_write_blocked()) {
		WARN_PRINT_ONCE("Asset tag index writes are blocked until load recovery completes.");
		abort_batch();
		return ERR_UNAVAILABLE;
	}
	batch_depth--;
	if (batch_depth == 0) {
		AssetTagManager *manager = AssetTagManager::get_singleton();
		const bool save_dictionary = manager && manager->is_dictionary_dirty();
		const bool save_index = index_dirty;

		if (manager) {
			const Error dict_err = manager->commit_batch(false);
			if (dict_err != OK) {
				pending_changed_paths.clear();
				dirty_index_paths.clear();
				index_dirty = false;
				save_scheduled = false;
				load();
				return dict_err;
			}
		}

		if (save_dictionary || save_index) {
			save_scheduled = false;
			const bool committed = AssetTagRegistryCommitPolicy::persist_batch_changes(
					manager,
					asset_index,
					dirty_index_paths,
					save_dictionary,
					save_index);
			if (committed) {
				index_dirty = false;
			}
			if (!committed) {
				pending_changed_paths.clear();
				dirty_index_paths.clear();
				index_dirty = false;
				if (manager) {
					manager->load();
				}
				load();
				return ERR_CANT_CREATE;
			}
		}
		if (!pending_changed_paths.is_empty()) {
			_emit_batch_changed(pending_changed_paths);
		}
		pending_changed_paths.clear();
		if (consume_queued_prune()) {
			schedule_prune_removed_paths();
		}
	}
	return OK;
}

void AssetTagRegistry::abort_batch() {
	if (batch_depth <= 0) {
		return;
	}
	batch_depth--;
	if (batch_depth == 0) {
		save_scheduled = false;
		index_dirty = false;
		dirty_index_paths.clear();
		pending_changed_paths.clear();
		if (AssetTagManager *manager = AssetTagManager::get_singleton()) {
			manager->abort_batch();
		}
		if (index_load_failed) {
			asset_index.clear();
			_rebuild_reverse_lookup();
			AssetTagStorage::set_index_write_blocked(true);
			return;
		}
		load();
	}
}

void AssetTagRegistry::cancel_opened_batch() {
	if (batch_depth <= 0) {
		return;
	}
	batch_depth--;
	if (batch_depth == 0) {
		save_scheduled = false;
		pending_changed_paths.clear();
		if (AssetTagManager *manager = AssetTagManager::get_singleton()) {
			manager->abort_batch();
		}
	}
}

Error AssetTagRegistry::load() {
	ERR_FAIL_COND_V_MSG(this != singleton, ERR_UNCONFIGURED, "AssetTagRegistry: only the module singleton may load the asset index.");
	asset_index.clear();
	index_load_failed = false;
	AssetTagStorage::set_index_write_blocked(false);
	if (!AssetTagStorage::load_index(asset_index)) {
		index_load_failed = true;
		AssetTagStorage::set_index_write_blocked(true);
		_rebuild_reverse_lookup();
		return ERR_FILE_CORRUPT;
	}
	_rebuild_reverse_lookup();
	return OK;
}

Error AssetTagRegistry::recover_after_load_failure() {
	if (!index_load_failed) {
		return OK;
	}
	WARN_PRINT("AssetTagRegistry: recovering from index load failure.");
	if (!AssetTagStorage::quarantine_corrupt_index()) {
		WARN_PRINT("AssetTagRegistry: failed to quarantine corrupt index file.");
	}
	AssetTagStorage::quarantine_index_dirty_sidecars();
	AssetTagStorage::set_index_write_blocked(false);
	asset_index.clear();
	if (!AssetTagStorage::load_index(asset_index)) {
		asset_index.clear();
	}
	_rebuild_reverse_lookup();
	index_load_failed = false;
	AssetTagRuntime::invalidate_cache();
	emit_signal(SNAME("index_reloaded"));
	return OK;
}

PackedStringArray AssetTagRegistry::get_tags_for_asset(const String &p_path) const {
	const String path = AssetTagStorage::normalize_asset_path(p_path);
	PackedStringArray result;
	if (asset_index.has(path)) {
		for (int i = 0; i < asset_index[path].size(); i++) {
			result.push_back(asset_index[path][i]);
		}
	}
	result.sort();
	return result;
}

Error AssetTagRegistry::rescan() {
	ERR_FAIL_COND_V_MSG(this != singleton, ERR_UNCONFIGURED, "AssetTagRegistry: only the module singleton may rescan.");
	if (batch_depth > 0 || index_dirty) {
		return ERR_BUSY;
	}
	const PackedStringArray before_paths = get_indexed_paths();
	load();
	prune_removed_paths();
	const PackedStringArray after_paths = get_indexed_paths();
	HashSet<String> all_paths;
	for (int i = 0; i < before_paths.size(); i++) {
		all_paths.insert(before_paths[i]);
	}
	for (int i = 0; i < after_paths.size(); i++) {
		all_paths.insert(after_paths[i]);
	}
	if (!all_paths.is_empty()) {
		_emit_batch_changed(all_paths);
	}
	emit_signal(SNAME("index_reloaded"));
	return OK;
}

PackedStringArray AssetTagRegistry::get_unused_tags() const {
	AssetTagManager *manager = AssetTagManager::get_singleton();
	if (!manager) {
		return PackedStringArray();
	}
	return manager->get_unused_tags(asset_index);
}

AssetTagRegistry::AssetTagRegistry() {
	if (!singleton) {
		singleton = this;
	}
}

AssetTagRegistry::~AssetTagRegistry() {
	prepare_for_teardown();
	if (singleton == this) {
		singleton = nullptr;
	}
}
