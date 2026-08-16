/**************************************************************************/
/*  test_justamcp_resource_providers.cpp                                  */
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

#include "test_justamcp_resource_providers.h"

#include "../tools/resources/justamcp_blazium_resource_registry.h"
#include "../tools/resources/justamcp_guides_resource_provider.h"
#include "../tools/resources/justamcp_logs_resource_provider.h"
#include "../tools/resources/justamcp_materials_resource_provider.h"
#include "../tools/resources/justamcp_project_resource_provider.h"
#include "../tools/resources/justamcp_selection_resource_provider.h"
#include "../tools/resources/justamcp_semantic_resource_provider.h"
#include "../tools/resources/justamcp_tags_resource_provider.h"

#include "modules/modules_enabled.gen.h"

#ifdef MODULE_ASSETTAGS_ENABLED
#include "modules/assettags/asset_tag_manager.h"
#include "modules/assettags/asset_tag_registry.h"
#include "modules/assettags/asset_tag_storage.h"
#endif

#ifdef MODULE_SEMANTICSEARCH_ENABLED
#include "modules/semanticsearch/semantic_asset_index.h"
#endif

#include "core/io/file_access.h"
#include "tests/test_macros.h"

void test_justamcp_tags_resource_provider_reads() {
#ifdef MODULE_ASSETTAGS_ENABLED
	AssetTagStorage::set_test_storage_dir("res://.blazium/test_tags_resource_provider");
	AssetTagManager manager;
	AssetTagRegistry registry;
	CHECK(JustAMCPTagsResourceProvider::can_read("blazium://tags/dictionary"));
	const String tagged_path = "res://tagged.tscn";
	Ref<FileAccess> tagged_file = FileAccess::open(tagged_path, FileAccess::WRITE);
	CHECK(tagged_file.is_valid());
	tagged_file.unref();
	PackedStringArray tags;
	tags.push_back("Resource.Provider");
	CHECK(registry.set_tags_for_asset(tagged_path, tags) == OK);
	Dictionary asset = JustAMCPTagsResourceProvider::read("blazium://tags/asset/res%3A%2F%2Ftagged.tscn", "blazium://tags/asset/res%3A%2F%2Ftagged.tscn");
	CHECK(asset.get("ok", false));
	AssetTagStorage::clear_test_storage_dir();
#else
	TEST_FAIL_COND(true, "MODULE_ASSETTAGS_ENABLED is required for tags resource provider tests");
#endif
}

void test_justamcp_semantic_resource_provider_reads() {
#ifdef MODULE_SEMANTICSEARCH_ENABLED
	SemanticAssetIndex *index = SemanticAssetIndex::get_singleton();
	CHECK(index != nullptr);
	const String test_path = "res://semantic_resource.tscn";
	Ref<FileAccess> resource_file = FileAccess::open(test_path, FileAccess::WRITE);
	CHECK(resource_file.is_valid());
	resource_file.unref();
	CHECK(index->upsert_entry(test_path) == OK);
	CHECK(JustAMCPSemanticResourceProvider::can_read("blazium://semantic/index/stats"));
	Dictionary stats = JustAMCPSemanticResourceProvider::read("blazium://semantic/index/stats", "blazium://semantic/index/stats");
	CHECK(stats.get("ok", false));
#else
	TEST_FAIL_COND(true, "MODULE_SEMANTICSEARCH_ENABLED is required for semantic resource provider tests");
#endif
}

void test_justamcp_resource_provider_registry_coverage() {
	CHECK(JustAMCPBlaziumResourceRegistry::can_read("blazium://project/info"));
	CHECK(JustAMCPBlaziumResourceRegistry::can_read("blazium://tags/dictionary"));
	CHECK(JustAMCPBlaziumResourceRegistry::can_read("blazium://semantic/index/stats"));
	CHECK(JustAMCPBlaziumResourceRegistry::can_read("blazium://logs/mcp"));
	CHECK(JustAMCPBlaziumResourceRegistry::can_read("blazium://guide/asset-tagging"));
	CHECK(JustAMCPBlaziumResourceRegistry::can_read("blazium://materials"));
	CHECK(JustAMCPBlaziumResourceRegistry::can_read("blazium://selection/current"));
	CHECK(!JustAMCPBlaziumResourceRegistry::can_read("blazium://unknown/resource"));
}

void test_justamcp_project_resource_provider_reads() {
	Dictionary info = JustAMCPProjectResourceProvider::read("blazium://project/info", "blazium://project/info");
	CHECK(info.get("ok", false));
}

void test_justamcp_logs_resource_provider_reads() {
	CHECK(JustAMCPLogsResourceProvider::can_read("blazium://logs/mcp"));
	Dictionary logs = JustAMCPLogsResourceProvider::read("blazium://logs/mcp", "blazium://logs/mcp");
	CHECK(logs.get("ok", false));
}

void test_justamcp_guides_resource_provider_reads() {
	CHECK(JustAMCPGuidesResourceProvider::can_read("blazium://guide/asset-tagging"));
	Dictionary guide = JustAMCPGuidesResourceProvider::read("blazium://guide/asset-tagging", "blazium://guide/asset-tagging");
	CHECK(guide.get("ok", false));
}

void test_justamcp_materials_resource_provider_reads() {
	CHECK(JustAMCPMaterialsResourceProvider::can_read("blazium://materials"));
	Dictionary materials = JustAMCPMaterialsResourceProvider::read("blazium://materials", "blazium://materials");
	CHECK(materials.get("ok", false));
}

void test_justamcp_selection_resource_provider_reads() {
	CHECK(JustAMCPSelectionResourceProvider::can_read("blazium://selection/current"));
	Dictionary selection = JustAMCPSelectionResourceProvider::read("blazium://selection/current", "blazium://selection/current");
	CHECK(selection.get("ok", false));
}

#endif

#endif
