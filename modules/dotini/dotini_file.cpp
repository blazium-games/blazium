/**************************************************************************/
/*  dotini_file.cpp                                                       */
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

#include "dotini_file.h"
#include "core/crypto/crypto_core.h"
#include "core/os/os.h"
#include "core/variant/variant_parser.h"

Ref<DotIniFile> DotIniFile::global_fallback = nullptr;

void DotIniFile::set_global_fallback(Ref<DotIniFile> p_fallback) {
	global_fallback = p_fallback;
}

Ref<DotIniFile> DotIniFile::get_global_fallback() {
	return global_fallback;
}

void DotIniFile::set_autosave(bool p_enabled) {
	MutexLock lock(mutex);
	autosave_enabled = p_enabled;
}

bool DotIniFile::is_autosave_enabled() const {
	MutexLock lock(mutex);
	return autosave_enabled;
}

void DotIniFile::set_read_only(bool p_read_only) {
	MutexLock lock(mutex);
	read_only = p_read_only;
}

bool DotIniFile::is_read_only() const {
	MutexLock lock(mutex);
	return read_only;
}

void DotIniFile::add_type_constraint(const String &p_section, const String &p_key, Variant::Type p_type) {
	MutexLock lock(mutex);
	if (!schema_constraints.has(p_section)) {
		schema_constraints[p_section] = HashMap<String, Variant::Type>();
	}
	schema_constraints[p_section][p_key] = p_type;
}

void DotIniFile::remove_type_constraint(const String &p_section, const String &p_key) {
	MutexLock lock(mutex);
	if (schema_constraints.has(p_section)) {
		schema_constraints[p_section].erase(p_key);
	}
}

void DotIniFile::add_macro(const String &p_name, const String &p_value) {
	MutexLock lock(mutex);
	internal_macros[p_name] = p_value;
}

String DotIniFile::get_macro(const String &p_name) const {
	MutexLock lock(mutex);
	return internal_macros.has(p_name) ? internal_macros[p_name] : "";
}

bool DotIniFile::check_for_updates() {
	MutexLock lock(mutex);
	if (file_path.is_empty()) {
		return false;
	}

	uint64_t current_modified = FileAccess::get_modified_time(file_path);
	if (current_modified > last_modified_time) {
		load(file_path);
		emit_signal("file_reloaded");
		return true;
	}

	for (int i = 0; i < included_files.size(); i++) {
		if (included_files[i].is_valid() && included_files[i]->check_for_updates()) {
			emit_signal("file_reloaded");
			return true;
		}
	}
	return false;
}

void DotIniFile::_bind_methods() {
	ClassDB::bind_static_method("DotIniFile", D_METHOD("set_global_fallback", "fallback"), &DotIniFile::set_global_fallback);
	ClassDB::bind_static_method("DotIniFile", D_METHOD("get_global_fallback"), &DotIniFile::get_global_fallback);

	ClassDB::bind_method(D_METHOD("set_autosave", "enabled"), &DotIniFile::set_autosave);
	ClassDB::bind_method(D_METHOD("is_autosave_enabled"), &DotIniFile::is_autosave_enabled);
	ClassDB::bind_method(D_METHOD("check_for_updates"), &DotIniFile::check_for_updates);

	ClassDB::bind_method(D_METHOD("set_read_only", "read_only"), &DotIniFile::set_read_only);
	ClassDB::bind_method(D_METHOD("is_read_only"), &DotIniFile::is_read_only);

	ClassDB::bind_method(D_METHOD("add_type_constraint", "section", "key", "type"), &DotIniFile::add_type_constraint);
	ClassDB::bind_method(D_METHOD("remove_type_constraint", "section", "key"), &DotIniFile::remove_type_constraint);
	ClassDB::bind_method(D_METHOD("add_macro", "name", "value"), &DotIniFile::add_macro);
	ClassDB::bind_method(D_METHOD("get_macro", "name"), &DotIniFile::get_macro);

	ClassDB::bind_method(D_METHOD("load", "path", "append"), &DotIniFile::load, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("save", "path"), &DotIniFile::save, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("save_all"), &DotIniFile::save_all);

	ClassDB::bind_method(D_METHOD("load_from_string", "string", "append"), &DotIniFile::load_from_string, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("save_to_string"), &DotIniFile::save_to_string);

	ClassDB::bind_method(D_METHOD("load_from_buffer", "buffer", "append"), &DotIniFile::load_from_buffer, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("save_to_buffer"), &DotIniFile::save_to_buffer);

	ClassDB::bind_method(D_METHOD("load_from_base64", "base64", "append"), &DotIniFile::load_from_base64, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("save_to_base64"), &DotIniFile::save_to_base64);

	ClassDB::bind_method(D_METHOD("load_encrypted", "path", "key", "append"), &DotIniFile::load_encrypted, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("save_encrypted", "path", "key"), &DotIniFile::save_encrypted);
	ClassDB::bind_method(D_METHOD("load_encrypted_pass", "path", "pass", "append"), &DotIniFile::load_encrypted_pass, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("save_encrypted_pass", "path", "pass"), &DotIniFile::save_encrypted_pass);

	ClassDB::bind_method(D_METHOD("merge_with", "other", "overwrite"), &DotIniFile::merge_with, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("set_default_padding", "pre_key", "post_key", "pre_val", "post_val"), &DotIniFile::set_default_padding);

	ClassDB::bind_method(D_METHOD("to_config_file"), &DotIniFile::to_config_file);
	ClassDB::bind_method(D_METHOD("from_config_file", "config"), &DotIniFile::from_config_file);

	ClassDB::bind_method(D_METHOD("to_dictionary"), &DotIniFile::to_dictionary);
	ClassDB::bind_method(D_METHOD("from_dictionary", "dict"), &DotIniFile::from_dictionary);

	ClassDB::bind_method(D_METHOD("set_value", "section", "key", "value"), &DotIniFile::set_value);
	ClassDB::bind_method(D_METHOD("set_value_b64", "section", "key", "utf8_string"), &DotIniFile::set_value_b64);

	ClassDB::bind_method(D_METHOD("get_value_array", "section", "key", "interpolate"), &DotIniFile::get_value_array, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("get_value", "section", "key", "default", "interpolate"), &DotIniFile::get_value, DEFVAL(Variant()), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("get_value_b64", "section", "key", "default"), &DotIniFile::get_value_b64, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("get_value_raw", "section", "key", "default"), &DotIniFile::get_value_raw, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("get_value_string", "section", "key", "default", "interpolate"), &DotIniFile::get_value_string, DEFVAL(""), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("get_value_int", "section", "key", "default", "interpolate"), &DotIniFile::get_value_int, DEFVAL(0), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("get_value_float", "section", "key", "default", "interpolate"), &DotIniFile::get_value_float, DEFVAL(0.0), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("get_value_bool", "section", "key", "default", "interpolate"), &DotIniFile::get_value_bool, DEFVAL(false), DEFVAL(true));

	ClassDB::bind_method(D_METHOD("set_section_comment", "section", "comment"), &DotIniFile::set_section_comment);
	ClassDB::bind_method(D_METHOD("set_key_comment", "section", "key", "comment"), &DotIniFile::set_key_comment);
	ClassDB::bind_method(D_METHOD("get_section_comment", "section"), &DotIniFile::get_section_comment);
	ClassDB::bind_method(D_METHOD("get_key_comment", "section", "key"), &DotIniFile::get_key_comment);

	ClassDB::bind_method(D_METHOD("has_section", "section"), &DotIniFile::has_section);
	ClassDB::bind_method(D_METHOD("has_section_key", "section", "key"), &DotIniFile::has_section_key);
	ClassDB::bind_method(D_METHOD("erase_section", "section"), &DotIniFile::erase_section);
	ClassDB::bind_method(D_METHOD("clear_section", "section"), &DotIniFile::clear_section);
	ClassDB::bind_method(D_METHOD("erase_section_key", "section", "key"), &DotIniFile::erase_section_key);
	ClassDB::bind_method(D_METHOD("get_sections"), &DotIniFile::get_sections);
	ClassDB::bind_method(D_METHOD("get_section_keys", "section"), &DotIniFile::get_section_keys);
	ClassDB::bind_method(D_METHOD("get_keys_matching", "section", "match_pattern"), &DotIniFile::get_keys_matching);

	ClassDB::bind_method(D_METHOD("rename_section", "old_section", "new_section"), &DotIniFile::rename_section);
	ClassDB::bind_method(D_METHOD("rename_key", "section", "old_key", "new_key"), &DotIniFile::rename_key);
	ClassDB::bind_method(D_METHOD("append_value", "section", "key", "value"), &DotIniFile::append_value);
	ClassDB::bind_method(D_METHOD("ensure_value", "section", "key", "default_value"), &DotIniFile::ensure_value);
	ClassDB::bind_method(D_METHOD("get_section_as_dict", "section"), &DotIniFile::get_section_as_dict);
	ClassDB::bind_method(D_METHOD("sort_section_keys", "section", "ascending"), &DotIniFile::sort_section_keys, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("clone"), &DotIniFile::clone);

	ClassDB::bind_method(D_METHOD("get_included_files"), &DotIniFile::get_included_files);
	ClassDB::bind_method(D_METHOD("clear"), &DotIniFile::clear);

	ADD_SIGNAL(MethodInfo("value_changed", PropertyInfo(Variant::STRING, "section"), PropertyInfo(Variant::STRING, "key"), PropertyInfo(Variant::NIL, "new_value")));
	ADD_SIGNAL(MethodInfo("section_erased", PropertyInfo(Variant::STRING, "section")));
	ADD_SIGNAL(MethodInfo("file_reloaded"));
}

DotIniFile::DotIniFile() {}

void DotIniFile::clear() {
	MutexLock lock(mutex);
	lines.clear();
	section_key_indices.clear();
	section_indices.clear();
	included_files.clear();
	file_path = "";
}

Error DotIniFile::load(const String &p_path, bool p_append) {
	MutexLock lock(mutex);
	if (!p_append) {
		clear();
	}
	file_path = p_path;

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return ERR_FILE_CANT_OPEN;
	}

	last_modified_time = FileAccess::get_modified_time(file_path);
	String current_section = "";

	while (!f->eof_reached()) {
		String line_str = f->get_line();

		IniLine line;
		line.raw_content = line_str;

		String trimmed = line_str.strip_edges();

		if (trimmed.is_empty()) {
			line.type = LINE_EMPTY;
		} else if (trimmed.begins_with(";") || trimmed.begins_with("#")) {
			if (trimmed.begins_with("#include")) {
				line.type = LINE_INCLUDE;
				int space_idx = trimmed.find(" ");
				if (space_idx != -1) {
					String inc_path = trimmed.substr(space_idx + 1).strip_edges();
					if (inc_path.begins_with("\"") && inc_path.ends_with("\"")) {
						inc_path = inc_path.substr(1, inc_path.length() - 2);
					}
					line.include_path = inc_path;

					Ref<DotIniFile> child_ini;
					child_ini.instantiate();
					String base_dir = p_path.get_base_dir();
					String full_child_path = base_dir.path_join(inc_path);
					Error err = child_ini->load(full_child_path);
					if (err == OK) {
						line.included_file = child_ini;
						included_files.push_back(child_ini);
					}
				}
			} else {
				line.type = LINE_COMMENT;
				line.value = trimmed;
			}
		} else if (trimmed.begins_with("[") && trimmed.ends_with("]")) {
			line.type = LINE_SECTION;
			line.section = trimmed.substr(1, trimmed.length() - 2);
			current_section = line.section;
		} else {
			int eq_pos = line_str.find("=");
			if (eq_pos != -1) {
				line.type = LINE_KEY_VALUE;
				line.section = current_section;
				String key_part = line_str.substr(0, eq_pos);
				String val_part = line_str.substr(eq_pos + 1);

				line.key = key_part.strip_edges();
				line.value = val_part.strip_edges();

				String pre_key_str = key_part.substr(0, key_part.find(line.key));
				String post_key_str = key_part.substr(key_part.find(line.key) + line.key.length());
				String pre_val_str = val_part.substr(0, val_part.find(line.value));
				String post_val_str = val_part.substr(val_part.find(line.value) + line.value.length());

				line.pre_key_space = pre_key_str;
				line.post_key_space = post_key_str;
				line.pre_val_space = pre_val_str;
				line.post_val_space = post_val_str;
			} else {
				line.type = LINE_UNKNOWN;
			}
		}

		lines.push_back(line);
	}

	_rebuild_indices();
	return OK;
}

Error DotIniFile::save(const String &p_path) {
	MutexLock lock(mutex);
	String target_path = p_path.is_empty() ? file_path : p_path;
	if (target_path.is_empty()) {
		return ERR_FILE_BAD_PATH;
	}

	Ref<FileAccess> f = FileAccess::open(target_path, FileAccess::WRITE);
	if (f.is_null()) {
		return ERR_FILE_CANT_WRITE;
	}

	f->store_string(save_to_string());
	f->close();

	if (target_path == file_path) {
		last_modified_time = FileAccess::get_modified_time(target_path);
	}

	return OK;
}

Error DotIniFile::save_all() {
	MutexLock lock(mutex);
	Error err = save();
	if (err != OK) {
		return err;
	}
	for (int i = 0; i < included_files.size(); i++) {
		if (included_files[i].is_valid()) {
			err = included_files[i]->save_all();
			if (err != OK) {
				return err;
			}
		}
	}
	return OK;
}

void DotIniFile::_rebuild_indices() {
	section_key_indices.clear();
	section_indices.clear();

	String current_section = "";
	for (int i = 0; i < lines.size(); i++) {
		const IniLine &line = lines[i];
		if (line.type == LINE_SECTION) {
			current_section = line.section;
			section_indices[current_section] = i;
		} else if (line.type == LINE_KEY_VALUE) {
			if (!section_key_indices.has(current_section)) {
				section_key_indices[current_section] = HashMap<String, Vector<int>>();
			}
			section_key_indices[current_section][line.key].push_back(i);
		}
	}
}

int DotIniFile::_find_last_comment_block(int p_target_idx) const {
	int first_comment_idx = p_target_idx;
	while (first_comment_idx > 0 && lines[first_comment_idx - 1].type == LINE_COMMENT) {
		first_comment_idx--;
	}
	return first_comment_idx;
}

void DotIniFile::set_value(const String &p_section, const String &p_key, const Variant &p_value) {
	MutexLock lock(mutex);
	if (read_only) {
		return;
	}

	if (schema_constraints.has(p_section) && schema_constraints[p_section].has(p_key)) {
		if (p_value.get_type() != schema_constraints[p_section][p_key]) {
			return; // Type mismatch, drop safely
		}
	}

	Ref<DotIniFile> owner = _get_owner_of_key(p_section, p_key);
	if (owner.is_valid() && owner != this) {
		owner->set_value(p_section, p_key, p_value);
		return;
	}

	String val_str;
	VariantWriter::write_to_string(p_value, val_str);

	if (section_key_indices.has(p_section) && section_key_indices[p_section].has(p_key)) {
		int line_idx = section_key_indices[p_section][p_key][0]; // Update the first instance
		lines.write[line_idx].value = val_str;

		String raw = lines[line_idx].pre_key_space + lines[line_idx].key + lines[line_idx].post_key_space +
				"=" + lines[line_idx].pre_val_space + val_str + lines[line_idx].post_val_space;
		lines.write[line_idx].raw_content = raw;
	} else {
		// New Key
		IniLine new_line;
		new_line.type = LINE_KEY_VALUE;
		new_line.section = p_section;
		new_line.key = p_key;
		new_line.value = val_str;
		new_line.pre_key_space = default_pre_key_space;
		new_line.post_key_space = default_post_key_space;
		new_line.pre_val_space = default_pre_val_space;
		new_line.post_val_space = default_post_val_space;

		String raw = default_pre_key_space + p_key + default_post_key_space + "=" + default_pre_val_space + val_str + default_post_val_space;
		new_line.raw_content = raw;

		if (!section_indices.has(p_section)) {
			// New Section
			IniLine sec_line;
			sec_line.type = LINE_SECTION;
			sec_line.section = p_section;
			sec_line.raw_content = "[" + p_section + "]";

			if (lines.size() > 0) {
				IniLine empty;
				empty.type = LINE_EMPTY;
				lines.push_back(empty);
			}

			lines.push_back(sec_line);
			lines.push_back(new_line);
		} else {
			// Insert under section
			int insert_idx = lines.size();
			for (int i = section_indices[p_section] + 1; i < lines.size(); i++) {
				if (lines[i].type == LINE_SECTION) {
					insert_idx = i;
					break;
				}
			}
			lines.insert(insert_idx, new_line);
		}

		_rebuild_indices();
	}

	emit_signal("value_changed", p_section, p_key, p_value);

	if (autosave_enabled) {
		save_all();
	}
}

void DotIniFile::set_value_b64(const String &p_section, const String &p_key, const String &p_utf8_string) {
	CharString u8 = p_utf8_string.utf8();
	String b64 = CryptoCore::b64_encode_str((const uint8_t *)u8.get_data(), u8.length());
	set_value(p_section, p_key, b64);
}

void DotIniFile::append_value(const String &p_section, const String &p_key, const Variant &p_value) {
	MutexLock lock(mutex);
	if (read_only) {
		return;
	}

	if (schema_constraints.has(p_section) && schema_constraints[p_section].has(p_key)) {
		if (p_value.get_type() != schema_constraints[p_section][p_key]) {
			return; // Type mismatch, drop safely
		}
	}

	String val_str;
	VariantWriter::write_to_string(p_value, val_str);

	IniLine new_line;
	new_line.type = LINE_KEY_VALUE;
	new_line.section = p_section;
	new_line.key = p_key;
	new_line.value = val_str;
	new_line.pre_key_space = default_pre_key_space;
	new_line.post_key_space = default_post_key_space;
	new_line.pre_val_space = default_pre_val_space;
	new_line.post_val_space = default_post_val_space;

	String raw = default_pre_key_space + p_key + default_post_key_space + "=" + default_pre_val_space + val_str + default_post_val_space;
	new_line.raw_content = raw;

	if (!section_indices.has(p_section)) {
		// New Section
		IniLine sec_line;
		sec_line.type = LINE_SECTION;
		sec_line.section = p_section;
		sec_line.raw_content = "[" + p_section + "]";

		if (lines.size() > 0) {
			IniLine empty;
			empty.type = LINE_EMPTY;
			lines.push_back(empty);
		}

		lines.push_back(sec_line);
		lines.push_back(new_line);
	} else {
		// Insert under section (append at bottom)
		int insert_idx = lines.size();
		for (int i = section_indices[p_section] + 1; i < lines.size(); i++) {
			if (lines[i].type == LINE_SECTION) {
				insert_idx = i;
				break;
			}
		}
		lines.insert(insert_idx, new_line);
	}

	_rebuild_indices();

	emit_signal("value_changed", p_section, p_key, p_value);

	if (autosave_enabled) {
		save_all();
	}
}

Variant DotIniFile::ensure_value(const String &p_section, const String &p_key, const Variant &p_default_value) {
	if (has_section_key(p_section, p_key)) {
		return get_value(p_section, p_key, p_default_value);
	}
	set_value(p_section, p_key, p_default_value);
	return p_default_value;
}

Dictionary DotIniFile::get_section_as_dict(const String &p_section) const {
	MutexLock lock(mutex);
	Dictionary dict;
	if (section_key_indices.has(p_section)) {
		for (const KeyValue<String, Vector<int>> &K : section_key_indices[p_section]) {
			dict[K.key] = get_value(p_section, K.key);
		}
	}
	return dict;
}

Array DotIniFile::get_value_array(const String &p_section, const String &p_key, bool p_interpolate) const {
	MutexLock lock(mutex);
	Array result;
	Ref<DotIniFile> owner = _get_owner_of_key(p_section, p_key);
	if (!owner.is_valid()) {
		if (global_fallback.is_valid() && global_fallback != this) {
			return global_fallback->get_value_array(p_section, p_key, p_interpolate);
		}
		return result;
	}

	Vector<int> indices = owner->section_key_indices[p_section][p_key];
	for (int i = 0; i < indices.size(); ++i) {
		String val_str = owner->lines[indices[i]].value;
		if (p_interpolate) {
			val_str = owner->_interpolate_value(val_str, p_section, 0);
		}
		Variant ret;
		String err_str;
		int err_line;
		VariantParser::StreamString ss;
		ss.s = val_str;
		if (VariantParser::parse(&ss, ret, err_str, err_line) == OK) {
			result.push_back(ret);
		} else {
			result.push_back(val_str);
		}
	}
	return result;
}

Variant DotIniFile::_interpolate_value(const String &p_original_value, const String &p_current_section, int p_depth) const {
	if (p_depth > 10) {
		return p_original_value;
	}

	String result = p_original_value;
	int start = 0;
	while ((start = result.find("%(", start)) != -1) {
		int end = result.find(")s", start);
		if (end != -1) {
			String macro_name = result.substr(start + 2, end - start - 2);
			String replacement;

			if (macro_name.begins_with("ENV:")) {
				String env_var = macro_name.substr(4);
				replacement = OS::get_singleton()->get_environment(env_var);
			} else if (macro_name.begins_with("MACRO:")) {
				String mac_var = macro_name.substr(6);
				if (internal_macros.has(mac_var)) {
					replacement = internal_macros[mac_var];
				}
			} else {
				String lookup_sec = p_current_section;
				String lookup_key = macro_name;

				int dot_idx = macro_name.find(".");
				if (dot_idx != -1) {
					lookup_sec = macro_name.substr(0, dot_idx);
					lookup_key = macro_name.substr(dot_idx + 1);
				}

				if (has_section_key(lookup_sec, lookup_key)) {
					Variant raw_val = get_value(lookup_sec, lookup_key, Variant(), false);
					replacement = raw_val.operator String();
					replacement = _interpolate_value(replacement, lookup_sec, p_depth + 1);
				}
			}

			result = result.replace("%(" + macro_name + ")s", replacement);
			start += replacement.length();
		} else {
			start += 2;
		}
	}

	return result;
}

Ref<DotIniFile> DotIniFile::_get_owner_of_key(const String &p_section, const String &p_key) const {
	if (section_key_indices.has(p_section) && section_key_indices[p_section].has(p_key)) {
		return Ref<DotIniFile>(const_cast<DotIniFile *>(this));
	}
	for (int i = 0; i < included_files.size(); i++) {
		if (included_files[i].is_valid()) {
			Ref<DotIniFile> owner = included_files[i]->_get_owner_of_key(p_section, p_key);
			if (owner.is_valid()) {
				return owner;
			}
		}
	}
	return Ref<DotIniFile>();
}

Variant DotIniFile::get_value(const String &p_section, const String &p_key, const Variant &p_default, bool p_interpolate) const {
	MutexLock lock(mutex);
	Ref<DotIniFile> owner = _get_owner_of_key(p_section, p_key);
	if (!owner.is_valid()) {
		if (global_fallback.is_valid() && global_fallback != this) {
			return global_fallback->get_value(p_section, p_key, p_default, p_interpolate);
		}
		return p_default;
	}

	String val_str = owner->lines[owner->section_key_indices[p_section][p_key][0]].value;
	if (p_interpolate) {
		val_str = owner->_interpolate_value(val_str, p_section, 0);
	}

	Variant ret;
	String err_str;
	int err_line;

	VariantParser::StreamString ss;
	ss.s = val_str;
	Error err = VariantParser::parse(&ss, ret, err_str, err_line);
	if (err == OK) {
		return ret;
	}
	return val_str;
}

String DotIniFile::get_value_b64(const String &p_section, const String &p_key, const String &p_default) const {
	String b64 = get_value_string(p_section, p_key, "");
	if (b64.is_empty()) {
		return p_default;
	}

	CharString u8 = b64.utf8();
	size_t decoded_len = 0;
	PackedByteArray decoded;
	decoded.resize(u8.length());
	Error err = CryptoCore::b64_decode(decoded.ptrw(), decoded.size(), &decoded_len, (const uint8_t *)u8.get_data(), u8.length());
	if (err == OK) {
		return String::utf8((const char *)decoded.ptr(), decoded_len);
	}
	return p_default;
}

String DotIniFile::get_value_raw(const String &p_section, const String &p_key, const String &p_default) const {
	MutexLock lock(mutex);
	Ref<DotIniFile> owner = _get_owner_of_key(p_section, p_key);
	if (owner.is_valid()) {
		return owner->lines[owner->section_key_indices[p_section][p_key][0]].value;
	}
	return p_default;
}

String DotIniFile::get_value_string(const String &p_section, const String &p_key, const String &p_default, bool p_interpolate) const {
	return get_value(p_section, p_key, p_default, p_interpolate);
}

int DotIniFile::get_value_int(const String &p_section, const String &p_key, int p_default, bool p_interpolate) const {
	return get_value(p_section, p_key, p_default, p_interpolate);
}

float DotIniFile::get_value_float(const String &p_section, const String &p_key, float p_default, bool p_interpolate) const {
	return get_value(p_section, p_key, p_default, p_interpolate);
}

bool DotIniFile::get_value_bool(const String &p_section, const String &p_key, bool p_default, bool p_interpolate) const {
	return get_value(p_section, p_key, p_default, p_interpolate);
}

PackedStringArray DotIniFile::get_keys_matching(const String &p_section, const String &p_match_pattern) const {
	MutexLock lock(mutex);
	PackedStringArray arr;
	if (section_key_indices.has(p_section)) {
		for (const KeyValue<String, Vector<int>> &E : section_key_indices[p_section]) {
			if (p_match_pattern.is_empty() || E.key.match(p_match_pattern)) {
				arr.push_back(E.key);
			}
		}
	}
	return arr;
}

void DotIniFile::set_section_comment(const String &p_section, const String &p_comment) {
	MutexLock lock(mutex);
	if (read_only) {
		return;
	}
	if (!section_indices.has(p_section)) {
		return;
	}

	int sec_idx = section_indices[p_section];
	int first_comment_idx = _find_last_comment_block(sec_idx);

	if (first_comment_idx < sec_idx) {
		for (int i = 0; i < sec_idx - first_comment_idx; i++) {
			lines.remove_at(first_comment_idx);
		}
	}

	if (!p_comment.is_empty()) {
		Vector<String> c_lines = p_comment.split("\n");
		for (int i = 0; i < c_lines.size(); i++) {
			IniLine cl;
			cl.type = LINE_COMMENT;
			cl.value = ";" + c_lines[i];
			cl.raw_content = cl.value;
			lines.insert(first_comment_idx + i, cl);
		}
	}

	_rebuild_indices();
	if (autosave_enabled) {
		save_all();
	}
}

void DotIniFile::set_key_comment(const String &p_section, const String &p_key, const String &p_comment) {
	MutexLock lock(mutex);
	if (read_only) {
		return;
	}
	if (!section_key_indices.has(p_section) || !section_key_indices[p_section].has(p_key)) {
		return;
	}

	int key_idx = section_key_indices[p_section][p_key][0];
	int first_comment_idx = _find_last_comment_block(key_idx);

	if (first_comment_idx < key_idx) {
		for (int i = 0; i < key_idx - first_comment_idx; i++) {
			lines.remove_at(first_comment_idx);
		}
	}

	if (!p_comment.is_empty()) {
		Vector<String> c_lines = p_comment.split("\n");
		for (int i = 0; i < c_lines.size(); i++) {
			IniLine cl;
			cl.type = LINE_COMMENT;
			cl.value = ";" + c_lines[i];
			cl.raw_content = cl.value;
			lines.insert(first_comment_idx + i, cl);
		}
	}

	_rebuild_indices();
	if (autosave_enabled) {
		save_all();
	}
}

String DotIniFile::get_section_comment(const String &p_section) const {
	MutexLock lock(mutex);
	if (!section_indices.has(p_section)) {
		return "";
	}

	int sec_idx = section_indices[p_section];
	int first_comment_idx = _find_last_comment_block(sec_idx);

	String comment = "";
	for (int i = first_comment_idx; i < sec_idx; i++) {
		if (i > first_comment_idx) {
			comment += "\n";
		}
		String lc = lines[i].value;
		if (lc.begins_with(";") || lc.begins_with("#")) {
			lc = lc.substr(1);
		}
		comment += lc;
	}
	return comment;
}

String DotIniFile::get_key_comment(const String &p_section, const String &p_key) const {
	MutexLock lock(mutex);
	if (!section_key_indices.has(p_section) || !section_key_indices[p_section].has(p_key)) {
		return "";
	}

	int key_idx = section_key_indices[p_section][p_key][0];
	int first_comment_idx = _find_last_comment_block(key_idx);

	String comment = "";
	for (int i = first_comment_idx; i < key_idx; i++) {
		if (i > first_comment_idx) {
			comment += "\n";
		}
		String lc = lines[i].value;
		if (lc.begins_with(";") || lc.begins_with("#")) {
			lc = lc.substr(1);
		}
		comment += lc;
	}
	return comment;
}

bool DotIniFile::has_section(const String &p_section) const {
	MutexLock lock(mutex);
	if (section_indices.has(p_section)) {
		return true;
	}
	for (int i = 0; i < included_files.size(); i++) {
		if (included_files[i].is_valid() && included_files[i]->has_section(p_section)) {
			return true;
		}
	}
	return false;
}

bool DotIniFile::has_section_key(const String &p_section, const String &p_key) const {
	MutexLock lock(mutex);
	Ref<DotIniFile> owner = _get_owner_of_key(p_section, p_key);
	return owner.is_valid();
}

void DotIniFile::erase_section(const String &p_section) {
	MutexLock lock(mutex);
	if (read_only) {
		return;
	}
	Ref<DotIniFile> target = this;
	if (!section_indices.has(p_section)) {
		for (int i = 0; i < included_files.size(); i++) {
			if (included_files[i].is_valid() && included_files[i]->has_section(p_section)) {
				target = included_files[i];
				break;
			}
		}
	}

	if (!target->section_indices.has(p_section)) {
		return;
	}

	int sec_idx = target->section_indices[p_section];
	int end_idx = target->lines.size() - 1;

	for (int i = sec_idx + 1; i < target->lines.size(); i++) {
		if (target->lines[i].type == LINE_SECTION) {
			end_idx = i - 1;
			break;
		}
	}

	int start_del = target->_find_last_comment_block(sec_idx);
	for (int i = 0; i <= end_idx - start_del; i++) {
		target->lines.remove_at(start_del);
	}

	target->_rebuild_indices();

	emit_signal("section_erased", p_section);
	if (autosave_enabled) {
		save_all();
	}
}

void DotIniFile::erase_section_key(const String &p_section, const String &p_key) {
	MutexLock lock(mutex);
	if (read_only) {
		return;
	}
	Ref<DotIniFile> owner = _get_owner_of_key(p_section, p_key);
	if (!owner.is_valid()) {
		return;
	}

	Vector<int> indices = owner->section_key_indices[p_section][p_key];
	// Delete all instances since we track them all
	for (int j = indices.size() - 1; j >= 0; --j) {
		int key_idx = indices[j];
		int start_del = owner->_find_last_comment_block(key_idx);
		for (int i = 0; i <= key_idx - start_del; i++) {
			owner->lines.remove_at(start_del);
		}
	}
	owner->_rebuild_indices();
	if (autosave_enabled) {
		save_all();
	}
}

void DotIniFile::clear_section(const String &p_section) {
	MutexLock lock(mutex);
	if (read_only) {
		return;
	}
	if (!section_indices.has(p_section)) {
		return;
	}

	int sec_idx = section_indices[p_section];
	int end_idx = lines.size() - 1;
	for (int i = sec_idx + 1; i < lines.size(); i++) {
		if (lines[i].type == LINE_SECTION) {
			end_idx = i - 1;
			break;
		}
	}

	for (int i = end_idx; i > sec_idx; i--) {
		lines.remove_at(i);
	}
	_rebuild_indices();
	emit_signal("section_erased", p_section);
	if (autosave_enabled) {
		save_all();
	}
}

void DotIniFile::rename_section(const String &p_old_section, const String &p_new_section) {
	MutexLock lock(mutex);
	if (read_only) {
		return;
	}
	if (!section_indices.has(p_old_section)) {
		return;
	}
	if (section_indices.has(p_new_section)) {
		return; // Prevent merging collision natively for rename
	}

	int idx = section_indices[p_old_section];
	lines.write[idx].section = p_new_section;
	lines.write[idx].raw_content = "[" + p_new_section + "]";

	for (int i = idx + 1; i < lines.size(); i++) {
		if (lines[i].type == LINE_SECTION) {
			break;
		}
		if (lines[i].type == LINE_KEY_VALUE) {
			lines.write[i].section = p_new_section;
		}
	}
	_rebuild_indices();
	if (autosave_enabled) {
		save_all();
	}
}

void DotIniFile::rename_key(const String &p_section, const String &p_old_key, const String &p_new_key) {
	MutexLock lock(mutex);
	if (read_only) {
		return;
	}
	if (!section_key_indices.has(p_section) || !section_key_indices[p_section].has(p_old_key)) {
		return;
	}
	if (section_key_indices[p_section].has(p_new_key)) {
		return;
	}

	Vector<int> &idx_arr = section_key_indices[p_section][p_old_key];
	for (int i = 0; i < idx_arr.size(); i++) {
		int idx = idx_arr[i];
		lines.write[idx].key = p_new_key;
		lines.write[idx].raw_content = lines[idx].pre_key_space + p_new_key + lines[idx].post_key_space + "=" + lines[idx].pre_val_space + lines[idx].value + lines[idx].post_val_space;
	}
	_rebuild_indices();

	// Resignal it as a change generically
	// emit_signal("value_changed", p_section, p_new_key, get_value(p_section, p_new_key));
	if (autosave_enabled) {
		save_all();
	}
}

void DotIniFile::sort_section_keys(const String &p_section, bool p_ascending) {
	MutexLock lock(mutex);
	if (read_only || !section_indices.has(p_section)) {
		return;
	}

	int sec_idx = section_indices[p_section];
	int end_idx = lines.size() - 1;
	for (int i = sec_idx + 1; i < lines.size(); i++) {
		if (lines[i].type == LINE_SECTION) {
			end_idx = i - 1;
			break;
		}
	}

	Vector<IniLine> extracted_keys;
	Vector<int> extracted_indices;
	for (int i = sec_idx + 1; i <= end_idx; i++) {
		if (lines[i].type == LINE_KEY_VALUE) {
			extracted_keys.push_back(lines[i]);
			extracted_indices.push_back(i);
		}
	}
	for (int i = 0; i < extracted_keys.size(); i++) {
		for (int j = i + 1; j < extracted_keys.size(); j++) {
			bool swap = p_ascending ? (extracted_keys[i].key > extracted_keys[j].key) : (extracted_keys[i].key < extracted_keys[j].key);
			if (swap) {
				IniLine temp = extracted_keys[i];
				extracted_keys.write[i] = extracted_keys[j];
				extracted_keys.write[j] = temp;
			}
		}
	}
	for (int i = 0; i < extracted_keys.size(); i++) {
		lines.write[extracted_indices[i]] = extracted_keys[i];
	}
	_rebuild_indices();
	if (autosave_enabled) {
		save_all();
	}
}

Ref<DotIniFile> DotIniFile::clone() const {
	MutexLock lock(mutex);
	Ref<DotIniFile> dup;
	dup.instantiate();
	dup->lines = lines; // Vector assignment handles structural duplication safely natively!
	dup->section_key_indices = section_key_indices;
	dup->section_indices = section_indices;
	dup->internal_macros = internal_macros;
	dup->schema_constraints = schema_constraints;
	dup->autosave_enabled = autosave_enabled;
	dup->read_only = read_only;
	dup->file_path = file_path;
	dup->default_pre_key_space = default_pre_key_space;
	dup->default_post_key_space = default_post_key_space;
	dup->default_pre_val_space = default_pre_val_space;
	dup->default_post_val_space = default_post_val_space;
	dup->included_files = included_files; // Vectors in Godot are automatically COW duplicated safely
	return dup;
}

PackedStringArray DotIniFile::get_sections() const {
	MutexLock lock(mutex);
	PackedStringArray arr;
	for (const KeyValue<String, int> &E : section_indices) {
		arr.push_back(E.key);
	}
	return arr;
}

PackedStringArray DotIniFile::get_section_keys(const String &p_section) const {
	MutexLock lock(mutex);
	Ref<DotIniFile> target(const_cast<DotIniFile *>(this));
	if (!section_key_indices.has(p_section)) {
		for (int i = 0; i < included_files.size(); i++) {
			if (included_files[i].is_valid() && included_files[i]->has_section(p_section)) {
				target = included_files[i];
				break;
			}
		}
	}

	PackedStringArray arr;
	if (target->section_key_indices.has(p_section)) {
		for (const KeyValue<String, Vector<int>> &E : target->section_key_indices[p_section]) {
			arr.push_back(E.key);
		}
	}
	return arr;
}

TypedArray<DotIniFile> DotIniFile::get_included_files() const {
	MutexLock lock(mutex);
	TypedArray<DotIniFile> arr;
	for (int i = 0; i < included_files.size(); i++) {
		arr.push_back(included_files[i]);
	}
	return arr;
}

Error DotIniFile::load_from_string(const String &p_string, bool p_append) {
	MutexLock lock(mutex);
	if (!p_append) {
		clear();
	}

	Vector<String> line_strs = p_string.split("\n");
	String current_section = "";

	for (int i = 0; i < line_strs.size(); i++) {
		String line_str = line_strs[i];
		if (line_str.ends_with("\r")) {
			line_str = line_str.substr(0, line_str.length() - 1);
		}

		IniLine line;
		line.raw_content = line_str;

		String trimmed = line_str.strip_edges();

		if (trimmed.is_empty()) {
			line.type = LINE_EMPTY;
		} else if (trimmed.begins_with(";") || trimmed.begins_with("#")) {
			if (trimmed.begins_with("#include")) {
				line.type = LINE_INCLUDE;
				int space_idx = trimmed.find(" ");
				if (space_idx != -1) {
					String inc_path = trimmed.substr(space_idx + 1).strip_edges();
					if (inc_path.begins_with("\"") && inc_path.ends_with("\"")) {
						inc_path = inc_path.substr(1, inc_path.length() - 2);
					}
					line.include_path = inc_path;
				}
			} else {
				line.type = LINE_COMMENT;
				line.value = trimmed;
			}
		} else if (trimmed.begins_with("[") && trimmed.ends_with("]")) {
			line.type = LINE_SECTION;
			line.section = trimmed.substr(1, trimmed.length() - 2);
			current_section = line.section;
		} else {
			int eq_pos = line_str.find("=");
			if (eq_pos != -1) {
				line.type = LINE_KEY_VALUE;
				line.section = current_section;
				String key_part = line_str.substr(0, eq_pos);
				String val_part = line_str.substr(eq_pos + 1);

				line.key = key_part.strip_edges();
				line.value = val_part.strip_edges();

				line.pre_key_space = key_part.substr(0, key_part.find(line.key));
				line.post_key_space = key_part.substr(key_part.find(line.key) + line.key.length());
				line.pre_val_space = val_part.substr(0, val_part.find(line.value));
				line.post_val_space = val_part.substr(val_part.find(line.value) + line.value.length());
			} else {
				line.type = LINE_UNKNOWN;
			}
		}

		lines.push_back(line);
	}

	_rebuild_indices();
	return OK;
}

String DotIniFile::save_to_string() const {
	MutexLock lock(mutex);
	String body = "";
	for (int i = 0; i < lines.size(); i++) {
		body += lines[i].raw_content;
		if (i < lines.size() - 1) {
			body += "\n";
		}
	}
	return body;
}

Error DotIniFile::load_from_base64(const String &p_base64, bool p_append) {
	CharString u8 = p_base64.utf8();
	size_t decoded_len = 0;
	PackedByteArray decoded;
	decoded.resize(u8.length()); // safe max length
	Error err = CryptoCore::b64_decode(decoded.ptrw(), decoded.size(), &decoded_len, (const uint8_t *)u8.get_data(), u8.length());
	if (err == OK) {
		String converted = String::utf8((const char *)decoded.ptr(), decoded_len);
		return load_from_string(converted, p_append);
	}
	return err;
}

String DotIniFile::save_to_base64() const {
	String payload = save_to_string();
	CharString u8 = payload.utf8();
	return CryptoCore::b64_encode_str((const uint8_t *)u8.get_data(), u8.length());
}

Error DotIniFile::load_from_buffer(const PackedByteArray &p_buffer, bool p_append) {
	String parsed;
	parsed.parse_utf8((const char *)p_buffer.ptr(), p_buffer.size());
	return load_from_string(parsed, p_append);
}

PackedByteArray DotIniFile::save_to_buffer() const {
	String p = save_to_string();
	CharString u8 = p.utf8();
	PackedByteArray buf;
	buf.resize(u8.length());
	memcpy(buf.ptrw(), u8.get_data(), u8.length());
	return buf;
}

Error DotIniFile::load_encrypted(const String &p_path, const PackedByteArray &p_key, bool p_append) {
	Ref<FileAccess> f = FileAccess::open_encrypted(p_path, FileAccess::READ, p_key);
	if (f.is_null()) {
		return ERR_FILE_CANT_OPEN;
	}

	uint64_t len = f->get_length();
	PackedByteArray buffer;
	buffer.resize(len);
	f->get_buffer(buffer.ptrw(), len);

	Error err = load_from_buffer(buffer, p_append);
	if (err == OK && !p_append) {
		file_path = p_path;
	}

	return err;
}

Error DotIniFile::save_encrypted(const String &p_path, const PackedByteArray &p_key) {
	MutexLock lock(mutex);
	String target_path = p_path.is_empty() ? file_path : p_path;
	if (target_path.is_empty()) {
		return ERR_FILE_BAD_PATH;
	}

	Ref<FileAccess> f = FileAccess::open_encrypted(target_path, FileAccess::WRITE, p_key);
	if (f.is_null()) {
		return ERR_FILE_CANT_WRITE;
	}

	PackedByteArray buffer = save_to_buffer();
	f->store_buffer(buffer.ptr(), buffer.size());

	return OK;
}

Error DotIniFile::load_encrypted_pass(const String &p_path, const String &p_pass, bool p_append) {
	Ref<FileAccess> f = FileAccess::open_encrypted_pass(p_path, FileAccess::READ, p_pass);
	if (f.is_null()) {
		return ERR_FILE_CANT_OPEN;
	}

	uint64_t len = f->get_length();
	PackedByteArray buffer;
	buffer.resize(len);
	f->get_buffer(buffer.ptrw(), len);

	Error err = load_from_buffer(buffer, p_append);
	if (err == OK && !p_append) {
		file_path = p_path;
	}

	return err;
}

Error DotIniFile::save_encrypted_pass(const String &p_path, const String &p_pass) {
	MutexLock lock(mutex);
	String target_path = p_path.is_empty() ? file_path : p_path;
	if (target_path.is_empty()) {
		return ERR_FILE_BAD_PATH;
	}

	Ref<FileAccess> f = FileAccess::open_encrypted_pass(target_path, FileAccess::WRITE, p_pass);
	if (f.is_null()) {
		return ERR_FILE_CANT_WRITE;
	}

	PackedByteArray buffer = save_to_buffer();
	f->store_buffer(buffer.ptr(), buffer.size());

	return OK;
}

void DotIniFile::set_default_padding(const String &p_pre_key, const String &p_post_key, const String &p_pre_val, const String &p_post_val) {
	MutexLock lock(mutex);
	default_pre_key_space = p_pre_key;
	default_post_key_space = p_post_key;
	default_pre_val_space = p_pre_val;
	default_post_val_space = p_post_val;
}

void DotIniFile::merge_with(Ref<DotIniFile> p_other, bool p_overwrite) {
	MutexLock lock(mutex);
	if (p_other.is_null() || read_only) {
		return;
	}

	PackedStringArray secs = p_other->get_sections();
	for (int i = 0; i < secs.size(); i++) {
		String sec = secs[i];
		PackedStringArray keys = p_other->get_section_keys(sec);
		for (int j = 0; j < keys.size(); j++) {
			String key = keys[j];
			if (p_overwrite || !has_section_key(sec, key)) {
				// if repeated keys exist
				Array pulls = p_other->get_value_array(sec, key, false);
				if (pulls.size() > 1) {
					// We might have multiple repeats, let's just insert standard set_value for now (merges first one)
					set_value(sec, key, pulls[0]);
				} else {
					set_value(sec, key, p_other->get_value(sec, key, Variant(), false));
				}
			}
		}
	}
	if (autosave_enabled) {
		save_all();
	}
}

Ref<ConfigFile> DotIniFile::to_config_file() const {
	MutexLock lock(mutex);
	Ref<ConfigFile> cf;
	cf.instantiate();

	for (const KeyValue<String, HashMap<String, Vector<int>>> &E : section_key_indices) {
		for (const KeyValue<String, Vector<int>> &K : E.value) {
			cf->set_value(E.key, K.key, get_value(E.key, K.key));
		}
	}
	return cf;
}

void DotIniFile::from_config_file(Ref<ConfigFile> p_config) {
	MutexLock lock(mutex);
	if (p_config.is_null() || read_only) {
		return;
	}

	List<String> secs_list;
	p_config->get_sections(&secs_list);
	for (const String &sec : secs_list) {
		List<String> keys_list;
		p_config->get_section_keys(sec, &keys_list);
		for (const String &key : keys_list) {
			set_value(sec, key, p_config->get_value(sec, key));
		}
	}
	if (autosave_enabled) {
		save_all();
	}
}

Dictionary DotIniFile::to_dictionary() const {
	MutexLock lock(mutex);
	Dictionary dict;
	for (const KeyValue<String, HashMap<String, Vector<int>>> &E : section_key_indices) {
		Dictionary sec_dict;
		for (const KeyValue<String, Vector<int>> &K : E.value) {
			sec_dict[K.key] = get_value(E.key, K.key);
		}
		dict[E.key] = sec_dict;
	}
	return dict;
}

void DotIniFile::from_dictionary(const Dictionary &p_dict) {
	MutexLock lock(mutex);
	if (read_only) {
		return;
	}
	Array keys = p_dict.keys();
	for (int i = 0; i < keys.size(); i++) {
		String sec = keys[i];
		if (p_dict[sec].get_type() == Variant::DICTIONARY) {
			Dictionary sec_dict = p_dict[sec];
			Array sec_keys = sec_dict.keys();
			for (int j = 0; j < sec_keys.size(); j++) {
				String key = sec_keys[j];
				set_value(sec, key, sec_dict[key]);
			}
		}
	}
	if (autosave_enabled) {
		save_all();
	}
}
