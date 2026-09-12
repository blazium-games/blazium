/**************************************************************************/
/*  asset_tag_storage.cpp                                                 */
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

#include "asset_tag_storage.h"

#include "asset_tag_index_persistence.h"
#include "asset_tag_runtime.h"
#include "asset_tag_serializer.h"
#include "asset_tag_sidecar_io.h"
#include "asset_tag_undo_stack.h"
#include "persistence/asset_tag_dictionary_persistence.h"
#include "persistence/asset_tag_file_io.h"
#include "persistence/incremental_index_sidecar_io.h"

#ifdef TESTS_ENABLED
#include "asset_tag_coordinator.h"
#endif

#include "core/config/project_settings.h"
#include "core/error/error_list.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "core/templates/hash_set.h"

String AssetTagStorage::test_storage_override;
static bool g_index_write_blocked = false;

void AssetTagStorage::set_index_write_blocked(bool p_blocked) {
	g_index_write_blocked = p_blocked;
}

bool AssetTagStorage::is_index_write_blocked() {
	return g_index_write_blocked;
}

#ifdef TESTS_ENABLED
static bool test_fail_index_commit = false;

void AssetTagStorage::set_test_fail_index_commit(bool p_fail) {
	test_fail_index_commit = p_fail;
}
#endif

static bool _remove_file_if_exists(const String &p_path) {
	return AssetTagFileIO::remove_file_if_exists(p_path);
}

static Ref<DirAccess> _open_storage_parent_dir() {
	const String storage_dir = AssetTagStorage::get_storage_dir();
	const String parent = storage_dir.get_base_dir();
	Ref<DirAccess> dir = DirAccess::open(parent);
	if (dir.is_valid()) {
		return dir;
	}
	if (storage_dir.begins_with("user://")) {
		return DirAccess::create(DirAccess::ACCESS_USERDATA);
	}
	if (storage_dir.begins_with("res://")) {
		return DirAccess::create(DirAccess::ACCESS_RESOURCES);
	}
	DirAccess::make_dir_recursive_absolute(parent);
	return DirAccess::create_for_path(parent);
}

#ifdef TESTS_ENABLED
static String _normalize_test_storage_dir(const String &p_dir) {
	String suffix = p_dir;
	if (p_dir.begins_with("res://.blazium/")) {
		suffix = p_dir.trim_prefix("res://.blazium/");
	} else if (p_dir.begins_with("user://") || p_dir.begins_with("res://")) {
		return p_dir;
	}
	return OS::get_singleton()->get_cache_path().path_join("blazium_assettags_test").path_join(suffix);
}

static void _ensure_assettags_test_defaults() {
	if (ProjectSettings *settings = ProjectSettings::get_singleton()) {
		settings->set_setting("blazium/assettags/strict_paths", false);
		settings->set_setting("blazium/assettags/strict_tags", false);
	}
}

static void _reset_isolated_test_storage_dir(const String &p_dir) {
	if (!p_dir.begins_with("user://") && !p_dir.begins_with("res://")) {
		if (DirAccess::dir_exists_absolute(p_dir)) {
			Ref<DirAccess> existing = DirAccess::create_for_path(p_dir);
			if (existing.is_valid() && existing->change_dir(p_dir) == OK) {
				existing->erase_contents_recursive();
			}
			DirAccess::remove_absolute(p_dir);
		}
		DirAccess::make_dir_recursive_absolute(p_dir);
		return;
	}

	Ref<DirAccess> dir = DirAccess::open(p_dir.get_base_dir());
	if (dir.is_null() && p_dir.begins_with("user://")) {
		dir = DirAccess::create(DirAccess::ACCESS_USERDATA);
	}
	if (dir.is_null() && p_dir.begins_with("res://")) {
		dir = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	}
	if (dir.is_null()) {
		return;
	}
	if (dir->dir_exists(p_dir)) {
		Ref<DirAccess> existing = DirAccess::open(p_dir);
		if (existing.is_valid()) {
			existing->erase_contents_recursive();
		}
		dir->remove(p_dir);
	}
	dir->make_dir_recursive(p_dir);
}
#endif

static bool _write_dictionary_payload(const HashMap<String, AssetTagEntry> &p_tags, const Vector<AssetTagRedirect> &p_redirects, const String &p_path) {
	return AssetTagDictionaryPersistence::write_to_path(p_path, p_tags, p_redirects);
}

#ifdef TESTS_ENABLED
void AssetTagStorage::set_test_storage_dir(const String &p_dir) {
	_ensure_assettags_test_defaults();
	set_index_write_blocked(false);
	test_fail_index_commit = false;
	assettags_reset_coordinator_test_state();
	clear_undo_state();
	AssetTagRuntime::invalidate_cache();
	const String normalized = _normalize_test_storage_dir(p_dir);
	_reset_isolated_test_storage_dir(normalized);
	test_storage_override = normalized;
}

void AssetTagStorage::clear_test_storage_dir() {
	assettags_reset_coordinator_test_state();
	clear_undo_state();
	AssetTagRuntime::invalidate_cache();
	if (!test_storage_override.is_empty()) {
		if (!test_storage_override.begins_with("user://") && !test_storage_override.begins_with("res://")) {
			if (DirAccess::dir_exists_absolute(test_storage_override)) {
				Ref<DirAccess> existing = DirAccess::create_for_path(test_storage_override);
				if (existing.is_valid() && existing->change_dir(test_storage_override) == OK) {
					existing->erase_contents_recursive();
				}
				DirAccess::remove_absolute(test_storage_override);
			}
		} else {
			Ref<DirAccess> dir = DirAccess::open(test_storage_override.get_base_dir());
			if (dir.is_null() && test_storage_override.begins_with("user://")) {
				dir = DirAccess::create(DirAccess::ACCESS_USERDATA);
			}
			if (dir.is_valid() && dir->dir_exists(test_storage_override)) {
				Ref<DirAccess> existing = DirAccess::open(test_storage_override);
				if (existing.is_valid()) {
					existing->erase_contents_recursive();
				}
				dir->remove(test_storage_override);
			}
		}
	}
	test_storage_override = String();
	test_fail_index_commit = false;
	set_index_write_blocked(false);
}
#endif

String AssetTagStorage::get_storage_dir() {
	if (!test_storage_override.is_empty()) {
		return test_storage_override;
	}
	return "res://.blazium/asset_tags";
}

String AssetTagStorage::get_tags_file_path() {
	return get_storage_dir().path_join("tags.json");
}

String AssetTagStorage::get_index_file_path() {
	return get_storage_dir().path_join("asset_index.json");
}

bool AssetTagStorage::ensure_storage_dir() {
	Ref<DirAccess> dir = _open_storage_parent_dir();
	if (dir.is_null()) {
		return false;
	}
	const String storage_dir = get_storage_dir();
	if (!dir->dir_exists(storage_dir)) {
		Error err = dir->make_dir_recursive(storage_dir);
		if (err != OK && err != ERR_ALREADY_EXISTS) {
			return false;
		}
	}
	const String index_dirty_dir = get_index_dirty_dir();
	if (!dir->dir_exists(index_dirty_dir)) {
		Error err = dir->make_dir_recursive(index_dirty_dir);
		if (err != OK && err != ERR_ALREADY_EXISTS) {
			return false;
		}
	}
	return true;
}

bool AssetTagStorage::load_dictionary(HashMap<String, AssetTagEntry> &r_tags, Vector<AssetTagRedirect> &r_redirects) {
	const String path = get_tags_file_path();
	if (!FileAccess::exists(path)) {
		r_tags.clear();
		r_redirects.clear();
		return true;
	}
	if (!AssetTagDictionaryPersistence::load_from_path(path, r_tags, r_redirects)) {
		quarantine_corrupt_dictionary();
		return false;
	}
	return true;
}

bool AssetTagStorage::save_dictionary(const HashMap<String, AssetTagEntry> &p_tags, const Vector<AssetTagRedirect> &p_redirects) {
	if (!ensure_storage_dir()) {
		return false;
	}
	return AssetTagDictionaryPersistence::write_to_path(get_tags_file_path(), p_tags, p_redirects);
}

bool AssetTagStorage::load_index(HashMap<String, Vector<String>> &r_index) {
	return AssetTagIndexPersistence::load_index(r_index);
}

bool AssetTagStorage::save_index(const HashMap<String, Vector<String>> &p_index) {
#ifdef TESTS_ENABLED
	if (test_fail_index_commit) {
		return false;
	}
#endif
	return AssetTagIndexPersistence::save_index(p_index);
}

bool AssetTagStorage::save_index_merge(const HashMap<String, Vector<String>> &p_full_index, const HashSet<String> &p_dirty_paths) {
	return AssetTagIndexPersistence::save_index_merge(p_full_index, p_dirty_paths);
}

String AssetTagStorage::get_index_dirty_dir() {
	return AssetTagIndexPersistence::get_index_dirty_dir();
}

bool AssetTagStorage::clear_index_dirty_sidecars() {
	return AssetTagIndexPersistence::clear_index_dirty_sidecars();
}

bool AssetTagStorage::quarantine_index_dirty_sidecars() {
	return AssetTagIndexPersistence::quarantine_index_dirty_sidecars();
}

bool AssetTagStorage::quarantine_corrupt_index() {
	return AssetTagIndexPersistence::quarantine_corrupt_index();
}

bool AssetTagStorage::quarantine_corrupt_dictionary() {
	const String tags_path = get_tags_file_path();
	if (!FileAccess::exists(tags_path)) {
		return true;
	}
	const String corrupt_path = tags_path + ".corrupt";
	WARN_PRINT("AssetTagStorage: quarantining corrupt tag dictionary.");
	if (!AssetTagFileIO::copy_file_strict(tags_path, corrupt_path)) {
		return false;
	}
	return AssetTagFileIO::remove_file_if_exists(tags_path);
}

bool AssetTagStorage::apply_dirty_sidecars(HashMap<String, Vector<String>> &r_index) {
	return AssetTagIndexPersistence::apply_dirty_sidecars(r_index);
}

bool AssetTagStorage::compact_index_sidecars(const HashMap<String, Vector<String>> &p_full_index) {
	return AssetTagIndexPersistence::compact_index_sidecars(p_full_index);
}

bool AssetTagStorage::save_index_dirty_sidecars(const HashMap<String, Vector<String>> &p_full_index, const HashSet<String> &p_dirty_paths) {
	return AssetTagIndexPersistence::save_index_dirty_sidecars(p_full_index, p_dirty_paths);
}

bool AssetTagStorage::commit_dictionary_and_index(
		const HashMap<String, AssetTagEntry> &p_tags,
		const Vector<AssetTagRedirect> &p_redirects,
		const HashMap<String, Vector<String>> &p_index,
		bool p_save_dictionary,
		bool p_save_index) {
	if (!p_save_dictionary && !p_save_index) {
		return true;
	}
	if (p_save_index && g_index_write_blocked) {
		return false;
	}
	if (!ensure_storage_dir()) {
		return false;
	}

	const String tags_path = get_tags_file_path();
	const String index_path = get_index_file_path();
	const String tags_backup = tags_path + ".rollback";
	const String index_backup = index_path + ".rollback";
	bool dictionary_committed = false;

	if (p_save_dictionary) {
		if (!AssetTagFileIO::backup_file_for_rollback(tags_path, tags_backup)) {
			return false;
		}
		if (!_write_dictionary_payload(p_tags, p_redirects, tags_path)) {
			AssetTagFileIO::restore_file_from_rollback(tags_backup, tags_path);
			_remove_file_if_exists(tags_backup);
			return false;
		}
		dictionary_committed = true;
	}

	if (p_save_index) {
#ifdef TESTS_ENABLED
		if (test_fail_index_commit) {
			if (dictionary_committed) {
				AssetTagFileIO::restore_file_from_rollback(tags_backup, tags_path);
			}
			_remove_file_if_exists(tags_backup);
			_remove_file_if_exists(index_backup);
			return false;
		}
#endif
		if (!AssetTagFileIO::backup_file_for_rollback(index_path, index_backup)) {
			if (dictionary_committed) {
				AssetTagFileIO::restore_file_from_rollback(tags_backup, tags_path);
			}
			_remove_file_if_exists(tags_backup);
			return false;
		}
		if (!AssetTagIndexPersistence::write_index_payload(p_index, index_path)) {
			AssetTagFileIO::restore_file_from_rollback(index_backup, index_path);
			if (dictionary_committed) {
				AssetTagFileIO::restore_file_from_rollback(tags_backup, tags_path);
			}
			_remove_file_if_exists(tags_backup);
			_remove_file_if_exists(index_backup);
			return false;
		}
		if (!clear_index_dirty_sidecars()) {
			AssetTagFileIO::restore_file_from_rollback(index_backup, index_path);
			if (dictionary_committed) {
				AssetTagFileIO::restore_file_from_rollback(tags_backup, tags_path);
			}
			_remove_file_if_exists(tags_backup);
			_remove_file_if_exists(index_backup);
			return false;
		}
	}

	_remove_file_if_exists(tags_backup);
	_remove_file_if_exists(index_backup);
	return true;
}

Error AssetTagStorage::snapshot_undo_state() {
	return AssetTagUndoStack::snapshot();
}

Error AssetTagStorage::restore_undo_state() {
	return AssetTagUndoStack::restore();
}

bool AssetTagStorage::has_undo_state() {
	return AssetTagUndoStack::has_state();
}

void AssetTagStorage::clear_undo_state() {
	AssetTagUndoStack::clear();
}

void AssetTagStorage::rotate_undo_stack_after_restore() {
	AssetTagUndoStack::rotate_after_restore();
}

void AssetTagStorage::mark_undo_snapshot_committed() {
	AssetTagUndoStack::mark_committed();
}

bool AssetTagStorage::is_taggable_extension(const String &p_path) {
	static HashSet<String> extensions;
	static String configured_extensions;
	if (ProjectSettings::get_singleton()) {
		const String current = String(GLOBAL_GET("blazium/assettags/taggable_extensions"));
		if (current != configured_extensions) {
			configured_extensions = current;
			extensions.clear();
			if (!configured_extensions.is_empty()) {
				const PackedStringArray parts = configured_extensions.split(",", false);
				for (int i = 0; i < parts.size(); i++) {
					const String ext = String(parts[i]).strip_edges().to_lower();
					if (!ext.is_empty()) {
						extensions.insert(ext);
					}
				}
			}
		}
	}
	if (extensions.is_empty()) {
		const char *exts[] = {
			"glb", "gltf", "obj", "fbx", "blend",
			"png", "jpg", "jpeg", "webp", "svg", "exr", "bmp", "tga",
			"tscn", "scn", "gd", "luau", "cs",
			"tres", "material", "gdshader", "gdshaderinc", "res", "remap",
			nullptr
		};
		for (int i = 0; exts[i]; i++) {
			extensions.insert(String(exts[i]));
		}
	}
	String ext = p_path.get_extension().to_lower();
	return extensions.has(ext);
}

String AssetTagStorage::normalize_asset_path(const String &p_path) {
	String path = p_path.strip_edges().replace("\\", "/");
	if (path.is_empty()) {
		return path;
	}
	if (path.begins_with("res://")) {
		const String rest = path.substr(6).simplify_path();
		path = rest.is_empty() ? String("res://") : String("res://") + rest;
	} else {
		path = path.simplify_path();
	}
	return path;
}
