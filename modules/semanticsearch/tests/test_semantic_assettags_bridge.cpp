/**************************************************************************/
/*  test_semantic_assettags_bridge.cpp                                    */
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

#ifdef TOOLS_ENABLED

#include "../semantic_assettags_bridge.h"

#include "modules/modules_enabled.gen.h"

#ifdef MODULE_ASSETTAGS_ENABLED
#include "modules/assettags/asset_tag_manager.h"
#include "modules/assettags/asset_tag_registry.h"
#include "modules/assettags/asset_tag_storage.h"
#endif

#include "../semantic_asset_index.h"
#include "core/io/file_access.h"
#include "tests/test_macros.h"

void test_semantic_assettags_bridge_signal_upsert() {
#ifdef MODULE_ASSETTAGS_ENABLED
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_bridge_signal");
	AssetTagManager manager;
	AssetTagRegistry registry;
	SemanticAssetIndex *index = SemanticAssetIndex::get_singleton();
	CHECK(index != nullptr);
	index->clear();
	SemanticAssettagsBridge::attach(&registry);
	const String bridge_path = "res://bridge_signal.tscn";
	Ref<FileAccess> bridge_file = FileAccess::open(bridge_path, FileAccess::WRITE);
	CHECK(bridge_file.is_valid());
	bridge_file.unref();
	PackedStringArray tags;
	tags.push_back("Bridge.Signal");
	CHECK(registry.set_tags_for_asset(bridge_path, tags) == OK);
	CHECK(index->search("bridge", 5).size() >= 1);
	SemanticAssettagsBridge::detach();
	AssetTagStorage::clear_test_storage_dir();
#else
	CHECK(true);
#endif
}

void test_semantic_assettags_bridge_remove_missing_asset() {
#ifdef MODULE_ASSETTAGS_ENABLED
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_bridge_remove");
	AssetTagManager manager;
	AssetTagRegistry registry;
	SemanticAssetIndex *index = SemanticAssetIndex::get_singleton();
	CHECK(index != nullptr);
	index->clear();
	index->upsert_entry("res://gone.tscn");
	SemanticAssettagsBridge::attach(&registry);
	CHECK(registry.set_tags_for_asset("res://gone.tscn", PackedStringArray()) == OK);
	Dictionary entry = index->get_asset_entry("res://gone.tscn");
	CHECK(!entry.get("ok", true));
	SemanticAssettagsBridge::detach();
	AssetTagStorage::clear_test_storage_dir();
#else
	CHECK(true);
#endif
}

TEST_CASE("[Modules][SemanticSearch] assettags bridge signal upsert") {
	test_semantic_assettags_bridge_signal_upsert();
}

TEST_CASE("[Modules][SemanticSearch] assettags bridge remove missing asset") {
	test_semantic_assettags_bridge_remove_missing_asset();
}

#endif

#endif
