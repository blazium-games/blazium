/**************************************************************************/
/*  env.cpp                                                               */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            BLAZIUM ENGINE                              */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* Copyright (c) 2024 Dragos Daian, Randolph William Aarseth II.          */
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

#include "env.h"
#include "core/config/engine.h"
#include "core/core_bind.h"
#include "core/io/json.h"
#include "core/os/os.h"

ENV *ENV::env_singleton = nullptr;

ENV *ENV::get_singleton() {
	return env_singleton;
}
void ENV::_bind_methods() {
	ClassDB::bind_method(D_METHOD("parse", "data"), &ENV::parse);
	ClassDB::bind_method(D_METHOD("populate", "env", "override"), &ENV::populate, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("refresh", "override"), &ENV::refresh, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("clear"), &ENV::clear);
	ClassDB::bind_method(D_METHOD("get_env", "key", "default"), &ENV::get_env, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("set_env", "key", "value"), &ENV::set_env);
	ClassDB::bind_method(D_METHOD("has_env", "key"), &ENV::has_env);
	ClassDB::bind_method(D_METHOD("remove_env", "key"), &ENV::remove_env);
	ClassDB::bind_method(D_METHOD("get_all_env"), &ENV::get_all_env);
	ClassDB::bind_method(D_METHOD("get_env_files"), &ENV::get_env_files);
	ClassDB::bind_method(D_METHOD("has_env_file", "file"), &ENV::has_env_file);
	ClassDB::bind_method(D_METHOD("get_envs_from_file", "file"), &ENV::get_envs_from_file);

	ClassDB::bind_method(D_METHOD("get_env_int", "key", "default"), &ENV::get_env_int, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_env_bool", "key", "default"), &ENV::get_env_bool, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_env_float", "key", "default"), &ENV::get_env_float, DEFVAL(0.0f));
	ClassDB::bind_method(D_METHOD("get_env_array", "key", "default", "delimiter"), &ENV::get_env_array, DEFVAL(TypedArray<String>()), DEFVAL(","));
	ClassDB::bind_method(D_METHOD("get_env_dict", "key", "default"), &ENV::get_env_dict, DEFVAL(Dictionary()));

	ClassDB::bind_method(D_METHOD("get_env_color", "key", "default"), &ENV::get_env_color, DEFVAL(Color()));
	ClassDB::bind_method(D_METHOD("get_env_vector2", "key", "default"), &ENV::get_env_vector2, DEFVAL(Vector2()));
	ClassDB::bind_method(D_METHOD("get_env_vector3", "key", "default"), &ENV::get_env_vector3, DEFVAL(Vector3()));
	ClassDB::bind_method(D_METHOD("get_env_vector4", "key", "default"), &ENV::get_env_vector4, DEFVAL(Vector4()));

	ClassDB::bind_method(D_METHOD("parse_buffer", "data"), &ENV::parse_buffer);
	ClassDB::bind_method(D_METHOD("bind_env", "node", "property_map"), &ENV::bind_env);
	ClassDB::bind_method(D_METHOD("get_envs_matching", "pattern"), &ENV::get_envs_matching);
	ClassDB::bind_method(D_METHOD("get_missing_envs", "keys"), &ENV::get_missing_envs);
	ClassDB::bind_method(D_METHOD("set_resolver", "callable"), &ENV::set_resolver);
	ClassDB::bind_method(D_METHOD("set_bool_whitelist", "true_terms", "false_terms"), &ENV::set_bool_whitelist);

	ClassDB::bind_method(D_METHOD("load_env_file", "file", "override"), &ENV::load_env_file, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("config", "file", "override"), &ENV::config, DEFVAL(true));

	ClassDB::bind_method(D_METHOD("auto_config", "dir", "mode"), &ENV::auto_config, DEFVAL("res://"), DEFVAL(""));
	ClassDB::bind_method(D_METHOD("require_envs", "keys"), &ENV::require_envs);

	ClassDB::bind_method(D_METHOD("get_env_group", "prefix"), &ENV::get_env_group);

	ClassDB::bind_method(D_METHOD("expand_string", "text"), &ENV::expand_string);

	ClassDB::bind_method(D_METHOD("set_prioritize_os_env", "prioritize"), &ENV::set_prioritize_os_env);
	ClassDB::bind_method(D_METHOD("get_prioritize_os_env"), &ENV::get_prioritize_os_env);

	ClassDB::bind_method(D_METHOD("push_to_os_env"), &ENV::push_to_os_env);

	ClassDB::bind_method(D_METHOD("generate_example", "file"), &ENV::generate_example);
	ClassDB::bind_method(D_METHOD("export_json", "file"), &ENV::export_json);
	ClassDB::bind_method(D_METHOD("save", "file"), &ENV::save);

	ClassDB::bind_method(D_METHOD("get_debug"), &ENV::get_debug);
	ClassDB::bind_method(D_METHOD("set_debug", "debug"), &ENV::set_debug);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug"), "set_debug", "get_debug");

	ADD_SIGNAL(MethodInfo("file_loaded", PropertyInfo(Variant::STRING, "file"), PropertyInfo(Variant::DICTIONARY, "env")));
	ADD_SIGNAL(MethodInfo("cleared"));
	ADD_SIGNAL(MethodInfo("refreshed", PropertyInfo(Variant::DICTIONARY, "env")));
	ADD_SIGNAL(MethodInfo("updated", PropertyInfo(Variant::STRING, "key"), PropertyInfo(Variant::STRING, "value")));
}

void ENV::clear() {
	env_vars.clear();
	env_files.clear();
	custom_resolver = Callable();
	prioritize_os_env = true;
	true_terms.clear();
	true_terms.push_back("true");
	true_terms.push_back("yes");
	true_terms.push_back("1");
	false_terms.clear();
	false_terms.push_back("false");
	false_terms.push_back("no");
	false_terms.push_back("0");
	emit_signal("cleared");
}

Variant ENV::get_env(const String &p_key, const Variant &p_default) {
	if (prioritize_os_env && OS::get_singleton()->has_environment(p_key)) {
		return OS::get_singleton()->get_environment(p_key);
	}
	if (env_vars.has(p_key)) {
		return env_vars[p_key];
	}
	if (!prioritize_os_env && OS::get_singleton()->has_environment(p_key)) {
		return OS::get_singleton()->get_environment(p_key);
	}

	if (custom_resolver.is_valid()) {
		Variant args[1] = { p_key };
		const Variant *argptrs[1] = { &args[0] };
		Callable::CallError ce;
		Variant result;
		custom_resolver.callp(argptrs, 1, result, ce);
		if (ce.error == Callable::CallError::CALL_OK && result.get_type() != Variant::NIL && !result.operator String().is_empty()) {
			return result;
		}
	}

	return p_default;
}
void ENV::set_env(const String &p_key, const Variant &p_value) {
	env_vars[p_key] = p_value;
	emit_signal("updated", p_key, p_value);
}

bool ENV::has_env(const String &p_key) {
	return env_vars.has(p_key) || OS::get_singleton()->has_environment(p_key);
}

int ENV::get_env_int(const String &p_key, int p_default) {
	return get_env(p_key, p_default).operator int();
}

bool ENV::get_env_bool(const String &p_key, bool p_default) {
	Variant val = get_env(p_key);
	if (val.get_type() == Variant::NIL || val.operator String().is_empty()) {
		return p_default;
	}
	String lower = val.operator String().strip_edges().to_lower();
	if (true_terms.has(lower)) {
		return true;
	} else if (false_terms.has(lower)) {
		return false;
	}
	return p_default;
}

float ENV::get_env_float(const String &p_key, float p_default) {
	return get_env(p_key, p_default).operator float();
}

void ENV::remove_env(const String &p_key) {
	if (env_vars.has(p_key)) {
		env_vars.erase(p_key);
		emit_signal("updated", p_key, Variant());
	}
}

Dictionary ENV::get_all_env() const {
	return env_vars;
}

TypedArray<String> ENV::get_env_files() const {
	return env_files;
}

bool ENV::has_env_file(const String &p_file) const {
	return env_files.has(p_file);
}

void ENV::auto_config(const String &p_dir, const String &p_mode) {
	String mode = p_mode;
	if (mode.is_empty()) {
		if (Engine::get_singleton()->is_editor_hint() || OS::get_singleton()->has_feature("editor")) {
			mode = "development";
		} else {
			mode = "production";
		}
	}

	String base_dir = p_dir;
	if (!base_dir.ends_with("/")) {
		base_dir += "/";
	}

	String f1 = base_dir + ".env";
	if (FileAccess::exists(f1)) {
		load_env_file(f1, true);
	}

	String f2 = base_dir + ".env.local";
	if (FileAccess::exists(f2)) {
		load_env_file(f2, true);
	}

	String f3 = base_dir + ".env." + mode;
	if (FileAccess::exists(f3)) {
		load_env_file(f3, true);
	}

	String f4 = base_dir + ".env." + mode + ".local";
	if (FileAccess::exists(f4)) {
		load_env_file(f4, true);
	}
}

bool ENV::require_envs(const TypedArray<String> &p_keys) {
	for (int i = 0; i < p_keys.size(); i++) {
		String key = p_keys[i];
		Variant val = get_env(key);
		if (val.get_type() == Variant::NIL || val.operator String().is_empty()) {
			print_error("ENV missing required key: " + key);
			return false;
		}
	}
	return true;
}

Dictionary ENV::get_envs_from_file(const String &p_file) {
	Ref<FileAccess> file = FileAccess::open(p_file, FileAccess::READ);
	if (!file.is_valid()) {
		print_debug("Failed to open file: " + p_file);
		return Dictionary();
	}
	return parse(file->get_as_text());
}

TypedArray<String> ENV::get_env_array(const String &p_key, const TypedArray<String> &p_default, const String &p_delimiter) {
	Variant val = get_env(p_key);
	if (val.get_type() == Variant::NIL || val.operator String().is_empty()) {
		return p_default;
	}
	String str_val = val.operator String();
	Vector<String> parts = str_val.split(p_delimiter);
	TypedArray<String> arr;
	for (int i = 0; i < parts.size(); i++) {
		arr.push_back(parts[i].strip_edges());
	}
	return arr;
}

Dictionary ENV::get_env_dict(const String &p_key, const Dictionary &p_default) {
	Variant val = get_env(p_key);
	if (val.get_type() == Variant::NIL || val.operator String().is_empty()) {
		return p_default;
	}
	Variant parsed = JSON::parse_string(val.operator String());
	if (parsed.get_type() == Variant::DICTIONARY) {
		return parsed;
	}
	return p_default;
}

Color ENV::get_env_color(const String &p_key, const Color &p_default) {
	Variant val = get_env(p_key);
	if (val.get_type() == Variant::NIL || val.operator String().is_empty()) {
		return p_default;
	}
	String s = val.operator String();
	if (s.begins_with("#")) {
		return Color::html(s);
	}
	Vector<String> parts = s.split(",");
	if (parts.size() >= 3) {
		float a = parts.size() >= 4 ? parts[3].to_float() : 1.0f;
		return Color(parts[0].to_float(), parts[1].to_float(), parts[2].to_float(), a);
	}
	return Color::html(s);
}

Vector2 ENV::get_env_vector2(const String &p_key, const Vector2 &p_default) {
	Variant val = get_env(p_key);
	if (val.get_type() == Variant::NIL || val.operator String().is_empty()) {
		return p_default;
	}
	Vector<String> parts = val.operator String().split(",");
	if (parts.size() >= 2) {
		return Vector2(parts[0].to_float(), parts[1].to_float());
	}
	return p_default;
}

Vector3 ENV::get_env_vector3(const String &p_key, const Vector3 &p_default) {
	Variant val = get_env(p_key);
	if (val.get_type() == Variant::NIL || val.operator String().is_empty()) {
		return p_default;
	}
	Vector<String> parts = val.operator String().split(",");
	if (parts.size() >= 3) {
		return Vector3(parts[0].to_float(), parts[1].to_float(), parts[2].to_float());
	}
	return p_default;
}

Vector4 ENV::get_env_vector4(const String &p_key, const Vector4 &p_default) {
	Variant val = get_env(p_key);
	if (val.get_type() == Variant::NIL || val.operator String().is_empty()) {
		return p_default;
	}
	Vector<String> parts = val.operator String().split(",");
	if (parts.size() >= 4) {
		return Vector4(parts[0].to_float(), parts[1].to_float(), parts[2].to_float(), parts[3].to_float());
	}
	return p_default;
}

Dictionary ENV::parse_buffer(const PackedByteArray &p_data) {
	if (p_data.is_empty()) {
		return Dictionary();
	}
	String s;
	s.parse_utf8((const char *)p_data.ptr(), p_data.size());
	return parse(s);
}

void ENV::bind_env(Object *p_node, const Dictionary &p_property_map) {
	if (!p_node) {
		return;
	}
	Array keys = p_property_map.keys();
	for (int i = 0; i < keys.size(); i++) {
		String env_key = keys[i];
		String prop_name = p_property_map[env_key];
		if (has_env(env_key)) {
			Vector<String> parts = prop_name.split(":");
			Vector<StringName> path;
			for (int j = 0; j < parts.size(); j++) {
				path.push_back(parts[j]);
			}
			p_node->set_indexed(path, get_env(env_key));
		}
	}
}

Dictionary ENV::get_envs_matching(const String &p_pattern) {
	Dictionary matched;
	Ref<RefCounted> regex;
	Object *re_obj = ClassDB::instantiate("RegEx");
	if (re_obj) {
		regex = Ref<RefCounted>(Object::cast_to<RefCounted>(re_obj));
	}
	if (regex.is_valid()) {
		regex->call("compile", p_pattern);
		if (regex->call("is_valid")) {
			Array keys = env_vars.keys();
			for (int i = 0; i < keys.size(); i++) {
				String key = keys[i];
				Variant m = regex->call("search", key);
				if (m.get_type() == Variant::OBJECT && m.operator Object *() != nullptr) {
					matched[key] = env_vars[key];
				}
			}
		}
	}
	return matched;
}

TypedArray<String> ENV::get_missing_envs(const TypedArray<String> &p_keys) {
	TypedArray<String> missing;
	for (int i = 0; i < p_keys.size(); i++) {
		if (!has_env(p_keys[i])) {
			missing.push_back(p_keys[i]);
		}
	}
	return missing;
}

void ENV::set_resolver(const Callable &p_callable) {
	custom_resolver = p_callable;
}

void ENV::set_bool_whitelist(const TypedArray<String> &p_true_terms, const TypedArray<String> &p_false_terms) {
	true_terms = p_true_terms;
	false_terms = p_false_terms;
}

String ENV::expand_string(const String &p_text) {
	String value = p_text;
	int start_idx = 0;
	while ((start_idx = value.find("$", start_idx)) != -1) {
		if (start_idx > 0 && value[start_idx - 1] == '\\') {
			start_idx += 1;
			continue;
		}

		int idx = start_idx + 1;
		bool bracket = false;
		if (idx < value.length() && value[idx] == '{') {
			bracket = true;
			idx++;
		}
		int key_start = idx;
		while (idx < value.length() && ((value[idx] >= 'a' && value[idx] <= 'z') || (value[idx] >= 'A' && value[idx] <= 'Z') || (value[idx] >= '0' && value[idx] <= '9') || value[idx] == '_')) {
			idx++;
		}
		if (idx > key_start) {
			String var_key = value.substr(key_start, idx - key_start);
			String var_val;
			bool found = false;

			if (env_vars.has(var_key)) {
				var_val = env_vars[var_key];
				found = true;
			} else if (OS::get_singleton()->has_environment(var_key)) {
				var_val = OS::get_singleton()->get_environment(var_key);
				found = true;
			}

			String to_replace;

			if (bracket) {
				if (idx < value.length() && value[idx] == ':') {
					if (idx + 1 < value.length() && value[idx + 1] == '-') {
						idx += 2;
						int def_start = idx;
						while (idx < value.length() && value[idx] != '}') {
							idx++;
						}
						if (!found) {
							var_val = value.substr(def_start, idx - def_start);
						}
					}
				}
				if (idx < value.length() && value[idx] == '}') {
					idx++;
				}
			}

			to_replace = value.substr(start_idx, idx - start_idx);
			value = value.replace(to_replace, var_val);
		} else {
			start_idx++;
		}
	}
	value = value.replace("\\$", "$");
	return value;
}

void ENV::set_prioritize_os_env(bool p_prioritize) {
	prioritize_os_env = p_prioritize;
}

bool ENV::get_prioritize_os_env() const {
	return prioritize_os_env;
}

Dictionary ENV::get_env_group(const String &p_prefix) {
	Dictionary result;
	Array keys = env_vars.keys();
	for (int i = 0; i < keys.size(); i++) {
		String key = keys[i];
		if (key.begins_with(p_prefix)) {
			result[key] = get_env(key);
		}
	}
	return result;
}

void ENV::push_to_os_env() {
	Array keys = env_vars.keys();
	for (int i = 0; i < keys.size(); i++) {
		String key = keys[i];
		String val = env_vars[key];
		OS::get_singleton()->set_environment(key, val);
	}
}

void ENV::export_json(const String &p_file) {
	Ref<FileAccess> file = FileAccess::open(p_file, FileAccess::WRITE);
	if (!file.is_valid()) {
		print_debug("Failed to open file: " + p_file);
		return;
	}
	String output = JSON::stringify(env_vars, "\t");
	file->store_string(output);
	file->close();
}

void ENV::generate_example(const String &p_file) {
	Ref<FileAccess> file = FileAccess::open(p_file, FileAccess::WRITE);
	if (!file.is_valid()) {
		print_debug("Failed to open file: " + p_file);
		return;
	}
	Array keys = env_vars.keys();
	for (int i = 0; i < keys.size(); i++) {
		String key = keys[i];
		file->store_line(key + "=");
	}
	file->close();
}

void ENV::save(const String &p_file) {
	Ref<FileAccess> file = FileAccess::open(p_file, FileAccess::WRITE);
	if (!file.is_valid()) {
		print_debug("Failed to open file for writing: " + p_file);
		return;
	}
	Array keys = env_vars.keys();
	for (int i = 0; i < keys.size(); i++) {
		String key = keys[i];
		String value = env_vars[key];
		value = value.replace("\n", "\\n").replace("\r", "\\r").replace("\"", "\\\"");
		file->store_line(key + "=\"" + value + "\"");
	}
	file->flush();
}

void ENV::print_debug(String debug_text) {
	if (!debug) {
		return;
	}
	print_line("[ENV] " + debug_text);
}

Dictionary ENV::config(const String &p_file, bool p_override) {
	Ref<FileAccess> file = FileAccess::open(p_file, FileAccess::READ);
	if (!file.is_valid()) {
		print_debug("Failed to open file: " + p_file);
		return Dictionary();
	}

	if (!env_files.has(p_file)) {
		env_files.push_back(p_file);
	}

	Dictionary env = populate(parse(file->get_as_text()), p_override);
	emit_signal("file_loaded", p_file, env);
	print_debug("Loaded environment variables from: " + p_file);

	return env;
}

Dictionary ENV::load_env_file(const String &p_file, bool p_override) {
	if (p_file != ".env" && !env_files.has(".env")) {
		config(".env", p_override);
	}
	return config(p_file, p_override);
}

Dictionary ENV::parse(const String &p_text) {
	Dictionary env;
	Vector<String> lines = p_text.split("\n");

	Vector<String> merged_lines;
	bool in_multiline = false;
	String current_merged = "";
	char32_t current_quote = 0;

	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i];

		if (!in_multiline) {
			String trimmed_start = line.strip_edges(true, false);
			if (trimmed_start.begins_with("#")) {
				merged_lines.push_back(line);
				continue;
			}
			int eq_idx = trimmed_start.find("=");
			if (eq_idx > 0) {
				String val = trimmed_start.substr(eq_idx + 1).strip_edges(true, false);
				if (val.begins_with("\"") || val.begins_with("'")) {
					char32_t quote = val[0];
					int end_idx = -1;
					for (int j = 1; j < val.length(); j++) {
						if (val[j] == quote) {
							if (val[j - 1] != '\\') {
								end_idx = j;
								break;
							}
						}
					}
					if (end_idx == -1) {
						in_multiline = true;
						current_quote = quote;
						current_merged = line;
						continue;
					}
				}
			}
			merged_lines.push_back(line);
		} else {
			current_merged += "\n" + line;
			int end_idx = -1;
			for (int j = 0; j < line.length(); j++) {
				if (line[j] == current_quote) {
					if (j == 0 || line[j - 1] != '\\') {
						end_idx = j;
						break;
					}
				}
			}
			if (end_idx != -1) {
				in_multiline = false;
				merged_lines.push_back(current_merged);
				current_merged = "";
			}
		}
	}
	if (in_multiline) {
		merged_lines.push_back(current_merged);
	}

	for (int i = 0; i < merged_lines.size(); i++) {
		String line = merged_lines[i].strip_edges();

		// Skip empty lines and comments
		if (line.is_empty() || line.begins_with("#")) {
			continue;
		}

		// Find the first `=` sign to split key/value
		int eq_pos = line.find("=");
		if (eq_pos == -1) {
			continue; // Invalid line (no `=`)
		}

		String key = line.substr(0, eq_pos).strip_edges();
		if (key.begins_with("export ")) {
			key = key.substr(7).strip_edges();
		}

		String value = line.substr(eq_pos + 1).strip_edges();

		// Remove inline comments (# after a value)
		int comment_pos = value.find(" #");
		if (comment_pos != -1) {
			value = value.substr(0, comment_pos).strip_edges();
		}

		// Handle quoted values
		bool is_literal = false;

		if ((value.begins_with("\"") && value.ends_with("\"")) ||
				(value.begins_with("'") && value.ends_with("'")) ||
				(value.begins_with("`") && value.ends_with("`"))) {
			if (value.begins_with("'")) {
				is_literal = true;
			}
			value = value.substr(1, value.length() - 2);
		}

		if (!is_literal) {
			// Replace escaped characters
			value = value.replace("\\n", "\n")
							.replace("\\t", "\t")
							.replace("\\r", "\r");

			// Variable Expansion ($VAR or ${VAR} or ${VAR:-default})
			int start_idx = 0;
			while ((start_idx = value.find("$", start_idx)) != -1) {
				if (start_idx > 0 && value[start_idx - 1] == '\\') {
					start_idx += 1;
					continue;
				}

				int idx = start_idx + 1;
				bool bracket = false;
				if (idx < value.length() && value[idx] == '{') {
					bracket = true;
					idx++;
				}
				int key_start = idx;
				while (idx < value.length() && ((value[idx] >= 'a' && value[idx] <= 'z') || (value[idx] >= 'A' && value[idx] <= 'Z') || (value[idx] >= '0' && value[idx] <= '9') || value[idx] == '_')) {
					idx++;
				}
				if (idx > key_start) {
					String var_key = value.substr(key_start, idx - key_start);
					String var_val;
					bool found = false;

					if (env.has(var_key)) {
						var_val = env[var_key];
						found = true;
					} else if (env_vars.has(var_key)) {
						var_val = env_vars[var_key];
						found = true;
					} else if (OS::get_singleton()->has_environment(var_key)) {
						var_val = OS::get_singleton()->get_environment(var_key);
						found = true;
					}

					String to_replace;

					if (bracket) {
						if (idx < value.length() && value[idx] == ':') {
							if (idx + 1 < value.length() && value[idx + 1] == '-') {
								idx += 2;
								int def_start = idx;
								while (idx < value.length() && value[idx] != '}') {
									idx++;
								}
								if (!found) {
									var_val = value.substr(def_start, idx - def_start);
								}
							}
						}
						if (idx < value.length() && value[idx] == '}') {
							idx++;
						}
					}

					to_replace = value.substr(start_idx, idx - start_idx);
					value = value.replace(to_replace, var_val);
				} else {
					start_idx++;
				}
			}

			value = value.replace("\\$", "$");
		}

		// Store the key-value pair
		env[key] = value;
		print_debug("Parsed env [" + key + "] = " + value);
	}

	return env;
}

Dictionary ENV::populate(const Dictionary &p_env, bool override) {
	Dictionary env;
	for (int i = 0; i < p_env.keys().size(); i++) {
		String key = p_env.keys()[i];
		Variant value = p_env[p_env.keys()[i]];

		if (env_vars.has(key)) {
			if (override) {
				env_vars[key] = value;
				print_debug("Updated env [" + key + "] = " + value.to_json_string());
				emit_signal("updated", key, value);
			}
		} else {
			env_vars[key] = value;
			print_debug("Added new env [" + key + "] = " + value.to_json_string());
			emit_signal("updated", key, value);
		}
	}
	return env;
}

Dictionary ENV::refresh(bool override) {
	for (int i = 0; i < env_files.size(); i++) {
		config(env_files[i], override);
	}
	emit_signal("refreshed", env_vars);
	return env_vars;
}
ENV::ENV() {
	env_singleton = this;
	true_terms.push_back("true");
	true_terms.push_back("yes");
	true_terms.push_back("1");

	false_terms.push_back("false");
	false_terms.push_back("no");
	false_terms.push_back("0");
}
ENV::~ENV() {
	custom_resolver = Callable();
	true_terms.clear();
	false_terms.clear();
	env_singleton = nullptr;
}
