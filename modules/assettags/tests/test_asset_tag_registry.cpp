/**************************************************************************/
/*  test_asset_tag_registry.cpp                                           */
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

#include "core/object/callable_mp.h"
#include "../asset_tag_coordinator.h"
#include "../asset_tag_hierarchy.h"
#include "../asset_tag_manager.h"
#include "../asset_tag_registry.h"
#include "../asset_tag_storage.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "tests/test_macros.h"

void test_asset_tag_registry_apply() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_registry");
	AssetTagManager manager;
	AssetTagRegistry registry;
	PackedStringArray tags;
	tags.push_back("Environment.Nature.Tree");
	tags.push_back("Character.Hero");
	CHECK(registry.set_tags_for_asset("res://tree.tscn", tags) == OK);

	CHECK(registry.apply_tag_rename("Environment", "Env") == 1);
	PackedStringArray updated = registry.get_tags_for_asset("res://tree.tscn");
	CHECK(updated.has("Env.Nature.Tree"));
	CHECK(updated.has("Character.Hero"));
	CHECK(!updated.has("Environment.Nature.Tree"));

	tags.clear();
	tags.push_back("Environment.Nature.Tree");
	tags.push_back("Environment.Nature.Tree");
	CHECK(registry.set_tags_for_asset("res://dup.tscn", tags) == OK);
	CHECK(registry.apply_tag_rename("Environment", "Env") >= 0);
	updated = registry.get_tags_for_asset("res://dup.tscn");
	CHECK(updated.size() == 1);

	CHECK(registry.apply_tag_remove("Character") == 1);
	updated = registry.get_tags_for_asset("res://tree.tscn");
	CHECK(updated.size() == 1);
	CHECK(updated.has("Env.Nature.Tree"));
	CHECK(!registry.find_assets_by_tag("Character.Hero", false).has("res://tree.tscn"));
	CHECK(!registry.find_assets_by_tag("Character", true).has("res://tree.tscn"));
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_registry_search_assets() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_search_assets");
	AssetTagManager manager;
	AssetTagRegistry registry;
	PackedStringArray hero_tags;
	hero_tags.push_back("Character.Hero");
	CHECK(registry.set_tags_for_asset("res://hero.tscn", hero_tags) == OK);
	PackedStringArray env_tags;
	env_tags.push_back("Environment.Nature");
	CHECK(registry.set_tags_for_asset("res://tree.tscn", env_tags) == OK);
	PackedStringArray both_tags;
	both_tags.push_back("Character.Hero");
	both_tags.push_back("Quest.Main");
	CHECK(registry.set_tags_for_asset("res://quest.tscn", both_tags) == OK);

	PackedStringArray query;
	query.push_back("Character.Hero");
	Dictionary single = registry.search_assets(query, "", "", "", false);
	CHECK(single.get("ok", true));
	CHECK(int(single.get("count", 0)) == 2);

	PackedStringArray multi;
	multi.push_back("Character.Hero");
	multi.push_back("Quest.Main");
	Dictionary intersection = registry.search_assets(multi, "", "", "", true);
	CHECK(intersection.get("ok", true));
	CHECK(int(intersection.get("count", 0)) == 1);

	PackedStringArray union_query;
	union_query.push_back("Character.Hero");
	union_query.push_back("Environment.Nature");
	Dictionary union_result = registry.search_assets(union_query, "", "", "", false);
	CHECK(union_result.get("ok", true));
	CHECK(int(union_result.get("count", 0)) == 3);
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_registry_find_assets_redirect() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_find_assets_redirect");
	AssetTagManager manager;
	AssetTagRegistry registry;
	AssetTagCoordinator coordinator;
	manager.add_tag("Legacy.Root");
	PackedStringArray tags;
	tags.push_back("Legacy.Root");
	CHECK(registry.set_tags_for_asset("res://legacy.tscn", tags) == OK);
	Dictionary result = coordinator.rename_tag_result("Legacy", "Modern");
	CHECK(result.get("ok", false));
	PackedStringArray via_alias = registry.find_assets_by_tag("Legacy", false);
	CHECK(via_alias.has("res://legacy.tscn"));
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_registry_rename_path() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_rename_path");
	AssetTagManager manager;
	AssetTagRegistry registry;
	PackedStringArray tags;
	tags.push_back("Props.Crate");
	CHECK(registry.set_tags_for_asset("res://old.tscn", tags) == OK);
	CHECK(registry.rename_asset_path("res://old.tscn", "res://new.tscn"));
	CHECK(!registry.get_indexed_paths().has("res://old.tscn"));
	CHECK(registry.get_indexed_paths().has("res://new.tscn"));
	CHECK(registry.get_tags_for_asset("res://new.tscn").has("Props.Crate"));
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_registry_set_tags_noop() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_set_tags_noop");
	AssetTagManager manager;
	AssetTagRegistry registry;
	PackedStringArray tags;
	tags.push_back("Props.Crate");
	CHECK(registry.set_tags_for_asset("res://crate.tscn", tags) == OK);
	CHECK(registry.save() == OK);
	CHECK(registry.set_tags_for_asset("res://crate.tscn", tags) == OK);
	CHECK(registry.get_tags_for_asset("res://crate.tscn").has("Props.Crate"));
	registry.schedule_prune_removed_paths();
	registry.schedule_prune_removed_paths();
	CHECK(registry.get_indexed_paths().has("res://crate.tscn"));
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_registry_rescan_guard() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_rescan_guard");
	AssetTagManager manager;
	AssetTagRegistry registry;
	registry.begin_batch();
	CHECK(registry.rescan() == ERR_BUSY);
	registry.abort_batch();
	CHECK(registry.rescan() == OK);
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_strict_tags() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_strict_tags");
	ProjectSettings::get_singleton()->set_setting("blazium/assettags/strict_tags", true);
	AssetTagManager manager;
	AssetTagRegistry registry;
	PackedStringArray tags;
	tags.push_back("Unknown.Tag");
	CHECK(registry.set_tags_for_asset("res://strict.tscn", tags) != OK);
	manager.add_tag("Known.Tag");
	tags.clear();
	tags.push_back("Known.Tag");
	CHECK(registry.set_tags_for_asset("res://strict.tscn", tags) == OK);
	ProjectSettings::get_singleton()->set_setting("blazium/assettags/strict_tags", false);
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_rename_collision() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_rename_collision");
	AssetTagManager manager;
	AssetTagRegistry registry;
	PackedStringArray first;
	first.push_back("First.Tag");
	PackedStringArray second;
	second.push_back("Second.Tag");
	CHECK(registry.set_tags_for_asset("res://first.tscn", first) == OK);
	CHECK(registry.set_tags_for_asset("res://second.tscn", second) == OK);
	CHECK(registry.commit_batch() == OK);
	CHECK(!registry.rename_asset_path("res://first.tscn", "res://second.tscn"));
	CHECK(registry.get_tags_for_asset("res://second.tscn").has("Second.Tag"));
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_registry_index_dirty_rescan() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_index_dirty_rescan");
	AssetTagManager manager;
	AssetTagRegistry registry;
	registry.begin_batch();
	PackedStringArray tags;
	tags.push_back("Dirty.Tag");
	manager.add_tag("Dirty.Tag");
	CHECK(registry.set_tags_for_asset("res://dirty.tscn", tags) == OK);
	CHECK(registry.is_index_dirty());
	CHECK(registry.rescan() == ERR_BUSY);
	registry.abort_batch();
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_registry() {
	CHECK(AssetTagHierarchy::remap_tag_for_rename("Environment.Nature.Tree", "Environment", "Env") == "Env.Nature.Tree");
	CHECK(AssetTagHierarchy::remap_tag_for_rename("Environment", "Environment", "Env") == "Env");
	CHECK(AssetTagHierarchy::remap_tag_for_rename("Character.Hero", "Environment", "Env") == "Character.Hero");

	CHECK(AssetTagHierarchy::tag_matches_prefix("Environment.Nature.Tree", "Environment"));
	CHECK(AssetTagHierarchy::tag_matches_prefix("Environment", "Environment"));
	CHECK(!AssetTagHierarchy::tag_matches_prefix("Character.Hero", "Environment"));
}

void test_asset_tag_nonbatch_auto_add_atomic() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_nonbatch_auto_add");
	AssetTagManager manager;
	AssetTagRegistry registry;
	PackedStringArray tags;
	tags.push_back("Auto.Added.Tag");
	AssetTagStorage::set_test_fail_index_commit(true);
	CHECK(registry.set_tags_for_asset("res://auto.tscn", tags) != OK);
	CHECK(!manager.has_tag_in_dictionary("Auto.Added.Tag"));
	CHECK(!registry.get_indexed_paths().has("res://auto.tscn"));
	AssetTagStorage::set_test_fail_index_commit(false);
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_registry_load_recovery() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_registry_load_recovery");
	CHECK(AssetTagStorage::ensure_storage_dir());
	Ref<FileAccess> corrupt = FileAccess::open(AssetTagStorage::get_index_file_path(), FileAccess::WRITE);
	if (corrupt.is_valid()) {
		corrupt->store_string("{not-json");
		corrupt.unref();
	}
	AssetTagRegistry registry;
	CHECK(registry.load() == ERR_FILE_CORRUPT);
	CHECK(registry.is_index_load_failed());
	CHECK(registry.recover_after_load_failure() == OK);
	CHECK(!registry.is_index_load_failed());
	CHECK(!FileAccess::exists(AssetTagStorage::get_index_file_path()));
	CHECK(FileAccess::exists(AssetTagStorage::get_index_file_path() + ".corrupt"));
	CHECK(registry.load() == OK);
	CHECK(!registry.is_index_load_failed());
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_registry_recovery_quarantine_sidecars() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_recovery_quarantine_sidecars");
	Ref<DirAccess> dir = DirAccess::open("res://");
	if (dir.is_valid()) {
		dir->make_dir_recursive(".blazium/test_recovery_quarantine_sidecars/index_dirty");
	}
	HashMap<String, Vector<String>> index;
	Vector<String> stale_tags;
	stale_tags.push_back("Stale.Tag");
	index.insert("res://stale.tscn", stale_tags);
	HashSet<String> dirty;
	dirty.insert("res://stale.tscn");
	CHECK(AssetTagStorage::save_index_dirty_sidecars(index, dirty));
	Ref<FileAccess> corrupt = FileAccess::open(AssetTagStorage::get_index_file_path(), FileAccess::WRITE);
	if (corrupt.is_valid()) {
		corrupt->store_string("{not-json");
	}
	AssetTagRegistry registry;
	CHECK(registry.load() == ERR_FILE_CORRUPT);
	CHECK(registry.recover_after_load_failure() == OK);
	CHECK(!registry.get_indexed_paths().has("res://stale.tscn"));
	const String quarantine_path = AssetTagStorage::get_index_dirty_dir().path_join("quarantine");
	CHECK(DirAccess::dir_exists_absolute(quarantine_path));
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_alias_search_unmigrated_index() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_alias_search_unmigrated");
	Ref<DirAccess> dir = DirAccess::open("res://");
	if (dir.is_valid()) {
		dir->make_dir_recursive(".blazium/test_alias_search_unmigrated");
	}
	AssetTagManager manager;
	AssetTagRegistry registry;
	manager.add_tag("Legacy.Tag");
	manager.add_tag("Canonical.Tag");
	PackedStringArray tags;
	tags.push_back("Legacy.Tag");
	CHECK(registry.set_tags_for_asset("res://legacy.tscn", tags) == OK);
	CHECK(registry.commit_batch() == OK);
	HashMap<String, AssetTagEntry> dict_tags;
	Vector<AssetTagRedirect> redirects;
	manager.get_dictionary_snapshot(dict_tags, redirects);
	AssetTagRedirect redirect;
	redirect.old_name = "Legacy.Tag";
	redirect.new_name = "Canonical.Tag";
	redirects.push_back(redirect);
	CHECK(AssetTagStorage::save_dictionary(dict_tags, redirects));
	CHECK(manager.load() == OK);
	CHECK(registry.load() == OK);
	PackedStringArray found = registry.find_assets_by_tag("Canonical.Tag");
	CHECK(found.has("res://legacy.tscn"));
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_prune_removed_paths() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_prune_removed_paths");
	Ref<DirAccess> dir = DirAccess::open("res://");
	if (dir.is_valid()) {
		dir->make_dir_recursive(".blazium/test_prune_removed_paths");
	}
	AssetTagManager manager;
	AssetTagRegistry registry;
	manager.add_tag("Prune.Tag");
	PackedStringArray tags;
	tags.push_back("Prune.Tag");
	CHECK(registry.set_tags_for_asset("res://missing_asset.tscn", tags) == OK);
	CHECK(registry.commit_batch() == OK);
	CHECK(registry.get_indexed_paths().has("res://missing_asset.tscn"));
	registry.prune_removed_paths();
	CHECK(!registry.get_indexed_paths().has("res://missing_asset.tscn"));
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_registry_atomic_commit_batch() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_atomic_commit_batch");
	AssetTagManager manager;
	AssetTagRegistry registry;
	manager.add_tag("Persisted.Tag");
	PackedStringArray initial;
	initial.push_back("Persisted.Tag");
	CHECK(registry.set_tags_for_asset("res://persisted.tscn", initial) == OK);
	CHECK(registry.commit_batch() == OK);

	Ref<FileAccess> tags_before = FileAccess::open(AssetTagStorage::get_tags_file_path(), FileAccess::READ);
	CHECK(tags_before.is_valid());
	const String tags_text_before = tags_before->get_as_text();

	registry.begin_batch();
	CHECK(manager.add_tag("Atomic.Fail") == OK);
	PackedStringArray batch_tags;
	batch_tags.push_back("Atomic.Fail");
	CHECK(registry.set_tags_for_asset("res://atomic_fail.tscn", batch_tags) == OK);
	AssetTagStorage::set_test_fail_index_commit(true);
	CHECK(registry.commit_batch() == ERR_CANT_CREATE);
	CHECK(!manager.has_tag_in_dictionary("Atomic.Fail"));
	CHECK(!registry.get_indexed_paths().has("res://atomic_fail.tscn"));

	Ref<FileAccess> tags_after = FileAccess::open(AssetTagStorage::get_tags_file_path(), FileAccess::READ);
	CHECK(tags_after.is_valid());
	CHECK(tags_after->get_as_text() == tags_text_before);
	AssetTagStorage::set_test_fail_index_commit(false);
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_registry_index_write_blocked_commit() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_index_write_blocked");
	Ref<DirAccess> dir = DirAccess::open("res://");
	if (dir.is_valid()) {
		dir->make_dir_recursive(".blazium/test_isolated_index_write_blocked");
	}
	Ref<FileAccess> corrupt = FileAccess::open(AssetTagStorage::get_index_file_path(), FileAccess::WRITE);
	REQUIRE(corrupt.is_valid());
	corrupt->store_string("{not-json");
	corrupt.unref();
	AssetTagManager manager;
	AssetTagRegistry registry;
	CHECK(registry.load() == ERR_FILE_CORRUPT);
	CHECK(registry.is_index_load_failed());
	REQUIRE(AssetTagStorage::is_index_write_blocked());

	registry.begin_batch();
	PackedStringArray tags;
	tags.push_back("Blocked.Tag");
	manager.add_tag("Blocked.Tag");
	CHECK(registry.set_tags_for_asset("res://blocked.tscn", tags) == OK);
	CHECK(registry.commit_batch() == ERR_UNAVAILABLE);
	CHECK(!registry.get_indexed_paths().has("res://blocked.tscn"));
	CHECK(!registry.is_in_batch());
	CHECK(!manager.is_in_batch());

	HashMap<String, Vector<String>> blocked_index;
	blocked_index.insert("res://blocked.tscn", tags);
	CHECK(!AssetTagStorage::save_index_merge(blocked_index, HashSet<String>{ "res://blocked.tscn" }));
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_alias_incremental_update() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_alias_incremental");
	Ref<DirAccess> dir = DirAccess::open("res://");
	if (dir.is_valid()) {
		dir->make_dir_recursive(".blazium/test_alias_incremental");
	}
	AssetTagManager manager;
	AssetTagRegistry registry;
	CHECK(manager.add_tag("Legacy.Tag") == OK);
	CHECK(manager.add_tag("Canonical.Tag") == OK);
	HashMap<String, AssetTagEntry> dict_tags;
	Vector<AssetTagRedirect> redirects;
	manager.get_dictionary_snapshot(dict_tags, redirects);
	AssetTagRedirect redirect;
	redirect.old_name = "Legacy.Tag";
	redirect.new_name = "Canonical.Tag";
	redirects.push_back(redirect);
	CHECK(AssetTagStorage::save_dictionary(dict_tags, redirects));
	CHECK(manager.load() == OK);
	PackedStringArray tags;
	tags.push_back("Legacy.Tag");
	CHECK(registry.set_tags_for_asset("res://legacy.tscn", tags) == OK);
	PackedStringArray via_canonical = registry.find_assets_by_tag("Canonical.Tag", false);
	CHECK(via_canonical.has("res://legacy.tscn"));
	PackedStringArray via_legacy = registry.find_assets_by_tag("Legacy.Tag", false);
	CHECK(via_legacy.has("res://legacy.tscn"));
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_alias_redirect_signal_rebuild() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_alias_redirect_signal");
	Ref<DirAccess> dir = DirAccess::open("res://");
	if (dir.is_valid()) {
		dir->make_dir_recursive(".blazium/test_alias_redirect_signal");
	}
	AssetTagManager manager;
	AssetTagRegistry registry;
	manager.connect(SNAME("redirects_changed"), callable_mp(&registry, &AssetTagRegistry::_on_redirects_changed));
	CHECK(manager.add_tag("Legacy.Tag") == OK);
	PackedStringArray tags;
	tags.push_back("Legacy.Tag");
	CHECK(registry.set_tags_for_asset("res://legacy.tscn", tags) == OK);
	CHECK(manager.rename_tag("Legacy.Tag", "Canonical.Tag") == OK);
	PackedStringArray found = registry.find_assets_by_tag("Canonical.Tag", false);
	CHECK(found.has("res://legacy.tscn"));
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_commit_policy_redirects_changed() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_commit_policy_redirects");
	AssetTagManager manager;
	AssetTagRegistry registry;
	CHECK(manager.add_tag("Old.Tag") == OK);
	CHECK(manager.rename_tag("Old.Tag", "New.Tag") == OK);
	PackedStringArray tags;
	tags.push_back("New.Tag");
	CHECK(registry.set_tags_for_asset("res://redirect_asset.tscn", tags) == OK);
	CHECK(registry.commit_batch() == OK);
	CHECK(manager.resolve_tag_alias("Old.Tag") == "New.Tag");
	HashMap<String, AssetTagEntry> snapshot_tags;
	Vector<AssetTagRedirect> snapshot_redirects;
	manager.get_dictionary_snapshot(snapshot_tags, snapshot_redirects);
	CHECK(snapshot_redirects.size() >= 1);
	CHECK(snapshot_redirects[0].old_name == "Old.Tag");
	CHECK(snapshot_redirects[0].new_name == "New.Tag");
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_teardown_deferred_noop() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_teardown_deferred_noop");
	AssetTagManager manager;
	AssetTagRegistry registry;
	manager.add_tag("Teardown.Tag");
	PackedStringArray tags;
	tags.push_back("Teardown.Tag");
	CHECK(registry.set_tags_for_asset("res://teardown_missing.tscn", tags) == OK);
	CHECK(registry.commit_batch() == OK);
	CHECK(registry.get_indexed_paths().has("res://teardown_missing.tscn"));

	registry.schedule_prune_removed_paths();
	registry.prepare_for_teardown();
	registry.schedule_prune_removed_paths();
	registry.flush_deferred_work_for_tests();
	registry.prune_removed_paths();

	CHECK(registry.get_indexed_paths().has("res://teardown_missing.tscn"));
	CHECK(!registry.consume_queued_prune());
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_prune_queues_during_transaction() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_prune_during_transaction");
	Ref<DirAccess> dir = DirAccess::open("res://");
	if (dir.is_valid()) {
		dir->make_dir_recursive(".blazium/test_prune_during_transaction");
	}
	AssetTagManager manager;
	AssetTagRegistry registry;
	AssetTagCoordinator coordinator;
	manager.add_tag("Txn.Prune");
	PackedStringArray tags;
	tags.push_back("Txn.Prune");
	CHECK(registry.set_tags_for_asset("res://txn_prune_missing.tscn", tags) == OK);
	CHECK(registry.commit_batch() == OK);
	CHECK(registry.get_indexed_paths().has("res://txn_prune_missing.tscn"));

	CHECK(coordinator.begin_transaction() == OK);
	registry.schedule_prune_removed_paths();
	registry.flush_deferred_work_for_tests();
	CHECK(registry.get_indexed_paths().has("res://txn_prune_missing.tscn"));

	CHECK(coordinator.commit_transaction() == OK);
	registry.flush_deferred_work_for_tests();
	CHECK(!registry.get_indexed_paths().has("res://txn_prune_missing.tscn"));
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_prune_queues_during_batch() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_prune_during_batch");
	Ref<DirAccess> dir = DirAccess::open("res://");
	if (dir.is_valid()) {
		dir->make_dir_recursive(".blazium/test_prune_during_batch");
	}
	AssetTagManager manager;
	AssetTagRegistry registry;
	manager.add_tag("Batch.Prune");
	PackedStringArray tags;
	tags.push_back("Batch.Prune");
	CHECK(registry.set_tags_for_asset("res://batch_prune_missing.tscn", tags) == OK);
	CHECK(registry.commit_batch() == OK);

	registry.begin_batch();
	registry.schedule_prune_removed_paths();
	registry.flush_deferred_work_for_tests();
	CHECK(registry.get_indexed_paths().has("res://batch_prune_missing.tscn"));
	CHECK(registry.commit_batch() == OK);
	registry.flush_deferred_work_for_tests();
	CHECK(!registry.get_indexed_paths().has("res://batch_prune_missing.tscn"));
	AssetTagStorage::clear_test_storage_dir();
}

#endif
