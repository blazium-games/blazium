/**************************************************************************/
/*  asset_tag_query.cpp                                                   */
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

#include "asset_tag_query.h"

#include "asset_tag_manager.h"

#include "core/os/thread.h"
#include "modules/regex/regex.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_file_system.h"
#endif

HashSet<String> AssetTagQuery::collect_paths_for_tag_filter(
		const HashMap<String, HashSet<String>> &p_reverse_lookup,
		AssetTagManager *p_manager,
		const PackedStringArray &p_tags,
		bool p_require_all) {
	HashSet<String> result;
	if (p_tags.is_empty()) {
		return result;
	}
	if (p_require_all) {
		bool first = true;
		for (int i = 0; i < p_tags.size(); i++) {
			const String resolved = p_manager ? p_manager->resolve_tag_alias(p_tags[i]) : p_tags[i];
			HashSet<String> tag_paths;
			if (p_reverse_lookup.has(resolved)) {
				for (const String &path : p_reverse_lookup[resolved]) {
					tag_paths.insert(path);
				}
			}
			if (first) {
				result = tag_paths;
				first = false;
			} else {
				HashSet<String> intersection;
				for (const String &path : result) {
					if (tag_paths.has(path)) {
						intersection.insert(path);
					}
				}
				result = intersection;
			}
			if (result.is_empty()) {
				break;
			}
		}
	} else {
		for (int i = 0; i < p_tags.size(); i++) {
			const String resolved = p_manager ? p_manager->resolve_tag_alias(p_tags[i]) : p_tags[i];
			if (p_reverse_lookup.has(resolved)) {
				for (const String &path : p_reverse_lookup[resolved]) {
					result.insert(path);
				}
			}
		}
	}
	return result;
}

Dictionary AssetTagQuery::search_assets(
		const HashMap<String, Vector<String>> &p_asset_index,
		const HashMap<String, HashSet<String>> &p_reverse_lookup,
		AssetTagManager *p_manager,
		const PackedStringArray &p_tags,
		const String &p_type_filter,
		const String &p_path_glob,
		const String &p_path_regex,
		bool p_require_all) {
	Dictionary result;
	Array matches;

	if (p_tags.size() > 0 && !p_manager) {
		result["ok"] = false;
		result["error"] = "AssetTagManager unavailable";
		result["assets"] = matches;
		result["count"] = 0;
		return result;
	}

#ifndef TOOLS_ENABLED
	if (!p_type_filter.is_empty()) {
		result["ok"] = false;
		result["error"] = "type_filter requires editor filesystem metadata";
		result["assets"] = matches;
		result["count"] = 0;
		return result;
	}
#else
	if (!p_type_filter.is_empty() && !EditorFileSystem::get_singleton()) {
		result["ok"] = false;
		result["error"] = "type_filter requires editor filesystem metadata";
		result["assets"] = matches;
		result["count"] = 0;
		return result;
	}
#endif

	HashSet<String> candidate_paths;
	bool has_candidates = false;
	if (p_tags.size() > 0) {
		candidate_paths = collect_paths_for_tag_filter(p_reverse_lookup, p_manager, p_tags, p_require_all);
		has_candidates = !candidate_paths.is_empty();
		if (p_require_all && !has_candidates) {
			candidate_paths.clear();
		}
	}

	Ref<RegEx> path_regex;
	bool use_path_regex = false;
	if (!p_path_regex.is_empty()) {
		path_regex.instantiate();
		if (path_regex->compile(p_path_regex) == OK) {
			use_path_regex = true;
		}
	}

	auto consider_path = [&](const String &p_path, const Vector<String> &p_asset_tags) {
		if (use_path_regex) {
			if (path_regex->search(p_path).is_null()) {
				return;
			}
		} else if (!p_path_glob.is_empty() && !p_path.matchn(p_path_glob)) {
			return;
		}
#ifdef TOOLS_ENABLED
		if (!p_type_filter.is_empty() && EditorFileSystem::get_singleton() && Thread::is_main_thread()) {
			const String file_type = EditorFileSystem::get_singleton()->get_file_type(p_path);
			if (file_type != p_type_filter) {
				return;
			}
		}
#endif
		Dictionary entry;
		entry["path"] = p_path;
		PackedStringArray tags;
		for (int i = 0; i < p_asset_tags.size(); i++) {
			tags.push_back(p_asset_tags[i]);
		}
		entry["tags"] = tags;
		matches.push_back(entry);
	};

	if (p_tags.size() > 0) {
		for (const String &path : candidate_paths) {
			if (!p_asset_index.has(path)) {
				continue;
			}
			consider_path(path, p_asset_index[path]);
		}
	} else {
		for (const KeyValue<String, Vector<String>> &kv : p_asset_index) {
			consider_path(kv.key, kv.value);
		}
	}

	result["ok"] = true;
	result["assets"] = matches;
	result["count"] = matches.size();
	return result;
}
