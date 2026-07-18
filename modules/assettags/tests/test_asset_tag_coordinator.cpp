/**************************************************************************/
/*  test_asset_tag_coordinator.cpp                                        */
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

#include "../asset_tag_coordinator.h"
#include "../asset_tag_manager.h"
#include "../asset_tag_registry.h"
#include "../asset_tag_storage.h"
#include "core/io/file_access.h"
#include "tests/test_macros.h"

void test_asset_tag_coordinator() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_coordinator");
	AssetTagManager manager;
	AssetTagRegistry registry;
	AssetTagCoordinator coordinator;
	manager.add_tag("Alpha.One");
	PackedStringArray tags;
	tags.push_back("Alpha.One");
	CHECK(registry.set_tags_for_asset("res://alpha.tscn", tags) == OK);
	Dictionary result = coordinator.rename_tag_result("Alpha", "A");
	CHECK(result.get("ok", false));
	CHECK(int(result.get("affected_assets", 0)) == 1);
	CHECK(manager.has_tag_in_dictionary("A.One"));
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_coordinator_redirect() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_coordinator_redirect");
	AssetTagManager manager;
	AssetTagRegistry registry;
	AssetTagCoordinator coordinator;
	manager.add_tag("Legacy.Root");
	PackedStringArray tags;
	tags.push_back("Legacy.Root");
	CHECK(registry.set_tags_for_asset("res://legacy.tscn", tags) == OK);
	CHECK(manager.rename_tag("Legacy", "Modern") == OK);
	CHECK(manager.resolve_tag_alias("Legacy") == "Modern");
	Dictionary result = coordinator.remove_tag_result("Legacy");
	CHECK(result.get("ok", false));
	CHECK(int(result.get("affected_assets", 0)) == 1);
	PackedStringArray updated = registry.get_tags_for_asset("res://legacy.tscn");
	CHECK(updated.is_empty());
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_coordinator_transaction_batch() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_coordinator_transaction_batch");
	AssetTagManager manager;
	AssetTagRegistry registry;
	AssetTagCoordinator coordinator;
	manager.add_tag("Unused.Alpha");
	manager.add_tag("Unused.Beta");
	CHECK(manager.has_tag_in_dictionary("Unused.Alpha"));
	CHECK(manager.has_tag_in_dictionary("Unused.Beta"));

	coordinator.begin_transaction();
	CHECK(coordinator.remove_tag("Unused.Alpha") == OK);
	CHECK(coordinator.remove_tag("Unused.Beta") == OK);
	CHECK(coordinator.commit_transaction() == OK);

	CHECK(!manager.has_tag_in_dictionary("Unused.Alpha"));
	CHECK(!manager.has_tag_in_dictionary("Unused.Beta"));
	CHECK(!registry.is_in_batch());
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_coordinator_abort_batch() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_coordinator_abort_batch");
	AssetTagManager manager;
	AssetTagRegistry registry;
	AssetTagCoordinator coordinator;
	manager.add_tag("Rollback.Tag");
	PackedStringArray tags;
	tags.push_back("Rollback.Tag");
	CHECK(registry.set_tags_for_asset("res://rollback.tscn", tags) == OK);
	CHECK(registry.commit_batch() == OK);

	registry.begin_batch();
	CHECK(manager.rename_tag("Rollback", "Committed") == OK);
	CHECK(registry.apply_tag_rename("Rollback.Tag", "Committed.Tag") > 0);
	registry.abort_batch();

	CHECK(manager.has_tag_in_dictionary("Rollback.Tag"));
	CHECK(!manager.has_tag_in_dictionary("Committed.Tag"));
	CHECK(registry.get_tags_for_asset("res://rollback.tscn").has("Rollback.Tag"));
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_coordinator_undo() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_coordinator_undo");
	AssetTagManager manager;
	AssetTagRegistry registry;
	AssetTagCoordinator coordinator;
	manager.add_tag("Undo.Tag");
	PackedStringArray tags;
	tags.push_back("Undo.Tag");
	CHECK(registry.set_tags_for_asset("res://undo.tscn", tags) == OK);
	CHECK(registry.commit_batch() == OK);

	CHECK(coordinator.begin_transaction() == OK);
	CHECK(coordinator.remove_tag("Undo.Tag") == OK);
	CHECK(coordinator.commit_transaction() == OK);
	CHECK(!manager.has_tag_in_dictionary("Undo.Tag"));

	CHECK(coordinator.can_undo());
	CHECK(coordinator.undo_last_change() == OK);
	CHECK(manager.has_tag_in_dictionary("Undo.Tag"));
	CHECK(!coordinator.can_undo());
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_coordinator_abort_clears_undo() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_coordinator_abort_undo");
	AssetTagManager manager;
	AssetTagRegistry registry;
	AssetTagCoordinator coordinator;
	manager.add_tag("Abort.Tag");
	PackedStringArray tags;
	tags.push_back("Abort.Tag");
	CHECK(registry.set_tags_for_asset("res://abort.tscn", tags) == OK);
	CHECK(registry.commit_batch() == OK);

	CHECK(coordinator.begin_transaction() == OK);
	CHECK(coordinator.remove_tag("Abort.Tag") == OK);
	coordinator.abort_transaction();
	CHECK(!coordinator.can_undo());
	CHECK(manager.has_tag_in_dictionary("Abort.Tag"));
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_coordinator_begin_transaction_missing_index() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_coordinator_missing_index");
	AssetTagManager manager;
	AssetTagRegistry registry;
	AssetTagCoordinator coordinator;
	manager.add_tag("First.Tag");
	CHECK(!FileAccess::exists(AssetTagStorage::get_index_file_path()));
	CHECK(coordinator.begin_transaction() == OK);
	coordinator.abort_transaction();
	CHECK(!coordinator.can_undo());
	CHECK(coordinator.begin_transaction() == OK);
	PackedStringArray tags;
	tags.push_back("First.Tag");
	CHECK(registry.set_tags_for_asset("res://first_asset.tscn", tags) == OK);
	CHECK(coordinator.commit_transaction() == OK);
	CHECK(registry.get_tags_for_asset("res://first_asset.tscn").has("First.Tag"));
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_coordinator_standalone_add_undo() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_coordinator_standalone_add_undo");
	AssetTagManager manager;
	AssetTagRegistry registry;
	AssetTagCoordinator coordinator;
	CHECK(!coordinator.can_undo());
	CHECK(coordinator.add_tag("Standalone.Tag", "comment") == OK);
	CHECK(manager.has_tag_in_dictionary("Standalone.Tag"));
	CHECK(coordinator.can_undo());
	CHECK(coordinator.undo_last_change() == OK);
	CHECK(!manager.has_tag_in_dictionary("Standalone.Tag"));
	CHECK(!coordinator.can_undo());
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_coordinator_nested_abort() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_coordinator_nested_abort");
	AssetTagManager manager;
	AssetTagRegistry registry;
	AssetTagCoordinator coordinator;
	manager.add_tag("Nested.Tag");
	PackedStringArray tags;
	tags.push_back("Nested.Tag");
	CHECK(registry.set_tags_for_asset("res://nested.tscn", tags) == OK);
	CHECK(registry.commit_batch() == OK);

	CHECK(coordinator.begin_transaction() == OK);
	CHECK(coordinator.begin_transaction() == OK);
	CHECK(registry.is_in_batch());
	CHECK(coordinator.remove_tag("Nested.Tag") == OK);
	coordinator.abort_transaction();
	CHECK(!registry.is_in_batch());
	CHECK(manager.has_tag_in_dictionary("Nested.Tag"));
	CHECK(registry.get_tags_for_asset("res://nested.tscn").has("Nested.Tag"));
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_coordinator_multi_commit_undo() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_coordinator_multi_commit_undo");
	AssetTagManager manager;
	AssetTagRegistry registry;
	AssetTagCoordinator coordinator;
	manager.add_tag("First.Tag");
	PackedStringArray initial_tags;
	initial_tags.push_back("First.Tag");
	CHECK(registry.set_tags_for_asset("res://first.tscn", initial_tags) == OK);
	CHECK(registry.commit_batch() == OK);

	CHECK(coordinator.begin_transaction() == OK);
	CHECK(coordinator.add_tag("Second.Tag") == OK);
	CHECK(coordinator.commit_transaction() == OK);
	CHECK(manager.has_tag_in_dictionary("Second.Tag"));

	CHECK(coordinator.begin_transaction() == OK);
	CHECK(coordinator.remove_tag("First.Tag") == OK);
	CHECK(coordinator.commit_transaction() == OK);
	CHECK(!manager.has_tag_in_dictionary("First.Tag"));
	CHECK(manager.has_tag_in_dictionary("Second.Tag"));

	CHECK(coordinator.can_undo());
	CHECK(coordinator.undo_last_change() == OK);
	CHECK(manager.has_tag_in_dictionary("First.Tag"));
	CHECK(manager.has_tag_in_dictionary("Second.Tag"));

	CHECK(coordinator.can_undo());
	CHECK(coordinator.undo_last_change() == OK);
	CHECK(!manager.has_tag_in_dictionary("Second.Tag"));
	CHECK(manager.has_tag_in_dictionary("First.Tag"));
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_coordinator_commit_failure_rollback() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_coordinator_commit_failure");
	AssetTagManager manager;
	AssetTagRegistry registry;
	AssetTagCoordinator coordinator;
	manager.add_tag("Stable.Tag");
	PackedStringArray initial;
	initial.push_back("Stable.Tag");
	CHECK(registry.set_tags_for_asset("res://stable.tscn", initial) == OK);
	CHECK(registry.commit_batch() == OK);

	CHECK(coordinator.begin_transaction() == OK);
	CHECK(manager.add_tag("Pending.Fail") == OK);
	PackedStringArray pending;
	pending.push_back("Pending.Fail");
	CHECK(registry.set_tags_for_asset("res://pending.tscn", pending) == OK);
	AssetTagStorage::set_test_fail_index_commit(true);
	CHECK(coordinator.commit_transaction() == ERR_CANT_CREATE);
	CHECK(!registry.get_indexed_paths().has("res://pending.tscn"));
	CHECK(!registry.is_in_batch());
	CHECK(!manager.is_in_batch());
	AssetTagStorage::set_test_fail_index_commit(false);
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_coordinator_scope_commit() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_coordinator_scope_commit");
	AssetTagManager manager;
	AssetTagRegistry registry;
	AssetTagCoordinator coordinator;
	{
		AssetTagCoordinatorScope scope;
		CHECK(scope.is_active());
		CHECK(manager.add_tag("Scope.Tag") == OK);
		CHECK(scope.commit() == OK);
	}
	CHECK(manager.has_tag_in_dictionary("Scope.Tag"));
	AssetTagStorage::clear_test_storage_dir();
}

#endif
