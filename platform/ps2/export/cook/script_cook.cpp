/**************************************************************************/
/*  script_cook.cpp                                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#include "script_cook.h"

#include "cooker.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"

namespace ps2_cook {

static String strip_line_comments(const String &p_source) {
	PackedStringArray lines = p_source.split("\n");
	String out;
	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i];
		const int gd = line.find("#");
		const int lua = line.find("--");
		int cut = -1;
		if (gd >= 0 && lua >= 0) {
			cut = MIN(gd, lua);
		} else if (gd >= 0) {
			cut = gd;
		} else if (lua >= 0) {
			cut = lua;
		}
		if (cut >= 0) {
			line = line.substr(0, cut);
		}
		out += line + "\n";
	}
	return out;
}

bool source_is_rejected(const String &p_source, String &r_why) {
	const String src = strip_line_comments(p_source);
	const char *bad[] = {
		"eval(", "ClassDB.", "HTTPClient", "Thread.", "await ",
		"OS.execute", "preload(", "JSON.parse"
	};
	for (const char *b : bad) {
		if (src.contains(b)) {
			r_why = b;
			return true;
		}
	}
	int p = 0;
	while (true) {
		const int hit = src.find("load(", p);
		if (hit < 0) {
			break;
		}
		p = hit + 5;
		if (hit > 0) {
			const char32_t prev = src[hit - 1];
			if ((prev >= 'a' && prev <= 'z') || (prev >= 'A' && prev <= 'Z') || (prev >= '0' && prev <= '9') || prev == '_') {
				continue;
			}
		}
		const int q = src.find("\"", hit);
		if (q < 0) {
			r_why = "load(";
			return true;
		}
		const int e = src.find("\"", q + 1);
		if (e <= q + 1) {
			r_why = "load(";
			return true;
		}
		const String path = src.substr(q + 1, e - q - 1);
		if (!path.ends_with(".tscn")) {
			r_why = "load(" + path + ")";
			return true;
		}
	}
	if (src.contains("FileAccess") && !src.contains("user://")) {
		r_why = "FileAccess (user:// only)";
		return true;
	}
	return false;
}

void collect_literal_scenes(const String &p_source, Vector<String> &r_paths) {
	const String src = strip_line_comments(p_source);
	int p = 0;
	while (true) {
		const int q = src.find("res://", p);
		if (q < 0) {
			break;
		}
		int e = q + 6;
		while (e < src.length()) {
			const char32_t c = src[e];
			if (c == '"' || c == '\'' || c == ')' || c == ' ' || c == '\n') {
				break;
			}
			e++;
		}
		const String path = src.substr(q, e - q);
		if (path.ends_with(".tscn") && !r_paths.has(path)) {
			r_paths.push_back(path);
		}
		p = e;
	}
}

static bool skip_script_dir(const String &p_name) {
	return p_name == "tests" || p_name == "tools" || p_name == ".godot" || p_name == "export";
}

static void walk_scripts(const String &p_dir, ScriptCook &r_out) {
	Ref<DirAccess> da = DirAccess::open(p_dir);
	if (da.is_null()) {
		return;
	}
	da->list_dir_begin();
	for (String n = da->get_next(); !n.is_empty(); n = da->get_next()) {
		if (n == "." || n == ".." || n.begins_with(".")) {
			continue;
		}
		const String path = p_dir.path_join(n);
		if (da->current_is_dir()) {
			if (!skip_script_dir(n)) {
				walk_scripts(path, r_out);
			}
			continue;
		}
		if (!(n.ends_with(".gd") || n.ends_with(".luau"))) {
			continue;
		}
		if (n == "run_tests.gd") {
			continue;
		}
		const String src = FileAccess::get_file_as_string(path);
		String why;
		if (source_is_rejected(src, why)) {
			r_out.rejected.push_back(path + " (" + why + ")");
			continue;
		}
		r_out.accepted.push_back(path);
		collect_literal_scenes(src, r_out.extra_scenes);
	}
}

static void append_u16(Vector<uint8_t> &o, uint16_t v) {
	o.push_back(uint8_t(v & 0xff));
	o.push_back(uint8_t(v >> 8));
}

static void append_u32(Vector<uint8_t> &o, uint32_t v) {
	append_u16(o, uint16_t(v & 0xffff));
	append_u16(o, uint16_t(v >> 16));
}

Error cook_scripts(const String &p_export_dir, ScriptCook &r_out) {
	walk_scripts("res://", r_out);
	if (!r_out.rejected.is_empty()) {
		return ERR_UNCONFIGURED;
	}
	Vector<uint8_t> out;
	out.push_back('S');
	out.push_back('C');
	out.push_back('R');
	out.push_back('P');
	append_u16(out, uint16_t(kCookABI));
	append_u16(out, 0);
	append_u32(out, uint32_t(r_out.accepted.size()));
	for (int i = 0; i < r_out.accepted.size(); i++) {
		const CharString utf = r_out.accepted[i].utf8();
		append_u16(out, uint16_t(utf.length()));
		for (int b = 0; b < utf.length(); b++) {
			out.push_back(uint8_t(utf[b]));
		}
	}
	r_out.scrp = out;
	r_out.ir_text = "PS2 script IR stub (P3). Allowlisted sources:\n";
	for (int i = 0; i < r_out.accepted.size(); i++) {
		r_out.ir_text += r_out.accepted[i] + "\n";
	}
	(void)p_export_dir;
	return OK;
}

} // namespace ps2_cook
