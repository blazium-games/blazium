/**************************************************************************/
/*  test_justamcp_toolset_schema_gate.cpp                                 */
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

#include "test_justamcp_toolset_schema_gate.h"

#include "modules/modules_enabled.gen.h"

#ifdef TOOLS_ENABLED

#include "../tools/justamcp_category_registry.h"
#include "../tools/justamcp_tool_schema_cache.h"
#include "../tools/justamcp_toolset_registry.h"
#include "core/templates/hash_set.h"
#include "core/templates/vector.h"
#include "tests/test_macros.h"

void test_justamcp_toolset_schema_gate() {
	TEST_FAIL_COND(!JustAMCPToolsetRegistry::get_singleton(), "JustAMCPToolsetRegistry singleton is required");

	JustAMCPToolSchemaCache::invalidate();

	Vector<String> expected_names;
	for (int i = 0; i < JustAMCPCategoryRegistry::get_entry_count(); i++) {
		const JustAMCPCategoryRegistryEntry &entry = JustAMCPCategoryRegistry::get_entry(i);
		if (entry.requires_multiuser) {
#ifndef MODULE_MULTIUSER_EDITOR_ENABLED
			continue;
#endif
		}
		if (entry.requires_autowork) {
#ifndef MODULE_AUTOWORK_ENABLED
			continue;
#endif
		}
		expected_names.push_back(entry.display_name);

		if (entry.requires_multiuser || entry.requires_autowork) {
			continue;
		}

		const Array category_schemas = JustAMCPToolSchemaCache::get_category_schemas(entry.category_id, false, true, true);
		CHECK_MESSAGE(category_schemas.size() > 0, vformat("Category schemas empty for %s", entry.category_id));
	}

	expected_names.push_back("MCPClient");
#ifdef MODULE_ASSETTAGS_ENABLED
	expected_names.push_back("AssetTags");
#endif
#ifdef MODULE_SEMANTICSEARCH_ENABLED
	expected_names.push_back("SemanticSearch");
#endif

	const Dictionary listed = JustAMCPToolsetRegistry::get_singleton()->list_toolsets();
	CHECK(listed.get("ok", false));
	const Array toolsets = listed.get("toolsets", Array());
	CHECK(toolsets.size() >= expected_names.size());

	HashSet<String> listed_names;
	for (int i = 0; i < toolsets.size(); i++) {
		Dictionary item = toolsets[i];
		listed_names.insert(String(item.get("name", "")));
	}

	for (int i = 0; i < expected_names.size(); i++) {
		const String &name = expected_names[i];
		CHECK_MESSAGE(listed_names.has(name), vformat("Missing toolset in list_toolsets: %s", name));
		const Dictionary described = JustAMCPToolsetRegistry::get_singleton()->describe_toolset(name);
		if (!bool(described.get("ok", false))) {
			const String err = String(described.get("error", ""));
			if (err.contains("disabled")) {
				continue;
			}
		}
		CHECK_MESSAGE(described.get("ok", false), vformat("describe_toolset failed for %s: %s", name, String(described.get("error", ""))));
		const Array tools = described.get("tools", Array());
		if (name == "Multiuser" || name == "Autowork") {
			continue;
		}
		CHECK_MESSAGE(tools.size() > 0, vformat("Toolset has zero schemas: %s", name));
	}
}

#else

void test_justamcp_toolset_schema_gate() {
	SUCCEED();
}

#endif

#endif
