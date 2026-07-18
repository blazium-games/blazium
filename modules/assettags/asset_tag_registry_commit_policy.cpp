/**************************************************************************/
/*  asset_tag_registry_commit_policy.cpp                                  */
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

#include "asset_tag_registry_commit_policy.h"

#include "asset_tag_manager.h"
#include "asset_tag_runtime.h"

static bool _redirects_equal(const Vector<AssetTagRedirect> &p_left, const Vector<AssetTagRedirect> &p_right) {
	if (p_left.size() != p_right.size()) {
		return false;
	}
	for (int i = 0; i < p_left.size(); i++) {
		if (p_left[i].old_name != p_right[i].old_name || p_left[i].new_name != p_right[i].new_name) {
			return false;
		}
	}
	return true;
}

bool AssetTagRegistryCommitPolicy::persist_batch_changes(
		AssetTagManager *p_manager,
		const HashMap<String, Vector<String>> &p_asset_index,
		HashSet<String> &p_dirty_index_paths,
		bool p_save_dictionary,
		bool p_save_index) {
	if (!p_save_dictionary && !p_save_index) {
		return true;
	}

	Vector<AssetTagRedirect> disk_redirects;
	if (p_save_dictionary) {
		HashMap<String, AssetTagEntry> disk_tags;
		AssetTagStorage::load_dictionary(disk_tags, disk_redirects);
	}

	bool committed = true;
	if (p_save_dictionary && p_save_index && p_manager) {
		HashMap<String, AssetTagEntry> tags_snapshot;
		Vector<AssetTagRedirect> redirects_snapshot;
		p_manager->get_dictionary_snapshot(tags_snapshot, redirects_snapshot);
		committed = AssetTagStorage::commit_dictionary_and_index(
				tags_snapshot,
				redirects_snapshot,
				p_asset_index,
				true,
				true);
		if (committed) {
			p_manager->mark_dictionary_persisted();
			p_manager->emit_signal(SNAME("tag_dictionary_changed"));
			if (!_redirects_equal(redirects_snapshot, disk_redirects)) {
				p_manager->emit_signal(SNAME("redirects_changed"));
			}
			AssetTagRuntime::invalidate_cache();
			p_dirty_index_paths.clear();
		}
	} else if (p_save_dictionary && p_manager) {
		HashMap<String, AssetTagEntry> tags_snapshot;
		Vector<AssetTagRedirect> redirects_snapshot;
		p_manager->get_dictionary_snapshot(tags_snapshot, redirects_snapshot);
		committed = AssetTagStorage::commit_dictionary_and_index(
				tags_snapshot,
				redirects_snapshot,
				p_asset_index,
				true,
				false);
		if (committed) {
			p_manager->mark_dictionary_persisted();
			p_manager->emit_signal(SNAME("tag_dictionary_changed"));
			if (!_redirects_equal(redirects_snapshot, disk_redirects)) {
				p_manager->emit_signal(SNAME("redirects_changed"));
			}
		}
	} else if (p_save_index) {
		HashSet<String> paths_to_merge = p_dirty_index_paths;
		if (paths_to_merge.is_empty()) {
			for (const KeyValue<String, Vector<String>> &kv : p_asset_index) {
				paths_to_merge.insert(kv.key);
			}
		}
		committed = AssetTagStorage::save_index_merge(p_asset_index, paths_to_merge);
		if (committed) {
			AssetTagRuntime::invalidate_cache();
			p_dirty_index_paths.clear();
		}
	}
	return committed;
}
