/**************************************************************************/
/*  semantic_search_backend.cpp                                           */
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

#include "semantic_search_backend.h"

void SemanticSearchBackend::_bind_methods() {
}

Array SemanticSearchBackend::search_filtered(const String &p_query, int p_limit, const PackedStringArray &p_tags, bool p_require_all) const {
	return search_with_filters(p_query, p_limit, p_tags, p_require_all, String(), String());
}

Array SemanticSearchBackend::search_with_filters(const String &p_query, int p_limit, const PackedStringArray &p_tags, bool p_require_all, const String &p_path_regex, const String &p_class_filter) const {
	(void)p_tags;
	(void)p_require_all;
	(void)p_path_regex;
	(void)p_class_filter;
	return search(p_query, p_limit);
}

Array SemanticSearchBackend::find_similar_with_filters(const String &p_path, int p_limit, const String &p_path_regex, const String &p_class_filter) const {
	(void)p_path_regex;
	(void)p_class_filter;
	return find_similar(p_path, p_limit);
}
