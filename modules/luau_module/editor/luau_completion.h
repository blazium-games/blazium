/**************************************************************************/
/*  luau_completion.h                                                     */
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

#include "core/string/char_utils.h"
#include "core/string/ustring.h"

struct LuauCompletionContext {
	String prefix;
	String base;
	bool wants_member = false;
	bool wants_method = false;
};

class LuauCompletionHelper {
public:
	static LuauCompletionContext extract_context(const String &p_source, int p_line, int p_col) {
		LuauCompletionContext ctx;
		const PackedStringArray lines = p_source.split("\n");
		if (p_line < 0 || p_line >= lines.size()) {
			return ctx;
		}

		const String line = lines[p_line];
		if (p_col < 0 || p_col > line.length()) {
			return ctx;
		}

		int start = p_col;
		while (start > 0 && (is_ascii_alphanumeric_char(line[start - 1]) || line[start - 1] == '_')) {
			start--;
		}

		int end = p_col;
		while (end < line.length() && (is_ascii_alphanumeric_char(line[end]) || line[end] == '_')) {
			end++;
		}

		ctx.prefix = line.substr(start, end - start);

		int dot = start - 1;
		while (dot >= 0 && (is_ascii_alphanumeric_char(line[dot]) || line[dot] == '_')) {
			dot--;
		}
		if (dot >= 0 && line[dot] == '.') {
			ctx.wants_member = true;
			int base_end = dot;
			int base_start = dot;
			while (base_start > 0 && (is_ascii_alphanumeric_char(line[base_start - 1]) || line[base_start - 1] == '_')) {
				base_start--;
			}
			ctx.base = line.substr(base_start, base_end - base_start);
		}

		if (line.find("function") >= 0 && line.find(":") >= 0) {
			ctx.wants_method = true;
		}

		return ctx;
	}

	static String normalize_completion_source(const String &p_code, int &r_line, int &r_col) {
		const int caret = p_code.find_char(0xFFFF);
		if (caret < 0) {
			const PackedStringArray lines = p_code.split("\n");
			r_line = lines.is_empty() ? 0 : lines.size() - 1;
			r_col = lines.is_empty() ? 0 : lines[lines.size() - 1].length();
			return p_code;
		}

		const String before = p_code.substr(0, caret);
		const String after = p_code.substr(caret + 1);
		r_line = before.count("\n");
		const int last_nl = before.rfind("\n");
		r_col = last_nl < 0 ? caret : caret - last_nl - 1;
		return before + after;
	}

	static bool matches_prefix(const String &p_candidate, const String &p_prefix) {
		if (p_prefix.is_empty()) {
			return true;
		}
		return p_candidate.begins_with(p_prefix);
	}
};
