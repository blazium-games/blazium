/**************************************************************************/
/*  asset_tag_registry_reverse_lookup.cpp                                 */
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

#include "asset_tag_hierarchy.h"
#include "asset_tag_manager.h"
#include "asset_tag_runtime.h"

void AssetTagRegistry::_rebuild_reverse_lookup() {
	reverse_lookup.clear();
	for (const KeyValue<String, Vector<String>> &kv : asset_index) {
		for (int i = 0; i < kv.value.size(); i++) {
			_insert_tag_into_reverse_lookup(kv.value[i], kv.key);
		}
	}
}

void AssetTagRegistry::_insert_tag_into_reverse_lookup(const String &p_tag, const String &p_path) {
	AssetTagManager *manager = AssetTagManager::get_singleton();
	String tag = p_tag;
	while (true) {
		reverse_lookup[tag].insert(p_path);
		if (manager) {
			const String resolved = manager->resolve_tag_alias(tag);
			if (resolved != tag) {
				reverse_lookup[resolved].insert(p_path);
			}
		}
		if (!tag.contains(".")) {
			break;
		}
		tag = tag.substr(0, tag.rfind("."));
	}
}

void AssetTagRegistry::_erase_tag_from_reverse_lookup(const String &p_tag, const String &p_path) {
	AssetTagManager *manager = AssetTagManager::get_singleton();
	String tag = p_tag;
	while (true) {
		if (reverse_lookup.has(tag)) {
			reverse_lookup[tag].erase(p_path);
			if (reverse_lookup[tag].is_empty()) {
				reverse_lookup.erase(tag);
			}
		}
		if (manager) {
			const String resolved = manager->resolve_tag_alias(tag);
			if (resolved != tag && reverse_lookup.has(resolved)) {
				reverse_lookup[resolved].erase(p_path);
				if (reverse_lookup[resolved].is_empty()) {
					reverse_lookup.erase(resolved);
				}
			}
		}
		if (!tag.contains(".")) {
			break;
		}
		tag = tag.substr(0, tag.rfind("."));
	}
}

void AssetTagRegistry::_remove_path_from_reverse_lookup(const String &p_path, const Vector<String> &p_tags) {
	for (int i = 0; i < p_tags.size(); i++) {
		_erase_tag_from_reverse_lookup(p_tags[i], p_path);
	}
}

void AssetTagRegistry::_update_reverse_lookup_for_path(const String &p_path, const Vector<String> &p_old_tags, const Vector<String> &p_new_tags) {
	for (int i = 0; i < p_old_tags.size(); i++) {
		_erase_tag_from_reverse_lookup(p_old_tags[i], p_path);
	}
	for (int i = 0; i < p_new_tags.size(); i++) {
		_insert_tag_into_reverse_lookup(p_new_tags[i], p_path);
	}
}

void AssetTagRegistry::_on_redirects_changed() {
	_rebuild_reverse_lookup();
	AssetTagRuntime::invalidate_cache();
}

Vector<String> AssetTagRegistry::_dedup_tags(const Vector<String> &p_tags) {
	return AssetTagHierarchy::dedup_tags(p_tags);
}
