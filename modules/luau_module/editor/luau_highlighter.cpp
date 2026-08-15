/**************************************************************************/
/*  luau_highlighter.cpp                                                  */
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

#include "editor/luau_highlighter.h"

#include "editor/settings/editor_settings.h"
#include "luau_script_language.h"
#include "scene/gui/text_edit.h"

namespace {

enum Region {
	REGION_NONE = 0,
	REGION_STRING,
	REGION_LONG_STRING,
	REGION_LONG_COMMENT,
};

static bool is_identifier_char(char32_t p_c) {
	return p_c == '_' || (p_c >= 'a' && p_c <= 'z') || (p_c >= 'A' && p_c <= 'Z') || (p_c >= '0' && p_c <= '9');
}

static bool is_keyword(const String &p_word) {
	static const char *keywords[] = {
		"and",
		"break",
		"do",
		"else",
		"elseif",
		"end",
		"false",
		"for",
		"function",
		"if",
		"in",
		"local",
		"nil",
		"not",
		"or",
		"repeat",
		"return",
		"then",
		"true",
		"until",
		"while",
		"continue",
		"type",
		"export",
		"await",
		"wait",
		"wait_signal",
		"gdclass",
		"super",
		nullptr,
	};
	for (const char **word = keywords; *word; word++) {
		if (p_word == *word) {
			return true;
		}
	}
	return false;
}

static void set_color(Dictionary &p_colors, int p_index, const Color &p_color) {
	Dictionary info;
	info["color"] = p_color;
	p_colors[p_index] = info;
}

} //namespace

void LuauSyntaxHighlighter::_update_cache() {
	keyword_color = EDITOR_GET("text_editor/theme/highlighting/keyword_color");
	string_color = EDITOR_GET("text_editor/theme/highlighting/string_color");
	number_color = EDITOR_GET("text_editor/theme/highlighting/number_color");
	comment_color = EDITOR_GET("text_editor/theme/highlighting/comment_color");
	annotation_color = EDITOR_GET("text_editor/theme/highlighting/gdscript/annotation_color");
	function_color = EDITOR_GET("text_editor/theme/highlighting/function_color");
	member_color = EDITOR_GET("text_editor/theme/highlighting/member_variable_color");
	type_color = EDITOR_GET("text_editor/theme/highlighting/base_type_color");
}

void LuauSyntaxHighlighter::_clear_highlighting_cache() {
	region_cache.clear();
}

Dictionary LuauSyntaxHighlighter::_get_line_syntax_highlighting_impl(int p_line) {
	Dictionary colors;
	TextEdit *edit = get_text_edit();
	if (!edit) {
		return colors;
	}

	static const char *type_names[] = {
		"Vector2",
		"Vector3",
		"Vector4",
		"Color",
		"Node",
		"Object",
		"String",
		"bool",
		"number",
		"table",
		nullptr,
	};

	auto is_type_name = [&](const String &p_word) -> bool {
		for (const char **type_name = type_names; *type_name; type_name++) {
			if (p_word == *type_name) {
				return true;
			}
		}
		return false;
	};

	const String &line = edit->get_line_with_ime(p_line);
	const int length = line.length();
	int region = REGION_NONE;
	if (p_line > 0 && region_cache.has(p_line - 1)) {
		region = region_cache[p_line - 1];
	}

	for (int i = 0; i < length;) {
		if (region == REGION_LONG_COMMENT) {
			set_color(colors, i, comment_color);
			if (line.find("--]]", i) == i) {
				i += 4;
				region = REGION_NONE;
				continue;
			}
			i++;
			continue;
		}

		if (region == REGION_LONG_STRING) {
			set_color(colors, i, string_color);
			if (line.find("]]", i) == i) {
				i += 2;
				region = REGION_NONE;
				continue;
			}
			i++;
			continue;
		}

		if (region == REGION_STRING) {
			set_color(colors, i, string_color);
			const char32_t c = line[i];
			if (c == '\\') {
				i += 2;
				continue;
			}
			if (c == '"' || c == '\'') {
				i++;
				region = REGION_NONE;
				continue;
			}
			i++;
			continue;
		}

		if (line[i] == '-' && i + 1 < length && line[i + 1] == '-') {
			if (i + 2 < length && line[i + 2] == '[') {
				const int close = line.find("]]", i + 3);
				if (close < 0) {
					set_color(colors, i, comment_color);
					region = REGION_LONG_COMMENT;
					i = length;
					continue;
				}
			}

			const String suffix = line.substr(i);
			if (suffix.begins_with("---") && suffix.find_char('@') >= 0) {
				set_color(colors, i, annotation_color);
			} else {
				set_color(colors, i, comment_color);
			}
			break;
		}

		if (line[i] == '"' || line[i] == '\'') {
			set_color(colors, i, string_color);
			region = REGION_STRING;
			i++;
			continue;
		}

		if (line[i] == '[' && i + 1 < length && line[i + 1] == '[') {
			set_color(colors, i, string_color);
			const int close = line.find("]]", i + 2);
			if (close < 0) {
				region = REGION_LONG_STRING;
				i = length;
				continue;
			}
			i = close + 2;
			continue;
		}

		if ((line[i] >= '0' && line[i] <= '9') || (line[i] == '.' && i + 1 < length && line[i + 1] >= '0' && line[i + 1] <= '9')) {
			set_color(colors, i, number_color);
			i++;
			while (i < length && ((line[i] >= '0' && line[i] <= '9') || line[i] == '.' || line[i] == 'x' || line[i] == 'X' || line[i] == 'e' || line[i] == 'E' || line[i] == '+' || line[i] == '-')) {
				i++;
			}
			continue;
		}

		if (is_identifier_char(line[i])) {
			const int start = i;
			i++;
			while (i < length && is_identifier_char(line[i])) {
				i++;
			}
			const String word = line.substr(start, i - start);
			if (is_keyword(word)) {
				set_color(colors, start, keyword_color);
			} else if (word.begins_with("@")) {
				set_color(colors, start, annotation_color);
			} else if (is_type_name(word)) {
				set_color(colors, start, type_color);
			} else if (start > 0 && line[start - 1] == '.') {
				set_color(colors, start, member_color);
			} else if (start > 0 && line[start - 1] == ':') {
				set_color(colors, start, function_color);
			} else if (start >= 9 && line.substr(start - 9, 9) == "function ") {
				set_color(colors, start, function_color);
			}
			continue;
		}

		i++;
	}

	region_cache[p_line] = region;
	return colors;
}

String LuauSyntaxHighlighter::_get_name() const {
	return "Luau";
}

PackedStringArray LuauSyntaxHighlighter::_get_supported_languages() const {
	PackedStringArray languages;
	languages.push_back("Luau");
	return languages;
}

Ref<EditorSyntaxHighlighter> LuauSyntaxHighlighter::_create() const {
	Ref<LuauSyntaxHighlighter> highlighter;
	highlighter.instantiate();
	return highlighter;
}

#endif
