/**************************************************************************/
/*  variant.cpp                                                           */
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

#include "bindings/variant.h"

#include "bindings/array.h"
#include "bindings/callable.h"
#include "bindings/dictionary.h"
#include "bindings/luau_blazium_types.h"
#include "bindings/object.h"
#include "helpers.h"
#include "lua_state.h"
#include "string_cache.h"

#include "core/error/error_macros.h"
#include "core/object/ref_counted.h"
#include <lua.h>
#include <lualib.h>

using namespace luau_module;

static const char *const VARIANT_METATABLE_NAME = "GDVariant";
static const char *const METAMETHOD_TOBLAZIUM = "__toblazium";
static const char *const METAMETHOD_TOGODOT_LEGACY = "__togodot";

static void variant_dtor(void *ud) {
	Variant *var = static_cast<Variant *>(ud);
	var->~Variant();
}

static int variant_tostring(lua_State *L) {
	Variant *var = static_cast<Variant *>(lua_touserdata(L, 1));
	CharString utf8 = var->stringify().utf8();
	lua_pop(L, 1);

	lua_pushlstring(L, utf8.get_data(), utf8.length());
	return 1;
}

template <Variant::Operator op>
static int variant_op(lua_State *L) {
	Variant a = to_variant(L, 1);
	Variant b = to_variant(L, 2);
	lua_pop(L, 2);

	Variant result;
	bool is_valid = false;
	Variant::evaluate(op, a, b, result, is_valid);

	if (!is_valid) {
		String error_msg = vformat("Cannot use operator %d on Variant types %s and %s", op, Variant::get_type_name(a.get_type()), Variant::get_type_name(b.get_type()));
		lua_pushstring(L, error_msg.utf8().get_data());
		lua_error(L);
	}

	push_variant(L, result);
	return 1;
}

static int variant_negate(lua_State *L) {
	Variant var = *static_cast<Variant *>(lua_touserdata(L, 1));
	lua_pop(L, 1);

	Variant result;
	bool is_valid = false;
	Variant::evaluate(Variant::OP_NEGATE, var, Variant(), result, is_valid);

	if (!is_valid) {
		String error_msg = vformat("Cannot use negation operator on Variant type %s", Variant::get_type_name(var.get_type()));
		lua_pushstring(L, error_msg.utf8().get_data());
		lua_error(L);
	}

	push_variant(L, result);
	return 1;
}

static int variant_index(lua_State *L) {
	Variant var = *static_cast<Variant *>(lua_touserdata(L, 1));
	Variant key = to_variant(L, 2);
	lua_pop(L, 2);

	bool is_valid = false;
	Variant result = var.get(key, &is_valid);

	if (!is_valid) {
		String error_msg = vformat("Cannot index Variant type %s with key of type %s", Variant::get_type_name(var.get_type()), Variant::get_type_name(key.get_type()));
		lua_pushstring(L, error_msg.utf8().get_data());
		lua_error(L);
	}

	push_variant(L, result);
	return 1;
}

static int variant_newindex(lua_State *L) {
	Variant *var = static_cast<Variant *>(lua_touserdata(L, 1));
	Variant key = to_variant(L, 2);
	Variant value = to_variant(L, 3);

	lua_pop(L, 2);

	bool is_valid = false;
	var->set(key, value, &is_valid);

	if (!is_valid) {
		String error_msg = vformat("Cannot index Variant type %s with key of type %s", Variant::get_type_name(var->get_type()), Variant::get_type_name(key.get_type()));
		lua_pushstring(L, error_msg.utf8().get_data());
		lua_error(L);
	}

	lua_pop(L, 1);
	return 0;
}

static int variant_iter_closure(lua_State *L) {
	luaL_checkstack(L, 3, "Variant.__iter.closure: could not grow stack");

	Variant *var = static_cast<Variant *>(lua_touserdata(L, lua_upvalueindex(1)));
	Variant iter = to_variant(L, lua_upvalueindex(2));
	if (iter.get_type() == Variant::NIL) {
		lua_pushnil(L);
		return 1;
	}

	bool valid;
	Variant current = var->iter_get(iter, valid);

	push_variant(L, iter);
	push_variant(L, current);

	if (var->iter_next(iter, valid)) {
		push_variant(L, iter);
		lua_replace(L, lua_upvalueindex(2));
	} else {
		lua_pushnil(L);
		lua_replace(L, lua_upvalueindex(2));
	}

	return 2;
}

static int variant_iter(lua_State *L) {
	Variant *var = static_cast<Variant *>(lua_touserdata(L, 1));

	Variant iter;
	bool valid;
	if (!var->iter_init(iter, valid)) {
		String error_msg = vformat("Variant type %s is not iterable", Variant::get_type_name(var->get_type()));
		lua_pushstring(L, error_msg.utf8().get_data());
		lua_error(L);
	}

	luaL_checkstack(L, 1, "Variant.__iter: not enough stack space for iterator");
	push_variant(L, iter);

	lua_pushcclosure(L, variant_iter_closure, "Variant.__iter.closure", 2);

	return 1;
}

static void push_variant_metatable(lua_State *L) {
	if (!luaL_newmetatable(L, VARIANT_METATABLE_NAME)) {
		return;
	}

	lua_pushcfunction(L, variant_tostring, "Variant.__tostring");
	lua_setfield(L, -2, "__tostring");

	lua_pushcfunction(L, variant_op<Variant::OP_ADD>, "Variant.__add");
	lua_setfield(L, -2, "__add");

	lua_pushcfunction(L, variant_op<Variant::OP_SUBTRACT>, "Variant.__sub");
	lua_setfield(L, -2, "__sub");

	lua_pushcfunction(L, variant_op<Variant::OP_MULTIPLY>, "Variant.__mul");
	lua_setfield(L, -2, "__mul");

	lua_pushcfunction(L, variant_op<Variant::OP_DIVIDE>, "Variant.__div");
	lua_setfield(L, -2, "__div");

	lua_pushcfunction(L, variant_op<Variant::OP_MODULE>, "Variant.__mod");
	lua_setfield(L, -2, "__mod");

	lua_pushcfunction(L, variant_op<Variant::OP_POWER>, "Variant.__pow");
	lua_setfield(L, -2, "__pow");

	lua_pushcfunction(L, variant_negate, "Variant.__unm");
	lua_setfield(L, -2, "__unm");

	lua_pushcfunction(L, generic_lua_concat, "Variant.__concat");
	lua_setfield(L, -2, "__concat");

	lua_pushcfunction(L, variant_op<Variant::OP_EQUAL>, "Variant.__eq");
	lua_setfield(L, -2, "__eq");

	lua_pushcfunction(L, variant_op<Variant::OP_LESS>, "Variant.__lt");
	lua_setfield(L, -2, "__lt");

	lua_pushcfunction(L, variant_op<Variant::OP_LESS_EQUAL>, "Variant.__le");
	lua_setfield(L, -2, "__le");

	lua_pushcfunction(L, variant_index, "Variant.__index");
	lua_setfield(L, -2, "__index");

	lua_pushcfunction(L, variant_newindex, "Variant.__newindex");
	lua_setfield(L, -2, "__newindex");

	lua_pushcfunction(L, variant_iter, "Variant.__iter");
	lua_setfield(L, -2, "__iter");

	lua_setreadonly(L, -1, 1);
}

static bool has_variant_metatable(lua_State *L, int p_index) {
	ERR_FAIL_COND_V_MSG(!lua_checkstack(L, 2), false, vformat("has_variant_metatable(%d): Stack overflow. Cannot grow stack.", p_index));

	if (!lua_getmetatable(L, p_index)) {
		return false;
	}

	luaL_getmetatable(L, VARIANT_METATABLE_NAME);
	bool mt_equal = lua_rawequal(L, -1, -2);
	lua_pop(L, 2);

	return mt_equal;
}

Variant luau_module::to_variant(lua_State *L, int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid_index(L, p_index), Variant(), vformat("to_variant(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	Variant toblazium_result;
	if (call_toblazium_metamethod(L, p_index, toblazium_result)) {
		return toblazium_result;
	}

	if (is_typed_blazium_value(L, p_index)) {
		return to_typed_blazium_value(L, p_index);
	}

	lua_Type type = static_cast<lua_Type>(lua_type(L, p_index));
	switch (type) {
		case LUA_TNIL:
			return Variant();

		case LUA_TBOOLEAN:
			return Variant(lua_toboolean(L, p_index) != 0);

		case LUA_TLIGHTUSERDATA:
			return to_light_object(L, p_index);

		case LUA_TNUMBER: {
			double num = lua_tonumber(L, p_index);
			if (num == nearbyint(num)) {
				return Variant(static_cast<int64_t>(num));
			} else {
				return Variant(num);
			}
		}

		case LUA_TVECTOR: {
			const float *vec = lua_tovector(L, p_index);
			return Variant(Vector3(vec[0], vec[1], vec[2]));
		}

		case LUA_TSTRING: {
			size_t len;
			const char *str = lua_tolstring(L, p_index, &len);
			return Variant(String::utf8(str, len));
		}

		case LUA_TTABLE: {
			bool is_array = false;
			Array arr = to_array(L, p_index, &is_array);
			if (is_array) {
				return Variant(arr);
			} else {
				return Variant(to_dictionary(L, p_index));
			}
		}

		case LUA_TFUNCTION:
			return Variant(to_callable(L, p_index));

		case LUA_TUSERDATA: {
			if (has_variant_metatable(L, p_index)) {
				return *static_cast<Variant *>(lua_touserdata(L, p_index));
			}

			Object *obj = to_full_object(L, p_index);
			if (obj) {
				return obj;
			}

			Callable callable = to_callable(L, p_index);
			if (callable.is_valid()) {
				return callable;
			}

			return Variant();
		}

		case LUA_TTHREAD: {
			lua_State *thread_L = lua_tothread(L, p_index);
			return LuaState::find_or_create_lua_state(thread_L);
		}

		case LUA_TBUFFER: {
			size_t len;
			void *ptr = lua_tobuffer(L, p_index, &len);

			PackedByteArray arr;
			arr.resize(len);
			memcpy(arr.ptrw(), ptr, len);

			return Variant(arr);
		}

		case LUA_TPROTO:
			[[fallthrough]];

		case LUA_TUPVAL:
			[[fallthrough]];

		case LUA_TDEADKEY:
			[[fallthrough]];

		default:
			ERR_FAIL_V_MSG(Variant(), vformat("to_variant(%d): Unsupported Lua type: %d", p_index, type));
	}
}

void luau_module::push_variant(lua_State *L, const Variant &p_variant) {
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 2), "push_variant(): Stack overflow. Cannot grow stack.");

	switch (p_variant.get_type()) {
		case Variant::NIL:
			lua_pushnil(L);
			return;

		case Variant::BOOL: {
			bool b = p_variant;
			lua_pushboolean(L, b ? 1 : 0);
			return;
		}

		case Variant::INT:

			[[fallthrough]];

		case Variant::FLOAT:
			lua_pushnumber(L, p_variant);
			return;

		case Variant::STRING_NAME: {
			CharString utf8 = char_string(p_variant);
			lua_pushlstring(L, utf8.get_data(), utf8.length());
			return;
		}

		case Variant::NODE_PATH:
			[[fallthrough]];

		case Variant::STRING: {
			String str = p_variant;
			CharString utf8 = str.utf8();
			lua_pushlstring(L, utf8.get_data(), utf8.length());
			return;
		}

		case Variant::VECTOR3: {
			Vector3 vec = p_variant;
			lua_pushvector(L, vec.x, vec.y, vec.z);
			return;
		}

		case Variant::OBJECT: {
			Object *obj = p_variant;
			LuaState *state = Object::cast_to<LuaState>(obj);
			if (state) {
				ERR_FAIL_COND_MSG(state->get_lua_state() != L, "push_variant(): Cannot push LuaState into a different Lua thread or VM.");
				lua_pushthread(L);
			} else {
				push_object(L, obj, LUA_NOTAG);
			}

			return;
		}

		case Variant::CALLABLE:
			push_callable(L, p_variant);
			return;

		case Variant::DICTIONARY:
			push_dictionary(L, p_variant);
			return;

		case Variant::ARRAY:
			push_array(L, p_variant);
			return;

		case Variant::PACKED_BYTE_ARRAY: {
			PackedByteArray arr = p_variant;
			void *buf = lua_newbuffer(L, arr.size());
			memcpy(buf, arr.ptr(), arr.size());
			return;
		}

		default: {
			void *ptr = lua_newuserdatadtor(L, sizeof(Variant), variant_dtor);
			memnew_placement(ptr, Variant(p_variant));

			push_variant_metatable(L);
			lua_setmetatable(L, -2);
		}
	}
}

bool luau_module::call_toblazium_metamethod(lua_State *L, int p_index, Variant &r_result, int p_tag) {
	ERR_FAIL_COND_V_MSG(!is_valid_index(L, p_index), false, vformat("call_toblazium_metamethod(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));
	ERR_FAIL_COND_V_MSG(!lua_checkstack(L, 2), false, vformat("call_toblazium_metamethod(%d): Stack overflow. Cannot grow stack.", p_index));

	if (!lua_getmetatable(L, p_index)) {
		return false;
	}

	int type = lua_getfield(L, -1, METAMETHOD_TOBLAZIUM);
	if (type == LUA_TNIL) {
		lua_pop(L, 1);
		type = lua_getfield(L, -1, METAMETHOD_TOGODOT_LEGACY);
	}
	if (type == LUA_TNIL) {
		lua_pop(L, 2);
		return false;
	}

	if (!is_blazium_callable(L, -1)) {
		lua_pop(L, 2);
		ERR_FAIL_V_MSG(false, vformat("call_toblazium_metamethod(%d): __toblazium is not a Blazium Callable.", p_index));
	}

	Callable callable = to_callable(L, -1);
	lua_pop(L, 2);

	r_result = callable.call(LuaState::find_or_create_lua_state(L), p_index, p_tag);
	return true;
}
