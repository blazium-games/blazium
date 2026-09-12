/**************************************************************************/
/*  dotini_file.h                                                         */
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

#include "core/io/config_file.h"
#include "core/io/file_access.h"
#include "core/object/class_db.h"
#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "core/variant/dictionary.h"
#include "core/variant/typed_array.h"

class DotIniFile : public RefCounted {
	GDCLASS(DotIniFile, RefCounted);

public:
	enum LineType {
		LINE_SECTION,
		LINE_KEY_VALUE,
		LINE_COMMENT,
		LINE_EMPTY,
		LINE_INCLUDE,
		LINE_UNKNOWN
	};

	struct IniLine {
		LineType type = LINE_EMPTY;
		String raw_content;
		String section;
		String key;
		String value;

		// Formatting preservation
		String pre_key_space;
		String post_key_space;
		String pre_val_space;
		String post_val_space;

		// For includes
		Ref<DotIniFile> included_file;
		String include_path;
	};

private:
	mutable Mutex mutex;

	Vector<IniLine> lines;
	HashMap<String, HashMap<String, Vector<int>>> section_key_indices;
	HashMap<String, int> section_indices;

	Vector<Ref<DotIniFile>> included_files;
	String file_path;

	String default_pre_key_space = "";
	String default_post_key_space = "";
	String default_pre_val_space = "";
	String default_post_val_space = "";

	bool autosave_enabled = false;
	uint64_t last_modified_time = 0;
	static Ref<DotIniFile> global_fallback;

	// Phase 5 additions
	bool read_only = false;
	HashMap<String, HashMap<String, Variant::Type>> schema_constraints;
	HashMap<String, String> internal_macros;

	void _rebuild_indices();
	int _find_last_comment_block(int p_target_idx) const;

	Variant _interpolate_value(const String &p_original_value, const String &p_current_section, int p_depth) const;
	Ref<DotIniFile> _get_owner_of_key(const String &p_section, const String &p_key) const;

protected:
	static void _bind_methods();

public:
	static void set_global_fallback(Ref<DotIniFile> p_fallback);
	static Ref<DotIniFile> get_global_fallback();

	void set_autosave(bool p_enabled);
	bool is_autosave_enabled() const;
	bool check_for_updates();

	// Phase 5 concurrency and rules
	void set_read_only(bool p_read_only);
	bool is_read_only() const;

	void add_type_constraint(const String &p_section, const String &p_key, Variant::Type p_type);
	void remove_type_constraint(const String &p_section, const String &p_key);

	void add_macro(const String &p_name, const String &p_value);
	String get_macro(const String &p_name) const;

	Error load(const String &p_path, bool p_append = false);
	Error save(const String &p_path = "");
	Error save_all();

	Error load_from_string(const String &p_string, bool p_append = false);
	String save_to_string() const;

	Error load_from_buffer(const PackedByteArray &p_buffer, bool p_append = false);
	PackedByteArray save_to_buffer() const;

	Error load_from_base64(const String &p_base64, bool p_append = false);
	String save_to_base64() const;

	Error load_encrypted(const String &p_path, const PackedByteArray &p_key, bool p_append = false);
	Error save_encrypted(const String &p_path, const PackedByteArray &p_key);
	Error load_encrypted_pass(const String &p_path, const String &p_pass, bool p_append = false);
	Error save_encrypted_pass(const String &p_path, const String &p_pass);

	void merge_with(Ref<DotIniFile> p_other, bool p_overwrite = true);
	void set_default_padding(const String &p_pre_key, const String &p_post_key, const String &p_pre_val, const String &p_post_val);

	Ref<ConfigFile> to_config_file() const;
	void from_config_file(Ref<ConfigFile> p_config);

	Dictionary to_dictionary() const;
	void from_dictionary(const Dictionary &p_dict);
	Dictionary get_section_as_dict(const String &p_section) const;

	void set_value(const String &p_section, const String &p_key, const Variant &p_value);
	void append_value(const String &p_section, const String &p_key, const Variant &p_value);
	Variant ensure_value(const String &p_section, const String &p_key, const Variant &p_default_value);
	void set_value_b64(const String &p_section, const String &p_key, const String &p_utf8_string); // Automatically converts to B64 in save

	// Retrieve identical repeated keys standard in some INI specs
	Array get_value_array(const String &p_section, const String &p_key, bool p_interpolate = true) const;

	Variant get_value(const String &p_section, const String &p_key, const Variant &p_default = Variant(), bool p_interpolate = true) const;
	String get_value_b64(const String &p_section, const String &p_key, const String &p_default = "") const;
	String get_value_raw(const String &p_section, const String &p_key, const String &p_default = "") const;

	String get_value_string(const String &p_section, const String &p_key, const String &p_default = "", bool p_interpolate = true) const;
	int get_value_int(const String &p_section, const String &p_key, int p_default = 0, bool p_interpolate = true) const;
	float get_value_float(const String &p_section, const String &p_key, float p_default = 0.0, bool p_interpolate = true) const;
	bool get_value_bool(const String &p_section, const String &p_key, bool p_default = false, bool p_interpolate = true) const;

	void set_section_comment(const String &p_section, const String &p_comment);
	void set_key_comment(const String &p_section, const String &p_key, const String &p_comment);
	String get_section_comment(const String &p_section) const;
	String get_key_comment(const String &p_section, const String &p_key) const;

	bool has_section(const String &p_section) const;
	bool has_section_key(const String &p_section, const String &p_key) const;

	void erase_section(const String &p_section);
	void clear_section(const String &p_section);
	void erase_section_key(const String &p_section, const String &p_key);

	void rename_section(const String &p_old_section, const String &p_new_section);
	void rename_key(const String &p_section, const String &p_old_key, const String &p_new_key);
	void sort_section_keys(const String &p_section, bool p_ascending = true);
	PackedStringArray get_sections() const;
	PackedStringArray get_section_keys(const String &p_section) const;
	PackedStringArray get_keys_matching(const String &p_section, const String &p_match_pattern) const;

	Ref<DotIniFile> clone() const;

	TypedArray<DotIniFile> get_included_files() const;

	void clear();

	DotIniFile();
};
