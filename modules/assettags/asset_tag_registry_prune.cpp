/**************************************************************************/
/*  asset_tag_registry_prune.cpp                                          */
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

#include "asset_tag_coordinator.h"

#include "core/io/file_access.h"
#include "core/os/os.h"
#include "scene/main/scene_tree.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_file_system.h"
#endif

namespace {

constexpr int PRUNE_PATHS_PER_FRAME = 400;

bool _asset_path_exists(const String &p_path) {
#ifdef TOOLS_ENABLED
	if (EditorFileSystem *efs = EditorFileSystem::get_singleton()) {
		if (efs->get_filesystem() != nullptr) {
			int file_idx = 0;
			if (efs->find_file(p_path, &file_idx) != nullptr) {
				return true;
			}
			if (efs->get_filesystem_path(p_path) != nullptr) {
				return true;
			}
			if (p_path.begins_with("res://")) {
				return false;
			}
		}
	}
#endif
	return FileAccess::exists(p_path);
}

} //namespace

void AssetTagRegistry::schedule_prune_removed_paths() {
	if (shutting_down) {
		return;
	}
	prune_queued_while_busy = true;
	if (prune_debounce_pending || prune_scheduled || prune_scan_active) {
		return;
	}

	AssetTagCoordinator *coordinator = AssetTagCoordinator::get_singleton();
	if ((coordinator && coordinator->is_in_transaction()) || batch_depth > 0) {
		return;
	}

	const uint64_t now = OS::get_singleton()->get_ticks_msec();
	const uint64_t elapsed = last_prune_invoke_msec == 0 ? 500 : (now - last_prune_invoke_msec);
	if (elapsed < 500) {
		prune_debounce_pending = true;
		const double wait_sec = double(500 - elapsed) / 1000.0;
		if (SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop())) {
			Ref<SceneTreeTimer> timer = tree->create_timer(wait_sec, true, true);
			if (timer.is_valid()) {
				timer->connect("timeout", callable_mp(this, &AssetTagRegistry::_on_prune_debounce_timeout), Object::CONNECT_ONE_SHOT);
				return;
			}
		}

		call_deferred(SNAME("_on_prune_debounce_timeout"));
		return;
	}

	prune_scheduled = true;
	prune_queued_while_busy = false;
	call_deferred(SNAME("prune_removed_paths"));
}

void AssetTagRegistry::_on_prune_debounce_timeout() {
	prune_debounce_pending = false;
	if (shutting_down) {
		return;
	}
	schedule_prune_removed_paths();
}

void AssetTagRegistry::_debounced_prune_removed_paths() {
	_on_prune_debounce_timeout();
}

void AssetTagRegistry::prune_removed_paths() {
	prune_scheduled = false;
	if (shutting_down) {
		return;
	}

	AssetTagCoordinator *coordinator = AssetTagCoordinator::get_singleton();
	if ((coordinator && coordinator->is_in_transaction()) || batch_depth > 0) {
		prune_queued_while_busy = true;
		prune_scan_active = false;
		prune_scan_paths.clear();
		prune_stale_paths.clear();
		prune_scan_cursor = 0;
		return;
	}

	if (!prune_scan_active) {
		last_prune_invoke_msec = OS::get_singleton()->get_ticks_msec();
		prune_queued_while_busy = false;
		prune_scan_paths.clear();
		prune_stale_paths.clear();
		prune_scan_cursor = 0;
		prune_scan_paths.resize(asset_index.size());
		int write_idx = 0;
		for (const KeyValue<String, Vector<String>> &kv : asset_index) {
			prune_scan_paths.write[write_idx++] = kv.key;
		}
		prune_scan_active = true;
	}

	const int end = MIN(prune_scan_cursor + PRUNE_PATHS_PER_FRAME, prune_scan_paths.size());
	for (int i = prune_scan_cursor; i < end; i++) {
		const String &path = prune_scan_paths[i];
		if (!asset_index.has(path)) {
			continue;
		}
		if (!_asset_path_exists(path)) {
			prune_stale_paths.push_back(path);
		}
	}
	prune_scan_cursor = end;

	if (prune_scan_cursor < prune_scan_paths.size()) {
		prune_scheduled = true;
		call_deferred(SNAME("prune_removed_paths"));
		return;
	}

	prune_scan_active = false;
	prune_scan_paths.clear();
	const Vector<String> stale = prune_stale_paths;
	prune_stale_paths.clear();
	prune_scan_cursor = 0;

	if (stale.is_empty()) {
		if (prune_queued_while_busy) {
			prune_queued_while_busy = false;
			schedule_prune_removed_paths();
		}
		return;
	}

	bool started_transaction = false;
	if (coordinator && !coordinator->is_in_transaction() && batch_depth == 0) {
		if (coordinator->begin_transaction() == OK) {
			started_transaction = true;
		}
	}

	const HashMap<String, Vector<String>> backup_index = asset_index;
	const HashMap<String, HashSet<String>> backup_reverse = reverse_lookup;

	HashSet<String> changed;
	for (int i = 0; i < stale.size(); i++) {
		if (!asset_index.has(stale[i])) {
			continue;
		}
		_remove_path_from_reverse_lookup(stale[i], asset_index[stale[i]]);
		asset_index.erase(stale[i]);
		changed.insert(stale[i]);
	}

	if (changed.is_empty()) {
		if (started_transaction && coordinator) {
			coordinator->abort_transaction();
		}
		return;
	}

	const Error err = _persist_and_notify(changed);
	if (err != OK) {
		_restore_index_state(backup_index, backup_reverse);
		if (started_transaction && coordinator) {
			coordinator->abort_transaction();
		}
		return;
	}
	if (started_transaction && coordinator) {
		coordinator->commit_transaction();
	}

	if (prune_queued_while_busy) {
		prune_queued_while_busy = false;
		schedule_prune_removed_paths();
	}
}

void AssetTagRegistry::prepare_for_teardown() {
	shutting_down = true;
	prune_scheduled = false;
	prune_debounce_pending = false;
	prune_queued_while_busy = false;
	prune_scan_active = false;
	prune_scan_paths.clear();
	prune_stale_paths.clear();
	prune_scan_cursor = 0;

	if (save_scheduled || index_dirty) {
		save_scheduled = false;
		_save_index_incremental();
	}
}

bool AssetTagRegistry::consume_queued_prune() {
	if (!prune_queued_while_busy || shutting_down) {
		return false;
	}
	prune_queued_while_busy = false;
	return true;
}

void AssetTagRegistry::flush_deferred_work_for_tests() {
	if (save_scheduled) {
		_deferred_save();
	}
	if (prune_scheduled || prune_queued_while_busy || prune_scan_active) {
		int guard = 0;
		do {
			prune_removed_paths();
		} while ((prune_scan_active || prune_scheduled) && ++guard < 100000);
	}
}
