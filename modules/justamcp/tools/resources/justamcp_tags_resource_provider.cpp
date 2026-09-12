/**************************************************************************/
/*  justamcp_tags_resource_provider.cpp                                   */
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

#include "justamcp_tags_resource_provider.h"

#include "../../justamcp_pagination.h"
#include "core/io/json.h"
#include "modules/modules_enabled.gen.h"

#ifdef MODULE_ASSETTAGS_ENABLED
#include "modules/assettags/asset_tag_manager.h"
#include "modules/assettags/asset_tag_registry.h"
#endif

static Dictionary _tags_json_contents(const String &p_uri, const Dictionary &p_payload) {
	Dictionary result;
	result["ok"] = true;
	Array contents;
	Dictionary content;
	content["uri"] = p_uri;
	content["mimeType"] = "application/json";
	content["text"] = JSON::stringify(p_payload, "\t");
	contents.push_back(content);
	result["contents"] = contents;
	return result;
}

static Dictionary _tags_json_error(const String &p_uri, const String &p_error) {
	Dictionary result;
	result["ok"] = false;
	result["error_code"] = -32602;
	result["error"] = p_error;
	result["uri"] = p_uri;
	return result;
}

static uint32_t g_tags_dictionary_cache_fingerprint = 0;
static Dictionary g_tags_dictionary_cache_payload;

void JustAMCPTagsResourceProvider::invalidate_dictionary_cache() {
	g_tags_dictionary_cache_fingerprint = 0;
	g_tags_dictionary_cache_payload = Dictionary();
}

#ifdef MODULE_ASSETTAGS_ENABLED
static uint32_t _tags_dictionary_fingerprint(AssetTagManager *p_manager) {
	uint32_t hash = 5381;
	const PackedStringArray all_tags = p_manager->list_all_tags();
	hash = hash * 33 + (uint32_t)all_tags.size();
	const int sample = MIN(all_tags.size(), 64);
	for (int i = 0; i < sample; i++) {
		hash = hash * 33 + (uint32_t)all_tags[i].hash();
	}
	if (all_tags.size() > sample) {
		hash = hash * 33 + (uint32_t)all_tags[all_tags.size() - 1].hash();
	}
	return hash;
}
#endif

bool JustAMCPTagsResourceProvider::can_read(const String &p_canonical_uri) {
#ifdef MODULE_ASSETTAGS_ENABLED
	return p_canonical_uri == "blazium://tags/dictionary" ||
			p_canonical_uri.begins_with("blazium://tags/dictionary/cursor/") ||
			p_canonical_uri.begins_with("blazium://tags/asset/") ||
			p_canonical_uri.begins_with("blazium://assets/by-tag/");
#else
	(void)p_canonical_uri;
	return false;
#endif
}

Dictionary JustAMCPTagsResourceProvider::read(const String &p_uri, const String &p_canonical_uri) {
#ifdef MODULE_ASSETTAGS_ENABLED
	if (p_canonical_uri == "blazium://tags/dictionary" || p_canonical_uri.begins_with("blazium://tags/dictionary/cursor/")) {
		AssetTagManager *tag_manager = AssetTagManager::get_singleton();
		if (!tag_manager) {
			return _tags_json_error(p_uri, "AssetTagManager unavailable.");
		}
		String cursor;
		if (p_canonical_uri.begins_with("blazium://tags/dictionary/cursor/")) {
			cursor = justamcp_pagination_cursor_from_uri_suffix(p_canonical_uri.substr(String("blazium://tags/dictionary/cursor/").length()));
		}
		PackedStringArray all_tags = tag_manager->list_all_tags();
		const uint32_t fingerprint = _tags_dictionary_fingerprint(tag_manager);
		if (cursor.is_empty() && fingerprint == g_tags_dictionary_cache_fingerprint && !g_tags_dictionary_cache_payload.is_empty()) {
			return _tags_json_contents(p_uri, g_tags_dictionary_cache_payload);
		}
		Dictionary page = justamcp_pagination_slice_strings(all_tags, cursor, "tag_names");
		Array page_tags = page.get("tag_names", Array());
		Array tag_infos;
		for (int i = 0; i < page_tags.size(); i++) {
			tag_infos.push_back(tag_manager->get_tag_info(String(page_tags[i])));
		}
		Dictionary payload;
		payload["tags"] = tag_infos;
		payload["count"] = tag_infos.size();
		if (page.has("nextCursor")) {
			payload["nextCursor"] = page["nextCursor"];
			payload["nextUri"] = justamcp_pagination_next_uri("blazium://tags/dictionary", String(page["nextCursor"]));
		}
		if (cursor.is_empty()) {
			g_tags_dictionary_cache_fingerprint = fingerprint;
			g_tags_dictionary_cache_payload = payload;
		}
		return _tags_json_contents(p_uri, payload);
	}

	if (p_canonical_uri.begins_with("blazium://tags/asset/")) {
		String path = p_canonical_uri.substr(String("blazium://tags/asset/").length()).uri_decode();
		if (!path.begins_with("res://")) {
			path = "res://" + path;
		}
		AssetTagRegistry *registry = AssetTagRegistry::get_singleton();
		if (!registry) {
			return _tags_json_error(p_uri, "AssetTagRegistry unavailable.");
		}
		Dictionary payload;
		payload["path"] = path;
		payload["tags"] = registry->get_tags_for_asset(path);
		return _tags_json_contents(p_uri, payload);
	}

	if (p_canonical_uri.begins_with("blazium://assets/by-tag/")) {
		String suffix = p_canonical_uri.substr(String("blazium://assets/by-tag/").length());
		String tag;
		String cursor;
		const int slash_cursor = suffix.find("/cursor/");
		if (slash_cursor != -1) {
			tag = suffix.substr(0, slash_cursor).uri_decode();
			cursor = justamcp_pagination_cursor_from_uri_suffix(suffix.substr(slash_cursor + String("/cursor/").length()));
		} else {
			tag = suffix.uri_decode();
		}
		AssetTagRegistry *registry = AssetTagRegistry::get_singleton();
		if (!registry) {
			return _tags_json_error(p_uri, "AssetTagRegistry unavailable.");
		}
		PackedStringArray paths = registry->find_assets_by_tag(tag, true);
		Array path_array;
		for (int i = 0; i < paths.size(); i++) {
			path_array.push_back(paths[i]);
		}
		Dictionary page = justamcp_pagination_slice_array(path_array, cursor, "paths");
		Dictionary payload;
		payload["tag"] = tag;
		payload["paths"] = page.get("paths", Array());
		payload["count"] = Array(payload["paths"]).size();
		if (page.has("nextCursor")) {
			payload["nextCursor"] = page["nextCursor"];
			payload["nextUri"] = justamcp_pagination_next_uri("blazium://assets/by-tag/" + tag, String(page["nextCursor"]));
		}
		return _tags_json_contents(p_uri, payload);
	}
#else
	(void)p_uri;
	(void)p_canonical_uri;
#endif
	return _tags_json_error(p_uri, "Asset tags module is not enabled");
}

#endif
