/**************************************************************************/
/*  justamcp_tool_schema_cache.h                                          */
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

#ifdef TOOLS_ENABLED

#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

class JustAMCPToolSchemaCache {
public:
	static void invalidate();
	static void invalidate_category(const String &p_category);
	static void mark_all_cached_categories_dirty();
	static Array get_schemas(bool p_register_only, bool p_ignore_settings, bool p_apply_discovery_filter, bool p_include_disabled_tools);
	static Dictionary find_tool_schema(const String &p_full_name, bool p_include_disabled_tools = true);
	static Array get_category_schemas(const String &p_category, bool p_register_only, bool p_ignore_settings, bool p_include_disabled_tools);

private:
	struct CacheKey {
		bool register_only = false;
		bool ignore_settings = false;
		bool apply_discovery_filter = false;
		bool include_disabled_tools = false;

		bool operator==(const CacheKey &p_other) const {
			return register_only == p_other.register_only &&
					ignore_settings == p_other.ignore_settings &&
					apply_discovery_filter == p_other.apply_discovery_filter &&
					include_disabled_tools == p_other.include_disabled_tools;
		}
	};

	struct CacheEntry {
		Array all_tools;
		HashMap<String, Dictionary> by_name;
		HashMap<String, Array> by_category;
		uint64_t generation = 0;
		bool all_tools_dirty = false;
	};

	static uint32_t _make_cache_key(bool p_register_only, bool p_ignore_settings, bool p_apply_discovery_filter, bool p_include_disabled_tools);
	static CacheEntry &_get_entry(bool p_register_only, bool p_ignore_settings, bool p_apply_discovery_filter, bool p_include_disabled_tools);
	static void _rebuild_if_needed(bool p_register_only, bool p_ignore_settings, bool p_apply_discovery_filter, bool p_include_disabled_tools);
	static void _rebuild_all_tools_from_categories(CacheEntry &p_cache);
	static void _rebuild_category_if_needed(bool p_register_only, bool p_ignore_settings, bool p_apply_discovery_filter, bool p_include_disabled_tools, const String &p_category);

	static HashMap<uint32_t, CacheEntry> cache_entries;
	static uint64_t current_generation;
	static HashSet<String> dirty_categories;
	static Mutex cache_mutex;

	static void _rebuild_dirty_categories_if_needed(bool p_register_only, bool p_ignore_settings, bool p_apply_discovery_filter, bool p_include_disabled_tools);
};

#endif
