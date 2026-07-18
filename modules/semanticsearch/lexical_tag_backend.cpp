/**************************************************************************/
/*  lexical_tag_backend.cpp                                               */
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

#include "lexical_tag_backend.h"

#include "semantic_asset_index.h"

void LexicalTagBackend::_bind_methods() {
	ClassDB::bind_method(D_METHOD("rebuild"), &LexicalTagBackend::rebuild);
	ClassDB::bind_method(D_METHOD("search", "query", "limit"), &LexicalTagBackend::search, DEFVAL(20));
	ClassDB::bind_method(D_METHOD("find_similar", "path", "limit"), &LexicalTagBackend::find_similar, DEFVAL(10));
	ClassDB::bind_method(D_METHOD("get_stats"), &LexicalTagBackend::get_stats);
}

Error LexicalTagBackend::rebuild() {
	SemanticAssetIndex *index = SemanticAssetIndex::get_singleton();
	if (!index) {
		return ERR_UNCONFIGURED;
	}
	return index->rebuild_index();
}

Array LexicalTagBackend::search(const String &p_query, int p_limit) const {
	SemanticAssetIndex *index = SemanticAssetIndex::get_singleton();
	if (!index) {
		return Array();
	}
	return index->search(p_query, p_limit);
}

Array LexicalTagBackend::search_filtered(const String &p_query, int p_limit, const PackedStringArray &p_tags, bool p_require_all) const {
	SemanticAssetIndex *index = SemanticAssetIndex::get_singleton();
	if (!index) {
		return Array();
	}
	return index->search_filtered(p_query, p_limit, p_tags, p_require_all);
}

Array LexicalTagBackend::search_with_filters(const String &p_query, int p_limit, const PackedStringArray &p_tags, bool p_require_all, const String &p_path_regex, const String &p_class_filter) const {
	SemanticAssetIndex *index = SemanticAssetIndex::get_singleton();
	if (!index) {
		return Array();
	}
	return index->search_with_filters(p_query, p_limit, p_tags, p_require_all, p_path_regex, p_class_filter);
}

Array LexicalTagBackend::find_similar(const String &p_path, int p_limit) const {
	SemanticAssetIndex *index = SemanticAssetIndex::get_singleton();
	if (!index) {
		return Array();
	}
	return index->find_similar(p_path, p_limit);
}

Array LexicalTagBackend::find_similar_with_filters(const String &p_path, int p_limit, const String &p_path_regex, const String &p_class_filter) const {
	SemanticAssetIndex *index = SemanticAssetIndex::get_singleton();
	if (!index) {
		return Array();
	}
	return index->find_similar_with_filters(p_path, p_limit, p_path_regex, p_class_filter);
}

Dictionary LexicalTagBackend::get_stats() const {
	SemanticAssetIndex *index = SemanticAssetIndex::get_singleton();
	if (!index) {
		return Dictionary();
	}
	return index->get_stats();
}
