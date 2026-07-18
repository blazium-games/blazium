/**************************************************************************/
/*  test_asset_tag_coordinator.h                                          */
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

void test_asset_tag_coordinator();
void test_asset_tag_coordinator_redirect();
void test_asset_tag_coordinator_transaction_batch();
void test_asset_tag_coordinator_abort_batch();
void test_asset_tag_coordinator_undo();
void test_asset_tag_coordinator_abort_clears_undo();
void test_asset_tag_coordinator_begin_transaction_missing_index();
void test_asset_tag_coordinator_standalone_add_undo();
void test_asset_tag_coordinator_nested_abort();
void test_asset_tag_coordinator_multi_commit_undo();
void test_asset_tag_coordinator_commit_failure_rollback();
void test_asset_tag_coordinator_scope_commit();

TEST_CASE("[Modules][AssetTags] coordinator") {
	test_asset_tag_coordinator();
}

TEST_CASE("[Modules][AssetTags] coordinator redirect") {
	test_asset_tag_coordinator_redirect();
}

TEST_CASE("[Modules][AssetTags] coordinator transaction batch") {
	test_asset_tag_coordinator_transaction_batch();
}

TEST_CASE("[Modules][AssetTags] coordinator abort batch") {
	test_asset_tag_coordinator_abort_batch();
}

TEST_CASE("[Modules][AssetTags] coordinator undo") {
	test_asset_tag_coordinator_undo();
}

TEST_CASE("[Modules][AssetTags] coordinator abort clears undo") {
	test_asset_tag_coordinator_abort_clears_undo();
}

TEST_CASE("[Modules][AssetTags] coordinator begin transaction missing index") {
	test_asset_tag_coordinator_begin_transaction_missing_index();
}

TEST_CASE("[Modules][AssetTags] coordinator standalone add undo") {
	test_asset_tag_coordinator_standalone_add_undo();
}

TEST_CASE("[Modules][AssetTags] coordinator nested abort") {
	test_asset_tag_coordinator_nested_abort();
}

TEST_CASE("[Modules][AssetTags] coordinator multi commit undo") {
	test_asset_tag_coordinator_multi_commit_undo();
}

TEST_CASE("[Modules][AssetTags] coordinator commit failure rollback") {
	test_asset_tag_coordinator_commit_failure_rollback();
}

TEST_CASE("[Modules][AssetTags] coordinator scope commit") {
	test_asset_tag_coordinator_scope_commit();
}
