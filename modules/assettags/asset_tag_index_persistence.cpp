/**************************************************************************/
/*  asset_tag_index_persistence.cpp                                       */
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

#include "asset_tag_index_persistence.h"

#include "asset_tag_runtime.h"
#include "asset_tag_serializer.h"
#include "asset_tag_sidecar_io.h"
#include "persistence/asset_tag_file_io.h"
#include "persistence/incremental_index_sidecar_io.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/templates/hash_set.h"

static int g_index_dirty_sidecar_count = -1;
static HashSet<String> g_index_dirty_sidecar_names;

bool AssetTagIndexPersistence::load_index(HashMap<String, Vector<String>> &r_index) {
	r_index.clear();

	String path = AssetTagStorage::get_index_file_path();
	if (!FileAccess::exists(path)) {
		return apply_dirty_sidecars(r_index);
	}

	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
	if (file.is_null()) {
		return false;
	}

	const String file_text = file->get_as_text();
	file.unref();
	Variant parsed = JSON::parse_string(file_text);
	if (parsed.get_type() != Variant::DICTIONARY) {
		WARN_PRINT("AssetTagStorage: corrupt asset index detected; writes blocked until recovery.");
		return false;
	}

	Dictionary root = parsed;
	Array keys = root.keys();
	for (int i = 0; i < keys.size(); i++) {
		String asset_path = keys[i];
		Vector<String> tag_list;
		if (root[asset_path].get_type() == Variant::ARRAY) {
			Array tags = root[asset_path];
			for (int j = 0; j < tags.size(); j++) {
				tag_list.push_back(String(tags[j]));
			}
		}
		r_index[asset_path] = tag_list;
	}

	return apply_dirty_sidecars(r_index);
}

bool AssetTagIndexPersistence::save_index(const HashMap<String, Vector<String>> &p_index) {
	if (AssetTagStorage::is_index_write_blocked()) {
		return false;
	}
	if (!AssetTagStorage::ensure_storage_dir()) {
		return false;
	}

	const String path = AssetTagStorage::get_index_file_path();
	return write_index_payload(p_index, path) && clear_index_dirty_sidecars();
}

bool AssetTagIndexPersistence::save_index_merge(const HashMap<String, Vector<String>> &p_full_index, const HashSet<String> &p_dirty_paths) {
	if (AssetTagStorage::is_index_write_blocked()) {
		return false;
	}
	if (!AssetTagStorage::ensure_storage_dir()) {
		return false;
	}
	if (p_dirty_paths.is_empty()) {
		return true;
	}
	return save_index_dirty_sidecars(p_full_index, p_dirty_paths);
}

String AssetTagIndexPersistence::get_index_dirty_dir() {
	return AssetTagStorage::get_storage_dir().path_join("index_dirty");
}

bool AssetTagIndexPersistence::clear_index_dirty_sidecars() {
	g_index_dirty_sidecar_count = 0;
	g_index_dirty_sidecar_names.clear();
	return AssetTagSidecarIO::clear_index_dirty_sidecars(get_index_dirty_dir());
}

bool AssetTagIndexPersistence::quarantine_index_dirty_sidecars() {
	g_index_dirty_sidecar_count = 0;
	g_index_dirty_sidecar_names.clear();
	return AssetTagSidecarIO::quarantine_index_dirty_sidecars(get_index_dirty_dir());
}

bool AssetTagIndexPersistence::quarantine_corrupt_index() {
	const String index_path = AssetTagStorage::get_index_file_path();
	if (!FileAccess::exists(index_path)) {
		return true;
	}
	const String corrupt_path = index_path + ".corrupt";
	if (!AssetTagFileIO::copy_file_strict(index_path, corrupt_path)) {
		return false;
	}
	return AssetTagFileIO::remove_file_if_exists(index_path);
}

bool AssetTagIndexPersistence::apply_dirty_sidecars(HashMap<String, Vector<String>> &r_index) {
	return AssetTagSidecarIO::apply_dirty_sidecars(get_index_dirty_dir(), r_index);
}

bool AssetTagIndexPersistence::compact_index_sidecars(const HashMap<String, Vector<String>> &p_full_index) {
	HashMap<String, Vector<String>> merged = p_full_index;
	if (!apply_dirty_sidecars(merged)) {
		return false;
	}
	if (!AssetTagStorage::save_index(merged)) {
		return false;
	}
	return clear_index_dirty_sidecars();
}

bool AssetTagIndexPersistence::save_index_dirty_sidecars(const HashMap<String, Vector<String>> &p_full_index, const HashSet<String> &p_dirty_paths) {
	if (AssetTagStorage::is_index_write_blocked()) {
		return false;
	}
	if (!AssetTagStorage::ensure_storage_dir()) {
		return false;
	}
	const String dirty_dir = get_index_dirty_dir();
	Vector<String> written_file_names;
	if (!AssetTagSidecarIO::save_dirty_sidecars(dirty_dir, p_full_index, p_dirty_paths, &written_file_names)) {
		return false;
	}
	if (g_index_dirty_sidecar_count < 0) {
		g_index_dirty_sidecar_names.clear();
		Ref<DirAccess> dir = DirAccess::open(dirty_dir);
		if (dir.is_valid()) {
			dir->list_dir_begin();
			String file_name = dir->get_next();
			while (!file_name.is_empty()) {
				if (!dir->current_is_dir() && file_name.ends_with(".json")) {
					g_index_dirty_sidecar_names.insert(file_name);
				}
				file_name = dir->get_next();
			}
			dir->list_dir_end();
			g_index_dirty_sidecar_count = g_index_dirty_sidecar_names.size();
		} else {
			g_index_dirty_sidecar_count = 0;
		}
	}
	for (int i = 0; i < written_file_names.size(); i++) {
		g_index_dirty_sidecar_names.insert(written_file_names[i]);
	}
	g_index_dirty_sidecar_count = g_index_dirty_sidecar_names.size();
	if (g_index_dirty_sidecar_count >= IncrementalIndexSidecarIO::COMPACT_THRESHOLD) {
		if (compact_index_sidecars(p_full_index)) {
			g_index_dirty_sidecar_count = 0;
			g_index_dirty_sidecar_names.clear();
		} else {
			WARN_PRINT_ONCE("AssetTagStorage: compact_index_sidecars failed; retaining dirty sidecar tracking for retry.");
			AssetTagRuntime::notify_sidecar_dirty();
			return false;
		}
	}
	AssetTagRuntime::notify_sidecar_dirty();
	return true;
}

bool AssetTagIndexPersistence::write_index_payload(const HashMap<String, Vector<String>> &p_index, const String &p_path) {
	return AssetTagFileIO::atomic_write_text_file(p_path, AssetTagSerializer::index_to_json(p_index));
}
