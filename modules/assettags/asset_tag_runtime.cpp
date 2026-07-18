/**************************************************************************/
/*  asset_tag_runtime.cpp                                                 */
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

#include "asset_tag_runtime.h"

#include "asset_tag_sidecar_io.h"
#include "asset_tag_storage.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/object/class_db.h"
#include "persistence/asset_tag_file_io.h"

static HashMap<String, Vector<String>> g_runtime_index_cache;
static uint64_t g_runtime_index_mtime = 0;
static uint64_t g_runtime_sidecar_mtime = 0;
static bool g_runtime_pending_sidecar_merge = false;

static void _refresh_runtime_index_cache_if_needed() {
	const String index_path = AssetTagStorage::get_index_file_path();
	const uint64_t index_mtime = FileAccess::exists(index_path) ? FileAccess::get_modified_time(index_path) : 0;
	const String dirty_dir = AssetTagStorage::get_index_dirty_dir();

	if (g_runtime_index_cache.is_empty()) {
		AssetTagStorage::load_index(g_runtime_index_cache);
		g_runtime_index_mtime = index_mtime;
		g_runtime_sidecar_mtime = AssetTagSidecarIO::get_index_dirty_max_mtime(dirty_dir);
		g_runtime_pending_sidecar_merge = false;
		return;
	}

	if (index_mtime != g_runtime_index_mtime) {
		g_runtime_index_cache.clear();
		AssetTagStorage::load_index(g_runtime_index_cache);
		g_runtime_index_mtime = index_mtime;
		g_runtime_sidecar_mtime = AssetTagSidecarIO::get_index_dirty_max_mtime(dirty_dir);
		g_runtime_pending_sidecar_merge = false;
		return;
	}

	if (g_runtime_pending_sidecar_merge) {
		AssetTagSidecarIO::apply_dirty_sidecars(dirty_dir, g_runtime_index_cache);
		g_runtime_pending_sidecar_merge = false;
		g_runtime_sidecar_mtime = AssetTagSidecarIO::get_index_dirty_max_mtime(dirty_dir);
		return;
	}

	const uint64_t sidecar_mtime = AssetTagSidecarIO::get_index_dirty_max_mtime(dirty_dir);
	if (sidecar_mtime != g_runtime_sidecar_mtime) {
		AssetTagSidecarIO::apply_dirty_sidecars(dirty_dir, g_runtime_index_cache);
		g_runtime_sidecar_mtime = sidecar_mtime;
	}
}

void AssetTagRuntime::_bind_methods() {
	ClassDB::bind_static_method("AssetTagRuntime", D_METHOD("read_tags_for_asset", "path"), &AssetTagRuntime::read_tags_for_asset);
	ClassDB::bind_static_method("AssetTagRuntime", D_METHOD("read_tags_for_export_bake", "export_output_dir", "path"), &AssetTagRuntime::read_tags_for_export_bake);
	ClassDB::bind_static_method("AssetTagRuntime", D_METHOD("bake_tags_for_export", "export_output_dir"), &AssetTagRuntime::bake_tags_for_export);
	ClassDB::bind_static_method("AssetTagRuntime", D_METHOD("invalidate_cache"), &AssetTagRuntime::invalidate_cache);
	ClassDB::bind_static_method("AssetTagRuntime", D_METHOD("notify_sidecar_dirty"), &AssetTagRuntime::notify_sidecar_dirty);
}

void AssetTagRuntime::invalidate_cache() {
	g_runtime_index_mtime = 0;
	g_runtime_sidecar_mtime = 0;
	g_runtime_pending_sidecar_merge = false;
	g_runtime_index_cache.clear();
}

void AssetTagRuntime::notify_sidecar_dirty() {
	g_runtime_pending_sidecar_merge = true;
}

PackedStringArray AssetTagRuntime::read_tags_for_asset(const String &p_path) {
	PackedStringArray result;
	_refresh_runtime_index_cache_if_needed();
	if (!g_runtime_index_cache.has(p_path)) {
		return result;
	}
	const Vector<String> &tags = g_runtime_index_cache[p_path];
	for (int i = 0; i < tags.size(); i++) {
		result.push_back(tags[i]);
	}
	result.sort();
	return result;
}

PackedStringArray AssetTagRuntime::read_tags_for_export_bake(const String &p_export_output_dir, const String &p_path) {
	PackedStringArray result;
	if (p_export_output_dir.is_empty() || p_path.is_empty()) {
		return result;
	}
	const String sidecar_path = p_export_output_dir.path_join(".blazium").path_join("asset_tags").path_join(AssetTagSidecarIO::encode_sidecar_name(p_path) + ".json");
	Ref<FileAccess> file = FileAccess::open(sidecar_path, FileAccess::READ);
	if (file.is_null()) {
		return result;
	}
	const Variant parsed = JSON::parse_string(file->get_as_text());
	if (parsed.get_type() != Variant::DICTIONARY) {
		return result;
	}
	const Array tags = Dictionary(parsed).get("tags", Array());
	for (int i = 0; i < tags.size(); i++) {
		result.push_back(String(tags[i]));
	}
	result.sort();
	return result;
}

static Ref<DirAccess> _open_export_dir(const String &p_path) {
	Ref<DirAccess> dir = DirAccess::open(p_path);
	if (dir.is_valid()) {
		return dir;
	}
	if (p_path.begins_with("user://")) {
		return DirAccess::create(DirAccess::ACCESS_USERDATA);
	}
	if (p_path.begins_with("res://")) {
		return DirAccess::create(DirAccess::ACCESS_RESOURCES);
	}
	return DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
}

static bool _remove_export_dir_if_exists(const String &p_dir) {
	if (!DirAccess::dir_exists_absolute(p_dir)) {
		return true;
	}
	Ref<DirAccess> dir = DirAccess::open(p_dir);
	if (dir.is_null()) {
		return false;
	}
	dir->list_dir_begin();
	String file_name = dir->get_next();
	while (!file_name.is_empty()) {
		if (!dir->current_is_dir()) {
			AssetTagFileIO::remove_file_if_exists(p_dir.path_join(file_name));
		}
		file_name = dir->get_next();
	}
	dir->list_dir_end();
	Ref<DirAccess> parent = DirAccess::open(p_dir.get_base_dir());
	if (parent.is_valid()) {
		return parent->remove(p_dir) == OK;
	}
	return false;
}

Error AssetTagRuntime::bake_tags_for_export(const String &p_export_output_dir) {
	if (p_export_output_dir.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}

	if (!p_export_output_dir.is_absolute_path()) {
		return ERR_INVALID_PARAMETER;
	}
	HashMap<String, Vector<String>> index;
	if (!AssetTagStorage::load_index(index)) {
		return ERR_CANT_OPEN;
	}
	Ref<DirAccess> dir = _open_export_dir(p_export_output_dir);
	if (dir.is_null()) {
		return ERR_CANT_CREATE;
	}
	const Error export_dir_err = dir->make_dir_recursive(p_export_output_dir);
	if (export_dir_err != OK && export_dir_err != ERR_ALREADY_EXISTS) {
		return export_dir_err;
	}
	const String bake_dir = p_export_output_dir.path_join(".blazium").path_join("asset_tags");
	const String temp_dir = bake_dir + ".tmp";
	if (DirAccess::dir_exists_absolute(temp_dir)) {
		_remove_export_dir_if_exists(temp_dir);
	}
	const Error temp_err = dir->make_dir_recursive(temp_dir);
	if (temp_err != OK && temp_err != ERR_ALREADY_EXISTS) {
		return temp_err;
	}
	for (const KeyValue<String, Vector<String>> &kv : index) {
		Dictionary payload;
		payload["path"] = kv.key;
		Array tags;
		for (int i = 0; i < kv.value.size(); i++) {
			tags.push_back(kv.value[i]);
		}
		payload["tags"] = tags;
		const String sidecar_path = temp_dir.path_join(AssetTagSidecarIO::encode_sidecar_name(kv.key) + ".json");
		Ref<FileAccess> file = FileAccess::open(sidecar_path, FileAccess::WRITE);
		if (file.is_null()) {
			_remove_export_dir_if_exists(temp_dir);
			return ERR_CANT_CREATE;
		}
		file->store_string(JSON::stringify(payload, "\t"));
	}
	if (DirAccess::dir_exists_absolute(bake_dir)) {
		Ref<DirAccess> bake_list = DirAccess::open(bake_dir);
		if (bake_list.is_valid()) {
			bake_list->list_dir_begin();
			String existing = bake_list->get_next();
			while (!existing.is_empty()) {
				if (!bake_list->current_is_dir()) {
					bake_list->remove(existing);
				}
				existing = bake_list->get_next();
			}
			bake_list->list_dir_end();
		}
	}
	if (DirAccess::dir_exists_absolute(bake_dir)) {
		Ref<DirAccess> bake_parent = _open_export_dir(bake_dir.get_base_dir());
		if (bake_parent.is_null()) {
			return ERR_CANT_CREATE;
		}
		const Error remove_err = bake_parent->remove(bake_dir);
		if (remove_err != OK) {
			Ref<DirAccess> bake_list = DirAccess::open(bake_dir);
			if (bake_list.is_valid()) {
				bake_list->list_dir_begin();
				String existing = bake_list->get_next();
				while (!existing.is_empty()) {
					if (!bake_list->current_is_dir()) {
						bake_list->remove(existing);
					}
					existing = bake_list->get_next();
				}
				bake_list->list_dir_end();
			}
			bake_parent->remove(bake_dir);
		}
	}
	if (!AssetTagFileIO::copy_dir_recursive(temp_dir, bake_dir)) {
		_remove_export_dir_if_exists(temp_dir);
		return ERR_CANT_CREATE;
	}
	_remove_export_dir_if_exists(temp_dir);
	return OK;
}
