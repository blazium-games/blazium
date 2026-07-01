/**************************************************************************/
/*  luau_blazium_types.cpp                                                */
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

#include "bindings/luau_blazium_types.h"

#include "bindings/variant.h"
#include "helpers.h"

#include "core/error/error_macros.h"
#include "core/variant/variant.h"
#include <lualib.h>

using namespace luau_module;

namespace {

static const char *const VECTOR2_MT = "GDVector2";
static const char *const VECTOR3_MT = "GDVector3";
static const char *const COLOR_MT = "GDColor";

static void push_typed_value(lua_State *L, const Variant &p_value) {
	switch (p_value.get_type()) {
		case Variant::VECTOR2: {
			void *mem = lua_newuserdata(L, sizeof(Vector2));
			new (mem) Vector2(p_value);
			luaL_getmetatable(L, VECTOR2_MT);
			lua_setmetatable(L, -2);
		} break;
		case Variant::VECTOR3: {
			void *mem = lua_newuserdata(L, sizeof(Vector3));
			new (mem) Vector3(p_value);
			luaL_getmetatable(L, VECTOR3_MT);
			lua_setmetatable(L, -2);
		} break;
		case Variant::COLOR: {
			void *mem = lua_newuserdata(L, sizeof(Color));
			new (mem) Color(p_value);
			luaL_getmetatable(L, COLOR_MT);
			lua_setmetatable(L, -2);
		} break;
		default:
			push_variant(L, p_value);
			break;
	}
}

static int typed_add(lua_State *L) {
	Variant a = to_typed_blazium_value(L, 1);
	Variant b = to_typed_blazium_value(L, 2);
	lua_pop(L, 2);
	Variant result;
	bool valid = false;
	Variant::evaluate(Variant::OP_ADD, a, b, result, valid);
	if (!valid) {
		luaL_error(L, "invalid typed __add");
	}
	push_typed_value(L, result);
	return 1;
}

static int typed_sub(lua_State *L) {
	Variant a = to_typed_blazium_value(L, 1);
	Variant b = to_typed_blazium_value(L, 2);
	lua_pop(L, 2);
	Variant result;
	bool valid = false;
	Variant::evaluate(Variant::OP_SUBTRACT, a, b, result, valid);
	if (!valid) {
		luaL_error(L, "invalid typed __sub");
	}
	push_typed_value(L, result);
	return 1;
}

static int typed_mul(lua_State *L) {
	Variant a = to_typed_blazium_value(L, 1);
	Variant b = to_typed_blazium_value(L, 2);
	lua_pop(L, 2);
	Variant result;
	bool valid = false;
	Variant::evaluate(Variant::OP_MULTIPLY, a, b, result, valid);
	if (!valid) {
		luaL_error(L, "invalid typed __mul");
	}
	push_typed_value(L, result);
	return 1;
}

static int typed_div(lua_State *L) {
	Variant a = to_typed_blazium_value(L, 1);
	Variant b = to_typed_blazium_value(L, 2);
	lua_pop(L, 2);
	Variant result;
	bool valid = false;
	Variant::evaluate(Variant::OP_DIVIDE, a, b, result, valid);
	if (!valid) {
		luaL_error(L, "invalid typed __div");
	}
	push_typed_value(L, result);
	return 1;
}

static int typed_unm(lua_State *L) {
	Variant a = to_typed_blazium_value(L, 1);
	lua_pop(L, 1);
	Variant result;
	bool valid = false;
	Variant::evaluate(Variant::OP_NEGATE, a, Variant(), result, valid);
	if (!valid) {
		luaL_error(L, "invalid typed __unm");
	}
	push_typed_value(L, result);
	return 1;
}

static int typed_tostring(lua_State *L) {
	Variant value = to_typed_blazium_value(L, 1);
	lua_pop(L, 1);
	CharString utf8 = value.stringify().utf8();
	lua_pushlstring(L, utf8.get_data(), utf8.length());
	return 1;
}

static int typed_eq(lua_State *L) {
	Variant a = to_typed_blazium_value(L, 1);
	Variant b = to_typed_blazium_value(L, 2);
	lua_pop(L, 2);
	bool valid = false;
	Variant result;
	Variant::evaluate(Variant::OP_EQUAL, a, b, result, valid);
	lua_pushboolean(L, valid && bool(result));
	return 1;
}

static int typed_vector2_index(lua_State *L) {
	Vector2 *vec = static_cast<Vector2 *>(lua_touserdata(L, 1));
	ERR_FAIL_NULL_V(vec, 0);
	const char *key = luaL_checkstring(L, 2);
	if (strcmp(key, "x") == 0) {
		lua_pushnumber(L, vec->x);
	} else if (strcmp(key, "y") == 0) {
		lua_pushnumber(L, vec->y);
	} else {
		luaL_error(L, "'Vector2' has no field '%s'", key);
	}
	return 1;
}

static void register_typed_metatable(lua_State *L, const char *p_mt) {
	if (!luaL_newmetatable(L, p_mt)) {
		if (strcmp(p_mt, VECTOR2_MT) == 0) {
			lua_pushcfunction(L, typed_vector2_index, "__index");
			lua_setfield(L, -2, "__index");
		}
		return;
	}

	lua_pushcfunction(L, typed_tostring, "__tostring");
	lua_setfield(L, -2, "__tostring");
	lua_pushcfunction(L, typed_add, "__add");
	lua_setfield(L, -2, "__add");
	lua_pushcfunction(L, typed_sub, "__sub");
	lua_setfield(L, -2, "__sub");
	lua_pushcfunction(L, typed_mul, "__mul");
	lua_setfield(L, -2, "__mul");
	lua_pushcfunction(L, typed_div, "__div");
	lua_setfield(L, -2, "__div");
	lua_pushcfunction(L, typed_unm, "__unm");
	lua_setfield(L, -2, "__unm");
	lua_pushcfunction(L, typed_eq, "__eq");
	lua_setfield(L, -2, "__eq");
	if (strcmp(p_mt, VECTOR2_MT) == 0) {
		lua_pushcfunction(L, typed_vector2_index, "__index");
		lua_setfield(L, -2, "__index");
	}
	lua_setreadonly(L, -1, 1);
	lua_pop(L, 1);
}

} //namespace

void luau_module::push_typed_vector2(lua_State *p_L, const Vector2 &p_value) {
	void *mem = lua_newuserdata(p_L, sizeof(Vector2));
	new (mem) Vector2(p_value);
	luaL_getmetatable(p_L, VECTOR2_MT);
	lua_setmetatable(p_L, -2);
}

void luau_module::push_typed_vector3(lua_State *p_L, const Vector3 &p_value) {
	void *mem = lua_newuserdata(p_L, sizeof(Vector3));
	new (mem) Vector3(p_value);
	luaL_getmetatable(p_L, VECTOR3_MT);
	lua_setmetatable(p_L, -2);
}

void luau_module::push_typed_color(lua_State *p_L, const Color &p_value) {
	void *mem = lua_newuserdata(p_L, sizeof(Color));
	new (mem) Color(p_value);
	luaL_getmetatable(p_L, COLOR_MT);
	lua_setmetatable(p_L, -2);
}

bool luau_module::is_typed_blazium_value(lua_State *p_L, int p_index) {
	if (!lua_getmetatable(p_L, p_index)) {
		return false;
	}

	const char *names[] = { VECTOR2_MT, VECTOR3_MT, COLOR_MT, nullptr };
	for (const char **name = names; *name; name++) {
		luaL_getmetatable(p_L, *name);
		if (lua_rawequal(p_L, -1, -2)) {
			lua_pop(p_L, 2);
			return true;
		}
		lua_pop(p_L, 1);
	}

	lua_pop(p_L, 1);
	return false;
}

Variant luau_module::to_typed_blazium_value(lua_State *p_L, int p_index) {
	void *ud = lua_touserdata(p_L, p_index);
	if (!ud || !is_typed_blazium_value(p_L, p_index)) {
		return Variant();
	}

	if (lua_getmetatable(p_L, p_index)) {
		luaL_getmetatable(p_L, VECTOR2_MT);
		if (lua_rawequal(p_L, -1, -2)) {
			lua_pop(p_L, 2);
			return *static_cast<Vector2 *>(ud);
		}
		lua_pop(p_L, 1);

		luaL_getmetatable(p_L, VECTOR3_MT);
		if (lua_rawequal(p_L, -1, -2)) {
			lua_pop(p_L, 2);
			return *static_cast<Vector3 *>(ud);
		}
		lua_pop(p_L, 1);

		luaL_getmetatable(p_L, COLOR_MT);
		if (lua_rawequal(p_L, -1, -2)) {
			lua_pop(p_L, 2);
			return *static_cast<Color *>(ud);
		}
		lua_pop(p_L, 2);
	}

	return Variant();
}

void luau_module::install_blazium_type_metatables(lua_State *p_L) {
	ERR_FAIL_NULL(p_L);
	ERR_FAIL_COND_MSG(!lua_checkstack(p_L, 2), "install_blazium_type_metatables(): stack overflow.");

	register_typed_metatable(p_L, VECTOR2_MT);
	register_typed_metatable(p_L, VECTOR3_MT);
	register_typed_metatable(p_L, COLOR_MT);
}

namespace {

static int script_vector2_constructor(lua_State *p_L) {
	const double x = luaL_checknumber(p_L, 1);
	const double y = luaL_checknumber(p_L, 2);
	lua_pop(p_L, 2);
	push_typed_vector2(p_L, Vector2(x, y));
	return 1;
}

static int script_vector3_constructor(lua_State *p_L) {
	const double x = luaL_checknumber(p_L, 1);
	const double y = luaL_checknumber(p_L, 2);
	const double z = luaL_checknumber(p_L, 3);
	lua_pop(p_L, 3);
	push_typed_vector3(p_L, Vector3(x, y, z));
	return 1;
}

static int script_color_constructor(lua_State *p_L) {
	const double r = luaL_checknumber(p_L, 1);
	const double g = luaL_checknumber(p_L, 2);
	const double b = luaL_checknumber(p_L, 3);
	const double a = lua_gettop(p_L) >= 4 ? luaL_checknumber(p_L, 4) : 1.0;
	lua_settop(p_L, 0);
	push_typed_color(p_L, Color(r, g, b, a));
	return 1;
}

} //namespace

void luau_module::install_script_math_globals(lua_State *p_L) {
	ERR_FAIL_NULL(p_L);
	ERR_FAIL_COND_MSG(!lua_checkstack(p_L, 3), "install_script_math_globals(): stack overflow.");

	install_blazium_type_metatables(p_L);

	lua_pushcfunction(p_L, script_vector2_constructor, "Vector2");
	lua_setglobal(p_L, "Vector2");
	lua_pushcfunction(p_L, script_vector3_constructor, "Vector3");
	lua_setglobal(p_L, "Vector3");
	lua_pushcfunction(p_L, script_color_constructor, "Color");
	lua_setglobal(p_L, "Color");
}
