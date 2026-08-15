/**************************************************************************/
/*  test_justamcp_settings_resolver.cpp                                   */
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

#include "test_justamcp_settings_resolver.h"
#include "../tools/justamcp_resource_manifest.h"
#include "../tools/justamcp_settings_resolver.h"
#include "../tools/justamcp_tool_executor.h"
#include "../tools/justamcp_tool_schema_cache.h"
#include "modules/modules_enabled.gen.h"
#include "tests/test_macros.h"

void test_justamcp_settings_resolver() {
	bool cat_enabled = false;
	bool tool_enabled = false;
	CHECK(JustAMCPSettingsResolver::resolve_tool_enabled("asset_tags_tools", "blazium_tags_list", true, false, cat_enabled, tool_enabled));
	CHECK(cat_enabled);
	CHECK(tool_enabled);
	CHECK(JustAMCPSettingsResolver::resolve_category_enabled("asset_tags_tools", true));
}

void test_justamcp_resource_manifest() {
	Array resources = JustAMCPResourceManifest::get_static_resource_schemas();
	CHECK(resources.size() >= 10);
	bool has_dictionary = false;
	for (int i = 0; i < resources.size(); i++) {
		Dictionary item = resources[i];
		if (String(item.get("uri", "")) == "blazium://tags/dictionary") {
			has_dictionary = true;
		}
	}
#ifdef MODULE_ASSETTAGS_ENABLED
	CHECK(has_dictionary);
#else
	(void)has_dictionary;
#endif
	Array templates = JustAMCPResourceManifest::get_static_resource_template_schemas();
	CHECK(templates.size() >= 5);
	CHECK(JustAMCPToolSchemaCache::get_schemas(false, true, true, true).size() > 0);
	JustAMCPToolExecutor counter;
	(void)counter;
	CHECK(JustAMCPToolExecutor::get_active_instance() == nullptr);
}

#endif
