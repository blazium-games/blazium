/**************************************************************************/
/*  env.h                                                                 */
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

#pragma once

#include "core/io/file_access.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/variant/typed_array.h"

class ENV : public Object {
	GDCLASS(ENV, Object);
	static ENV *env_singleton;
	bool debug = false;
	TypedArray<String> true_terms;
	TypedArray<String> false_terms;
	Callable custom_resolver;

	Dictionary get_envs_matching(const String &p_pattern);
	TypedArray<String> get_missing_envs(const TypedArray<String> &p_keys);
	void set_resolver(const Callable &p_callable);
	void set_bool_whitelist(const TypedArray<String> &p_true_terms, const TypedArray<String> &p_false_terms);

	bool prioritize_os_env = true;
	Dictionary env_vars = Dictionary();
	TypedArray<String> env_files;

public:
	static ENV *get_singleton();
	static void _bind_methods();

	Dictionary config(const String &p_file, bool p_override = true);

	void auto_config(const String &p_dir = "res://", const String &p_mode = "");
	bool require_envs(const TypedArray<String> &p_keys);

	Dictionary load_env_file(const String &p_file, bool override = false);
	Dictionary parse(const String &p_data);
	Dictionary populate(const Dictionary &p_env, bool override = false);
	Dictionary refresh(bool override = false);
	void clear();
	Variant get_env(const String &p_key, const Variant &p_default = Variant());
	void set_env(const String &p_key, const Variant &p_value);
	bool has_env(const String &p_key);
	void remove_env(const String &p_key);
	Dictionary get_all_env() const;
	TypedArray<String> get_env_files() const;
	bool has_env_file(const String &p_file) const;
	Dictionary get_envs_from_file(const String &p_file);

	int get_env_int(const String &p_key, int p_default = 0);
	bool get_env_bool(const String &p_key, bool p_default = false);
	float get_env_float(const String &p_key, float p_default = 0.0f);
	TypedArray<String> get_env_array(const String &p_key, const TypedArray<String> &p_default = TypedArray<String>(), const String &p_delimiter = ",");
	Dictionary get_env_dict(const String &p_key, const Dictionary &p_default = Dictionary());

	Color get_env_color(const String &p_key, const Color &p_default = Color());
	Vector2 get_env_vector2(const String &p_key, const Vector2 &p_default = Vector2());
	Vector3 get_env_vector3(const String &p_key, const Vector3 &p_default = Vector3());
	Vector4 get_env_vector4(const String &p_key, const Vector4 &p_default = Vector4());

	Dictionary parse_buffer(const PackedByteArray &p_data);
	void bind_env(Object *p_node, const Dictionary &p_property_map);

	Dictionary get_env_group(const String &p_prefix);

	String expand_string(const String &p_text);

	void set_prioritize_os_env(bool p_prioritize);
	bool get_prioritize_os_env() const;

	void push_to_os_env();

	void generate_example(const String &p_file);
	void export_json(const String &p_file);
	void save(const String &p_file);

	bool get_debug() const { return debug; }
	void set_debug(bool p_debug) { debug = p_debug; }

	void print_debug(String text);

	ENV();
	~ENV();
};
