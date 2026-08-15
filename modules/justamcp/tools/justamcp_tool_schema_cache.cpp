/**************************************************************************/
/*  justamcp_tool_schema_cache.cpp                                        */
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

#include "justamcp_tool_schema_cache.h"

#include "justamcp_tool_executor.h"

HashMap<uint32_t, JustAMCPToolSchemaCache::CacheEntry> JustAMCPToolSchemaCache::cache_entries;
uint64_t JustAMCPToolSchemaCache::current_generation = 1;
HashSet<String> JustAMCPToolSchemaCache::dirty_categories;
Mutex JustAMCPToolSchemaCache::cache_mutex;

void JustAMCPToolSchemaCache::invalidate() {
	MutexLock lock(cache_mutex);
	current_generation++;
	dirty_categories.clear();
	cache_entries.clear();
}

void JustAMCPToolSchemaCache::invalidate_category(const String &p_category) {
	if (p_category.is_empty()) {
		invalidate();
		return;
	}
	MutexLock lock(cache_mutex);
	dirty_categories.insert(p_category);
}

void JustAMCPToolSchemaCache::mark_all_cached_categories_dirty() {
	MutexLock lock(cache_mutex);
	for (KeyValue<uint32_t, CacheEntry> &kv : cache_entries) {
		for (const KeyValue<String, Array> &cat : kv.value.by_category) {
			dirty_categories.insert(cat.key);
		}
	}
	if (dirty_categories.is_empty()) {
		current_generation++;
		cache_entries.clear();
	}
}

uint32_t JustAMCPToolSchemaCache::_make_cache_key(bool p_register_only, bool p_ignore_settings, bool p_apply_discovery_filter, bool p_include_disabled_tools) {
	uint32_t key = 0;
	if (p_register_only) {
		key |= 1;
	}
	if (p_ignore_settings) {
		key |= 2;
	}
	if (p_apply_discovery_filter) {
		key |= 4;
	}
	if (p_include_disabled_tools) {
		key |= 8;
	}
	return key;
}

JustAMCPToolSchemaCache::CacheEntry &JustAMCPToolSchemaCache::_get_entry(bool p_register_only, bool p_ignore_settings, bool p_apply_discovery_filter, bool p_include_disabled_tools) {
	const uint32_t key = _make_cache_key(p_register_only, p_ignore_settings, p_apply_discovery_filter, p_include_disabled_tools);
	return cache_entries[key];
}

void JustAMCPToolSchemaCache::_rebuild_all_tools_from_categories(CacheEntry &p_cache) {
	Array rebuilt_all;
	HashMap<String, Dictionary> rebuilt_by_name;
	for (const KeyValue<String, Array> &kv : p_cache.by_category) {
		const Array &bucket = kv.value;
		for (int i = 0; i < bucket.size(); i++) {
			rebuilt_all.push_back(bucket[i]);
			Dictionary tool = bucket[i];
			const String name = tool.get("name", "");
			if (!name.is_empty()) {
				rebuilt_by_name.insert(name, tool);
			}
		}
	}
	p_cache.all_tools = rebuilt_all;
	p_cache.by_name = rebuilt_by_name;
	p_cache.all_tools_dirty = false;
}

void JustAMCPToolSchemaCache::_rebuild_if_needed(bool p_register_only, bool p_ignore_settings, bool p_apply_discovery_filter, bool p_include_disabled_tools) {
	const uint32_t key = _make_cache_key(p_register_only, p_ignore_settings, p_apply_discovery_filter, p_include_disabled_tools);
	uint64_t target_generation = 0;
	bool needs_rebuild = false;
	{
		MutexLock lock(cache_mutex);
		CacheEntry &cache = cache_entries[key];
		if (cache.generation == current_generation && !cache.all_tools.is_empty() && !cache.all_tools_dirty) {
			return;
		}
		target_generation = current_generation;
		needs_rebuild = true;
	}
	if (!needs_rebuild) {
		return;
	}

	CacheEntry built;
	built.all_tools = JustAMCPToolExecutor::get_tool_schemas(p_register_only, p_ignore_settings, p_apply_discovery_filter, p_include_disabled_tools);
	for (int i = 0; i < built.all_tools.size(); i++) {
		Dictionary tool = built.all_tools[i];
		const String name = tool.get("name", "");
		if (!name.is_empty()) {
			built.by_name.insert(name, tool);
		}
		if (tool.has("_meta")) {
			Dictionary meta = tool["_meta"];
			const String category = meta.get("category", "");
			if (!category.is_empty()) {
				if (!built.by_category.has(category)) {
					built.by_category.insert(category, Array());
				}
				Array &bucket = built.by_category[category];
				bucket.push_back(tool);
			}
		}
	}
	built.generation = target_generation;
	built.all_tools_dirty = false;

	MutexLock lock(cache_mutex);
	if (current_generation != target_generation) {
		return;
	}
	CacheEntry &cache = cache_entries[key];
	if (cache.generation != current_generation || cache.all_tools.is_empty() || cache.all_tools_dirty) {
		cache = built;
	}
}

void JustAMCPToolSchemaCache::_rebuild_category_if_needed(bool p_register_only, bool p_ignore_settings, bool p_apply_discovery_filter, bool p_include_disabled_tools, const String &p_category) {
	{
		MutexLock lock(cache_mutex);
		if (!dirty_categories.has(p_category)) {
			return;
		}
	}

	Vector<uint32_t> keys_to_refresh;
	{
		MutexLock lock(cache_mutex);
		if (!dirty_categories.has(p_category)) {
			return;
		}
		for (const KeyValue<uint32_t, CacheEntry> &kv : cache_entries) {
			if (kv.value.by_category.has(p_category)) {
				keys_to_refresh.push_back(kv.key);
			}
		}

		const uint32_t caller_key = _make_cache_key(p_register_only, p_ignore_settings, p_apply_discovery_filter, p_include_disabled_tools);
		if (!keys_to_refresh.has(caller_key)) {
			keys_to_refresh.push_back(caller_key);
		}
	}

	for (int ki = 0; ki < keys_to_refresh.size(); ki++) {
		const uint32_t key = keys_to_refresh[ki];
		const bool register_only = (key & 1) != 0;
		const bool ignore_settings = (key & 2) != 0;
		const bool include_disabled = (key & 8) != 0;
		const Array refreshed_category = JustAMCPToolExecutor::collect_tool_schemas_for_category(
				p_category, register_only, ignore_settings, include_disabled);

		MutexLock lock(cache_mutex);
		if (!dirty_categories.has(p_category)) {
			return;
		}
		CacheEntry &cache = cache_entries[key];
		if (cache.by_category.has(p_category)) {
			const Array &old_bucket = cache.by_category[p_category];
			for (int i = 0; i < old_bucket.size(); i++) {
				Dictionary tool = old_bucket[i];
				const String name = tool.get("name", "");
				if (!name.is_empty()) {
					cache.by_name.erase(name);
				}
			}
		}

		for (int i = 0; i < refreshed_category.size(); i++) {
			Dictionary tool = refreshed_category[i];
			const String name = tool.get("name", "");
			if (!name.is_empty()) {
				cache.by_name.insert(name, tool);
			}
		}

		if (cache.by_category.has(p_category)) {
			cache.by_category[p_category] = refreshed_category;
		} else {
			cache.by_category.insert(p_category, refreshed_category);
		}
		cache.all_tools_dirty = true;
	}

	MutexLock lock(cache_mutex);
	dirty_categories.erase(p_category);
}

void JustAMCPToolSchemaCache::_rebuild_dirty_categories_if_needed(bool p_register_only, bool p_ignore_settings, bool p_apply_discovery_filter, bool p_include_disabled_tools) {
	Vector<String> pending;
	{
		MutexLock lock(cache_mutex);
		if (dirty_categories.is_empty()) {
			return;
		}
		for (const String &category : dirty_categories) {
			pending.push_back(category);
		}
	}
	for (int i = 0; i < pending.size(); i++) {
		_rebuild_category_if_needed(p_register_only, p_ignore_settings, p_apply_discovery_filter, p_include_disabled_tools, pending[i]);
	}
}

Array JustAMCPToolSchemaCache::get_schemas(bool p_register_only, bool p_ignore_settings, bool p_apply_discovery_filter, bool p_include_disabled_tools) {
	_rebuild_dirty_categories_if_needed(p_register_only, p_ignore_settings, p_apply_discovery_filter, p_include_disabled_tools);
	_rebuild_if_needed(p_register_only, p_ignore_settings, p_apply_discovery_filter, p_include_disabled_tools);
	MutexLock lock(cache_mutex);
	CacheEntry &cache = _get_entry(p_register_only, p_ignore_settings, p_apply_discovery_filter, p_include_disabled_tools);
	if (cache.all_tools_dirty) {
		_rebuild_all_tools_from_categories(cache);
	}
	return cache.all_tools;
}

Dictionary JustAMCPToolSchemaCache::find_tool_schema(const String &p_full_name, bool p_include_disabled_tools) {
	_rebuild_dirty_categories_if_needed(false, false, false, p_include_disabled_tools);
	_rebuild_if_needed(false, false, false, p_include_disabled_tools);
	MutexLock lock(cache_mutex);
	CacheEntry &cache = _get_entry(false, false, false, p_include_disabled_tools);
	if (cache.all_tools_dirty) {
		_rebuild_all_tools_from_categories(cache);
	}
	if (cache.by_name.has(p_full_name)) {
		return cache.by_name[p_full_name];
	}
	return Dictionary();
}

Array JustAMCPToolSchemaCache::get_category_schemas(const String &p_category, bool p_register_only, bool p_ignore_settings, bool p_include_disabled_tools) {
	bool category_dirty = false;
	{
		MutexLock lock(cache_mutex);
		category_dirty = dirty_categories.has(p_category);
	}
	if (category_dirty) {
		_rebuild_category_if_needed(p_register_only, p_ignore_settings, false, p_include_disabled_tools, p_category);
	} else {
		_rebuild_if_needed(p_register_only, p_ignore_settings, false, p_include_disabled_tools);
	}
	MutexLock lock(cache_mutex);
	const CacheEntry &cache = _get_entry(p_register_only, p_ignore_settings, false, p_include_disabled_tools);
	if (cache.by_category.has(p_category)) {
		return cache.by_category[p_category];
	}
	return Array();
}

#endif
