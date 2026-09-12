/**************************************************************************/
/*  asset_tag_serializer.cpp                                              */
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

#include "asset_tag_serializer.h"

#include "core/io/json.h"

String AssetTagSerializer::dictionary_to_json(const HashMap<String, AssetTagEntry> &p_tags, const Vector<AssetTagRedirect> &p_redirects) {
	Dictionary root;
	Dictionary tags_dict;
	for (const KeyValue<String, AssetTagEntry> &kv : p_tags) {
		Dictionary info;
		info["comment"] = kv.value.comment;
		info["source"] = kv.value.source;
		tags_dict[kv.key] = info;
	}
	root["tags"] = tags_dict;

	Array redirects;
	for (int i = 0; i < p_redirects.size(); i++) {
		Dictionary redirect;
		redirect["old"] = p_redirects[i].old_name;
		redirect["new"] = p_redirects[i].new_name;
		redirects.push_back(redirect);
	}
	root["redirects"] = redirects;
	return JSON::stringify(root, "\t");
}

String AssetTagSerializer::index_to_json(const HashMap<String, Vector<String>> &p_index) {
	Dictionary root;
	for (const KeyValue<String, Vector<String>> &kv : p_index) {
		Array tags;
		for (int i = 0; i < kv.value.size(); i++) {
			tags.push_back(kv.value[i]);
		}
		root[kv.key] = tags;
	}
	return JSON::stringify(root, "\t");
}
