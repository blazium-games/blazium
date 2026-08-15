/**************************************************************************/
/*  asset_tag_sidecar_io.cpp                                              */
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

#include "asset_tag_sidecar_io.h"
#include "persistence/asset_tag_file_io.h"
#include "persistence/incremental_index_sidecar_io.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/templates/hash_map.h"

static bool _quarantine_sidecar_file(const String &p_dirty_dir, const String &p_file_name) {
	const String quarantine_dir = p_dirty_dir.path_join("quarantine");
	Ref<DirAccess> parent = DirAccess::open(p_dirty_dir);
	if (parent.is_null() && p_dirty_dir.begins_with("user://")) {
		parent = DirAccess::create(DirAccess::ACCESS_USERDATA);
	}
	if (parent.is_null()) {
		return false;
	}
	if (!parent->dir_exists("quarantine")) {
		parent->make_dir("quarantine");
	}
	const String source = p_dirty_dir.path_join(p_file_name);
	const String destination = quarantine_dir.path_join(p_file_name);
	WARN_PRINT(vformat("AssetTagSidecarIO: quarantining malformed sidecar '%s'.", p_file_name));
	return AssetTagFileIO::rename_file_absolute(source, destination);
}

static bool _atomic_write_text_file(const String &p_path, const String &p_text) {
	return AssetTagFileIO::atomic_write_text_file(p_path, p_text);
}

String AssetTagSidecarIO::encode_sidecar_name(const String &p_path) {
	return IncrementalIndexSidecarIO::encode_sidecar_name(p_path);
}

uint64_t AssetTagSidecarIO::get_index_dirty_max_mtime(const String &p_dirty_dir) {
	if (!DirAccess::dir_exists_absolute(p_dirty_dir)) {
		return 0;
	}
	uint64_t max_mtime = FileAccess::get_modified_time(p_dirty_dir);
	Ref<DirAccess> dir = DirAccess::open(p_dirty_dir);
	if (dir.is_null()) {
		return max_mtime;
	}
	dir->list_dir_begin();
	String file_name = dir->get_next();
	while (!file_name.is_empty()) {
		if (!dir->current_is_dir() && file_name.ends_with(".json")) {
			max_mtime = MAX(max_mtime, FileAccess::get_modified_time(p_dirty_dir.path_join(file_name)));
		}
		file_name = dir->get_next();
	}
	dir->list_dir_end();
	return max_mtime;
}

bool AssetTagSidecarIO::apply_dirty_sidecars(const String &p_dirty_dir, HashMap<String, Vector<String>> &r_index) {
	const Vector<IncrementalIndexSidecarIO::SidecarApplyItem> sidecar_files = IncrementalIndexSidecarIO::collect_sorted_sidecar_files(p_dirty_dir);
	for (int i = 0; i < sidecar_files.size(); i++) {
		const String sidecar_file_name = sidecar_files[i].file_name;
		Ref<FileAccess> file = FileAccess::open(p_dirty_dir.path_join(sidecar_file_name), FileAccess::READ);
		if (file.is_valid()) {
			const Variant parsed = JSON::parse_string(file->get_as_text());
			if (parsed.get_type() == Variant::DICTIONARY) {
				const Dictionary item = parsed;
				const String path = item.get("path", "");
				if (!path.is_empty()) {
					if (bool(item.get("deleted", false))) {
						r_index.erase(path);
					} else {
						const Array tags_array = item.get("tags", Array());
						Vector<String> tags;
						for (int j = 0; j < tags_array.size(); j++) {
							tags.push_back(String(tags_array[j]));
						}
						r_index[path] = tags;
					}
				}
			} else {
				_quarantine_sidecar_file(p_dirty_dir, sidecar_file_name);
			}
		}
	}
	return true;
}

bool AssetTagSidecarIO::save_dirty_sidecars(
		const String &p_dirty_dir,
		const HashMap<String, Vector<String>> &p_full_index,
		const HashSet<String> &p_dirty_paths,
		Vector<String> *r_written_file_names) {
	Vector<String> written_sidecars;
	for (const String &path : p_dirty_paths) {
		const String sidecar_file_name = resolve_unique_sidecar_file_name(p_dirty_dir, path);
		const String sidecar_path = p_dirty_dir.path_join(sidecar_file_name);
		Dictionary payload;
		payload["path"] = path;
		if (p_full_index.has(path)) {
			Array tags;
			for (int i = 0; i < p_full_index[path].size(); i++) {
				tags.push_back(p_full_index[path][i]);
			}
			payload["tags"] = tags;
		} else {
			payload["deleted"] = true;
		}
		if (!_atomic_write_text_file(sidecar_path, JSON::stringify(payload, "\t"))) {
			for (int i = 0; i < written_sidecars.size(); i++) {
				Ref<DirAccess> rollback_dir = DirAccess::open(p_dirty_dir);
				if (rollback_dir.is_valid()) {
					rollback_dir->remove(written_sidecars[i]);
				}
			}
			return false;
		}
		written_sidecars.push_back(sidecar_file_name);
	}
	if (r_written_file_names) {
		*r_written_file_names = written_sidecars;
	}
	return true;
}

bool AssetTagSidecarIO::clear_index_dirty_sidecars(const String &p_dirty_dir) {
	if (!DirAccess::dir_exists_absolute(p_dirty_dir)) {
		return true;
	}
	Ref<DirAccess> dir = DirAccess::open(p_dirty_dir);
	if (dir.is_null()) {
		return false;
	}
	dir->list_dir_begin();
	String file_name = dir->get_next();
	while (!file_name.is_empty()) {
		if (!dir->current_is_dir()) {
			AssetTagFileIO::remove_file_if_exists(p_dirty_dir.path_join(file_name));
		}
		file_name = dir->get_next();
	}
	dir->list_dir_end();
	return true;
}

bool AssetTagSidecarIO::quarantine_index_dirty_sidecars(const String &p_dirty_dir) {
	if (!DirAccess::dir_exists_absolute(p_dirty_dir)) {
		return true;
	}
	const String quarantine_dir = p_dirty_dir.path_join("quarantine");
	Ref<DirAccess> parent = DirAccess::open(p_dirty_dir);
	if (parent.is_null()) {
		return false;
	}
	if (!parent->dir_exists("quarantine")) {
		parent->make_dir("quarantine");
	}
	Ref<DirAccess> dir = DirAccess::open(p_dirty_dir);
	if (dir.is_null()) {
		return false;
	}
	dir->list_dir_begin();
	String file_name = dir->get_next();
	while (!file_name.is_empty()) {
		if (!dir->current_is_dir() && file_name.ends_with(".json")) {
			const String source = p_dirty_dir.path_join(file_name);
			const String destination = quarantine_dir.path_join(file_name);
			if (!AssetTagFileIO::rename_file_absolute(source, destination)) {
				WARN_PRINT(vformat("AssetTagSidecarIO: failed to quarantine sidecar '%s'.", file_name));
			}
		}
		file_name = dir->get_next();
	}
	dir->list_dir_end();
	return true;
}

String AssetTagSidecarIO::resolve_unique_sidecar_file_name(const String &p_dirty_dir, const String &p_path) {
	return IncrementalIndexSidecarIO::resolve_unique_sidecar_file_name(p_dirty_dir, p_path);
}
