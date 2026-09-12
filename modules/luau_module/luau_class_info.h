/**************************************************************************/
/*  luau_class_info.h                                                     */
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

#include "core/object/class_db.h"
#include "core/object/ref_counted.h"
#include "core/templates/hash_map.h"
#include "core/variant/variant.h"

#include "lua_state.h"

#define LUAU_CLASS_MT_SCRIPT "__script"
#define LUAU_CLASS_KEY_PROPERTY "__property"
#define LUAU_CLASS_KEY_SIGNAL "__signal"
#define LUAU_CLASS_KEY_BLAZIUM_CLASS "__blazium_class"
#define LUAU_CLASS_KEY_GODOT_CLASS "__godot_class"

struct LuauClassProperty {
	PropertyInfo info;
	StringName getter;
	StringName setter;
	Variant default_value;
	String description;
};

struct LuauClassMethod {
	MethodInfo info;
	String description;
};

struct LuauClassSignal {
	MethodInfo info;
	String description;
};

struct LuauClassInfo {
	String extends = "RefCounted";
	StringName class_name;
	bool tool = false;
	bool abstract = false;
	String icon_path;
	String class_description;
	Dictionary rpc_config;

	HashMap<StringName, LuauClassProperty> properties;
	HashMap<StringName, LuauClassMethod> methods;
	HashMap<StringName, LuauClassSignal> signals;
	HashMap<StringName, Variant> constants;

	void clear();

	static bool is_reserved_key(const StringName &p_key);
	static Error parse_from_table(const Ref<luau_module::LuaState> &p_state, int p_table_index, LuauClassInfo &r_info, int *r_table_ref = nullptr);
	static Error parse_from_object(const Ref<luau_module::LuaState> &p_state, int p_object_index, LuauClassInfo &r_info, int *r_table_ref = nullptr);
	static Error parse_from_source(const Ref<luau_module::LuaState> &p_registry_state, const String &p_source, const String &p_path, PackedByteArray &r_bytecode, LuauClassInfo &r_info, Ref<luau_module::LuaState> &r_class_vm, int *r_table_ref = nullptr);
	static Error parse_info_from_source(const Ref<luau_module::LuaState> &p_registry_state, const String &p_source, const String &p_path, PackedByteArray &r_bytecode, LuauClassInfo &r_info);
	static void parse_global_class_metadata_from_source(const String &p_source, LuauClassInfo *r_info);

	static void register_script_helpers(const Ref<luau_module::LuaState> &p_state);
};
