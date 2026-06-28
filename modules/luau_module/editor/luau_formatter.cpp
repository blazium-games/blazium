/**************************************************************************/
/*  luau_formatter.cpp                                                    */
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

#include "editor/luau_formatter.h"

#include "core/io/file_access.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/string/ustring.h"
#include "editor/editor_settings.h"

namespace {

static bool is_word_char(char32_t p_c) {
	return p_c == '_' || (p_c >= 'a' && p_c <= 'z') || (p_c >= 'A' && p_c <= 'Z') || (p_c >= '0' && p_c <= '9');
}

static bool line_starts_block(const String &p_trimmed) {
	return p_trimmed.begins_with("function") || p_trimmed.begins_with("local function") ||
			p_trimmed.begins_with("do") || p_trimmed.begins_with("then") ||
			p_trimmed == "else" || p_trimmed.begins_with("elseif") || p_trimmed.begins_with("repeat") ||
			p_trimmed.begins_with("for") || p_trimmed.begins_with("while") || p_trimmed.begins_with("if") ||
			p_trimmed.ends_with("then") || p_trimmed.ends_with("{");
}

static bool line_ends_block(const String &p_trimmed) {
	return p_trimmed.begins_with("end") || p_trimmed.begins_with("until") ||
			p_trimmed.begins_with("else") || p_trimmed.begins_with("elseif") ||
			p_trimmed.ends_with("}") || p_trimmed == "}";
}

static bool should_preserve_comment_line(const String &p_line) {
	const String trimmed = p_line.strip_edges();
	if (trimmed.begins_with("--!")) {
		return true;
	}
	if (trimmed.begins_with("---") && trimmed.find_char('@') >= 0) {
		return true;
	}
	if (trimmed.begins_with("--") && trimmed.find_char('@') >= 0) {
		return true;
	}
	return false;
}

} //namespace

void LuauFormatter::_bind_methods() {
	ClassDB::bind_static_method("LuauFormatter", D_METHOD("format_source", "source"), &LuauFormatter::format_source);
	ClassDB::bind_static_method("LuauFormatter", D_METHOD("format_with_stylua", "source", "path"), &LuauFormatter::format_with_stylua, DEFVAL(String()));
	ClassDB::bind_static_method("LuauFormatter", D_METHOD("format_selection", "source", "from", "to"), &LuauFormatter::format_selection);
	ClassDB::bind_static_method("LuauFormatter", D_METHOD("minify_source", "source"), &LuauFormatter::minify_source);
	ClassDB::bind_static_method("LuauFormatter", D_METHOD("tree_shake_source", "source"), &LuauFormatter::tree_shake_source);
}

String LuauFormatter::format_range(const String &p_source, int p_from_line, int p_to_line) {
	PackedStringArray lines = p_source.split("\n", false);
	if (lines.is_empty()) {
		return p_source;
	}

	const int from_line = MAX(p_from_line, 0);
	const int to_line = MIN(p_to_line, lines.size() - 1);
	if (from_line > to_line) {
		return p_source;
	}

	int indent = 0;
	for (int i = 0; i < from_line; i++) {
		const String trimmed = String(lines[i]).strip_edges();
		if (line_ends_block(trimmed)) {
			indent = MAX(indent - 1, 0);
		}
		if (line_starts_block(trimmed)) {
			indent++;
		}
	}

	for (int i = from_line; i <= to_line; i++) {
		String trimmed = String(lines[i]).strip_edges();
		if (trimmed.is_empty()) {
			lines.write[i] = "";
			continue;
		}

		if (line_ends_block(trimmed)) {
			indent = MAX(indent - 1, 0);
		}

		String prefix;
		for (int s = 0; s < indent; s++) {
			prefix += "\t";
		}
		lines.write[i] = prefix + trimmed;

		if (line_starts_block(trimmed)) {
			indent++;
		}
	}

	return String("\n").join(lines);
}

String LuauFormatter::format_lines(const String &p_source, int p_from_line, int p_to_line) {
	PackedStringArray original_lines = p_source.split("\n", false);
	if (original_lines.is_empty()) {
		return p_source;
	}

	const int line_to = MIN(p_to_line, original_lines.size() - 1);
	if (p_from_line > line_to) {
		return p_source;
	}

	PackedStringArray formatted_lines = format_range(p_source, p_from_line, line_to).split("\n", false);
	for (int i = p_from_line; i <= line_to && i < original_lines.size() && i - p_from_line < formatted_lines.size(); i++) {
		original_lines.write[i] = formatted_lines[i - p_from_line];
	}
	return String("\n").join(original_lines);
}

String LuauFormatter::format_with_stylua(const String &p_source, const String &p_path) {
#ifdef TOOLS_ENABLED
	String stylua_path = EDITOR_GET("luau/editor/stylua_path");
	if (stylua_path.is_empty()) {
		return String();
	}
	if (!FileAccess::exists(stylua_path)) {
		return String();
	}

	const String temp_path = OS::get_singleton()->get_cache_path().path_join("luau_stylua_format.luau");
	{
		Ref<FileAccess> file = FileAccess::open(temp_path, FileAccess::WRITE);
		if (file.is_null()) {
			return String();
		}
		file->store_string(p_source);
	}

	List<String> args;
	args.push_back(temp_path);

	int exit_code = 0;
	Error err = OS::get_singleton()->execute(stylua_path, args, nullptr, &exit_code, false);
	if (err != OK || exit_code != 0) {
		return String();
	}

	Ref<FileAccess> formatted = FileAccess::open(temp_path, FileAccess::READ);
	if (formatted.is_null()) {
		return String();
	}
	return formatted->get_as_text();
#else
	(void)p_source;
	(void)p_path;
	return String();
#endif
}

String LuauFormatter::format_source(const String &p_source) {
	if (p_source.is_empty()) {
		return p_source;
	}
	const String stylua_result = format_with_stylua(p_source);
	if (!stylua_result.is_empty()) {
		return stylua_result;
	}
	PackedStringArray lines = p_source.split("\n", false);
	return format_range(p_source, 0, lines.is_empty() ? 0 : lines.size() - 1);
}

String LuauFormatter::format_selection(const String &p_source, int p_from, int p_to) {
	if (p_source.is_empty() || p_from < 0 || p_to <= p_from) {
		return p_source;
	}

	const int line_from = p_source.substr(0, p_from).count("\n");
	const int line_to = p_source.substr(0, p_to).count("\n");

	String formatted = format_range(p_source, line_from, line_to);
	PackedStringArray original_lines = p_source.split("\n", false);
	PackedStringArray formatted_lines = formatted.split("\n", false);

	for (int i = line_from; i <= line_to && i < original_lines.size() && i - line_from < formatted_lines.size(); i++) {
		original_lines.write[i] = formatted_lines[i - line_from];
	}

	return String("\n").join(original_lines);
}

String LuauFormatter::minify_source(const String &p_source) {
	PackedStringArray lines = p_source.split("\n", false);
	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i];
		if (should_preserve_comment_line(line)) {
			lines.write[i] = line.strip_edges(false, true);
			continue;
		}

		int comment = -1;
		bool in_string = false;
		char32_t string_quote = 0;
		for (int j = 0; j < line.length(); j++) {
			const char32_t c = line[j];
			if (!in_string && c == '-' && j + 1 < line.length() && line[j + 1] == '-') {
				comment = j;
				break;
			}
			if (!in_string && (c == '"' || c == '\'')) {
				in_string = true;
				string_quote = c;
			} else if (in_string && c == string_quote && (j == 0 || line[j - 1] != '\\')) {
				in_string = false;
			}
		}

		if (comment >= 0) {
			line = line.substr(0, comment).strip_edges(false, true);
		} else {
			line = line.strip_edges(false, true);
		}
		lines.write[i] = line;
	}

	PackedStringArray compact;
	for (int i = 0; i < lines.size(); i++) {
		if (!String(lines[i]).is_empty() || (i > 0 && i + 1 < lines.size() && !String(lines[i + 1]).is_empty())) {
			compact.push_back(lines[i]);
		}
	}

	return String("\n").join(compact);
}

String LuauFormatter::tree_shake_source(const String &p_source) {
	HashSet<String> declared;
	HashSet<String> referenced;

	PackedStringArray lines = p_source.split("\n", false);
	for (int i = 0; i < lines.size(); i++) {
		const String trimmed = String(lines[i]).strip_edges();
		if (trimmed.begins_with("local function ")) {
			const int name_start = String("local function ").length();
			int name_end = name_start;
			while (name_end < trimmed.length() && is_word_char(trimmed[name_end])) {
				name_end++;
			}
			if (name_end > name_start) {
				declared.insert(trimmed.substr(name_start, name_end - name_start));
			}
		}
	}

	const String body = p_source;
	for (const String &name : declared) {
		const String call_pattern = name + "(";
		const String ref_pattern = name;
		if (body.find(call_pattern) >= 0 || body.find(" " + ref_pattern) >= 0 || body.find("\t" + ref_pattern) >= 0) {
			referenced.insert(name);
		}
	}

	PackedStringArray kept;
	for (int i = 0; i < lines.size(); i++) {
		const String trimmed = String(lines[i]).strip_edges();
		bool drop = false;
		if (trimmed.begins_with("local function ")) {
			const int name_start = String("local function ").length();
			int name_end = name_start;
			while (name_end < trimmed.length() && is_word_char(trimmed[name_end])) {
				name_end++;
			}
			if (name_end > name_start) {
				const String name = trimmed.substr(name_start, name_end - name_start);
				if (declared.has(name) && !referenced.has(name)) {
					drop = true;
				}
			}
		}
		if (!drop) {
			kept.push_back(lines[i]);
		}
	}

	return String("\n").join(kept);
}

int LuauFormatter::compute_indent(const String &p_line, int p_current_indent) {
	const String trimmed = p_line.strip_edges();
	if (trimmed.is_empty() || should_preserve_comment_line(p_line)) {
		return p_current_indent;
	}
	if (line_ends_block(trimmed)) {
		return MAX(p_current_indent - 1, 0);
	}
	return p_current_indent;
}

#endif
