/**************************************************************************/
/*  test_asset_tag_storage.cpp                                            */
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

#ifdef TESTS_ENABLED

#include "../asset_tag_manager.h"
#include "../asset_tag_registry.h"
#include "../asset_tag_runtime.h"
#include "../asset_tag_sidecar_io.h"
#include "../asset_tag_storage.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "tests/test_macros.h"

void test_asset_tag_storage_roundtrip() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_storage_roundtrip");
	HashMap<String, AssetTagEntry> tags;
	AssetTagEntry entry;
	entry.comment = "Roundtrip comment";
	entry.source = "test";
	tags.insert("Alpha.One", entry);
	Vector<AssetTagRedirect> redirects;
	AssetTagRedirect redirect;
	redirect.old_name = "Legacy";
	redirect.new_name = "Modern";
	redirects.push_back(redirect);
	CHECK(AssetTagStorage::save_dictionary(tags, redirects));
	HashMap<String, AssetTagEntry> loaded_tags;
	Vector<AssetTagRedirect> loaded_redirects;
	CHECK(AssetTagStorage::load_dictionary(loaded_tags, loaded_redirects));
	CHECK(loaded_tags.has("Alpha.One"));
	CHECK(loaded_tags["Alpha.One"].comment == "Roundtrip comment");
	TEST_FAIL_COND(loaded_redirects.is_empty(), "Expected one redirect after dictionary roundtrip.");
	CHECK(loaded_redirects.size() == 1);
	CHECK(loaded_redirects[0].old_name == "Legacy");

	HashMap<String, Vector<String>> index;
	Vector<String> asset_tags;
	asset_tags.push_back("Alpha.One");
	index.insert("res://alpha.tscn", asset_tags);
	CHECK(AssetTagStorage::save_index(index));
	HashMap<String, Vector<String>> loaded_index;
	CHECK(AssetTagStorage::load_index(loaded_index));
	CHECK(loaded_index.has("res://alpha.tscn"));
	CHECK(loaded_index["res://alpha.tscn"].size() == 1);
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_paired_commit_rollback() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_paired_commit_rollback");
	AssetTagManager manager;
	AssetTagRegistry registry;
	manager.add_tag("Rollback.Tag");
	PackedStringArray tags;
	tags.push_back("Rollback.Tag");
	CHECK(registry.set_tags_for_asset("res://rollback.tscn", tags) == OK);
	CHECK(registry.commit_batch() == OK);
	CHECK(FileAccess::exists(AssetTagStorage::get_tags_file_path()));
	CHECK(registry.get_indexed_paths().has("res://rollback.tscn"));

	registry.begin_batch();
	CHECK(manager.add_tag("Should.Not.Persist") == OK);
	tags.clear();
	tags.push_back("Should.Not.Persist");
	CHECK(registry.set_tags_for_asset("res://fail.tscn", tags) == OK);
	AssetTagStorage::set_test_fail_index_commit(true);
	CHECK(registry.commit_batch() == ERR_CANT_CREATE);
	CHECK(!manager.has_tag_in_dictionary("Should.Not.Persist"));
	CHECK(!registry.get_indexed_paths().has("res://fail.tscn"));
	AssetTagStorage::set_test_fail_index_commit(false);
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_partial_undo_snapshot() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_partial_undo");
	const String undo_dir = AssetTagStorage::get_storage_dir().path_join("undo");
	Ref<DirAccess> dir = DirAccess::open(AssetTagStorage::get_storage_dir().get_base_dir());
	if (dir.is_null()) {
		dir = DirAccess::create(DirAccess::ACCESS_USERDATA);
	}
	if (dir.is_valid() && !dir->dir_exists(undo_dir)) {
		dir->make_dir_recursive(undo_dir);
	}
	Ref<FileAccess> tags_only = FileAccess::open(undo_dir.path_join("tags.json"), FileAccess::WRITE);
	TEST_FAIL_COND(tags_only.is_null(), "Failed to create partial undo snapshot fixture.");
	tags_only->store_string("{\"tags\":{}}");
	tags_only.unref();
	CHECK(!AssetTagStorage::has_undo_state());
	CHECK(AssetTagStorage::restore_undo_state() != OK);
	if (dir.is_valid()) {
		if (dir->file_exists(undo_dir.path_join("tags.json"))) {
			dir->remove(undo_dir.path_join("tags.json"));
		}
	}
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_index_sidecar_roundtrip() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_index_sidecar");
	HashMap<String, Vector<String>> base;
	Vector<String> alpha_tags;
	alpha_tags.push_back("Alpha");
	base.insert("res://alpha.tscn", alpha_tags);
	CHECK(AssetTagStorage::save_index(base));

	HashMap<String, Vector<String>> memory(base);
	Vector<String> beta_tags;
	beta_tags.push_back("Beta");
	memory.insert("res://beta.tscn", beta_tags);
	HashSet<String> dirty;
	dirty.insert("res://beta.tscn");
	CHECK(AssetTagStorage::save_index_dirty_sidecars(memory, dirty));

	HashMap<String, Vector<String>> loaded;
	CHECK(AssetTagStorage::load_index(loaded));
	CHECK(loaded.has("res://alpha.tscn"));
	CHECK(loaded.has("res://beta.tscn"));
	CHECK(loaded["res://beta.tscn"][0] == "Beta");
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_sidecar_stale_after_full_write() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_sidecar_stale_full");
	HashMap<String, Vector<String>> base;
	Vector<String> alpha_tags;
	alpha_tags.push_back("Alpha");
	base.insert("res://alpha.tscn", alpha_tags);
	CHECK(AssetTagStorage::save_index(base));

	HashSet<String> dirty;
	dirty.insert("res://alpha.tscn");
	CHECK(AssetTagStorage::save_index_dirty_sidecars(base, dirty));

	HashMap<String, Vector<String>> committed;
	Vector<String> beta_tags;
	beta_tags.push_back("Beta");
	committed.insert("res://beta.tscn", beta_tags);
	CHECK(AssetTagStorage::commit_dictionary_and_index(HashMap<String, AssetTagEntry>(), Vector<AssetTagRedirect>(), committed, false, true));

	HashMap<String, Vector<String>> loaded;
	CHECK(AssetTagStorage::load_index(loaded));
	CHECK(!loaded.has("res://alpha.tscn"));
	CHECK(loaded.has("res://beta.tscn"));
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_undo_sidecar_parity() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_undo_sidecar");
	HashMap<String, Vector<String>> base;
	Vector<String> alpha_tags;
	alpha_tags.push_back("Alpha");
	base.insert("res://alpha.tscn", alpha_tags);
	CHECK(AssetTagStorage::save_index(base));

	HashMap<String, Vector<String>> memory(base);
	Vector<String> beta_tags;
	beta_tags.push_back("Beta");
	memory.insert("res://beta.tscn", beta_tags);
	HashSet<String> dirty;
	dirty.insert("res://beta.tscn");
	CHECK(AssetTagStorage::save_index_dirty_sidecars(memory, dirty));
	CHECK(AssetTagStorage::snapshot_undo_state() == OK);

	Vector<String> gamma_tags;
	gamma_tags.push_back("Gamma");
	memory.insert("res://gamma.tscn", gamma_tags);
	dirty.clear();
	dirty.insert("res://gamma.tscn");
	CHECK(AssetTagStorage::save_index_dirty_sidecars(memory, dirty));
	CHECK(AssetTagStorage::restore_undo_state() == OK);

	HashMap<String, Vector<String>> loaded;
	CHECK(AssetTagStorage::load_index(loaded));
	CHECK(loaded.has("res://alpha.tscn"));
	CHECK(loaded.has("res://beta.tscn"));
	CHECK(!loaded.has("res://gamma.tscn"));
	AssetTagStorage::clear_undo_state();
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_sidecar_path_encoding() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_sidecar_encoding");
	const String path_a = "res://a/b";
	const String path_b = "res://a__b";
	HashMap<String, Vector<String>> memory;
	Vector<String> tags_a;
	tags_a.push_back("A");
	Vector<String> tags_b;
	tags_b.push_back("B");
	memory.insert(path_a, tags_a);
	memory.insert(path_b, tags_b);
	HashSet<String> dirty;
	dirty.insert(path_a);
	dirty.insert(path_b);
	CHECK(AssetTagStorage::save_index_dirty_sidecars(memory, dirty));

	const String dirty_dir = AssetTagStorage::get_index_dirty_dir();
	Ref<DirAccess> dir = DirAccess::open(dirty_dir);
	CHECK(dir.is_valid());
	int file_count = 0;
	dir->list_dir_begin();
	String file_name = dir->get_next();
	while (!file_name.is_empty()) {
		if (!dir->current_is_dir() && file_name.ends_with(".json")) {
			file_count++;
		}
		file_name = dir->get_next();
	}
	dir->list_dir_end();
	CHECK(file_count == 2);

	HashMap<String, Vector<String>> loaded;
	CHECK(AssetTagStorage::load_index(loaded));
	CHECK(loaded.has(path_a));
	CHECK(loaded.has(path_b));
	CHECK(loaded[path_a][0] == "A");
	CHECK(loaded[path_b][0] == "B");
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_runtime_sidecar_cache() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_runtime_sidecar_cache");
	HashMap<String, Vector<String>> base;
	Vector<String> alpha_tags;
	alpha_tags.push_back("Alpha");
	base.insert("res://alpha.tscn", alpha_tags);
	CHECK(AssetTagStorage::save_index(base));

	PackedStringArray initial = AssetTagRuntime::read_tags_for_asset("res://alpha.tscn");
	CHECK(initial.size() == 1);
	CHECK(initial[0] == "Alpha");

	HashMap<String, Vector<String>> memory(base);
	Vector<String> beta_tags;
	beta_tags.push_back("Beta");
	memory.insert("res://beta.tscn", beta_tags);
	HashSet<String> dirty;
	dirty.insert("res://beta.tscn");
	CHECK(AssetTagStorage::save_index_dirty_sidecars(memory, dirty));

	PackedStringArray beta_read = AssetTagRuntime::read_tags_for_asset("res://beta.tscn");
	CHECK(beta_read.size() == 1);
	CHECK(beta_read[0] == "Beta");
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_runtime_export_bake_read() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_runtime_export_bake");
	CHECK(AssetTagStorage::ensure_storage_dir());
	HashMap<String, Vector<String>> index;
	Vector<String> export_tags;
	export_tags.push_back("Export.Tag");
	index.insert("res://export_me.tscn", export_tags);
	CHECK(AssetTagStorage::save_index(index));

	const String export_dir = AssetTagStorage::get_storage_dir().path_join("export_out");
	CHECK(AssetTagStorage::ensure_storage_dir());
	CHECK(AssetTagRuntime::bake_tags_for_export(export_dir) == OK);

	PackedStringArray baked = AssetTagRuntime::read_tags_for_export_bake(export_dir, "res://export_me.tscn");
	CHECK(baked.size() == 1);
	TEST_FAIL_COND(baked.is_empty(), "Export bake should contain one tag entry.");
	CHECK(baked[0] == "Export.Tag");

	PackedStringArray runtime = AssetTagRuntime::read_tags_for_asset("res://export_me.tscn");
	CHECK(runtime.size() == 1);
	TEST_FAIL_COND(runtime.is_empty(), "Runtime index should contain export asset tags.");
	CHECK(runtime[0] == "Export.Tag");
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_runtime_notify_sidecar_dirty() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_runtime_notify_sidecar");
	HashMap<String, Vector<String>> base;
	Vector<String> alpha_tags;
	alpha_tags.push_back("Alpha");
	base.insert("res://alpha.tscn", alpha_tags);
	CHECK(AssetTagStorage::save_index(base));

	PackedStringArray initial = AssetTagRuntime::read_tags_for_asset("res://alpha.tscn");
	CHECK(initial.size() == 1);

	HashMap<String, Vector<String>> memory(base);
	Vector<String> gamma_tags;
	gamma_tags.push_back("Gamma");
	memory.insert("res://gamma.tscn", gamma_tags);
	HashSet<String> dirty;
	dirty.insert("res://gamma.tscn");
	AssetTagRuntime::notify_sidecar_dirty();
	CHECK(AssetTagStorage::save_index_dirty_sidecars(memory, dirty));

	PackedStringArray gamma_read = AssetTagRuntime::read_tags_for_asset("res://gamma.tscn");
	CHECK(gamma_read.size() == 1);
	CHECK(gamma_read[0] == "Gamma");
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_sidecar_compact_at_64() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_sidecar_compact_64");
	CHECK(AssetTagStorage::ensure_storage_dir());
	HashMap<String, Vector<String>> base;
	Vector<String> seed_tags;
	seed_tags.push_back("Seed");
	base.insert("res://seed.tscn", seed_tags);
	CHECK(AssetTagStorage::save_index(base));

	HashMap<String, Vector<String>> memory(base);
	for (int i = 0; i < 64; i++) {
		const String path = vformat("res://asset_%03d.tscn", i);
		Vector<String> tags;
		tags.push_back("Bulk");
		memory.insert(path, tags);
		HashSet<String> dirty;
		dirty.insert(path);
		CHECK(AssetTagStorage::save_index_dirty_sidecars(memory, dirty));
	}
	Ref<DirAccess> dirty_dir = DirAccess::open(AssetTagStorage::get_index_dirty_dir());
	int remaining = 0;
	if (dirty_dir.is_valid()) {
		dirty_dir->list_dir_begin();
		String file_name = dirty_dir->get_next();
		while (!file_name.is_empty()) {
			if (!dirty_dir->current_is_dir() && file_name.ends_with(".json")) {
				remaining++;
			}
			file_name = dirty_dir->get_next();
		}
		dirty_dir->list_dir_end();
	}
	CHECK(remaining == 0);
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_runtime_export_failure_cleanup() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_export_failure_cleanup");
	AssetTagManager manager;
	AssetTagRegistry registry;
	PackedStringArray tags;
	tags.push_back("Export.Fail");
	CHECK(registry.set_tags_for_asset("res://fail.tscn", tags) == OK);
	CHECK(registry.commit_batch() == OK);
	const String export_root = AssetTagStorage::get_storage_dir().path_join("export_out");
	CHECK(AssetTagStorage::ensure_storage_dir());
	CHECK(AssetTagRuntime::bake_tags_for_export(export_root) == OK);
	CHECK(DirAccess::dir_exists_absolute(export_root.path_join(".blazium").path_join("asset_tags")));
	CHECK(!DirAccess::dir_exists_absolute(export_root.path_join(".blazium").path_join("asset_tags.tmp")));
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_sidecar_disambiguated_tracking() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_sidecar_disambiguated");
	CHECK(AssetTagStorage::ensure_storage_dir());
	const String dirty_dir = AssetTagStorage::get_index_dirty_dir();
	const String path_b = "res://beta.tscn";
	const String base_name = AssetTagSidecarIO::encode_sidecar_name(path_b);
	const String base_file = base_name + ".json";
	Dictionary blocker;
	blocker["path"] = "res://blocker.tscn";
	blocker["tags"] = Array();
	Ref<FileAccess> blocker_file = FileAccess::open(dirty_dir.path_join(base_file), FileAccess::WRITE);
	CHECK(blocker_file.is_valid());
	blocker_file->store_string(JSON::stringify(blocker));
	blocker_file.unref();

	const String resolved_b = AssetTagSidecarIO::resolve_unique_sidecar_file_name(dirty_dir, path_b);
	CHECK(resolved_b != base_file);

	HashMap<String, Vector<String>> memory;
	Vector<String> tags_b;
	tags_b.push_back("Beta");
	memory.insert(path_b, tags_b);
	HashSet<String> dirty_b;
	dirty_b.insert(path_b);
	CHECK(AssetTagStorage::save_index_dirty_sidecars(memory, dirty_b));
	CHECK(FileAccess::exists(dirty_dir.path_join(resolved_b)));

	HashMap<String, Vector<String>> base;
	Vector<String> seed_tags;
	seed_tags.push_back("Seed");
	base.insert("res://seed.tscn", seed_tags);
	CHECK(AssetTagStorage::save_index(base));
	AssetTagStorage::set_test_fail_index_commit(true);
	for (int i = 0; i < 64; i++) {
		const String path = vformat("res://bulk_%03d.tscn", i);
		Vector<String> bulk_tags;
		bulk_tags.push_back("Bulk");
		memory.insert(path, bulk_tags);
		HashSet<String> dirty;
		dirty.insert(path);
		if (i == 63) {
			CHECK(!AssetTagStorage::save_index_dirty_sidecars(memory, dirty));
		} else {
			CHECK(AssetTagStorage::save_index_dirty_sidecars(memory, dirty));
		}
	}
	AssetTagStorage::set_test_fail_index_commit(false);
	int remaining = 0;
	Ref<DirAccess> dirty_list = DirAccess::open(dirty_dir);
	if (dirty_list.is_valid()) {
		dirty_list->list_dir_begin();
		String file_name = dirty_list->get_next();
		while (!file_name.is_empty()) {
			if (!dirty_list->current_is_dir() && file_name.ends_with(".json")) {
				remaining++;
			}
			file_name = dirty_list->get_next();
		}
		dirty_list->list_dir_end();
	}
	CHECK(remaining > 0);
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_normalize_asset_path() {
	CHECK(AssetTagStorage::normalize_asset_path("res://folder/../asset.tscn") == "res://asset.tscn");
	CHECK(AssetTagStorage::normalize_asset_path("res://folder\\sub\\file.tscn") == "res://folder/sub/file.tscn");
	CHECK(AssetTagStorage::normalize_asset_path("  res://a.tscn  ") == "res://a.tscn");
	CHECK(AssetTagStorage::normalize_asset_path("res://./props/crate.tscn") == "res://props/crate.tscn");
}

#endif
