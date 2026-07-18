/**************************************************************************/
/*  asset_tag_registry_query.cpp                                          */
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

#include "asset_tag_registry.h"

#include "asset_tag_manager.h"
#include "asset_tag_query.h"

PackedStringArray AssetTagRegistry::find_assets_by_tag(const String &p_tag, bool p_match_parent) const {
	PackedStringArray result;
	AssetTagManager *manager = AssetTagManager::get_singleton();
	const String resolved = manager ? manager->resolve_tag_alias(p_tag) : p_tag;
	if (reverse_lookup.has(resolved)) {
		for (const String &path : reverse_lookup[resolved]) {
			if (!p_match_parent) {
				if (!asset_index.has(path)) {
					continue;
				}
				bool exact = false;
				for (int j = 0; j < asset_index[path].size(); j++) {
					const String asset_tag = asset_index[path][j];
					if (asset_tag == resolved || asset_tag == p_tag) {
						exact = true;
						break;
					}
					if (manager && manager->matches_tag(asset_tag, resolved)) {
						exact = true;
						break;
					}
				}
				if (!exact) {
					continue;
				}
			}
			result.push_back(path);
		}
	}

	result.sort();
	return result;
}

Dictionary AssetTagRegistry::search_assets(const PackedStringArray &p_tags, const String &p_type_filter, const String &p_path_glob, const String &p_path_regex, bool p_require_all) const {
	return AssetTagQuery::search_assets(asset_index, reverse_lookup, AssetTagManager::get_singleton(), p_tags, p_type_filter, p_path_glob, p_path_regex, p_require_all);
}

HashSet<String> AssetTagRegistry::collect_paths_for_tag_filter(const PackedStringArray &p_tags, bool p_require_all) const {
	return AssetTagQuery::collect_paths_for_tag_filter(reverse_lookup, AssetTagManager::get_singleton(), p_tags, p_require_all);
}

bool AssetTagRegistry::asset_matches_tag_filter(const String &p_path, const PackedStringArray &p_tag_tokens, bool p_require_all) const {
	if (p_tag_tokens.is_empty()) {
		return true;
	}
	if (!asset_index.has(p_path)) {
		return false;
	}
	AssetTagManager *manager = AssetTagManager::get_singleton();
	if (!manager) {
		return false;
	}
	PackedStringArray asset_tags;
	for (int i = 0; i < asset_index[p_path].size(); i++) {
		asset_tags.push_back(asset_index[p_path][i]);
	}
	if (p_require_all) {
		return manager->container_has_all(asset_tags, p_tag_tokens);
	}
	return manager->container_has_any(asset_tags, p_tag_tokens);
}
