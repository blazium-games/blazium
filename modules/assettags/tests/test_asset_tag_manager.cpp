/**************************************************************************/
/*  test_asset_tag_manager.cpp                                            */
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

#include "../asset_tag_hierarchy.h"
#include "../asset_tag_manager.h"
#include "../asset_tag_storage.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "tests/test_macros.h"

void test_asset_tag_manager() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_manager");
	AssetTagManager manager;

	CHECK(manager.matches_tag("Environment.Nature.Tree", "Environment"));
	CHECK(manager.matches_tag("Environment.Nature.Tree", "Environment.Nature"));
	CHECK(!manager.matches_tag("Environment", "Environment.Nature"));

	PackedStringArray container;
	container.push_back("Environment.Nature.Tree");
	CHECK(manager.container_has_tag(container, "Environment.Nature"));
	PackedStringArray required_tags;
	required_tags.push_back("Character.Hero");
	CHECK(!manager.container_has_all(container, required_tags));

	manager.add_tag("A.B.C", "leaf");
	PackedStringArray implicit_children = manager.list_tags("A");
	CHECK(implicit_children.has("A.B"));

	manager.add_tag("Root.One");
	manager.add_tag("Root.One.Child");
	manager.add_tag("Other");

	PackedStringArray roots = manager.list_tags("");
	CHECK(roots.size() == 3);
	CHECK(roots.has("A"));
	CHECK(roots.has("Root"));
	CHECK(roots.has("Other"));

	PackedStringArray children = manager.list_tags("Root.One");
	CHECK(children.size() == 1);
	CHECK(children.has("Root.One.Child"));

	CHECK(manager.update_tag_comment("Root.One", "Updated comment") == OK);
	Dictionary info = manager.get_tag_info("Root.One");
	CHECK(info.get("ok", false));
	CHECK(String(info.get("comment", "")) == "Updated comment");
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_manager_idempotent_rename() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_idempotent_rename");
	AssetTagManager manager;
	manager.add_tag("Legacy.Root");
	CHECK(manager.rename_tag("Legacy", "Modern") == OK);
	CHECK(manager.rename_tag("Legacy", "Modern") == OK);
	AssetTagStorage::clear_test_storage_dir();
}

void test_asset_tag_manager_corrupt_dictionary_recovery() {
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_isolated_corrupt_dictionary");
	CHECK(AssetTagStorage::ensure_storage_dir());
	Ref<FileAccess> corrupt = FileAccess::open(AssetTagStorage::get_tags_file_path(), FileAccess::WRITE);
	if (corrupt.is_valid()) {
		corrupt->store_string("{not-json");
		corrupt.unref();
	}
	AssetTagManager manager;
	CHECK(manager.load() == ERR_FILE_CORRUPT);
	CHECK(AssetTagStorage::quarantine_corrupt_dictionary());
	CHECK(manager.load() == OK);
	CHECK(!FileAccess::exists(AssetTagStorage::get_tags_file_path()));
	CHECK(FileAccess::exists(AssetTagStorage::get_tags_file_path() + ".corrupt"));
	AssetTagStorage::clear_test_storage_dir();
}

#endif
