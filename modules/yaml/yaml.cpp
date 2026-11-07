/**************************************************************************/
/*  yaml.cpp                                                              */
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

#include "yaml.h"
#include "core/error/error_macros.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "core/string/ustring.h"

#include <csetjmp>

#define RYML_SINGLE_HDR_DEFINE_NOW
#include "ryml_all.hpp"

// ---------- Error handling ----------
static thread_local bool s_ryml_error = false;
static thread_local String s_ryml_error_msg;
static thread_local std::jmp_buf s_ryml_jmp_buf;

// New-style error handlers (rapidyaml ≥ 0.11)
static void ryml_error_basic(c4::csubstr msg, const ryml::ErrorDataBasic &errdata, void *) {
	s_ryml_error = true;
	s_ryml_error_msg = String::utf8(msg.str, msg.len);
	if (errdata.location.line != ryml::npos) {
		s_ryml_error_msg += vformat(" (line %d, col %d)",
				(int)errdata.location.line,
				(int)errdata.location.col);
	}
	std::longjmp(s_ryml_jmp_buf, 1); // must never return
}

static void ryml_error_parse(c4::csubstr msg, const ryml::ErrorDataParse &errdata, void *) {
	s_ryml_error = true;
	s_ryml_error_msg = String::utf8(msg.str, msg.len);
	if (errdata.ymlloc.line != ryml::npos) {
		s_ryml_error_msg += vformat(" (line %d, col %d)",
				(int)errdata.ymlloc.line,
				(int)errdata.ymlloc.col);
	}
	std::longjmp(s_ryml_jmp_buf, 1);
}

static void ryml_error_visit(c4::csubstr msg, const ryml::ErrorDataVisit &errdata, void *) {
	s_ryml_error = true;
	s_ryml_error_msg = String::utf8(msg.str, msg.len);
	std::longjmp(s_ryml_jmp_buf, 1);
}

static void ryml_setup_error_handling() {
	static bool initialized = false;
	if (initialized) {
		return;
	}
	initialized = true;

	ryml::Callbacks cb = ryml::get_callbacks();
	cb.m_error_basic = ryml_error_basic;
	cb.m_error_parse = ryml_error_parse;
	cb.m_error_visit = ryml_error_visit;
	ryml::set_callbacks(cb);
}

// ---------- ryml → Variant ----------
static Variant _ryml_to_variant(const ryml::ConstNodeRef &node) {
	if (!node.readable()) {
		return Variant();
	}

	if (node.is_map()) {
		Dictionary dict;
		for (ryml::ConstNodeRef const &child : node.children()) {
			if (!child.has_key()) {
				continue;
			}
			ryml::csubstr k = child.key();
			String key = String::utf8(k.str, k.len);
			dict[key] = _ryml_to_variant(child);
		}
		return dict;
	}

	if (node.is_seq()) {
		Array arr;
		for (ryml::ConstNodeRef const &child : node.children()) {
			arr.push_back(_ryml_to_variant(child));
		}
		return arr;
	}

	if (node.has_val()) {
		ryml::csubstr val = node.val();
		if (val.empty()) {
			return String();
		}
		String s = String::utf8(val.str, val.len);

		if (s.is_valid_int()) {
			return s.to_int();
		}
		if (s.is_valid_float()) {
			return s.to_float();
		}
		if (s == "true" || s == "True" || s == "TRUE") {
			return true;
		}
		if (s == "false" || s == "False" || s == "FALSE") {
			return false;
		}
		if (s == "null" || s == "Null" || s == "NULL" || s == "~") {
			return Variant();
		}
		return s;
	}

	return Variant();
}

// ---------- Variant → ryml ----------
static void _variant_to_ryml(ryml::NodeRef node, const Variant &v) {
	switch (v.get_type()) {
		case Variant::NIL:
			node.save("null");
			break;
		case Variant::BOOL:
			node.save(v.operator bool() ? "true" : "false");
			break;
		case Variant::INT:
			node.save(int64_t(v));
			break;
		case Variant::FLOAT:
			node.save(double(v));
			break;
		case Variant::STRING:
			node.save(String(v).utf8().get_data());
			break;
		case Variant::DICTIONARY: {
			Dictionary d = v;
			node.set_map();
			Array keys = d.keys();
			for (int i = 0; i < keys.size(); i++) {
				String key = keys[i];
				ryml::NodeRef child = node.append_child();
				child.set_key(key.utf8().get_data());
				_variant_to_ryml(child, d[key]);
			}
			break;
		}
		case Variant::ARRAY: {
			Array a = v;
			node.set_seq(); // works on your version
			for (int i = 0; i < a.size(); i++) {
				ryml::NodeRef child = node.append_child();
				_variant_to_ryml(child, a[i]);
			}
			break;
		}
		default:
			node.save(String(v).utf8().get_data());
			break;
	}
}

// ---------- Public API ----------
Variant YAML::parse_string(const String &p_yaml) {
	ryml_setup_error_handling();
	s_ryml_error = false;
	s_ryml_error_msg.clear();

	CharString utf8 = p_yaml.utf8();
	ryml::csubstr src(utf8.get_data(), utf8.length());

	ryml::Tree *tree = nullptr;

	if (setjmp(s_ryml_jmp_buf) == 0) {
		tree = new ryml::Tree(ryml::parse_in_arena(src));

		if (s_ryml_error || !tree->rootref().readable()) {
			delete tree;
			ERR_PRINT("YAML parse error: " + s_ryml_error_msg);
			return Variant();
		}

		Variant result = _ryml_to_variant(tree->rootref());
		delete tree;
		return result;
	} else {
		delete tree; // safe if still nullptr
		ERR_PRINT("YAML parse error: " + s_ryml_error_msg);
		return Variant();
	}
}

String YAML::stringify(const Variant &p_value, int p_indent) {
	ryml::Tree tree;
	ryml::NodeRef root = tree.rootref();
	_variant_to_ryml(root, p_value);

	std::string result = ryml::emitrs_yaml<std::string>(tree);
	return String::utf8(result.data(), result.size());
}

Variant YAML::parse_file(const String &p_path) {
	Error err;
	String content = FileAccess::get_file_as_string(p_path, &err);
	if (err != OK) {
		ERR_PRINT("Failed to open YAML file: " + p_path);
		return Variant();
	}
	return parse_string(content);
}

void YAML::_bind_methods() {
	ryml_setup_error_handling();

	ClassDB::bind_static_method("YAML", D_METHOD("parse_string", "yaml"), &YAML::parse_string);
	ClassDB::bind_static_method("YAML", D_METHOD("stringify", "value", "indent"), &YAML::stringify, DEFVAL(2));
	ClassDB::bind_static_method("YAML", D_METHOD("parse_file", "path"), &YAML::parse_file);
}
