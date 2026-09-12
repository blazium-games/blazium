/**************************************************************************/
/*  incremental_index_sidecar_io.cpp                                      */
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

#include "incremental_index_sidecar_io.h"

#include "asset_tag_file_io.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"

bool IncrementalIndexSidecarIO::SidecarApplyItem::operator<(const SidecarApplyItem &p_other) const {
	if (modified_time == p_other.modified_time) {
		return file_name < p_other.file_name;
	}
	return modified_time < p_other.modified_time;
}

String IncrementalIndexSidecarIO::sanitize_sidecar_path_suffix(const String &p_path) {
	String suffix = p_path;
	for (int i = 0; i < suffix.length(); i++) {
		const char32_t c = suffix[i];
		if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|' || c < 32) {
			suffix = suffix.substr(0, i) + "_" + suffix.substr(i + 1);
		}
	}
	const int max_len = 48;
	if (suffix.length() > max_len) {
		suffix = suffix.substr(suffix.length() - max_len);
	}
	return suffix;
}

String IncrementalIndexSidecarIO::encode_sidecar_name(const String &p_path) {
	return String::num_uint64(HashMapHasherDefault::hash(p_path)) + "_" + sanitize_sidecar_path_suffix(p_path);
}

Vector<IncrementalIndexSidecarIO::SidecarApplyItem> IncrementalIndexSidecarIO::collect_sorted_sidecar_files(const String &p_dirty_dir) {
	Vector<SidecarApplyItem> sidecar_files;
	if (!DirAccess::dir_exists_absolute(p_dirty_dir)) {
		return sidecar_files;
	}
	Ref<DirAccess> dir = DirAccess::open(p_dirty_dir);
	if (dir.is_null()) {
		return sidecar_files;
	}
	dir->list_dir_begin();
	String file_name = dir->get_next();
	while (!file_name.is_empty()) {
		if (!dir->current_is_dir() && file_name != "quarantine" && file_name.ends_with(".json")) {
			SidecarApplyItem item;
			item.file_name = file_name;
			item.modified_time = FileAccess::get_modified_time(p_dirty_dir.path_join(file_name));
			sidecar_files.push_back(item);
		}
		file_name = dir->get_next();
	}
	dir->list_dir_end();
	sidecar_files.sort();
	return sidecar_files;
}

String IncrementalIndexSidecarIO::resolve_unique_sidecar_file_name(const String &p_dirty_dir, const String &p_path) {
	const String base_name = encode_sidecar_name(p_path);
	String candidate = base_name;
	for (int disambiguator = 0; disambiguator < 16; disambiguator++) {
		const String file_name = candidate + ".json";
		const String file_path = p_dirty_dir.path_join(file_name);
		if (!FileAccess::exists(file_path)) {
			return file_name;
		}
		Ref<FileAccess> file = FileAccess::open(file_path, FileAccess::READ);
		if (file.is_valid()) {
			const Variant parsed = JSON::parse_string(file->get_as_text());
			if (parsed.get_type() == Variant::DICTIONARY) {
				const String existing_path = String(Dictionary(parsed).get("path", ""));
				if (existing_path == p_path) {
					return file_name;
				}
			}
		}
		candidate = base_name + "_" + String::num_int64(disambiguator + 1);
	}
	WARN_PRINT(vformat("IncrementalIndexSidecarIO: sidecar name collision for '%s'; using disambiguated filename.", p_path));
	return candidate + ".json";
}

bool IncrementalIndexSidecarIO::clear_sidecars(const String &p_dirty_dir) {
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
