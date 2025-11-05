/**************************************************************************/
/*  kick_categories_requests.cpp                                          */
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

#include "kick_categories_requests.h"

void KickCategoriesRequests::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_categories", "search_query", "page"), &KickCategoriesRequests::get_categories, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("get_category", "category_id"), &KickCategoriesRequests::get_category);
}

void KickCategoriesRequests::get_categories(const String &p_search_query, int p_page) {
	ERR_FAIL_COND_MSG(p_search_query.is_empty(), "search_query cannot be empty");

	Dictionary params;
	params["q"] = p_search_query;
	if (p_page > 1) {
		params["page"] = p_page;
	}

	_request_get("categories_received", "/categories", params);
}

void KickCategoriesRequests::get_category(int p_category_id) {
	ERR_FAIL_COND_MSG(p_category_id <= 0, "category_id must be positive");

	String path = "/categories/" + itos(p_category_id);
	_request_get("category_received", path);
}
