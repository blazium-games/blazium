/**************************************************************************/
/*  source_map.cpp                                                        */
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

#include "seeds/source_map.h"

#include "seeds/output_names.h"

#include "core/io/dir_access.h"
#include "core/object/class_db.h"
#include "core/templates/hash_set.h"

static bool _is_ident_start(char32_t c) {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static bool _is_ident_cont(char32_t c) {
	return _is_ident_start(c) || (c >= '0' && c <= '9');
}

static bool _is_path_char(char32_t c) {
	return _is_ident_cont(c) || c == '/' || c == '.' || c == '-' || c == '@';
}

static HashSet<String> _reserved_words;
static bool _reserved_ready = false;

static void _ensure_reserved() {
	if (_reserved_ready) {
		return;
	}
	_reserved_ready = true;
	const char *words[] = {
		"if", "elif", "else", "for", "while", "match", "when", "break", "continue", "pass", "return",
		"class", "class_name", "extends", "is", "as", "self", "super", "preload", "load", "assert",
		"await", "signal", "func", "static", "const", "var", "enum", "breakpoint", "and", "or", "not",
		"in", "null", "true", "false", "PI", "TAU", "INF", "NAN", "void", "int", "float", "bool",
		"local", "function", "end", "then", "elseif", "repeat", "until", "do", "nil", "require",
		"gdclass", "export", "ipairs", "pairs", "pcall", "xpcall", "error", "warn", "select", "unpack",
		"bit32", "table", "string", "math", "task", "wait", "spawn", "delay", "typeof", "module",
		"String", "Array", "Dictionary", "PackedStringArray", "PackedByteArray", "PackedInt32Array",
		"PackedFloat32Array", "PackedInt64Array", "PackedFloat64Array", "PackedVector2Array",
		"PackedVector3Array", "PackedColorArray", "Variant", "Object", "RefCounted", "Resource",
		"Node", "Node2D", "Node3D", "Control", "CanvasItem", "Viewport", "SceneTree", "Window",
		"OS", "Engine", "Input", "Time", "DisplayServer", "ProjectSettings", "FileAccess", "DirAccess",
		"JSON", "Image", "ImageTexture", "Texture2D", "TextureRect", "Label", "Button", "ItemList",
		"LineEdit", "TextEdit", "HBoxContainer", "VBoxContainer", "MarginContainer", "ColorRect",
		"AudioStream", "AudioStreamWAV", "RandomNumberGenerator", "Color", "Vector2", "Vector2i",
		"Vector3", "Vector3i", "Vector4", "Rect2", "Rect2i", "Transform2D", "Transform3D", "AABB",
		"Plane", "Quaternion", "Basis", "RID", "Callable", "Signal", "NodePath", "StringName",
		"Error", "OK", "FAILED", "print", "printerr", "print_debug", "print_rich", "print_verbose",
		"push_error", "push_warning", "range", "len", "str", "typeof", "type_exists", "convert",
		"weakref", "emit_signal", "connect", "disconnect", "is_connected", "get_node", "has_node",
		"add_child", "remove_child", "queue_free", "duplicate", "set", "get", "call", "callv",
		"has_method", "is_instance_valid", "new", "instantiate", "take_over_path", "get_tree",
		"get_parent", "get_children", "find_child", "is_inside_tree", "set_text", "get_text",
		"set_anchors_preset", "add_theme_constant_override", "add_theme_font_size_override",
		"create", "fill", "save_png", "hex_encode", "store_string", "get_file_as_string",
		"make_dir_recursive_absolute", "copy_absolute", "dir_exists_absolute", "file_exists",
		"globalize_path", "get_user_data_dir", "path_join", "get_extension", "get_file",
		"get_base_dir", "begins_with", "ends_with", "contains", "replace", "strip_edges",
		"to_lower", "to_upper", "is_empty", "size", "clear", "append", "push_back",
		"_init", "_initialize", "_finalize", "_ready", "_enter_tree", "_exit_tree", "_notification", "_process",
		"_physics_process", "_input", "_unhandled_input", "_unhandled_key_input", "_gui_input",
		"_draw", "_to_string", "_get", "_set", "_get_property_list", "_validate_property",
		"_get_configuration_warnings", "_iter_init", "_iter_next", "_iter_get", "_static_init",
		"_integrate_forces", "_edit_get_state", "_edit_set_state",
		"_CK", "_CI", "_CR", "_obfuscation_seed_salt", "_obfuscation_mix_seed", "_obfuscation_cr_mix",
		"_obfuscation_cref", "source_code", "resource_path", "comment_ref", "parse_script_comments", "hash",
		"Obfuscation", "ObfuscationScan", "ObfuscationClaim", "ObfuscationKey",
		nullptr
	};
	for (int i = 0; words[i]; i++) {
		_reserved_words.insert(String(words[i]));
	}
}

bool ObfuscationSourceMap::is_reserved(const String &p_name) {
	_ensure_reserved();
	if (_reserved_words.has(p_name)) {
		return true;
	}
	if (ClassDB::class_exists(p_name)) {
		return true;
	}
	return false;
}

static int _skip_string(const String &p_src, int p_i) {
	const int n = p_src.length();
	if (p_i >= n) {
		return p_i;
	}
	const char32_t q = p_src[p_i];
	bool triple = false;
	int i = p_i + 1;
	if ((q == '"' || q == '\'') && i + 1 < n && p_src[i] == q && p_src[i + 1] == q) {
		triple = true;
		i += 2;
	}
	while (i < n) {
		if (p_src[i] == '\\' && i + 1 < n) {
			i += 2;
			continue;
		}
		if (triple) {
			if (i + 2 < n && p_src[i] == q && p_src[i + 1] == q && p_src[i + 2] == q) {
				return i + 3;
			}
		} else if (p_src[i] == q) {
			return i + 1;
		} else if (p_src[i] == '\n' && !triple) {
			return i;
		}
		i++;
	}
	return n;
}

static int _skip_luau_long(const String &p_src, int p_i) {
	const int n = p_src.length();
	if (p_i >= n || p_src[p_i] != '[') {
		return p_i;
	}
	int i = p_i + 1;
	int eq = 0;
	while (i < n && p_src[i] == '=') {
		eq++;
		i++;
	}
	if (i >= n || p_src[i] != '[') {
		return p_i + 1;
	}
	i++;
	while (i < n) {
		if (p_src[i] == ']') {
			int j = i + 1;
			int e = 0;
			while (j < n && p_src[j] == '=') {
				e++;
				j++;
			}
			if (e == eq && j < n && p_src[j] == ']') {
				return j + 1;
			}
		}
		i++;
	}
	return n;
}

static int _skip_luau_comment(const String &p_src, int p_i) {
	const int n = p_src.length();
	if (p_i + 1 >= n || p_src[p_i] != '-' || p_src[p_i + 1] != '-') {
		return p_i;
	}
	if (p_i + 3 < n && p_src[p_i + 2] == '[' && (p_src[p_i + 3] == '[' || p_src[p_i + 3] == '=')) {
		return _skip_luau_long(p_src, p_i + 2);
	}
	int i = p_i + 2;
	while (i < n && p_src[i] != '\n') {
		i++;
	}
	return i;
}

bool ObfuscationSourceMap::is_script_ext(const String &p_ext) {
	const String e = p_ext.to_lower();
	return e == "gd" || e == "luau" || e == "lua";
}

ObfuscationSourceMap::ScriptLang ObfuscationSourceMap::script_lang_from_path(const String &p_path) {
	const String e = p_path.get_extension().to_lower();
	if (e == "luau" || e == "lua") {
		return SCRIPT_LUAU;
	}
	return SCRIPT_GDSCRIPT;
}

static String _read_ident(const String &p_src, int &p_i) {
	const int n = p_src.length();
	const int start = p_i;
	p_i++;
	while (p_i < n && _is_ident_cont(p_src[p_i])) {
		p_i++;
	}
	return p_src.substr(start, p_i - start);
}

static void _skip_ws(const String &p_src, int &p_i) {
	const int n = p_src.length();
	while (p_i < n) {
		const char32_t c = p_src[p_i];
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
			p_i++;
			continue;
		}
		if (c == '#') {
			while (p_i < n && p_src[p_i] != '\n') {
				p_i++;
			}
			continue;
		}
		break;
	}
}

static void _collect_func_params(const String &p_src, int &p_i, HashSet<String> &r_out) {
	const int n = p_src.length();
	if (p_i >= n || p_src[p_i] != '(') {
		return;
	}
	p_i++;
	while (p_i < n) {
		_skip_ws(p_src, p_i);
		if (p_i >= n) {
			break;
		}
		if (p_src[p_i] == ')') {
			p_i++;
			break;
		}
		if (p_src[p_i] == ',') {
			p_i++;
			continue;
		}
		if (_is_ident_start(p_src[p_i])) {
			const String name = _read_ident(p_src, p_i);
			if (!ObfuscationSourceMap::is_reserved(name)) {
				r_out.insert(name);
			}
			while (p_i < n && p_src[p_i] != ',' && p_src[p_i] != ')') {
				if (p_src[p_i] == '"' || p_src[p_i] == '\'') {
					p_i = _skip_string(p_src, p_i);
					continue;
				}
				p_i++;
			}
			continue;
		}
		p_i++;
	}
}

void ObfuscationSourceMap::collect_split(const String &p_source, HashSet<String> &r_funcs, HashSet<String> &r_vars) {
	collect_split(p_source, r_funcs, r_vars, SCRIPT_GDSCRIPT);
}

void ObfuscationSourceMap::collect_split(const String &p_source, HashSet<String> &r_funcs, HashSet<String> &r_vars, ScriptLang p_lang) {
	const int n = p_source.length();
	int i = 0;
	while (i < n) {
		const char32_t c = p_source[i];
		if (p_lang == SCRIPT_LUAU && c == '-' && i + 1 < n && p_source[i + 1] == '-') {
			i = _skip_luau_comment(p_source, i);
			continue;
		}
		if (p_lang != SCRIPT_LUAU && c == '#') {
			while (i < n && p_source[i] != '\n') {
				i++;
			}
			continue;
		}
		if (p_lang == SCRIPT_LUAU && c == '[' && i + 1 < n && (p_source[i + 1] == '[' || p_source[i + 1] == '=')) {
			i = _skip_luau_long(p_source, i);
			continue;
		}
		if (c == '"' || c == '\'') {
			i = _skip_string(p_source, i);
			continue;
		}
		if (!_is_ident_start(c)) {
			i++;
			continue;
		}
		const String tok = _read_ident(p_source, i);
		if (p_lang == SCRIPT_LUAU && tok == "function") {
			_skip_ws(p_source, i);
			if (i < n && _is_ident_start(p_source[i])) {
				const String name = _read_ident(p_source, i);
				if (i < n && (p_source[i] == '.' || p_source[i] == ':')) {
					if (!is_reserved(name)) {
						r_vars.insert(name);
					}
					i++;
					_skip_ws(p_source, i);
					if (i < n && _is_ident_start(p_source[i])) {
						const String method = _read_ident(p_source, i);
						if (!is_reserved(method)) {
							r_funcs.insert(method);
						}
					}
				} else if (!is_reserved(name)) {
					r_funcs.insert(name);
				}
			}
			_skip_ws(p_source, i);
			_collect_func_params(p_source, i, r_vars);
			continue;
		}
		if (p_lang == SCRIPT_LUAU && tok == "local") {
			_skip_ws(p_source, i);
			if (i < n && _is_ident_start(p_source[i])) {
				const int save = i;
				const String next = _read_ident(p_source, i);
				if (next == "function") {
					i = save;
					continue;
				}
				if (!is_reserved(next)) {
					r_vars.insert(next);
				}
			}
			continue;
		}
		if (p_lang == SCRIPT_LUAU) {
			const int save = i;
			_skip_ws(p_source, i);
			if (i < n && p_source[i] == '=') {
				i++;
				_skip_ws(p_source, i);
				if (i < n && _is_ident_start(p_source[i])) {
					const int fn_at = i;
					const String maybe_fn = _read_ident(p_source, i);
					if (maybe_fn == "function") {
						if (!is_reserved(tok)) {
							r_funcs.insert(tok);
						}
						_skip_ws(p_source, i);
						_collect_func_params(p_source, i, r_vars);
						continue;
					}
					i = fn_at;
				}
			}
			i = save;
		}
		const bool kw_var = tok == "var" || tok == "const" || tok == "signal" || tok == "enum" || tok == "class_name" || tok == "class";
		const bool kw_func = tok == "func";
		if (!kw_var && !kw_func) {
			continue;
		}
		_skip_ws(p_source, i);
		if (i < n && _is_ident_start(p_source[i])) {
			const String name = _read_ident(p_source, i);
			if (!is_reserved(name)) {
				if (kw_func) {
					r_funcs.insert(name);
				} else {
					r_vars.insert(name);
				}
			}
			if (kw_func) {
				_skip_ws(p_source, i);
				_collect_func_params(p_source, i, r_vars);
			}
		}
	}
}

void ObfuscationSourceMap::collect_identifiers(const String &p_source, HashSet<String> &r_out) {
	HashSet<String> funcs;
	HashSet<String> vars;
	collect_split(p_source, funcs, vars);
	for (const String &n : funcs) {
		r_out.insert(n);
	}
	for (const String &n : vars) {
		r_out.insert(n);
	}
}

HashMap<String, String> ObfuscationSourceMap::map_identifiers(const PackedByteArray &p_hmac_key, const HashSet<String> &p_names) {
	HashMap<String, String> out;
	for (const String &name : p_names) {
		if (is_reserved(name)) {
			continue;
		}
		out[name] = ObfuscationOutputNames::scramble_identifier(p_hmac_key, name);
	}
	return out;
}

bool ObfuscationSourceMap::should_collect_name(const String &p_name) {
	if (p_name.is_empty() || p_name == "." || p_name == "..") {
		return false;
	}
	if (is_reserved(p_name)) {
		return false;
	}
	bool numeric = p_name.length() > 0;
	for (int i = 0; i < p_name.length(); i++) {
		const char32_t c = p_name[i];
		if (c < '0' || c > '9') {
			numeric = false;
			break;
		}
	}
	if (numeric) {
		return false;
	}
	return true;
}

static void _insert_scene_name(const String &p_name, HashSet<String> &r_out) {
	if (ObfuscationSourceMap::should_collect_name(p_name)) {
		r_out.insert(p_name);
	}
}

static void _insert_path_segments(const String &p_path, HashSet<String> &r_out) {
	PackedStringArray segs = p_path.split("/");
	for (int i = 0; i < segs.size(); i++) {
		String s = segs[i];
		if (s.begins_with("%")) {
			s = s.substr(1);
		}
		const int colon = s.find(":");
		if (colon >= 0) {
			s = s.substr(0, colon);
		}
		_insert_scene_name(s, r_out);
	}
}

static bool _attr_boundary(const String &p_text, int p_i) {
	if (p_i <= 0) {
		return true;
	}
	const char32_t prev = p_text[p_i - 1];
	if (_is_ident_cont(prev) || prev == '/' || prev == '.') {
		return false;
	}
	return prev == ' ' || prev == '\t' || prev == '\n' || prev == '\r' || prev == '[';
}

static void _collect_quoted_attr(const String &p_text, const String &p_attr, bool p_as_path, HashSet<String> &r_out) {
	const String needle = p_attr + "=\"";
	int p = 0;
	while (true) {
		const int i = p_text.find(needle, p);
		if (i < 0) {
			break;
		}
		if (!_attr_boundary(p_text, i)) {
			p = i + 1;
			continue;
		}
		const int start = i + needle.length();
		int j = start;
		while (j < p_text.length() && p_text[j] != '"') {
			j++;
		}
		const String val = p_text.substr(start, j - start);
		if (p_as_path) {
			_insert_path_segments(val, r_out);
		} else {
			_insert_scene_name(val, r_out);
		}
		p = j + 1;
	}
}

void ObfuscationSourceMap::collect_scene_names(const String &p_text, HashSet<String> &r_out) {
	_collect_quoted_attr(p_text, "name", false, r_out);
	_collect_quoted_attr(p_text, "parent", true, r_out);
	_collect_quoted_attr(p_text, "from", true, r_out);
	_collect_quoted_attr(p_text, "to", true, r_out);
	_collect_quoted_attr(p_text, "id", false, r_out);
	int gp = 0;
	while (true) {
		const int i = p_text.find("groups=", gp);
		if (i < 0) {
			break;
		}
		int j = p_text.find("[", i);
		const int end = p_text.find("]", i);
		if (j < 0 || end < 0 || j > end) {
			break;
		}
		j++;
		while (j < end) {
			if (p_text[j] == '"' || p_text[j] == '\'') {
				const int q = j;
				j = _skip_string(p_text, j);
				if (j > q + 1) {
					_insert_scene_name(p_text.substr(q + 1, j - q - 2), r_out);
				}
				continue;
			}
			j++;
		}
		gp = end + 1;
	}
	const String np_needle = "NodePath(\"";
	int np = 0;
	while (true) {
		const int i = p_text.find(np_needle, np);
		if (i < 0) {
			break;
		}
		const int start = i + np_needle.length();
		int j = start;
		while (j < p_text.length() && p_text[j] != '"') {
			j++;
		}
		_insert_path_segments(p_text.substr(start, j - start), r_out);
		np = j + 1;
	}
	String section;
	PackedStringArray lines = p_text.split("\n");
	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i].strip_edges();
		if (line.begins_with("[") && line.ends_with("]")) {
			section = line.substr(1, line.length() - 2).strip_edges();
			continue;
		}
		if (section != "autoload" && section != "input") {
			continue;
		}
		if (line.is_empty() || line.begins_with(";") || line.begins_with("#")) {
			continue;
		}
		int eq = line.find("=");
		int brace = line.find("{");
		int cut = eq;
		if (cut < 0 || (brace >= 0 && brace < cut)) {
			cut = brace;
		}
		if (cut <= 0) {
			continue;
		}
		_insert_scene_name(line.substr(0, cut).strip_edges(), r_out);
	}
}

String ObfuscationSourceMap::rewrite_node_path(const String &p_path, const HashMap<String, String> &p_idents) {
	if (p_path.is_empty() || p_path.begins_with("res://") || p_path.begins_with("uid://") || p_path.begins_with("user://")) {
		return p_path;
	}
	PackedStringArray segs = p_path.split("/");
	for (int i = 0; i < segs.size(); i++) {
		String s = segs[i];
		String prefix;
		if (s.begins_with("%")) {
			prefix = "%";
			s = s.substr(1);
		}
		String prop;
		const int colon = s.find(":");
		if (colon >= 0) {
			prop = s.substr(colon);
			s = s.substr(0, colon);
		}
		if (s != "." && s != "..") {
			const String *mapped = p_idents.getptr(s);
			if (mapped) {
				s = *mapped;
			}
		}
		segs.write[i] = prefix + s + prop;
	}
	return String("/").join(segs);
}

static String _rewrite_quoted_attr(const String &p_text, const String &p_attr, bool p_as_path, const HashMap<String, String> &p_idents) {
	String text = p_text;
	const String needle = p_attr + "=\"";
	int p = 0;
	while (true) {
		const int i = text.find(needle, p);
		if (i < 0) {
			break;
		}
		if (!_attr_boundary(text, i)) {
			p = i + 1;
			continue;
		}
		const int start = i + needle.length();
		int j = start;
		while (j < text.length() && text[j] != '"') {
			j++;
		}
		const String val = text.substr(start, j - start);
		String next = p_as_path ? ObfuscationSourceMap::rewrite_node_path(val, p_idents) : val;
		if (!p_as_path) {
			const String *mapped = p_idents.getptr(val);
			if (mapped) {
				next = *mapped;
			}
		}
		text = text.substr(0, start) + next + text.substr(j);
		p = start + next.length() + 1;
	}
	return text;
}

static String _rewrite_call_quoted(const String &p_text, const String &p_call, const HashMap<String, String> &p_idents, bool p_as_path) {
	String text = p_text;
	const String needle = p_call + "(\"";
	int p = 0;
	while (true) {
		const int i = text.find(needle, p);
		if (i < 0) {
			break;
		}
		const int start = i + needle.length();
		int j = start;
		while (j < text.length() && text[j] != '"') {
			j++;
		}
		const String val = text.substr(start, j - start);
		String next = p_as_path ? ObfuscationSourceMap::rewrite_node_path(val, p_idents) : val;
		if (!p_as_path) {
			const String *mapped = p_idents.getptr(val);
			if (mapped) {
				next = *mapped;
			}
		}
		text = text.substr(0, start) + next + text.substr(j);
		p = start + next.length() + 1;
	}
	return text;
}

String ObfuscationSourceMap::rewrite_scene(const String &p_text, const HashMap<String, String> &p_idents, const PackedByteArray &p_hmac_key) {
	String text = rewrite_paths(p_text, p_hmac_key);
	text = _rewrite_quoted_attr(text, "name", false, p_idents);
	text = _rewrite_quoted_attr(text, "parent", true, p_idents);
	text = _rewrite_quoted_attr(text, "from", true, p_idents);
	text = _rewrite_quoted_attr(text, "to", true, p_idents);
	text = _rewrite_quoted_attr(text, "method", false, p_idents);
	text = _rewrite_quoted_attr(text, "id", false, p_idents);
	text = _rewrite_call_quoted(text, "NodePath", p_idents, true);
	text = _rewrite_call_quoted(text, "ExtResource", p_idents, false);
	text = _rewrite_call_quoted(text, "SubResource", p_idents, false);
	int gp = 0;
	while (true) {
		const int i = text.find("groups=", gp);
		if (i < 0) {
			break;
		}
		int j = text.find("[", i);
		const int end = text.find("]", i);
		if (j < 0 || end < 0 || j > end) {
			break;
		}
		String inner = text.substr(j, end - j + 1);
		int q = 0;
		while (true) {
			const int qs = inner.find("\"", q);
			if (qs < 0) {
				break;
			}
			int qe = qs + 1;
			while (qe < inner.length() && inner[qe] != '"') {
				qe++;
			}
			const String val = inner.substr(qs + 1, qe - qs - 1);
			const String *mapped = p_idents.getptr(val);
			if (mapped) {
				inner = inner.substr(0, qs + 1) + *mapped + inner.substr(qe);
				q = qs + 1 + mapped->length() + 1;
			} else {
				q = qe + 1;
			}
		}
		text = text.substr(0, j) + inner + text.substr(end + 1);
		gp = j + inner.length();
	}
	PackedStringArray lines = text.split("\n");
	String section;
	for (int i = 0; i < lines.size(); i++) {
		const String stripped = lines[i].strip_edges();
		if (stripped.begins_with("[") && stripped.ends_with("]")) {
			section = stripped.substr(1, stripped.length() - 2).strip_edges();
			continue;
		}
		if (section != "autoload" && section != "input") {
			continue;
		}
		String line = lines[i];
		int lead = 0;
		while (lead < line.length() && (line[lead] == ' ' || line[lead] == '\t')) {
			lead++;
		}
		String rest = line.substr(lead);
		if (rest.is_empty() || rest.begins_with(";") || rest.begins_with("#")) {
			continue;
		}
		int cut = rest.find("=");
		int brace = rest.find("{");
		if (cut < 0 || (brace >= 0 && brace < cut)) {
			cut = brace;
		}
		if (cut <= 0) {
			continue;
		}
		const String key = rest.substr(0, cut).strip_edges();
		const String *mapped = p_idents.getptr(key);
		if (!mapped) {
			continue;
		}
		lines.write[i] = line.substr(0, lead) + *mapped + rest.substr(cut);
	}
	return String("\n").join(lines);
}

String ObfuscationSourceMap::rewrite_paths(const String &p_text, const PackedByteArray &p_hmac_key) {
	String text = p_text;
	int p = 0;
	while (true) {
		const int i = text.find("res://", p);
		if (i < 0) {
			break;
		}
		int j = i + 6;
		while (j < text.length() && _is_path_char(text[j])) {
			j++;
		}
		const String logical = text.substr(i, j - i);
		if (logical == "res://" || keep_original_path(logical)) {
			p = j;
			continue;
		}
		const String scrambled = ObfuscationOutputNames::scramble_output_path(p_hmac_key, logical);
		text = text.substr(0, i) + scrambled + text.substr(j);
		p = i + scrambled.length();
	}
	return text;
}

bool ObfuscationSourceMap::keep_original_path(const String &p_logical) {
	const String n = p_logical.replace("\\", "/").get_file().to_lower();
	return n == "project.godot";
}

static String _rewrite_comment(const String &p_comment, const HashMap<String, String> &p_idents) {
	const String trimmed = p_comment.strip_edges();
	if (trimmed.begins_with("# ~ ") || trimmed.begins_with("-- ~ ")) {
		return p_comment;
	}
	String out;
	int i = 0;
	const int n = p_comment.length();
	while (i < n) {
		if (_is_ident_start(p_comment[i])) {
			const String id = _read_ident(p_comment, i);
			const String *mapped = p_idents.getptr(id);
			if (mapped) {
				out += *mapped;
			} else {
				out += id;
			}
		} else {
			out += String::chr(p_comment[i]);
			i++;
		}
	}
	return out;
}

static bool _looks_like_node_path(const String &p_inner) {
	if (p_inner.is_empty() || p_inner.begins_with("res://") || p_inner.begins_with("uid://") || p_inner.begins_with("user://")) {
		return false;
	}
	if (p_inner.find("res://") >= 0 || p_inner.find("uid://") >= 0) {
		return false;
	}
	return p_inner.find("/") >= 0;
}

String ObfuscationSourceMap::rewrite_script(const String &p_source, const HashMap<String, String> &p_idents, const HashSet<String> &p_funcs, const PackedByteArray &p_hmac_key, bool p_scramble_paths) {
	HashSet<String> none;
	return rewrite_script(p_source, p_idents, p_funcs, none, p_hmac_key, p_scramble_paths, SCRIPT_GDSCRIPT);
}

String ObfuscationSourceMap::rewrite_script(const String &p_source, const HashMap<String, String> &p_idents, const HashSet<String> &p_funcs, const HashSet<String> &p_scene_names, const PackedByteArray &p_hmac_key, bool p_scramble_paths, ScriptLang p_lang) {
	String out;
	const int n = p_source.length();
	int i = 0;
	bool after_dot = false;
	while (i < n) {
		const char32_t c = p_source[i];
		if (p_lang == SCRIPT_LUAU && c == '-' && i + 1 < n && p_source[i + 1] == '-') {
			const int start = i;
			i = _skip_luau_comment(p_source, i);
			out += _rewrite_comment(p_source.substr(start, i - start), p_idents);
			after_dot = false;
			continue;
		}
		if (p_lang != SCRIPT_LUAU && c == '#') {
			const int start = i;
			while (i < n && p_source[i] != '\n') {
				i++;
			}
			out += _rewrite_comment(p_source.substr(start, i - start), p_idents);
			after_dot = false;
			continue;
		}
		if (p_lang == SCRIPT_LUAU && c == '[' && i + 1 < n && (p_source[i + 1] == '[' || p_source[i + 1] == '=')) {
			const int start = i;
			i = _skip_luau_long(p_source, i);
			String lit = p_source.substr(start, i - start);
			int open_end = 2;
			while (open_end < lit.length() && lit[open_end - 1] != '[') {
				open_end++;
			}
			const int close_len = open_end;
			String inner = lit.length() >= open_end + close_len ? lit.substr(open_end, lit.length() - open_end - close_len) : String();
			if (p_scramble_paths && inner.find("res://") >= 0) {
				out += lit.substr(0, open_end) + rewrite_paths(inner, p_hmac_key) + lit.substr(lit.length() - close_len, close_len);
			} else {
				out += lit;
			}
			after_dot = false;
			continue;
		}
		if (c == '"' || c == '\'') {
			const int start = i;
			i = _skip_string(p_source, i);
			String lit = p_source.substr(start, i - start);
			String inner;
			String quote;
			if (lit.length() >= 6 && lit.substr(0, 3) == lit.substr(lit.length() - 3, 3) && (lit[0] == '"' || lit[0] == '\'')) {
				quote = lit.substr(0, 3);
				inner = lit.substr(3, lit.length() - 6);
			} else if (lit.length() >= 2) {
				quote = lit.substr(0, 1);
				inner = lit.substr(1, lit.length() - 2);
			} else {
				out += lit;
				after_dot = false;
				continue;
			}
			const String *mapped_lit = p_idents.getptr(inner);
			if (mapped_lit && (p_funcs.has(inner) || p_scene_names.has(inner))) {
				out += quote + *mapped_lit + quote;
			} else if (_looks_like_node_path(inner)) {
				out += quote + rewrite_node_path(inner, p_idents) + quote;
			} else if (p_scramble_paths) {
				out += quote + rewrite_paths(inner, p_hmac_key) + quote;
			} else {
				out += lit;
			}
			after_dot = false;
			continue;
		}
		if (_is_ident_start(c)) {
			const String id = _read_ident(p_source, i);
			const String *mapped_id = p_idents.getptr(id);
			const bool remap = mapped_id && (!after_dot || p_funcs.has(id) || p_scene_names.has(id));
			if (remap) {
				out += *mapped_id;
			} else {
				out += id;
			}
			after_dot = false;
			continue;
		}
		out += String::chr(c);
		if (c == '.' || (p_lang == SCRIPT_LUAU && c == ':')) {
			after_dot = true;
		} else if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
			after_dot = false;
		}
		i++;
	}
	return out;
}

bool ObfuscationSourceMap::skip_relative_path(const String &p_rel) {
	const String r = p_rel.replace("\\", "/").to_lower();
	if (r.begins_with(".godot/") || r == ".godot" || r.begins_with(".git/") || r == ".git") {
		return true;
	}
	if (r.begins_with("build/") || r == "build") {
		return true;
	}
	const String file = r.get_file();
	if (file == "export_presets.cfg" || file == ".gitignore" || file == ".gitattributes") {
		return true;
	}
	if (file.ends_with(".md") || file.ends_with(".uid") || file.ends_with(".import") || file.ends_with(".luauc")) {
		return true;
	}
	return false;
}

void ObfuscationSourceMap::list_files(const String &p_dir, Vector<String> &r_files) {
	Ref<DirAccess> da = DirAccess::open(p_dir);
	if (da.is_null()) {
		return;
	}
	da->list_dir_begin();
	String fn = da->get_next();
	while (!fn.is_empty()) {
		if (fn != "." && fn != "..") {
			const String path = p_dir.path_join(fn);
			if (da->current_is_dir()) {
				if (fn != ".godot" && fn != ".git" && fn != "build") {
					list_files(path, r_files);
				}
			} else {
				r_files.push_back(path);
			}
		}
		fn = da->get_next();
	}
}

int ObfuscationSourceMap::gdscript_preamble_end(const String &p_source) {
	const int n = p_source.length();
	int i = 0;
	while (i < n) {
		while (i < n && (p_source[i] == ' ' || p_source[i] == '\t' || p_source[i] == '\r')) {
			i++;
		}
		if (i >= n) {
			break;
		}
		if (p_source[i] == '\n') {
			i++;
			continue;
		}
		if (p_source[i] == '#' || p_source[i] == '@') {
			while (i < n && p_source[i] != '\n') {
				i++;
			}
			continue;
		}
		if (p_source[i] == '-' && i + 1 < n && p_source[i + 1] == '-') {
			i = _skip_luau_comment(p_source, i);
			continue;
		}
		if (_is_ident_start(p_source[i])) {
			const int start = i;
			const String tok = _read_ident(p_source, i);
			if (tok == "extends" || tok == "class_name") {
				while (i < n && p_source[i] != '\n') {
					i++;
				}
				if (i < n && p_source[i] == '\n') {
					i++;
				}
				continue;
			}
			i = start;
		}
		break;
	}
	return i;
}

String ObfuscationSourceMap::insert_after_preamble(const String &p_source, const String &p_block) {
	if (p_block.is_empty()) {
		return p_source;
	}
	const int at = gdscript_preamble_end(p_source);
	String block = p_block;
	if (!block.ends_with("\n")) {
		block += "\n";
	}
	return p_source.substr(0, at) + block + p_source.substr(at);
}

String ObfuscationSourceMap::insert_before_chunk_return(const String &p_source, const String &p_block) {
	if (p_block.is_empty()) {
		return p_source;
	}
	String block = p_block;
	if (!block.ends_with("\n")) {
		block += "\n";
	}
	int at = -1;
	int line = 0;
	const int n = p_source.length();
	while (line <= n) {
		int eol = line;
		while (eol < n && p_source[eol] != '\n') {
			eol++;
		}
		int i = line;
		while (i < eol && (p_source[i] == ' ' || p_source[i] == '\t' || p_source[i] == '\r')) {
			i++;
		}
		if (i == line && i + 6 <= eol && p_source.substr(i, 6) == "return") {
			at = line;
		}
		if (eol >= n) {
			break;
		}
		line = eol + 1;
	}
	if (at < 0) {
		if (!p_source.is_empty() && !p_source.ends_with("\n")) {
			return p_source + "\n" + block;
		}
		return p_source + block;
	}
	return p_source.substr(0, at) + block + p_source.substr(at);
}

static bool _lattice_comment_at(const String &p_src, int p_i, ObfuscationSourceMap::ScriptLang p_lang) {
	if (p_lang == ObfuscationSourceMap::SCRIPT_LUAU) {
		return p_src.substr(p_i, 5) == "-- ~ ";
	}
	return p_src.substr(p_i, 4) == "# ~ ";
}

String ObfuscationSourceMap::strip_minify(const String &p_source, ScriptLang p_lang) {
	String out;
	const int n = p_source.length();
	int i = 0;
	String indent;
	String line;
	bool at_line_start = true;
	bool last_space = false;

	auto flush_line = [&]() {
		while (line.ends_with(" ") || line.ends_with("\t")) {
			line = line.substr(0, line.length() - 1);
		}
		if (!line.is_empty()) {
			out += indent + line + "\n";
		}
		indent = String();
		line = String();
		at_line_start = true;
		last_space = false;
	};

	while (i < n) {
		if (at_line_start) {
			while (i < n && (p_source[i] == ' ' || p_source[i] == '\t')) {
				indent += String::chr(p_source[i]);
				i++;
			}
			at_line_start = false;
			if (i < n && _lattice_comment_at(p_source, i, p_lang)) {
				const int start = i;
				while (i < n && p_source[i] != '\n' && p_source[i] != '\r') {
					i++;
				}
				line += p_source.substr(start, i - start);
				continue;
			}
		}
		if (i >= n) {
			break;
		}
		const char32_t c = p_source[i];
		if (p_lang == SCRIPT_LUAU && c == '-' && i + 1 < n && p_source[i + 1] == '-') {
			if (_lattice_comment_at(p_source, i, p_lang)) {
				const int start = i;
				while (i < n && p_source[i] != '\n' && p_source[i] != '\r') {
					i++;
				}
				line += p_source.substr(start, i - start);
				last_space = false;
				continue;
			}
			i = _skip_luau_comment(p_source, i);
			continue;
		}
		if (p_lang != SCRIPT_LUAU && c == '#') {
			if (_lattice_comment_at(p_source, i, p_lang)) {
				const int start = i;
				while (i < n && p_source[i] != '\n' && p_source[i] != '\r') {
					i++;
				}
				line += p_source.substr(start, i - start);
				last_space = false;
				continue;
			}
			while (i < n && p_source[i] != '\n' && p_source[i] != '\r') {
				i++;
			}
			continue;
		}
		if (p_lang == SCRIPT_LUAU && c == '[' && i + 1 < n && (p_source[i + 1] == '[' || p_source[i + 1] == '=')) {
			const int start = i;
			i = _skip_luau_long(p_source, i);
			line += p_source.substr(start, i - start);
			last_space = false;
			continue;
		}
		if (c == '"' || c == '\'') {
			const int start = i;
			i = _skip_string(p_source, i);
			line += p_source.substr(start, i - start);
			last_space = false;
			continue;
		}
		if (c == '\r') {
			i++;
			if (i < n && p_source[i] == '\n') {
				i++;
			}
			flush_line();
			continue;
		}
		if (c == '\n') {
			i++;
			flush_line();
			continue;
		}
		if (c == ' ' || c == '\t') {
			if (!last_space && !line.is_empty()) {
				line += " ";
				last_space = true;
			}
			i++;
			continue;
		}
		line += String::chr(c);
		last_space = false;
		i++;
	}
	flush_line();
	return out;
}
