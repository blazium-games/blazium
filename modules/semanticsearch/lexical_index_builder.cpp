/**************************************************************************/
/*  lexical_index_builder.cpp                                             */
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

#include "lexical_index_builder.h"

void LexicalIndexBuilder::index_tokens_for_entry(
		const String &p_path,
		const Vector<String> &p_tokens,
		HashMap<String, HashSet<String>> &r_token_index,
		HashMap<String, HashSet<String>> &r_prefix_index) {
	for (int i = 0; i < p_tokens.size(); i++) {
		r_token_index[p_tokens[i]].insert(p_path);
	}
	for (int i = 0; i < p_tokens.size(); i++) {
		const String token = p_tokens[i];
		if (token.length() < 3) {
			continue;
		}
		const int max_len = MIN(token.length(), PREFIX_MAX_LEN);
		for (int len = 3; len <= max_len; len++) {
			const String prefix = token.substr(0, len);
			r_prefix_index[prefix].insert(p_path);
		}
	}
}

void LexicalIndexBuilder::unindex_tokens_for_entry(
		const String &p_path,
		const Vector<String> &p_tokens,
		HashMap<String, HashSet<String>> &r_token_index,
		HashMap<String, HashSet<String>> &r_prefix_index) {
	for (int i = 0; i < p_tokens.size(); i++) {
		if (r_token_index.has(p_tokens[i])) {
			r_token_index[p_tokens[i]].erase(p_path);
			if (r_token_index[p_tokens[i]].is_empty()) {
				r_token_index.erase(p_tokens[i]);
			}
		}
	}
	for (int i = 0; i < p_tokens.size(); i++) {
		const String token = p_tokens[i];
		if (token.length() < 3) {
			continue;
		}
		const int max_len = MIN(token.length(), PREFIX_MAX_LEN);
		for (int len = 3; len <= max_len; len++) {
			const String prefix = token.substr(0, len);
			if (r_prefix_index.has(prefix)) {
				r_prefix_index[prefix].erase(p_path);
				if (r_prefix_index[prefix].is_empty()) {
					r_prefix_index.erase(prefix);
				}
			}
		}
	}
}
