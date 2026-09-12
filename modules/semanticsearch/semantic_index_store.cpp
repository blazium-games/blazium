/**************************************************************************/
/*  semantic_index_store.cpp                                              */
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

#include "semantic_index_store.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "hash_vector_embedding.h"
#include "modules/assettags/persistence/incremental_index_sidecar_io.h"

static Ref<DirAccess> _semantic_open_dir_for_path(const String &p_path) {
	const String base_dir = p_path.get_base_dir();
	Ref<DirAccess> dir = DirAccess::open(base_dir);
	if (dir.is_valid()) {
		return dir;
	}
	if (base_dir.begins_with("user://")) {
		return DirAccess::create(DirAccess::ACCESS_USERDATA);
	}
	if (base_dir.begins_with("res://")) {
		return DirAccess::create(DirAccess::ACCESS_RESOURCES);
	}
	DirAccess::make_dir_recursive_absolute(base_dir);
	return DirAccess::create_for_path(base_dir);
}

static bool _semantic_remove_file_if_exists(const String &p_path) {
	if (!FileAccess::exists(p_path)) {
		return true;
	}
	const String parent = p_path.get_base_dir();
	const String file_name = p_path.get_file();
	Ref<DirAccess> dir = DirAccess::open(parent);
	if (dir.is_valid() && dir->remove(file_name) == OK) {
		return true;
	}
	if (ProjectSettings::get_singleton()) {
		Ref<DirAccess> fs = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		if (fs.is_valid()) {
			return fs->remove(ProjectSettings::get_singleton()->globalize_path(p_path)) == OK;
		}
	}
	return false;
}

static bool _semantic_atomic_write_text_file(const String &p_path, const String &p_text) {
	const String temp_path = p_path + ".tmp";
	Ref<FileAccess> file = FileAccess::open(temp_path, FileAccess::WRITE);
	if (file.is_null()) {
		return false;
	}
	file->store_string(p_text);
	file.unref();
	Ref<DirAccess> dir = _semantic_open_dir_for_path(p_path);
	if (dir.is_valid() && dir->rename_absolute(temp_path, p_path) == OK) {
		return true;
	}
	if (DirAccess::copy_absolute(temp_path, p_path) != OK) {
		_semantic_remove_file_if_exists(temp_path);
		return false;
	}
	_semantic_remove_file_if_exists(temp_path);
	return true;
}

static bool _semantic_ensure_index_dir(const String &p_dir_path) {
	Ref<DirAccess> dir = DirAccess::open(p_dir_path);
	if (dir.is_valid()) {
		return true;
	}
	dir = _semantic_open_dir_for_path(p_dir_path.path_join("."));
	if (dir.is_null()) {
		return false;
	}
	const Error err = dir->make_dir_recursive(p_dir_path);
	return err == OK || err == ERR_ALREADY_EXISTS;
}

static const int SEMANTIC_INDEX_VERSION = 2;

String SemanticIndexStore::test_index_override;

#ifdef TESTS_ENABLED
static String _normalize_semantic_test_index_dir(const String &p_dir) {
	String suffix = p_dir;
	if (p_dir.begins_with("res://.blazium/")) {
		suffix = p_dir.trim_prefix("res://.blazium/");
	} else if (p_dir.begins_with("user://") || p_dir.begins_with("res://")) {
		return p_dir;
	}
	return OS::get_singleton()->get_cache_path().path_join("blazium_semanticsearch_test").path_join(suffix);
}

static void _reset_semantic_test_index_dir(const String &p_dir) {
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

void SemanticIndexStore::set_test_index_dir(const String &p_dir) {
	const String normalized = _normalize_semantic_test_index_dir(p_dir);
	_reset_semantic_test_index_dir(normalized);
	test_index_override = normalized;
}

void SemanticIndexStore::clear_test_index_dir() {
	if (!test_index_override.is_empty()) {
		if (!test_index_override.begins_with("user://") && !test_index_override.begins_with("res://")) {
			if (DirAccess::dir_exists_absolute(test_index_override)) {
				Ref<DirAccess> existing = DirAccess::create_for_path(test_index_override);
				if (existing.is_valid() && existing->change_dir(test_index_override) == OK) {
					existing->erase_contents_recursive();
				}
				DirAccess::remove_absolute(test_index_override);
			}
		} else {
			Ref<DirAccess> dir = DirAccess::open(test_index_override.get_base_dir());
			if (dir.is_null() && test_index_override.begins_with("user://")) {
				dir = DirAccess::create(DirAccess::ACCESS_USERDATA);
			}
			if (dir.is_valid() && dir->dir_exists(test_index_override)) {
				Ref<DirAccess> existing = DirAccess::open(test_index_override);
				if (existing.is_valid()) {
					existing->erase_contents_recursive();
				}
				dir->remove(test_index_override);
			}
		}
	}
	test_index_override = String();
}
#endif

String SemanticIndexStore::get_index_dir_path() {
	if (!test_index_override.is_empty()) {
		return test_index_override;
	}
	return "res://.blazium/semantic_index";
}

String SemanticIndexStore::get_index_file_path() {
	return get_index_dir_path().path_join("index.json");
}

static Error _parse_index_entries(const Variant &p_data, Array &r_items, int &r_version) {
	r_version = 0;
	if (p_data.get_type() == Variant::DICTIONARY) {
		const Dictionary root = p_data;
		r_version = int(root.get("version", 0));
		if (r_version > SEMANTIC_INDEX_VERSION) {
			return ERR_UNAVAILABLE;
		}
		r_items = root.get("entries", Array());
		return OK;
	}
	if (p_data.get_type() == Variant::ARRAY) {
		r_version = 0;
		r_items = p_data;
		return OK;
	}
	return ERR_PARSE_ERROR;
}

static void _sanitize_entry_embedding(SemanticAssetEntry &r_entry) {
	if (!r_entry.embedding_vector.is_empty() && !HashVectorEmbedding::is_valid_embedding_dim(r_entry.embedding_vector.size())) {
		r_entry.embedding_vector.clear();
		r_entry.embedding_provider = String();
	}
}

static void _populate_entries_from_items(const Array &p_items, HashMap<String, SemanticAssetEntry> &r_entries) {
	for (int i = 0; i < p_items.size(); i++) {
		const Dictionary item = p_items[i];
		SemanticAssetEntry entry;
		entry.path = item.get("path", "");
		entry.caption = item.get("caption", "");
		entry.asset_class = item.get("asset_class", "");
		entry.path_segments = item.get("path_segments", "");
		const Array token_array = item.get("tokens", Array());
		for (int j = 0; j < token_array.size(); j++) {
			entry.tokens.push_back(String(token_array[j]));
		}
		const Array embedding_array = item.get("embedding_vector", Array());
		for (int j = 0; j < embedding_array.size(); j++) {
			entry.embedding_vector.push_back(double(embedding_array[j]));
		}
		entry.embedding_provider = item.get("embedding_provider", "");
		_sanitize_entry_embedding(entry);
		if (!entry.path.is_empty()) {
			r_entries.insert(entry.path, entry);
		}
	}
}

Error SemanticIndexStore::load() {
	const String index_path = get_index_file_path();
	entries.clear();
	if (!FileAccess::exists(index_path)) {
		return apply_dirty_sidecars(entries) ? OK : ERR_CANT_CREATE;
	}
	Ref<FileAccess> file = FileAccess::open(index_path, FileAccess::READ);
	if (file.is_null()) {
		return ERR_CANT_OPEN;
	}
	Ref<JSON> json;
	json.instantiate();
	if (json->parse(file->get_as_text()) != OK) {
		return ERR_PARSE_ERROR;
	}
	Array items;
	int loaded_version = 0;
	const Error parse_err = _parse_index_entries(json->get_data(), items, loaded_version);
	if (parse_err != OK) {
		return parse_err;
	}
	(void)loaded_version;
	_populate_entries_from_items(items, entries);
	return apply_dirty_sidecars(entries) ? OK : ERR_CANT_CREATE;
}

Error SemanticIndexStore::save() const {
	const String dir_path = get_index_dir_path();
	if (!_semantic_ensure_index_dir(dir_path)) {
		return ERR_CANT_CREATE;
	}
	Array items;
	for (const KeyValue<String, SemanticAssetEntry> &kv : entries) {
		Dictionary item;
		item["path"] = kv.value.path;
		item["caption"] = kv.value.caption;
		if (!kv.value.asset_class.is_empty()) {
			item["asset_class"] = kv.value.asset_class;
		}
		if (!kv.value.path_segments.is_empty()) {
			item["path_segments"] = kv.value.path_segments;
		}
		Array token_array;
		for (int i = 0; i < kv.value.tokens.size(); i++) {
			token_array.push_back(kv.value.tokens[i]);
		}
		item["tokens"] = token_array;
		if (!kv.value.embedding_vector.is_empty()) {
			Array embedding_array;
			for (int i = 0; i < kv.value.embedding_vector.size(); i++) {
				embedding_array.push_back(kv.value.embedding_vector[i]);
			}
			item["embedding_vector"] = embedding_array;
		}
		if (!kv.value.embedding_provider.is_empty()) {
			item["embedding_provider"] = kv.value.embedding_provider;
		}
		items.push_back(item);
	}
	Dictionary root;
	root["version"] = SEMANTIC_INDEX_VERSION;
	root["entries"] = items;
	const String index_path = dir_path.path_join("index.json");
	if (!_semantic_atomic_write_text_file(index_path, JSON::stringify(root, "\t"))) {
		return ERR_CANT_CREATE;
	}
	return clear_index_dirty_sidecars() ? OK : ERR_CANT_CREATE;
}

Error SemanticIndexStore::save_dirty(const HashSet<String> &p_dirty_paths, const HashMap<String, SemanticAssetEntry> &p_entries) {
	if (p_dirty_paths.is_empty()) {
		return OK;
	}
	if (!save_dirty_sidecars(p_dirty_paths, p_entries)) {
		return ERR_CANT_CREATE;
	}
	return OK;
}

String SemanticIndexStore::get_index_dirty_dir() {
	return get_index_dir_path().path_join("index_dirty");
}

bool SemanticIndexStore::clear_index_dirty_sidecars() {
	const String dirty_dir = get_index_dirty_dir();
	if (!DirAccess::dir_exists_absolute(dirty_dir)) {
		return true;
	}
	Ref<DirAccess> dir = DirAccess::open(dirty_dir);
	if (dir.is_null()) {
		return false;
	}
	dir->list_dir_begin();
	String file_name = dir->get_next();
	while (!file_name.is_empty()) {
		if (!dir->current_is_dir()) {
			_semantic_remove_file_if_exists(dirty_dir.path_join(file_name));
		}
		file_name = dir->get_next();
	}
	dir->list_dir_end();
	return true;
}

bool SemanticIndexStore::apply_dirty_sidecars(HashMap<String, SemanticAssetEntry> &r_entries) {
	const String dirty_dir = get_index_dirty_dir();
	const Vector<IncrementalIndexSidecarIO::SidecarApplyItem> sidecar_files = IncrementalIndexSidecarIO::collect_sorted_sidecar_files(dirty_dir);
	for (int i = 0; i < sidecar_files.size(); i++) {
		const String file_name = sidecar_files[i].file_name;
		Ref<FileAccess> file = FileAccess::open(dirty_dir.path_join(file_name), FileAccess::READ);
		if (file.is_valid()) {
			const Variant parsed = JSON::parse_string(file->get_as_text());
			if (parsed.get_type() == Variant::DICTIONARY) {
				const Dictionary item = parsed;
				const String path = item.get("path", "");
				if (!path.is_empty()) {
					if (bool(item.get("deleted", false))) {
						r_entries.erase(path);
					} else {
						SemanticAssetEntry entry;
						entry.path = path;
						entry.caption = item.get("caption", "");
						entry.asset_class = item.get("asset_class", "");
						entry.path_segments = item.get("path_segments", "");
						const Array token_array = item.get("tokens", Array());
						for (int j = 0; j < token_array.size(); j++) {
							entry.tokens.push_back(String(token_array[j]));
						}
						const Array embedding_array = item.get("embedding_vector", Array());
						for (int j = 0; j < embedding_array.size(); j++) {
							entry.embedding_vector.push_back(double(embedding_array[j]));
						}
						entry.embedding_provider = item.get("embedding_provider", "");
						_sanitize_entry_embedding(entry);
						r_entries[path] = entry;
					}
				}
			}
		}
	}
	return true;
}

static Dictionary _semantic_entry_to_sidecar_payload(const SemanticAssetEntry &p_entry, bool p_deleted) {
	Dictionary payload;
	payload["path"] = p_entry.path;
	if (p_deleted) {
		payload["deleted"] = true;
		return payload;
	}
	payload["caption"] = p_entry.caption;
	if (!p_entry.asset_class.is_empty()) {
		payload["asset_class"] = p_entry.asset_class;
	}
	if (!p_entry.path_segments.is_empty()) {
		payload["path_segments"] = p_entry.path_segments;
	}
	Array token_array;
	for (int i = 0; i < p_entry.tokens.size(); i++) {
		token_array.push_back(p_entry.tokens[i]);
	}
	payload["tokens"] = token_array;
	if (!p_entry.embedding_vector.is_empty()) {
		Array embedding_array;
		for (int i = 0; i < p_entry.embedding_vector.size(); i++) {
			embedding_array.push_back(p_entry.embedding_vector[i]);
		}
		payload["embedding_vector"] = embedding_array;
	}
	if (!p_entry.embedding_provider.is_empty()) {
		payload["embedding_provider"] = p_entry.embedding_provider;
	}
	return payload;
}

bool SemanticIndexStore::compact_index_sidecars(const HashMap<String, SemanticAssetEntry> &p_entries) {
	SemanticIndexStore merged;
	for (const KeyValue<String, SemanticAssetEntry> &kv : p_entries) {
		merged.set_entry(kv.key, kv.value);
	}
	if (!apply_dirty_sidecars(merged.entries)) {
		return false;
	}
	if (merged.save() != OK) {
		return false;
	}
	return clear_index_dirty_sidecars();
}

static String _semantic_resolve_unique_sidecar_file_name(const String &p_dirty_dir, const String &p_path) {
	return IncrementalIndexSidecarIO::resolve_unique_sidecar_file_name(p_dirty_dir, p_path);
}

bool SemanticIndexStore::save_dirty_sidecars(const HashSet<String> &p_dirty_paths, const HashMap<String, SemanticAssetEntry> &p_entries) {
	const String dir_path = get_index_dir_path();
	if (!_semantic_ensure_index_dir(dir_path)) {
		return false;
	}
	const String dirty_dir = get_index_dirty_dir();
	if (!_semantic_ensure_index_dir(dirty_dir)) {
		return false;
	}
	Ref<DirAccess> dir = DirAccess::open(dirty_dir);
	if (dir.is_null()) {
		return false;
	}
	Vector<String> written_sidecars;
	for (const String &path : p_dirty_paths) {
		const String sidecar_name = _semantic_resolve_unique_sidecar_file_name(dirty_dir, path);
		const String sidecar_path = dirty_dir.path_join(sidecar_name);
		Dictionary payload;
		if (p_entries.has(path)) {
			payload = _semantic_entry_to_sidecar_payload(p_entries[path], false);
		} else {
			SemanticAssetEntry tombstone;
			tombstone.path = path;
			payload = _semantic_entry_to_sidecar_payload(tombstone, true);
		}
		if (!_semantic_atomic_write_text_file(sidecar_path, JSON::stringify(payload, "\t"))) {
			for (int i = 0; i < written_sidecars.size(); i++) {
				dir->remove(written_sidecars[i]);
			}
			return false;
		}
		written_sidecars.push_back(sidecar_name);
	}
	int sidecar_count = 0;
	dir->list_dir_begin();
	String file_name = dir->get_next();
	while (!file_name.is_empty()) {
		if (!dir->current_is_dir() && file_name.ends_with(".json")) {
			sidecar_count++;
		}
		file_name = dir->get_next();
	}
	dir->list_dir_end();
	if (sidecar_count >= IncrementalIndexSidecarIO::COMPACT_THRESHOLD) {
		return compact_index_sidecars(p_entries);
	}
	return true;
}

void SemanticIndexStore::clear() {
	entries.clear();
}

bool SemanticIndexStore::has_entry(const String &p_path) const {
	return entries.has(p_path);
}

const SemanticAssetEntry *SemanticIndexStore::get_entry(const String &p_path) const {
	if (!entries.has(p_path)) {
		return nullptr;
	}
	return &entries[p_path];
}

void SemanticIndexStore::set_entry(const String &p_path, const SemanticAssetEntry &p_entry) {
	entries[p_path] = p_entry;
}

void SemanticIndexStore::remove_entry(const String &p_path) {
	entries.erase(p_path);
}

int SemanticIndexStore::get_index_version() {
	return SEMANTIC_INDEX_VERSION;
}
