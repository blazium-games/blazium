/**************************************************************************/
/*  test_asset_tag_storage.h                                              */
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

void test_asset_tag_storage_roundtrip();
void test_asset_tag_paired_commit_rollback();
void test_asset_tag_partial_undo_snapshot();
void test_asset_tag_index_sidecar_roundtrip();
void test_asset_tag_sidecar_stale_after_full_write();
void test_asset_tag_undo_sidecar_parity();
void test_asset_tag_sidecar_path_encoding();
void test_asset_tag_runtime_sidecar_cache();
void test_asset_tag_runtime_export_bake_read();
void test_asset_tag_runtime_notify_sidecar_dirty();
void test_asset_tag_sidecar_compact_at_64();
void test_asset_tag_sidecar_disambiguated_tracking();
void test_asset_tag_runtime_export_failure_cleanup();
void test_asset_tag_normalize_asset_path();

TEST_CASE("[Modules][AssetTags] storage roundtrip") {
	test_asset_tag_storage_roundtrip();
}

TEST_CASE("[Modules][AssetTags] paired commit rollback") {
	test_asset_tag_paired_commit_rollback();
}

TEST_CASE("[Modules][AssetTags] partial undo snapshot") {
	test_asset_tag_partial_undo_snapshot();
}

TEST_CASE("[Modules][AssetTags] index sidecar roundtrip") {
	test_asset_tag_index_sidecar_roundtrip();
}

TEST_CASE("[Modules][AssetTags] sidecar stale after full write") {
	test_asset_tag_sidecar_stale_after_full_write();
}

TEST_CASE("[Modules][AssetTags] undo sidecar parity") {
	test_asset_tag_undo_sidecar_parity();
}

TEST_CASE("[Modules][AssetTags] sidecar path encoding") {
	test_asset_tag_sidecar_path_encoding();
}

TEST_CASE("[Modules][AssetTags] runtime sidecar cache") {
	test_asset_tag_runtime_sidecar_cache();
}

TEST_CASE("[Modules][AssetTags] runtime export bake read") {
	test_asset_tag_runtime_export_bake_read();
}

TEST_CASE("[Modules][AssetTags] runtime notify sidecar dirty") {
	test_asset_tag_runtime_notify_sidecar_dirty();
}

TEST_CASE("[Modules][AssetTags] sidecar compact at 64") {
	test_asset_tag_sidecar_compact_at_64();
}

TEST_CASE("[Modules][AssetTags] sidecar disambiguated tracking") {
	test_asset_tag_sidecar_disambiguated_tracking();
}

TEST_CASE("[Modules][AssetTags] runtime export failure cleanup") {
	test_asset_tag_runtime_export_failure_cleanup();
}

TEST_CASE("[Modules][AssetTags] normalize asset path") {
	test_asset_tag_normalize_asset_path();
}
