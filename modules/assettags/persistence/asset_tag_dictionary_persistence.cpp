/**************************************************************************/
/*  asset_tag_dictionary_persistence.cpp                                  */
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

#include "asset_tag_dictionary_persistence.h"

#include "../asset_tag_serializer.h"
#include "asset_tag_file_io.h"

#include "core/io/file_access.h"
#include "core/io/json.h"

bool AssetTagDictionaryPersistence::load_from_text(const String &p_text, HashMap<String, AssetTagEntry> &r_tags, Vector<AssetTagRedirect> &r_redirects) {
	r_tags.clear();
	r_redirects.clear();

	Variant parsed = JSON::parse_string(p_text);
	if (parsed.get_type() != Variant::DICTIONARY) {
		return false;
	}

	const Dictionary root = parsed;
	if (root.has("tags")) {
		const Dictionary tags_dict = root["tags"];
		const Array keys = tags_dict.keys();
		for (int i = 0; i < keys.size(); i++) {
			const String tag_name = keys[i];
			AssetTagEntry entry;
			if (tags_dict[tag_name].get_type() == Variant::DICTIONARY) {
				const Dictionary info = tags_dict[tag_name];
				entry.comment = info.get("comment", "");
				entry.source = info.get("source", "default");
			}
			r_tags[tag_name] = entry;
		}
	}

	if (root.has("redirects")) {
		const Array redirects = root["redirects"];
		for (int i = 0; i < redirects.size(); i++) {
			if (redirects[i].get_type() != Variant::DICTIONARY) {
				continue;
			}
			const Dictionary redirect = redirects[i];
			AssetTagRedirect r;
			r.old_name = redirect.get("old", "");
			r.new_name = redirect.get("new", "");
			if (!r.old_name.is_empty() && !r.new_name.is_empty()) {
				r_redirects.push_back(r);
			}
		}
	}

	return true;
}

bool AssetTagDictionaryPersistence::load_from_path(const String &p_path, HashMap<String, AssetTagEntry> &r_tags, Vector<AssetTagRedirect> &r_redirects) {
	if (!FileAccess::exists(p_path)) {
		return true;
	}

	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	if (file.is_null()) {
		return false;
	}

	const String file_text = file->get_as_text();
	file.unref();
	return load_from_text(file_text, r_tags, r_redirects);
}

bool AssetTagDictionaryPersistence::write_to_path(const String &p_path, const HashMap<String, AssetTagEntry> &p_tags, const Vector<AssetTagRedirect> &p_redirects) {
	return AssetTagFileIO::atomic_write_text_file(p_path, AssetTagSerializer::dictionary_to_json(p_tags, p_redirects));
}
