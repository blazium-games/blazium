/**************************************************************************/
/*  object.cpp                                                            */
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

#include "bindings/object.h"

#include "bindings/callable.h"
#include "bindings/variant.h"
#include "helpers.h"
#include "lua_state.h"
#include "luau_script_language.h"
#include "static_strings.h"

#include "core/error/error_macros.h"
#include "core/object/class_db.h"
#include "core/object/ref_counted.h"
#include "core/variant/variant.h"
#include <lua.h>
#include <lualib.h>

using namespace luau_module;

static const char *const OBJECT_METATABLE_NAME = "GDObject";

static ObjectID get_userdata(void *ud) {
	return *static_cast<ObjectID *>(ud);
}

static Object *get_userdata_instance(void *ud) {
	return ObjectDB::get_instance(get_userdata(ud));
}

static void set_userdata_instance(void *ud, Object *p_obj) {
	*static_cast<ObjectID *>(ud) = ObjectID(p_obj->get_instance_id());
}

static void inline_refcounted_dtor(void *ud) {
	RefCounted *rc = static_cast<RefCounted *>(get_userdata_instance(ud));
	if (rc && rc->unreference()) {
		memdelete(rc);
	}
}

static void weak_object_dtor(void *) {
}

static void tagged_object_dtor(lua_State *L, void *ud) {
	ObjectID id = get_userdata(ud);
	if (!id.is_ref_counted()) {
		return;
	}

	RefCounted *rc = static_cast<RefCounted *>(ObjectDB::get_instance(id));
	if (rc && rc->unreference()) {
		memdelete(rc);
	}
}

static int object_tostring(lua_State *L) {
	Object *obj = get_userdata_instance(lua_touserdata(L, 1));
	lua_pop(L, 1);

	if (!obj) {
		lua_pushliteral(L, "<null>");
		return 1;
	}

	CharString utf8 = obj->to_string().utf8();
	lua_pushlstring(L, utf8.get_data(), utf8.length());
	return 1;
}

static int object_eq(lua_State *L) {
	ObjectID a = get_userdata(lua_touserdata(L, 1));
	ObjectID b = get_userdata(lua_touserdata(L, 2));
	lua_pop(L, 2);

	lua_pushboolean(L, a == b);
	return 1;
}

static int object_lt(lua_State *L) {
	ObjectID a = get_userdata(lua_touserdata(L, 1));
	ObjectID b = get_userdata(lua_touserdata(L, 2));
	lua_pop(L, 2);

	lua_pushboolean(L, a < b);
	return 1;
}

static int object_le(lua_State *L) {
	ObjectID a = get_userdata(lua_touserdata(L, 1));
	ObjectID b = get_userdata(lua_touserdata(L, 2));
	lua_pop(L, 2);

	lua_pushboolean(L, !(b < a));
	return 1;
}

static Object *resolve_object(lua_State *L, int p_index) {
	if (lua_islightuserdata(L, p_index)) {
		return to_light_object(L, p_index, LUA_NOTAG);
	}
	return to_full_object(L, p_index, LUA_NOTAG);
}

static StringName read_index_key(lua_State *L, int p_index) {
	switch (lua_type(L, p_index)) {
		case LUA_TSTRING: {
			size_t len = 0;
			const char *str = lua_tolstring(L, p_index, &len);
			return StringName(String::utf8(str, len));
		}
		case LUA_TNUMBER:
			return StringName(String::num_int64((int64_t)lua_tointeger(L, p_index)));
		default:
			return StringName();
	}
}

static int object_index(lua_State *L) {
	Object *obj = resolve_object(L, 1);
	if (!obj) {
		luaL_error(L, "attempt to index a null Object");
	}

	const StringName key = read_index_key(L, 2);
	if (key.is_empty()) {
		lua_pushnil(L);
		return 1;
	}

	if (ScriptInstance *script_instance = obj->get_script_instance()) {
		Variant value;
		if (script_instance->get(key, value)) {
			push_variant(L, value);
			return 1;
		}
		if (script_instance->has_method(key)) {
			push_callable(L, Callable(obj, key));
			return 1;
		}
	}

	Variant classdb_value;
	if (::ClassDB::get_property(obj, key, classdb_value)) {
		push_variant(L, classdb_value);
		return 1;
	}

	if (::ClassDB::has_method(obj->get_class(), key)) {
		push_callable(L, Callable(obj, key));
		return 1;
	}

	if (obj->has_signal(key)) {
		CharString utf8 = String(key).utf8();
		lua_pushlstring(L, utf8.get_data(), utf8.length());
		return 1;
	}

	lua_pushnil(L);
	return 1;
}

static int object_newindex(lua_State *L) {
	Object *obj = resolve_object(L, 1);
	if (!obj) {
		luaL_error(L, "attempt to set property on a null Object");
	}

	const StringName key = read_index_key(L, 2);
	if (key.is_empty()) {
		luaL_error(L, "invalid property key");
	}

	const Variant value = to_variant(L, 3);

	if (ScriptInstance *script_instance = obj->get_script_instance()) {
		if (script_instance->set(key, value)) {
			return 0;
		}
	}

	bool valid = false;
	if (::ClassDB::set_property(obj, key, value, &valid) && valid) {
		return 0;
	}

	luaL_error(L, "cannot assign property '%s' on Object", String(key).utf8().get_data());
	return 0;
}

static bool has_object_metatable(lua_State *L, int p_index) {
	ERR_FAIL_COND_V_MSG(!lua_checkstack(L, 3), false, vformat("has_object_metatable(%d): Stack overflow. Cannot grow stack.", p_index));

	int abs_index = lua_absindex(L, p_index);

	luaL_getmetatable(L, OBJECT_METATABLE_NAME);
	if (!lua_getmetatable(L, abs_index)) {
		lua_pop(L, 1);
		return false;
	}

	while (!lua_rawequal(L, -1, -2)) {
		int type = lua_rawgetfield(L, -1, "__index");
		lua_remove(L, -2);

		if (type != LUA_TTABLE) {
			lua_pop(L, 2);
			return false;
		}
	}

	lua_pop(L, 2);
	return true;
}

void luau_module::push_object_metatable(lua_State *L) {
	if (!luaL_newmetatable(L, OBJECT_METATABLE_NAME)) {
		return;
	}

	lua_pushcfunction(L, object_tostring, "Object.__tostring");
	lua_setfield(L, -2, "__tostring");

	lua_pushcfunction(L, generic_lua_concat, "Object.__concat");
	lua_setfield(L, -2, "__concat");

	lua_pushcfunction(L, object_eq, "Object.__eq");
	lua_setfield(L, -2, "__eq");

	lua_pushcfunction(L, object_lt, "Object.__lt");
	lua_setfield(L, -2, "__lt");

	lua_pushcfunction(L, object_le, "Object.__le");
	lua_setfield(L, -2, "__le");

	lua_pushcfunction(L, object_index, "Object.__index");
	lua_setfield(L, -2, "__index");

	lua_pushcfunction(L, object_newindex, "Object.__newindex");
	lua_setfield(L, -2, "__newindex");

	lua_setreadonly(L, -1, 1);
}

bool luau_module::is_object(lua_State *L, int p_index, int p_tag) {
	ERR_FAIL_COND_V_MSG(!is_valid_index(L, p_index), false, vformat("is_object(%d, %d): Invalid stack index. Stack has %d elements.", p_index, p_tag, lua_gettop(L)));

	switch (lua_type(L, p_index)) {
		case LUA_TLIGHTUSERDATA:
			return p_tag == LUA_NOTAG || lua_lightuserdatatag(L, p_index) == p_tag;

		case LUA_TUSERDATA: {
			int actual_tag = lua_userdatatag(L, p_index);
			if (p_tag != LUA_NOTAG) {
				return actual_tag == p_tag;
			} else if (actual_tag < 0 || actual_tag >= LUA_UTAG_LIMIT) {
				if (!has_object_metatable(L, p_index)) {
					return false;
				}
			}

			return true;
		}

		default:
			return false;
	}
}

Object *luau_module::to_full_object(lua_State *L, int p_index, int p_tag) {
	ERR_FAIL_COND_V_MSG(!is_valid_index(L, p_index), nullptr, vformat("to_object(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	if (lua_type(L, p_index) != LUA_TUSERDATA || !is_object(L, p_index, p_tag)) {
		return nullptr;
	}

	return get_userdata_instance(lua_touserdata(L, p_index));
}

Object *luau_module::to_light_object(lua_State *L, int p_index, int p_tag) {
	ERR_FAIL_COND_V_MSG(!is_valid_index(L, p_index), nullptr, vformat("to_light_object(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	if (!lua_islightuserdata(L, p_index)) {
		return nullptr;
	}

	void *ud = p_tag == LUA_NOTAG ? lua_tolightuserdata(L, p_index) : lua_tolightuserdatatagged(L, p_index, p_tag);
	return static_cast<Object *>(ud);
}

Object *luau_module::to_object(lua_State *L, int p_index, int p_tag) {
	ERR_FAIL_COND_V_MSG(!is_valid_index(L, p_index), nullptr, vformat("to_object(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	Variant toblazium_result;
	if (call_toblazium_metamethod(L, p_index, toblazium_result, p_tag)) {
		Variant::Type type = toblazium_result.get_type();
		ERR_FAIL_COND_V_MSG(type != Variant::NIL && type != Variant::OBJECT, nullptr, vformat("to_object(%d): __toblazium did not return an Object.", p_index));

		return static_cast<Object *>(toblazium_result);
	}

	switch (lua_type(L, p_index)) {
		case LUA_TLIGHTUSERDATA:
			return to_light_object(L, p_index, p_tag);

		case LUA_TUSERDATA:
			return to_full_object(L, p_index, p_tag);

		default:
			return nullptr;
	}
}

static void push_refcounted_object(lua_State *L, RefCounted *p_obj) {
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 2), "push_refcounted_object(): Stack overflow. Cannot grow stack.");

	if (!p_obj->init_ref()) {
		lua_pushnil(L);
		return;
	}

	void *ud = lua_newuserdatadtor(L, sizeof(ObjectID), inline_refcounted_dtor);
	set_userdata_instance(ud, p_obj);

	push_object_metatable(L);
	lua_setmetatable(L, -2);
}

static void push_refcounted_object_custom(lua_State *L, RefCounted *p_obj, int p_tag) {
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 2), "push_refcounted_object_custom(): Stack overflow. Cannot grow stack.");

	if (!p_obj->init_ref()) {
		lua_pushnil(L);
		return;
	}

	void *ud = lua_newuserdatatagged(L, sizeof(ObjectID), p_tag);
	set_userdata_instance(ud, p_obj);
	lua_setuserdatadtor(L, p_tag, tagged_object_dtor);

	lua_getuserdatametatable(L, p_tag);
	lua_setmetatable(L, -2);
}

static void push_weak_object(lua_State *L, Object *p_obj) {
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 2), "push_weak_object(): Stack overflow. Cannot grow stack.");

	void *ud = lua_newuserdatadtor(L, sizeof(ObjectID), weak_object_dtor);
	set_userdata_instance(ud, p_obj);

	push_object_metatable(L);
	lua_setmetatable(L, -2);
}

static void push_weak_object_custom(lua_State *L, Object *p_obj, int p_tag) {
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 1), "push_weak_object_custom(): Stack overflow. Cannot grow stack.");

	void *ud = lua_newuserdatatagged(L, sizeof(ObjectID), p_tag);
	set_userdata_instance(ud, p_obj);

	lua_getuserdatametatable(L, p_tag);
	lua_setmetatable(L, -2);
}

static void push_full_object_given_tag(lua_State *L, Object *p_obj, int p_tag) {
	RefCounted *rc = Object::cast_to<RefCounted>(p_obj);
	if (rc && p_tag == LUA_NOTAG) {
		push_refcounted_object(L, rc);
	} else if (rc && p_tag != LUA_NOTAG) {
		push_refcounted_object_custom(L, rc, p_tag);
	} else if (p_tag == LUA_NOTAG) {
		push_weak_object(L, p_obj);
	} else {
		push_weak_object_custom(L, p_obj, p_tag);
	}
}

void luau_module::push_full_object(lua_State *L, Object *p_obj, int p_tag) {
	if (!p_obj) {
		ERR_FAIL_COND_MSG(!lua_checkstack(L, 1), "push_full_object(): Stack overflow. Cannot grow stack.");
		lua_pushnil(L);
		return;
	}

	Variant tag_variant = p_obj->get(static_strings->lua_userdata_tag);
	if (tag_variant.get_type() != Variant::NIL) {
		int tag_value = tag_variant;
		ERR_FAIL_COND_MSG(tag_value < 0 || tag_value >= LUA_UTAG_LIMIT, vformat("push_full_object(): Object %s has invalid lua_userdata_tag constant: %d", p_obj, tag_value));

		if (p_tag != LUA_NOTAG && p_tag != tag_value) {
			WARN_PRINT(vformat("push_full_object(): Object %s has lua_userdata_tag set to %d, but a different tag %d was given. Using %d.", p_obj, tag_value, p_tag, p_tag));
			tag_value = p_tag;
		}

		push_full_object_given_tag(L, p_obj, tag_value);
	} else {
		push_full_object_given_tag(L, p_obj, p_tag);
	}
}

void luau_module::push_light_object(lua_State *L, Object *p_obj, int p_tag) {
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 1), "push_light_object(): Stack overflow. Cannot grow stack.");

	if (!p_obj) {
		lua_pushnil(L);
		return;
	}

	if (p_tag == LUA_NOTAG) {
		lua_pushlightuserdata(L, static_cast<void *>(p_obj));
	} else {
		lua_pushlightuserdatatagged(L, static_cast<void *>(p_obj), p_tag);
	}
}

void luau_module::push_object(lua_State *L, Object *p_obj, int p_tag) {
	if (!p_obj) {
		ERR_FAIL_COND_MSG(!lua_checkstack(L, 1), "push_object(): Stack overflow. Cannot grow stack.");
		lua_pushnil(L);
		return;
	} else if (p_obj->has_method(static_strings->push_to_lua)) {
		p_obj->call(static_strings->push_to_lua, LuaState::find_or_create_lua_state(L), p_tag);
		return;
	} else {
		push_full_object(L, p_obj, p_tag);
	}
}

void luau_module::update_full_object_tag(lua_State *L, int p_index, int p_tag) {
	lua_setuserdatatag(L, p_index, p_tag);
	lua_setuserdatadtor(L, p_tag, tagged_object_dtor);
}
