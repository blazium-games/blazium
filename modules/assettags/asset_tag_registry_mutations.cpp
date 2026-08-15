/**************************************************************************/
/*  asset_tag_registry_mutations.cpp                                      */
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

#include "asset_tag_manager.h"
#include "core/config/project_settings.h"
#include "core/io/file_access.h"

Error AssetTagRegistry::set_tags_for_asset(const String &p_path, const PackedStringArray &p_tags) {
	const String path = AssetTagStorage::normalize_asset_path(p_path);
	if (!path.begins_with("res://") || path == "res://") {
		return ERR_INVALID_PARAMETER;
	}
#ifdef TOOLS_ENABLED
	if (ProjectSettings::get_singleton() && bool(GLOBAL_GET("blazium/assettags/strict_paths")) && !FileAccess::exists(path)) {
		return ERR_DOES_NOT_EXIST;
	}
#endif

	const bool opened_batch = batch_depth == 0;
	if (opened_batch) {
		begin_batch();
	}

	AssetTagManager *manager = AssetTagManager::get_singleton();
	if (!manager) {
		if (opened_batch) {
			abort_batch();
		}
		return ERR_UNCONFIGURED;
	}

	Vector<String> validated;
	for (int i = 0; i < p_tags.size(); i++) {
		const String tag = p_tags[i];
		if (tag.is_empty()) {
			continue;
		}
		if (!manager->has_tag_in_dictionary(tag)) {
			if (ProjectSettings::get_singleton() && bool(GLOBAL_GET("blazium/assettags/strict_tags"))) {
				if (opened_batch) {
					abort_batch();
				}
				return ERR_INVALID_PARAMETER;
			}
			const Error add_err = manager->add_tag(tag);
			if (add_err != OK && add_err != ERR_ALREADY_EXISTS) {
				if (opened_batch) {
					abort_batch();
				}
				return add_err;
			}
		}
		validated.push_back(tag);
	}
	validated = _dedup_tags(validated);

	Vector<String> old_tags;
	if (asset_index.has(path)) {
		old_tags = asset_index[path];
	}

	if (validated.size() == old_tags.size()) {
		Vector<String> sorted_old = old_tags;
		sorted_old.sort();
		Vector<String> sorted_new = validated;
		sorted_new.sort();
		bool unchanged = true;
		for (int i = 0; i < sorted_new.size(); i++) {
			if (sorted_new[i] != sorted_old[i]) {
				unchanged = false;
				break;
			}
		}
		if (unchanged) {
			// Still notify when the asset is gone so indexers can drop stale entries.
			if (FileAccess::exists(path)) {
				if (opened_batch) {
					cancel_opened_batch();
				}
				return OK;
			}
		}
	}

	const HashMap<String, Vector<String>> backup_index(asset_index);
	const HashMap<String, HashSet<String>> backup_reverse(reverse_lookup);

	if (validated.is_empty()) {
		if (asset_index.has(path)) {
			_remove_path_from_reverse_lookup(path, old_tags);
			asset_index.erase(path);
		}
	} else {
		asset_index[path] = validated;
		_update_reverse_lookup_for_path(path, old_tags, validated);
	}

	HashSet<String> changed;
	changed.insert(path);
	const Error err = _persist_and_notify(changed);
	if (err != OK) {
		_restore_index_state(backup_index, backup_reverse);
		if (opened_batch) {
			abort_batch();
		}
		return err;
	}
	if (opened_batch) {
		const Error commit_err = commit_batch();
		if (commit_err != OK) {
			abort_batch();
			return commit_err;
		}
	}
	return OK;
}

Error AssetTagRegistry::add_tags_to_asset(const String &p_path, const PackedStringArray &p_tags) {
	const String path = AssetTagStorage::normalize_asset_path(p_path);
	HashSet<String> merged;
	PackedStringArray current = get_tags_for_asset(path);
	for (int i = 0; i < current.size(); i++) {
		merged.insert(current[i]);
	}
	for (int i = 0; i < p_tags.size(); i++) {
		merged.insert(p_tags[i]);
	}
	PackedStringArray combined;
	for (const String &tag : merged) {
		combined.push_back(tag);
	}
	return set_tags_for_asset(path, combined);
}

Error AssetTagRegistry::remove_tags_from_asset(const String &p_path, const PackedStringArray &p_tags) {
	const String path = AssetTagStorage::normalize_asset_path(p_path);
	PackedStringArray current = get_tags_for_asset(path);
	for (int i = 0; i < p_tags.size(); i++) {
		for (int j = current.size() - 1; j >= 0; j--) {
			if (current[j] == p_tags[i] || tag_matches_prefix(current[j], p_tags[i])) {
				current.remove_at(j);
			}
		}
	}
	return set_tags_for_asset(path, current);
}

bool AssetTagRegistry::rename_asset_path(const String &p_old_path, const String &p_new_path) {
	const String old_path = AssetTagStorage::normalize_asset_path(p_old_path);
	const String new_path = AssetTagStorage::normalize_asset_path(p_new_path);
	if (old_path.is_empty() || new_path.is_empty() || old_path == new_path) {
		return false;
	}
	if (!asset_index.has(old_path)) {
		return false;
	}
	if (asset_index.has(new_path)) {
		return false;
	}

	const HashMap<String, Vector<String>> backup_index(asset_index);
	const HashMap<String, HashSet<String>> backup_reverse(reverse_lookup);

	const Vector<String> tags = asset_index[old_path];
	_remove_path_from_reverse_lookup(old_path, tags);
	asset_index.erase(old_path);
	asset_index.insert(new_path, tags);
	_update_reverse_lookup_for_path(new_path, Vector<String>(), tags);

	HashSet<String> changed;
	changed.insert(old_path);
	changed.insert(new_path);
	const Error err = _persist_and_notify(changed);
	if (err != OK) {
		_restore_index_state(backup_index, backup_reverse);
		return false;
	}
	return true;
}

int AssetTagRegistry::apply_tag_rename(const String &p_old_name, const String &p_new_name) {
	if (p_old_name.is_empty() || p_new_name.is_empty()) {
		return 0;
	}

	const HashMap<String, Vector<String>> backup_index(asset_index);
	const HashMap<String, HashSet<String>> backup_reverse(reverse_lookup);

	HashSet<String> candidate_paths;
	for (const KeyValue<String, HashSet<String>> &rl : reverse_lookup) {
		if (rl.key == p_old_name || rl.key.begins_with(p_old_name + ".")) {
			for (const String &path : rl.value) {
				candidate_paths.insert(path);
			}
		}
	}

	int updated_assets = 0;
	HashSet<String> changed;
	for (const String &path : candidate_paths) {
		if (!asset_index.has(path)) {
			continue;
		}
		Vector<String> &tags = asset_index[path];
		Vector<String> updated_tags;
		Vector<String> old_tags = tags;
		bool tag_changed = false;
		for (int i = 0; i < tags.size(); i++) {
			const String remapped = remap_tag_for_rename(tags[i], p_old_name, p_new_name);
			if (remapped != tags[i]) {
				tag_changed = true;
			}
			updated_tags.push_back(remapped);
		}
		if (tag_changed) {
			tags = _dedup_tags(updated_tags);
			_update_reverse_lookup_for_path(path, old_tags, tags);
			changed.insert(path);
			updated_assets++;
		}
	}

	if (updated_assets > 0) {
		const Error err = _persist_and_notify(changed);
		if (err != OK) {
			_restore_index_state(backup_index, backup_reverse);
			return 0;
		}
	}
	return updated_assets;
}

PackedStringArray AssetTagRegistry::get_indexed_paths() const {
	PackedStringArray paths;
	for (const KeyValue<String, Vector<String>> &kv : asset_index) {
		paths.push_back(kv.key);
	}
	paths.sort();
	return paths;
}

int AssetTagRegistry::apply_tag_remove(const String &p_tag_name) {
	if (p_tag_name.is_empty()) {
		return 0;
	}

	AssetTagManager *manager = AssetTagManager::get_singleton();
	HashMap<String, AssetTagEntry> dictionary_tags;
	Vector<AssetTagRedirect> redirects;
	if (manager) {
		manager->get_dictionary_snapshot(dictionary_tags, redirects);
	}

	auto tag_matches_remove = [&](const String &p_tag) -> bool {
		if (tag_matches_prefix(p_tag, p_tag_name)) {
			return true;
		}
		if (manager && manager->matches_tag(p_tag, p_tag_name)) {
			return true;
		}
		for (int i = 0; i < redirects.size(); i++) {
			const String &old_name = redirects[i].old_name;
			const String &new_name = redirects[i].new_name;
			if (tag_matches_prefix(new_name, p_tag_name) || new_name == p_tag_name) {
				if (tag_matches_prefix(p_tag, old_name) || p_tag == old_name) {
					return true;
				}
			}
		}
		return false;
	};

	const HashMap<String, Vector<String>> backup_index(asset_index);
	const HashMap<String, HashSet<String>> backup_reverse(reverse_lookup);

	HashSet<String> candidate_paths;
	for (const KeyValue<String, HashSet<String>> &rl : reverse_lookup) {
		if (tag_matches_remove(rl.key)) {
			for (const String &path : rl.value) {
				candidate_paths.insert(path);
			}
		}
	}

	int updated_assets = 0;
	HashSet<String> changed;
	Vector<String> paths_to_erase;
	for (const String &path : candidate_paths) {
		if (!asset_index.has(path)) {
			continue;
		}
		Vector<String> &tags = asset_index[path];
		const Vector<String> old_tags = tags;
		Vector<String> kept;
		bool tag_changed = false;
		for (int i = 0; i < tags.size(); i++) {
			if (tag_matches_remove(tags[i])) {
				tag_changed = true;
				continue;
			}
			kept.push_back(tags[i]);
		}
		if (tag_changed) {
			if (kept.is_empty()) {
				_remove_path_from_reverse_lookup(path, old_tags);
				paths_to_erase.push_back(path);
			} else {
				tags = kept;
				_update_reverse_lookup_for_path(path, old_tags, tags);
			}
			changed.insert(path);
			updated_assets++;
		}
	}

	for (int i = 0; i < paths_to_erase.size(); i++) {
		asset_index.erase(paths_to_erase[i]);
	}

	if (updated_assets > 0) {
		const Error err = _persist_and_notify(changed);
		if (err != OK) {
			_restore_index_state(backup_index, backup_reverse);
			return 0;
		}
	}
	return updated_assets;
}
