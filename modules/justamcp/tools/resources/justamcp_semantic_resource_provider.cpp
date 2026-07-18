/**************************************************************************/
/*  justamcp_semantic_resource_provider.cpp                               */
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

#include "justamcp_semantic_resource_provider.h"

#include "core/io/json.h"
#include "modules/modules_enabled.gen.h"

#ifdef MODULE_SEMANTICSEARCH_ENABLED
#include "modules/semanticsearch/semantic_asset_index.h"
#include "modules/semanticsearch/semantic_search_backend.h"
#include "modules/semanticsearch/semantic_search_backend_factory.h"
#endif

static Dictionary _semantic_json_contents(const String &p_uri, const Dictionary &p_payload) {
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

static Dictionary _semantic_json_error(const String &p_uri, const String &p_error) {
	Dictionary result;
	result["ok"] = false;
	result["error_code"] = -32602;
	result["error"] = p_error;
	result["uri"] = p_uri;
	return result;
}

bool JustAMCPSemanticResourceProvider::can_read(const String &p_canonical_uri) {
#ifdef MODULE_SEMANTICSEARCH_ENABLED
	return p_canonical_uri.begins_with("blazium://semantic/");
#else
	(void)p_canonical_uri;
	return false;
#endif
}

Dictionary JustAMCPSemanticResourceProvider::read(const String &p_uri, const String &p_canonical_uri) {
#ifdef MODULE_SEMANTICSEARCH_ENABLED
	if (p_canonical_uri == "blazium://semantic/index/rebuild") {
		return _semantic_json_error(p_uri, "Use blazium_semantic_rebuild_index tool instead of resources/read for index rebuild.");
	}

	if (p_canonical_uri.begins_with("blazium://semantic/search/")) {
		return _semantic_json_error(p_uri, "Use blazium_semantic_search tool instead of resources/read for search queries.");
	}

	if (p_canonical_uri.begins_with("blazium://semantic/similar/")) {
		return _semantic_json_error(p_uri, "Use blazium_semantic_search tool instead of resources/read for similarity queries.");
	}

	if (p_canonical_uri == "blazium://semantic/index/stats") {
		Ref<SemanticSearchBackend> backend = SemanticSearchBackendFactory::create_active_backend();
		if (backend.is_null()) {
			return _semantic_json_error(p_uri, "SemanticSearchBackend unavailable.");
		}
		Dictionary stats = backend->get_stats();
		stats["backend"] = SemanticSearchBackendFactory::get_effective_backend_name();
		return _semantic_json_contents(p_uri, stats);
	}

	if (p_canonical_uri.begins_with("blazium://semantic/asset/")) {
		String path = p_canonical_uri.substr(String("blazium://semantic/asset/").length()).uri_decode();
		if (!path.begins_with("res://")) {
			path = "res://" + path;
		}
		SemanticAssetIndex *index = SemanticAssetIndex::get_singleton();
		if (!index) {
			return _semantic_json_error(p_uri, "SemanticAssetIndex unavailable.");
		}
		return _semantic_json_contents(p_uri, index->get_asset_entry(path));
	}
#else
	(void)p_uri;
	(void)p_canonical_uri;
#endif
	return _semantic_json_error(p_uri, "Semantic search module is not enabled");
}

#endif
