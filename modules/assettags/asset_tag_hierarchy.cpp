/**************************************************************************/
/*  asset_tag_hierarchy.cpp                                               */
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

#include "asset_tag_hierarchy.h"

#include "core/templates/hash_set.h"

bool AssetTagHierarchy::matches_tag(const String &p_tag, const String &p_query_tag) {
	if (p_tag == p_query_tag) {
		return true;
	}
	return p_tag.begins_with(p_query_tag + ".");
}

String AssetTagHierarchy::remap_tag_for_rename(const String &p_tag, const String &p_old_name, const String &p_new_name) {
	if (p_tag == p_old_name) {
		return p_new_name;
	}
	if (p_tag.begins_with(p_old_name + ".")) {
		return p_new_name + p_tag.substr(p_old_name.length());
	}
	return p_tag;
}

bool AssetTagHierarchy::tag_matches_prefix(const String &p_tag, const String &p_prefix) {
	return p_tag == p_prefix || p_tag.begins_with(p_prefix + ".");
}

Vector<String> AssetTagHierarchy::dedup_tags(const Vector<String> &p_tags) {
	Vector<String> deduped;
	HashSet<String> seen;
	for (int i = 0; i < p_tags.size(); i++) {
		if (!seen.has(p_tags[i])) {
			seen.insert(p_tags[i]);
			deduped.push_back(p_tags[i]);
		}
	}
	return deduped;
}
