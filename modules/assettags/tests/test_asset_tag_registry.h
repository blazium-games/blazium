/**************************************************************************/
/*  test_asset_tag_registry.h                                             */
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

#pragma once

#include "tests/test_macros.h"

void test_asset_tag_registry_apply();
void test_asset_tag_registry_search_assets();
void test_asset_tag_registry_find_assets_redirect();
void test_asset_tag_registry_rename_path();
void test_asset_tag_registry_set_tags_noop();
void test_asset_tag_registry_rescan_guard();
void test_asset_tag_strict_tags();
void test_asset_tag_rename_collision();
void test_asset_tag_registry_index_dirty_rescan();
void test_asset_tag_registry();
void test_asset_tag_nonbatch_auto_add_atomic();
void test_asset_tag_registry_load_recovery();
void test_asset_tag_registry_recovery_quarantine_sidecars();
void test_asset_tag_alias_search_unmigrated_index();
void test_asset_tag_prune_removed_paths();
void test_asset_tag_registry_atomic_commit_batch();
void test_asset_tag_registry_index_write_blocked_commit();
void test_asset_tag_alias_incremental_update();
void test_asset_tag_alias_redirect_signal_rebuild();
void test_asset_tag_commit_policy_redirects_changed();
void test_asset_tag_teardown_deferred_noop();
void test_asset_tag_prune_queues_during_transaction();
void test_asset_tag_prune_queues_during_batch();

TEST_CASE("[Modules][AssetTags] registry apply") {
	test_asset_tag_registry_apply();
}

TEST_CASE("[Modules][AssetTags] registry search assets") {
	test_asset_tag_registry_search_assets();
}

TEST_CASE("[Modules][AssetTags] registry find assets redirect") {
	test_asset_tag_registry_find_assets_redirect();
}

TEST_CASE("[Modules][AssetTags] registry rename path") {
	test_asset_tag_registry_rename_path();
}

TEST_CASE("[Modules][AssetTags] registry set tags noop") {
	test_asset_tag_registry_set_tags_noop();
}

TEST_CASE("[Modules][AssetTags] registry rescan guard") {
	test_asset_tag_registry_rescan_guard();
}

TEST_CASE("[Modules][AssetTags] strict tags") {
	test_asset_tag_strict_tags();
}

TEST_CASE("[Modules][AssetTags] rename collision") {
	test_asset_tag_rename_collision();
}

TEST_CASE("[Modules][AssetTags] registry index dirty rescan") {
	test_asset_tag_registry_index_dirty_rescan();
}

TEST_CASE("[Modules][AssetTags] registry") {
	test_asset_tag_registry();
}

TEST_CASE("[Modules][AssetTags] nonbatch auto add atomic") {
	test_asset_tag_nonbatch_auto_add_atomic();
}

TEST_CASE("[Modules][AssetTags] registry load recovery") {
	test_asset_tag_registry_load_recovery();
}

TEST_CASE("[Modules][AssetTags] registry recovery quarantine sidecars") {
	test_asset_tag_registry_recovery_quarantine_sidecars();
}

TEST_CASE("[Modules][AssetTags] alias search unmigrated index") {
	test_asset_tag_alias_search_unmigrated_index();
}

TEST_CASE("[Modules][AssetTags] prune removed paths") {
	test_asset_tag_prune_removed_paths();
}

TEST_CASE("[Modules][AssetTags] registry atomic commit batch") {
	test_asset_tag_registry_atomic_commit_batch();
}

TEST_CASE("[Modules][AssetTags] registry index write blocked commit") {
	test_asset_tag_registry_index_write_blocked_commit();
}

TEST_CASE("[Modules][AssetTags] alias incremental update") {
	test_asset_tag_alias_incremental_update();
}

TEST_CASE("[Modules][AssetTags] alias redirect signal rebuild") {
	test_asset_tag_alias_redirect_signal_rebuild();
}

TEST_CASE("[Modules][AssetTags] commit policy emits redirects_changed") {
	test_asset_tag_commit_policy_redirects_changed();
}

TEST_CASE("[Modules][AssetTags] teardown deferred noop") {
	test_asset_tag_teardown_deferred_noop();
}

TEST_CASE("[Modules][AssetTags] prune queues during transaction") {
	test_asset_tag_prune_queues_during_transaction();
}

TEST_CASE("[Modules][AssetTags] prune queues during batch") {
	test_asset_tag_prune_queues_during_batch();
}
