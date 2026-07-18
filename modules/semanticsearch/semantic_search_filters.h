/**************************************************************************/
/*  semantic_search_filters.h                                             */
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

#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/variant/variant.h"

class SemanticAssetIndex;

struct SemanticEntryMetadata {
	String asset_class;
	String path_segments;
};

struct SemanticFilterSnapshot {
	HashMap<String, SemanticEntryMetadata> metadata;
	HashSet<String> allowed_paths;
	String filter_error;
	bool has_tag_filter = false;
	bool has_metadata_filter = false;
	uint64_t mutation_generation = 0;
};

class SemanticSearchFilters {
public:
	static HashSet<String> intersect_path_sets(const HashSet<String> &p_left, const HashSet<String> &p_right);
	static HashSet<String> collect_paths_matching_metadata_snapshot(
			const HashMap<String, SemanticEntryMetadata> &p_metadata,
			const String &p_path_regex,
			const String &p_class_filter,
			String &r_filter_error);
	static SemanticFilterSnapshot build_filter_snapshot(
			const HashMap<String, SemanticEntryMetadata> &p_metadata,
			const PackedStringArray &p_tags,
			bool p_require_all,
			const String &p_path_regex,
			const String &p_class_filter,
			const HashSet<String> *p_tag_allowed_paths = nullptr,
			uint64_t p_mutation_generation = 0);
	static HashSet<String> resolve_allowed_paths_from_metadata(
			const HashMap<String, SemanticEntryMetadata> &p_metadata,
			const PackedStringArray &p_tags,
			bool p_require_all,
			const String &p_path_regex,
			const String &p_class_filter,
			String &r_filter_error);
	static HashSet<String> resolve_allowed_paths(
			const SemanticAssetIndex *p_index,
			const PackedStringArray &p_tags,
			bool p_require_all,
			const String &p_path_regex,
			const String &p_class_filter,
			String &r_filter_error);
};
