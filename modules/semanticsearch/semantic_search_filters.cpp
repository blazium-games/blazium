/**************************************************************************/
/*  semantic_search_filters.cpp                                           */
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

#include "semantic_search_filters.h"

#include "modules/modules_enabled.gen.h"
#include "semantic_asset_index.h"

#ifdef MODULE_ASSETTAGS_ENABLED
#include "modules/assettags/asset_tag_registry.h"
#endif
#include "modules/regex/regex.h"

HashSet<String> SemanticSearchFilters::intersect_path_sets(const HashSet<String> &p_left, const HashSet<String> &p_right) {
	if (p_left.is_empty() || p_right.is_empty()) {
		return HashSet<String>();
	}
	HashSet<String> intersection;
	for (const String &path : p_left) {
		if (p_right.has(path)) {
			intersection.insert(path);
		}
	}
	return intersection;
}

HashSet<String> SemanticSearchFilters::collect_paths_matching_metadata_snapshot(
		const HashMap<String, SemanticEntryMetadata> &p_metadata,
		const String &p_path_regex,
		const String &p_class_filter,
		String &r_filter_error) {
	HashSet<String> result;
	r_filter_error = String();
	Ref<RegEx> path_re;
	if (!p_path_regex.is_empty()) {
		path_re.instantiate();
		if (path_re->compile(p_path_regex) != OK) {
			r_filter_error = "Invalid path_regex pattern.";
			return result;
		}
	}
	for (const KeyValue<String, SemanticEntryMetadata> &kv : p_metadata) {
		const String path = kv.key;
		if (!p_path_regex.is_empty()) {
			const String relative = path.replace("res://", "");
			if (path_re->search(relative).is_null()) {
				continue;
			}
		}
		if (!p_class_filter.is_empty() && kv.value.asset_class != p_class_filter) {
			continue;
		}
		result.insert(path);
	}
	return result;
}

SemanticFilterSnapshot SemanticSearchFilters::build_filter_snapshot(
		const HashMap<String, SemanticEntryMetadata> &p_metadata,
		const PackedStringArray &p_tags,
		bool p_require_all,
		const String &p_path_regex,
		const String &p_class_filter,
		const HashSet<String> *p_tag_allowed_paths,
		uint64_t p_mutation_generation) {
	SemanticFilterSnapshot snapshot;
	snapshot.has_tag_filter = p_tags.size() > 0;
	snapshot.has_metadata_filter = !p_path_regex.is_empty() || !p_class_filter.is_empty();
	snapshot.metadata = p_metadata;
	snapshot.mutation_generation = p_mutation_generation;

	const HashSet<String> metadata_filtered = collect_paths_matching_metadata_snapshot(
			p_metadata, p_path_regex, p_class_filter, snapshot.filter_error);
	if (!snapshot.filter_error.is_empty()) {
		return snapshot;
	}
	if (snapshot.has_metadata_filter && metadata_filtered.is_empty()) {
		return snapshot;
	}

#ifdef MODULE_ASSETTAGS_ENABLED
	if (snapshot.has_tag_filter) {
		if (p_tag_allowed_paths) {
			snapshot.allowed_paths = *p_tag_allowed_paths;
			if (!metadata_filtered.is_empty()) {
				snapshot.allowed_paths = intersect_path_sets(snapshot.allowed_paths, metadata_filtered);
			}
		} else if (AssetTagRegistry *registry = AssetTagRegistry::get_singleton()) {
			snapshot.allowed_paths = registry->collect_paths_for_tag_filter(p_tags, p_require_all);
			if (!metadata_filtered.is_empty()) {
				snapshot.allowed_paths = intersect_path_sets(snapshot.allowed_paths, metadata_filtered);
			}
		}
	} else if (!metadata_filtered.is_empty()) {
		snapshot.allowed_paths = metadata_filtered;
	}
#else
	(void)p_require_all;
	(void)p_tags;
	if (!metadata_filtered.is_empty()) {
		snapshot.allowed_paths = metadata_filtered;
	}
#endif
	return snapshot;
}

HashSet<String> SemanticSearchFilters::resolve_allowed_paths_from_metadata(
		const HashMap<String, SemanticEntryMetadata> &p_metadata,
		const PackedStringArray &p_tags,
		bool p_require_all,
		const String &p_path_regex,
		const String &p_class_filter,
		String &r_filter_error) {
	HashSet<String> allowed_paths;
	r_filter_error = String();
	const HashSet<String> metadata_filtered = collect_paths_matching_metadata_snapshot(
			p_metadata, p_path_regex, p_class_filter, r_filter_error);
	if (!r_filter_error.is_empty()) {
		return allowed_paths;
	}
	if ((!p_path_regex.is_empty() || !p_class_filter.is_empty()) && metadata_filtered.is_empty()) {
		return allowed_paths;
	}
#ifdef MODULE_ASSETTAGS_ENABLED
	if (p_tags.size() > 0) {
		if (AssetTagRegistry *registry = AssetTagRegistry::get_singleton()) {
			allowed_paths = registry->collect_paths_for_tag_filter(p_tags, p_require_all);
			if (!metadata_filtered.is_empty()) {
				allowed_paths = intersect_path_sets(allowed_paths, metadata_filtered);
			}
		}
	} else if (!metadata_filtered.is_empty()) {
		allowed_paths = metadata_filtered;
	}
#else
	(void)p_require_all;
	(void)p_tags;
	if (!metadata_filtered.is_empty()) {
		allowed_paths = metadata_filtered;
	}
#endif
	return allowed_paths;
}

HashSet<String> SemanticSearchFilters::resolve_allowed_paths(
		const SemanticAssetIndex *p_index,
		const PackedStringArray &p_tags,
		bool p_require_all,
		const String &p_path_regex,
		const String &p_class_filter,
		String &r_filter_error) {
	if (!p_index) {
		r_filter_error = String();
		return HashSet<String>();
	}
	const SemanticFilterSnapshot snapshot = p_index->build_filter_snapshot(p_tags, p_require_all, p_path_regex, p_class_filter);
	r_filter_error = snapshot.filter_error;
	return snapshot.allowed_paths;
}
