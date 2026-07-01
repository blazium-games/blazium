/**************************************************************************/
/*  luau_class_info.cpp                                                   */
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

#include "luau_class_info.h"
#include "analysis/luau_analysis.h"
#include "luau_codegen.h"
#include "luau_parser_pool.h"
#include "luau_script_language.h"
#include "scheduler/luau_task_scheduler.h"

#include "bindings/dictionary.h"
#include "bindings/luau_blazium_types.h"
#include "bindings/variant.h"
#include "helpers.h"
#include "lua_blazium_classes.h"
#include "luau.h"
#include "require/luau_package_path.h"
#include "string_cache.h"

#include "core/error/error_macros.h"
#include "core/object/class_db.h"
#include <lualib.h>

using namespace luau_module;

namespace {

static bool table_has_marker(lua_State *L, int p_index, const char *p_key) {
	if (!lua_istable(L, p_index)) {
		return false;
	}

	lua_getfield(L, p_index, p_key);
	bool found = lua_isboolean(L, -1) && lua_toboolean(L, -1);
	lua_pop(L, 1);
	return found;
}

static Variant read_variant_default(lua_State *L, int p_index) {
	return to_variant(L, p_index);
}

static LuauClassProperty read_property_descriptor(lua_State *L, int p_index, const StringName &p_name) {
	LuauClassProperty prop;
	prop.info.name = p_name;
	prop.info.usage = PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SCRIPT_VARIABLE;

	if (!lua_istable(L, p_index)) {
		prop.default_value = read_variant_default(L, p_index);
		prop.info.type = prop.default_value.get_type();
		if (prop.info.type == Variant::OBJECT && prop.default_value.get_type() == Variant::OBJECT) {
			prop.info.class_name = prop.default_value.get_validated_object()->get_class_name();
		}
		prop.info.usage |= PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE;
		return prop;
	}

	lua_rawgeti(L, p_index, 1);
	if (!lua_isnil(L, -1)) {
		if (lua_isnumber(L, -1)) {
			prop.info.type = Variant::INT;
			prop.default_value = lua_tonumber(L, -1);
		} else if (lua_isboolean(L, -1)) {
			prop.info.type = Variant::BOOL;
			prop.default_value = lua_toboolean(L, -1) != 0;
		} else if (lua_isstring(L, -1)) {
			prop.info.type = Variant::STRING;
			prop.default_value = String::utf8(lua_tostring(L, -1));
		} else {
			prop.default_value = read_variant_default(L, -1);
			prop.info.type = prop.default_value.get_type();
		}
	}
	lua_pop(L, 1);

	lua_getfield(L, p_index, "type");
	if (!lua_isnil(L, -1)) {
		if (lua_isnumber(L, -1)) {
			prop.info.type = (Variant::Type)(int)lua_tointeger(L, -1);
		} else if (lua_isstring(L, -1)) {
			String type_name = String::utf8(lua_tostring(L, -1));
			prop.info.type = Variant::OBJECT;
			prop.info.class_name = type_name;
		}
	}
	lua_pop(L, 1);

	lua_getfield(L, p_index, "hint");
	if (lua_isnumber(L, -1)) {
		prop.info.hint = (PropertyHint)(int)lua_tointeger(L, -1);
	}
	lua_pop(L, 1);

	lua_getfield(L, p_index, "hint_string");
	if (!lua_isstring(L, -1)) {
		lua_pop(L, 1);
		lua_getfield(L, p_index, "hintString");
	}
	if (lua_isstring(L, -1)) {
		prop.info.hint_string = String::utf8(lua_tostring(L, -1));
	}
	lua_pop(L, 1);

	lua_getfield(L, p_index, "usage");
	if (lua_isnumber(L, -1)) {
		prop.info.usage = (PropertyUsageFlags)(int)lua_tointeger(L, -1);
	}
	lua_pop(L, 1);

	lua_getfield(L, p_index, "class_name");
	if (!lua_isstring(L, -1)) {
		lua_pop(L, 1);
		lua_getfield(L, p_index, "className");
	}
	if (lua_isstring(L, -1)) {
		prop.info.class_name = StringName(String::utf8(lua_tostring(L, -1)));
	}
	lua_pop(L, 1);

	lua_getfield(L, p_index, "default");
	if (!lua_isnil(L, -1)) {
		prop.default_value = read_variant_default(L, -1);
		if (prop.info.type == Variant::NIL && prop.default_value.get_type() != Variant::NIL) {
			prop.info.type = prop.default_value.get_type();
		}
	}
	lua_pop(L, 1);

	lua_getfield(L, p_index, "get");
	if (lua_isstring(L, -1)) {
		prop.getter = StringName(String::utf8(lua_tostring(L, -1)));
		prop.info.usage &= ~PROPERTY_USAGE_STORAGE;
	}
	lua_pop(L, 1);

	lua_getfield(L, p_index, "set");
	if (lua_isstring(L, -1)) {
		prop.setter = StringName(String::utf8(lua_tostring(L, -1)));
	}
	lua_pop(L, 1);

	if (prop.info.type == Variant::NIL && prop.default_value.get_type() != Variant::NIL) {
		prop.info.type = prop.default_value.get_type();
	}

	if (prop.info.type == Variant::OBJECT && prop.info.class_name != StringName()) {
		if (ClassDB::is_parent_class(prop.info.class_name, "Node")) {
			prop.info.hint = PROPERTY_HINT_NODE_TYPE;
			prop.info.hint_string = prop.info.class_name;
		} else if (ClassDB::is_parent_class(prop.info.class_name, "Resource")) {
			prop.info.hint = PROPERTY_HINT_RESOURCE_TYPE;
			prop.info.hint_string = prop.info.class_name;
		}
	}

	prop.info.usage |= PROPERTY_USAGE_SCRIPT_VARIABLE;
	return prop;
}

static LuauClassSignal read_signal_descriptor(lua_State *L, int p_index, const StringName &p_name) {
	LuauClassSignal signal;
	signal.info.name = p_name;

	if (lua_istable(L, p_index)) {
		lua_getfield(L, p_index, "args");
		if (lua_istable(L, -1)) {
			int len = lua_objlen(L, -1);
			for (int i = 1; i <= len; i++) {
				lua_rawgeti(L, -1, i);
				PropertyInfo arg;
				if (lua_isnumber(L, -1)) {
					arg.type = (Variant::Type)(int)lua_tointeger(L, -1);
				} else if (lua_isstring(L, -1)) {
					arg.type = Variant::OBJECT;
					arg.class_name = StringName(String::utf8(lua_tostring(L, -1)));
				}
				signal.info.arguments.push_back(arg);
				lua_pop(L, 1);
			}
		}
		lua_pop(L, 1);
	}

	return signal;
}

static int luau_helper_export(lua_State *L) {
	lua_newtable(L);
	lua_pushboolean(L, 1);
	lua_setfield(L, -2, LUAU_CLASS_KEY_PROPERTY);

	if (lua_gettop(L) >= 1 && !lua_isnil(L, 1)) {
		lua_pushvalue(L, 1);
		lua_rawseti(L, -2, 1);
	}
	return 1;
}

static int luau_helper_signal(lua_State *L) {
	lua_newtable(L);
	lua_pushboolean(L, 1);
	lua_setfield(L, -2, LUAU_CLASS_KEY_SIGNAL);
	return 1;
}

static int luau_helper_gdclass(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);

	if (lua_getmetatable(L, 1)) {
		lua_pop(L, 1);
		luaL_error(L, "custom metatables are not supported on class definitions");
	}

	lua_createtable(L, 0, 2);
	lua_pushstring(L, "This metatable is locked.");
	lua_setfield(L, -2, "__metatable");
	lua_pushboolean(L, 1);
	lua_setfield(L, -2, "__gdclass");
	lua_setmetatable(L, 1);
	lua_pushvalue(L, 1);
	return 1;
}

struct LuauClassDescriptor {
	int table_ref = LUA_NOREF;
};

static const char *k_class_descriptor_metatable = "LuauClassDescriptor";

static void ensure_class_descriptor_metatable(lua_State *L) {
	if (luaL_newmetatable(L, k_class_descriptor_metatable)) {
		lua_pushstring(L, "Luau class descriptor");
		lua_setfield(L, -2, "__metatable");
	}
	lua_pop(L, 1);
}

static int luau_helper_class(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);

	if (lua_getmetatable(L, 1)) {
		lua_pop(L, 1);
		luaL_error(L, "custom metatables are not supported on class definitions");
	}

	lua_createtable(L, 0, 2);
	lua_pushstring(L, "This metatable is locked.");
	lua_setfield(L, -2, "__metatable");
	lua_pushboolean(L, 1);
	lua_setfield(L, -2, "__gdclass");
	lua_setmetatable(L, 1);

	ensure_class_descriptor_metatable(L);
	LuauClassDescriptor *descriptor = static_cast<LuauClassDescriptor *>(lua_newuserdata(L, sizeof(LuauClassDescriptor)));
	descriptor->table_ref = lua_ref(L, 1);
	luaL_getmetatable(L, k_class_descriptor_metatable);
	lua_setmetatable(L, -2);
	return 1;
}

static bool is_class_descriptor(lua_State *L, int p_index) {
	if (!lua_isuserdata(L, p_index)) {
		return false;
	}
	if (!lua_getmetatable(L, p_index)) {
		return false;
	}
	luaL_getmetatable(L, k_class_descriptor_metatable);
	const bool match = lua_rawequal(L, -1, -2);
	lua_pop(L, 2);
	return match;
}

static bool read_extends_field(lua_State *L, int p_index, String &r_extends) {
	if (lua_isstring(L, p_index)) {
		r_extends = String::utf8(lua_tostring(L, p_index));
		return true;
	}
	if (lua_istable(L, p_index)) {
		lua_getfield(L, p_index, LUAU_CLASS_KEY_BLAZIUM_CLASS);
		if (lua_isstring(L, -1)) {
			r_extends = String::utf8(lua_tostring(L, -1));
			lua_pop(L, 1);
			return true;
		}
		lua_pop(L, 1);
		lua_getfield(L, p_index, LUAU_CLASS_KEY_GODOT_CLASS);
		if (lua_isstring(L, -1)) {
			r_extends = String::utf8(lua_tostring(L, -1));
			lua_pop(L, 1);
			return true;
		}
		lua_pop(L, 1);
	}
	Variant ext = to_variant(L, p_index);
	if (ext.get_type() == Variant::DICTIONARY) {
		Dictionary dict = ext;
		if (dict.has(LUAU_CLASS_KEY_BLAZIUM_CLASS)) {
			r_extends = dict[LUAU_CLASS_KEY_BLAZIUM_CLASS];
			return true;
		}
		if (dict.has(LUAU_CLASS_KEY_GODOT_CLASS)) {
			r_extends = dict[LUAU_CLASS_KEY_GODOT_CLASS];
			return true;
		}
	} else if (ext.get_type() == Variant::STRING || ext.get_type() == Variant::STRING_NAME) {
		r_extends = ext;
		return true;
	} else if (ext.get_type() == Variant::OBJECT && ext.get_validated_object()) {
		r_extends = ext.get_validated_object()->get_class_name();
		return true;
	}
	return false;
}

} //namespace

void LuauClassInfo::clear() {
	extends = "RefCounted";
	class_name = StringName();
	tool = false;
	abstract = false;
	icon_path = String();
	class_description = String();
	rpc_config = Dictionary();
	properties.clear();
	methods.clear();
	signals.clear();
	constants.clear();
}

bool LuauClassInfo::is_reserved_key(const StringName &p_key) {
	static const char *reserved_keys[] = {
		"extends",
		"class_name",
		"tool",
		"abstract",
		"icon",
		"rpc_config",
		nullptr,
	};

	for (const char **key = reserved_keys; *key; key++) {
		if (p_key == StringName(*key)) {
			return true;
		}
	}
	return false;
}

void LuauClassInfo::register_script_helpers(const Ref<LuaState> &p_state) {
	ERR_FAIL_COND(p_state.is_null() || !p_state->is_valid());

	lua_State *L = p_state->get_lua_state();
	lua_pushcfunction(L, luau_helper_export, "export");
	lua_setglobal(L, "export");
	lua_pushcfunction(L, luau_helper_signal, "signal");
	lua_setglobal(L, "signal");
	lua_pushcfunction(L, luau_helper_gdclass, "gdclass");
	lua_setglobal(L, "gdclass");
	lua_pushcfunction(L, luau_helper_class, "class");
	lua_setglobal(L, "class");
}

Error LuauClassInfo::parse_from_table(const Ref<LuaState> &p_state, int p_table_index, LuauClassInfo &r_info, int *r_table_ref) {
	ERR_FAIL_COND_V(p_state.is_null() || !p_state->is_valid(), ERR_INVALID_PARAMETER);

	lua_State *L = p_state->get_lua_state();
	ERR_FAIL_COND_V(!lua_istable(L, p_table_index), ERR_INVALID_DATA);

	const int abs_table_index = lua_absindex(L, p_table_index);

	r_info.clear();

	lua_getfield(L, abs_table_index, "extends");
	if (!lua_isnil(L, -1)) {
		read_extends_field(L, -1, r_info.extends);
	}
	lua_pop(L, 1);

	lua_getfield(L, abs_table_index, "class_name");
	if (lua_isstring(L, -1)) {
		r_info.class_name = StringName(String::utf8(lua_tostring(L, -1)));
	}
	lua_pop(L, 1);

	lua_getfield(L, abs_table_index, "tool");
	if (lua_isboolean(L, -1)) {
		r_info.tool = lua_toboolean(L, -1);
	}
	lua_pop(L, 1);

	lua_getfield(L, abs_table_index, "abstract");
	if (lua_isboolean(L, -1)) {
		r_info.abstract = lua_toboolean(L, -1);
	}
	lua_pop(L, 1);

	lua_getfield(L, abs_table_index, "icon");
	if (lua_isstring(L, -1)) {
		r_info.icon_path = String::utf8(lua_tostring(L, -1));
	}
	lua_pop(L, 1);

	lua_getfield(L, abs_table_index, "rpc_config");
	if (!lua_isnil(L, -1)) {
		r_info.rpc_config = to_dictionary(L, -1);
	}
	lua_pop(L, 1);

	lua_pushnil(L);
	while (lua_next(L, abs_table_index) != 0) {
		if (lua_type(L, -2) != LUA_TSTRING) {
			lua_pop(L, 1);
			continue;
		}

		StringName key = to_string_name(L, -2);
		if (is_reserved_key(key)) {
			lua_pop(L, 1);
			continue;
		}

		if (lua_isfunction(L, -1)) {
			LuauClassMethod method;
			method.info.name = key;
			r_info.methods[key] = method;
		} else if (table_has_marker(L, -1, LUAU_CLASS_KEY_SIGNAL)) {
			r_info.signals[key] = read_signal_descriptor(L, -1, key);
		} else if (table_has_marker(L, -1, LUAU_CLASS_KEY_PROPERTY)) {
			LuauClassProperty prop = read_property_descriptor(L, -1, key);
			prop.info.usage |= PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE;
			r_info.properties[key] = prop;
		} else if (lua_istable(L, -1)) {
			bool looks_like_property = false;
			lua_getfield(L, -1, "type");
			looks_like_property |= !lua_isnil(L, -1);
			lua_pop(L, 1);
			lua_getfield(L, -1, "default");
			looks_like_property |= !lua_isnil(L, -1);
			lua_pop(L, 1);
			lua_rawgeti(L, -1, 1);
			looks_like_property |= !lua_isnil(L, -1);
			lua_pop(L, 1);

			if (looks_like_property) {
				r_info.properties[key] = read_property_descriptor(L, -1, key);
			}
		} else {
			LuauClassProperty prop = read_property_descriptor(L, -1, key);
			r_info.properties[key] = prop;
		}

		lua_pop(L, 1);
	}

	if (r_table_ref) {
		lua_pushvalue(L, abs_table_index);
		*r_table_ref = lua_ref(L, -1);
		lua_pop(L, 1);
	}

	return OK;
}

Error LuauClassInfo::parse_from_object(const Ref<LuaState> &p_state, int p_object_index, LuauClassInfo &r_info, int *r_table_ref) {
	ERR_FAIL_COND_V(p_state.is_null() || !p_state->is_valid(), ERR_INVALID_PARAMETER);

	lua_State *L = p_state->get_lua_state();
	const int abs_index = lua_absindex(L, p_object_index);
	ERR_FAIL_COND_V(!is_class_descriptor(L, abs_index), ERR_INVALID_DATA);

	const LuauClassDescriptor *descriptor = static_cast<const LuauClassDescriptor *>(lua_touserdata(L, abs_index));
	ERR_FAIL_COND_V(!descriptor || descriptor->table_ref == LUA_NOREF, ERR_INVALID_DATA);

	lua_rawgeti(L, LUA_REGISTRYINDEX, descriptor->table_ref);
	ERR_FAIL_COND_V(!lua_istable(L, -1), ERR_INVALID_DATA);

	const Error err = parse_from_table(p_state, -1, r_info, r_table_ref);
	lua_pop(L, 1);
	return err;
}

Error LuauClassInfo::parse_from_source(const Ref<LuaState> &p_registry_state, const String &p_source, const String &p_path, PackedByteArray &r_bytecode, LuauClassInfo &r_info, Ref<LuaState> &r_class_vm, int *r_table_ref) {
	r_class_vm.unref();

	if (r_bytecode.is_empty()) {
		if (p_source.is_empty()) {
			return ERR_COMPILATION_FAILED;
		}
		r_bytecode = Luau::compile(p_source);
		if (r_bytecode.is_empty()) {
			return ERR_COMPILATION_FAILED;
		}
	}

	Ref<LuaState> parser;
	const bool reuse_registry_vm = p_registry_state.is_valid() && p_registry_state->is_valid();
	if (reuse_registry_vm) {
		parser = p_registry_state;
	} else {
		parser.instantiate();
		LuauParserPool::configure_parser_vm(parser);
	}

	String chunk_name = p_path.is_empty() ? "@luau_script" : "@" + p_path;
	if (!parser->load_bytecode(r_bytecode, chunk_name)) {
		return ERR_COMPILATION_FAILED;
	}

	LuauCodegen::compile_loaded(parser->get_lua_state(), -1, p_source);

	LuaState::Status status = parser->pcall(0, LUA_MULTRET);
	if (status != luau_module::LuaState::STATUS_OK) {
		if (parser->get_top() > 0) {
			ERR_PRINT(String("LuauClassInfo: script load failed: ") + parser->to_string_inplace(-1));
		}
		parser->set_top(0);
		return ERR_COMPILATION_FAILED;
	}

	if (parser->get_top() < 1) {
		ERR_PRINT("LuauClassInfo: script must return a class table or class descriptor");
		parser->set_top(0);
		return ERR_INVALID_DATA;
	}

	Error err = ERR_INVALID_DATA;
	if (parser->is_table(-1)) {
		err = parse_from_table(parser, -1, r_info, r_table_ref);
	} else if (is_class_descriptor(parser->get_lua_state(), -1)) {
		err = parse_from_object(parser, -1, r_info, r_table_ref);
	} else {
		ERR_PRINT("LuauClassInfo: script must return a class table, gdclass/class descriptor, or call class({...})");
		parser->set_top(0);
		return ERR_INVALID_DATA;
	}

	if (err != OK) {
		parser->set_top(0);
		return err;
	}

	LuauAnalysis::parse_annotations(p_source, &r_info);

	if (parser->is_table(-1)) {
		lua_State *L = parser->get_lua_state();
		for (const KeyValue<StringName, Variant> &pair : r_info.constants) {
			push_variant(L, pair.value);
			lua_setfield(L, -2, char_string(pair.key).get_data());
		}
	}

	r_class_vm = parser;
	parser->set_top(0);
	return err;
}

Error LuauClassInfo::parse_info_from_source(const Ref<LuaState> &p_registry_state, const String &p_source, const String &p_path, PackedByteArray &r_bytecode, LuauClassInfo &r_info) {
	ERR_FAIL_COND_V(p_registry_state.is_null() || !p_registry_state->is_valid(), ERR_INVALID_PARAMETER);
	ERR_FAIL_NULL_V(LuauScriptLanguage::get_singleton(), ERR_UNAVAILABLE);

	LuauParserPool &pool = LuauScriptLanguage::get_singleton()->get_parser_pool();

	if (r_bytecode.is_empty()) {
		if (p_source.is_empty()) {
			return ERR_COMPILATION_FAILED;
		}
		r_bytecode = Luau::compile(p_source);
		if (r_bytecode.is_empty()) {
			return ERR_COMPILATION_FAILED;
		}
	}

	Ref<LuaState> parser = pool.acquire();

	String chunk_name = p_path.is_empty() ? "@luau_script" : "@" + p_path;
	if (!parser->load_bytecode(r_bytecode, chunk_name)) {
		pool.release(parser);
		return ERR_COMPILATION_FAILED;
	}

	LuauCodegen::compile_loaded(parser->get_lua_state(), -1, p_source);

	LuaState::Status status = parser->pcall(0, LUA_MULTRET);
	if (status != luau_module::LuaState::STATUS_OK) {
		if (parser->get_top() > 0) {
			ERR_PRINT(String("LuauClassInfo: script load failed: ") + parser->to_string_inplace(-1));
		}
		parser->set_top(0);
		pool.release(parser);
		return ERR_COMPILATION_FAILED;
	}

	if (parser->get_top() < 1) {
		ERR_PRINT("LuauClassInfo: script must return a class table or class descriptor");
		parser->set_top(0);
		pool.release(parser);
		return ERR_INVALID_DATA;
	}

	Error err = ERR_INVALID_DATA;
	if (parser->is_table(-1)) {
		err = parse_from_table(parser, -1, r_info, nullptr);
	} else if (is_class_descriptor(parser->get_lua_state(), -1)) {
		err = parse_from_object(parser, -1, r_info, nullptr);
	} else {
		ERR_PRINT("LuauClassInfo: script must return a class table, gdclass/class descriptor, or call class({...})");
		parser->set_top(0);
		pool.release(parser);
		return ERR_INVALID_DATA;
	}

	LuauAnalysis::parse_annotations(p_source, &r_info);

	parser->set_top(0);
	pool.release(parser);
	return err;
}

void LuauClassInfo::parse_global_class_metadata_from_source(const String &p_source, LuauClassInfo *r_info) {
	if (!r_info) {
		return;
	}

	r_info->clear();

	auto extract_quoted_field = [](const String &p_src, const String &p_key) -> String {
		for (const char quote : { '"', '\'' }) {
			const String q = String::chr(quote);
			for (const char *sep : { " = ", "=" }) {
				const String sep_str = sep;
				const String needle = p_key + sep_str + q;
				const int start = p_src.find(needle);
				if (start == -1) {
					continue;
				}
				const int value_start = start + needle.length();
				const int value_end = p_src.find(q, value_start);
				if (value_end != -1 && value_end > value_start) {
					return p_src.substr(value_start, value_end - value_start);
				}
			}
		}
		return String();
	};

	auto extract_optional_bool = [](const String &p_src, const String &p_key, bool &r_found) -> bool {
		r_found = false;
		for (const char *sep : { " = ", "=" }) {
			const String sep_str = sep;
			for (const char *value : { "true", "false" }) {
				const String value_str = value;
				if (p_src.find(p_key + sep_str + value_str) != -1) {
					r_found = true;
					return value_str == "true";
				}
			}
		}
		return false;
	};

	const String extends = extract_quoted_field(p_source, "extends");
	if (!extends.is_empty()) {
		r_info->extends = extends;
	}

	const String class_name = extract_quoted_field(p_source, "class_name");
	if (!class_name.is_empty()) {
		r_info->class_name = class_name;
	}

	bool found = false;
	const bool tool = extract_optional_bool(p_source, "tool", found);
	if (found) {
		r_info->tool = tool;
	}

	found = false;
	const bool abstract = extract_optional_bool(p_source, "abstract", found);
	if (found) {
		r_info->abstract = abstract;
	}

	const String icon = extract_quoted_field(p_source, "icon");
	if (!icon.is_empty()) {
		r_info->icon_path = icon;
	}

	LuauAnalysis::parse_annotations(p_source, r_info);
}
