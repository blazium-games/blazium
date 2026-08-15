/**************************************************************************/
/*  justamcp_executor_dispatch_table.cpp                                  */
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

#ifdef TOOLS_ENABLED

#include "justamcp_executor_dispatch_table.h"

#include "core/string/string_name.h"
#include "core/templates/hash_map.h"
#include "modules/modules_enabled.gen.h"

static const JustAMCPToolPrefixRoute PREFIX_ROUTES[] = {
#ifdef MODULE_ASSETTAGS_ENABLED
	{ "tags_", "AssetTags" },
#endif
#ifdef MODULE_SEMANTICSEARCH_ENABLED
	{ "semantic_", "SemanticSearch" },
#endif
	{ "mcp_client_", "MCPClient" },
};

static const JustAMCPToolPrefixRoute EXACT_ROUTES[] = {
#ifdef MODULE_ASSETTAGS_ENABLED
	{ "tags_list", "AssetTags" },
	{ "tags_search_assets", "AssetTags" },
#endif
#ifdef MODULE_SEMANTICSEARCH_ENABLED
	{ "semantic_search", "SemanticSearch" },
	{ "semantic_find_similar", "SemanticSearch" },
#endif
	{ "mcp_client_list_bridges", "MCPClient" },
	{ "mcp_client_call_remote_tool", "MCPClient" },
};

static HashMap<StringName, const char *> &get_exact_route_map() {
	static HashMap<StringName, const char *> routes;
	static bool initialized = false;
	if (!initialized) {
		for (int i = 0; i < (int)(sizeof(EXACT_ROUTES) / sizeof(EXACT_ROUTES[0])); i++) {
			routes[StringName(EXACT_ROUTES[i].prefix)] = EXACT_ROUTES[i].toolset;
		}
		initialized = true;
	}
	return routes;
}

int JustAMCPExecutorDispatchTable::get_prefix_route_count() {
	return (int)(sizeof(PREFIX_ROUTES) / sizeof(PREFIX_ROUTES[0]));
}

const JustAMCPToolPrefixRoute &JustAMCPExecutorDispatchTable::get_prefix_route(int p_index) {
	return PREFIX_ROUTES[p_index];
}

bool JustAMCPExecutorDispatchTable::matches_prefix_route(const String &p_internal_name) {
	for (int i = 0; i < get_prefix_route_count(); i++) {
		if (p_internal_name.begins_with(PREFIX_ROUTES[i].prefix)) {
			return true;
		}
	}
	return false;
}

const char *JustAMCPExecutorDispatchTable::get_toolset_for_prefix(const String &p_internal_name) {
	for (int i = 0; i < get_prefix_route_count(); i++) {
		if (p_internal_name.begins_with(PREFIX_ROUTES[i].prefix)) {
			return PREFIX_ROUTES[i].toolset;
		}
	}
	return nullptr;
}

const char *JustAMCPExecutorDispatchTable::get_toolset_for_tool(const String &p_internal_name) {
	if (const char *const *toolset = get_exact_route_map().getptr(p_internal_name)) {
		return *toolset;
	}
	return get_toolset_for_prefix(p_internal_name);
}

bool JustAMCPExecutorDispatchTable::matches_dispatch_route(const String &p_internal_name) {
	return get_toolset_for_tool(p_internal_name) != nullptr;
}

#endif
