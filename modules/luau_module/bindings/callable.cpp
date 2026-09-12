/**************************************************************************/
/*  callable.cpp                                                          */
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

#include "bindings/callable.h"
#include "luau_script_language.h"

#include "bindings/variant.h"
#include "helpers.h"
#include "lua_state.h"

#include "core/error/error_macros.h"
#include "core/os/memory.h"
#include "core/os/time.h"
#include "core/templates/hashfuncs.h"
#include "core/variant/variant.h"
#include <lualib.h>

using namespace luau_module;

static const char *const CALLABLE_METATABLE_NAME = "GDCallable";

static void callable_dtor(void *ud) {
	Callable *callable = static_cast<Callable *>(ud);
	callable->~Callable();
}

static int callable_tostring(lua_State *L) {
	Callable *callable = static_cast<Callable *>(lua_touserdata(L, 1));
	String str = Variant(*callable).stringify();
	lua_pop(L, 1);

	CharString utf8 = str.utf8();
	lua_pushlstring(L, utf8.get_data(), utf8.length());
	return 1;
}

static int callable_eq(lua_State *L) {
	Callable a = *static_cast<Callable *>(lua_touserdata(L, 1));
	Callable b = *static_cast<Callable *>(lua_touserdata(L, 2));
	lua_pop(L, 2);

	lua_pushboolean(L, a == b);
	return 1;
}

static int callable_call(lua_State *L) {
	Callable callable = *static_cast<Callable *>(lua_touserdata(L, 1));
	if (!callable.is_valid()) {
		luaL_argerror(L, 1, "Callable is not valid");
	}

	int arg_count = lua_gettop(L) - 1;
	int expected_args = callable.get_argument_count();
	int luastate_arg = 0;
	if (callable.is_custom()) {
		if (dynamic_cast<LuaStateBoundCallable *>(callable.get_custom())) {
			luastate_arg = 1;
		}
	}
	if (expected_args >= 0 && arg_count + luastate_arg < expected_args) {
		luaL_error(L, "Too few arguments for Callable (expected at least %d, got %d)", expected_args, arg_count);
	}

	Array args;
	args.resize(arg_count + luastate_arg);
	if (luastate_arg) {
		args[0] = LuaState::find_or_create_lua_state(L);
	}

	for (int argi = 0; argi < arg_count; argi++) {
		int stack_idx = argi + 2;
		args[argi + luastate_arg] = to_variant(L, stack_idx);
	}

	lua_pop(L, arg_count + 1);

#ifdef DEBUG_ENABLED
	const uint64_t profile_start = Time::get_singleton()->get_ticks_usec();
#endif

	Variant result = callable.callv(args);

#ifdef DEBUG_ENABLED
	if (LuauScriptLanguage *lang = LuauScriptLanguage::get_singleton()) {
		if (lang->is_profiling_active() && lang->is_profile_native_calls_enabled()) {
			bool record_native = true;
			if (callable.is_custom()) {
				record_native = dynamic_cast<LuaCallable *>(callable.get_custom()) == nullptr;
			}
			if (record_native) {
				String native_sig = callable.get_method().is_empty() ? Variant(callable).stringify() : String(callable.get_method());
				lang->profile_record_native_call(StringName(native_sig), Time::get_singleton()->get_ticks_usec() - profile_start);
			}
		}
	}
#endif

	int additional_returns = lua_gettop(L);

	push_variant(L, result);
	return 1 + additional_returns;
}

static void push_callable_metatable(lua_State *L) {
	if (!luaL_newmetatable(L, CALLABLE_METATABLE_NAME)) {
		return;
	}

	lua_pushcfunction(L, callable_tostring, "Callable.__tostring");
	lua_setfield(L, -2, "__tostring");

	lua_pushcfunction(L, generic_lua_concat, "Callable.__concat");
	lua_setfield(L, -2, "__concat");

	lua_pushcfunction(L, callable_eq, "Callable.__eq");
	lua_setfield(L, -2, "__eq");

	lua_pushcfunction(L, callable_call, "Callable.__call");
	lua_setfield(L, -2, "__call");

	lua_setreadonly(L, -1, 1);
}

bool luau_module::is_blazium_callable(lua_State *L, int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid_index(L, p_index), false, vformat("is_blazium_callable(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));
	ERR_FAIL_COND_V_MSG(!lua_checkstack(L, 2), false, vformat("is_blazium_callable(%d): Stack overflow. Cannot grow stack.", p_index));

	if (!lua_getmetatable(L, p_index)) {
		return false;
	}

	luaL_getmetatable(L, CALLABLE_METATABLE_NAME);
	bool mt_equal = lua_rawequal(L, -1, -2);
	lua_pop(L, 2);

	return mt_equal;
}

Callable luau_module::to_callable(lua_State *L, int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid_index(L, p_index), Callable(), vformat("to_callable(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	int type = lua_type(L, p_index);
	if (type != LUA_TFUNCTION && type != LUA_TUSERDATA && type != LUA_TTABLE) {
		return Callable();
	}

	if (is_blazium_callable(L, p_index)) {
		Callable *callable = static_cast<Callable *>(lua_touserdata(L, p_index));
		return *callable;
	}

	int value_ref = lua_ref(L, p_index);

	LuaState *state = LuaState::find_lua_state(L);
	ERR_FAIL_COND_V_MSG(!state, Callable(), "to_callable(): Could not find existing LuaState for the given lua_State.");

	LuaCallable *lc = memnew(LuaCallable(state, value_ref));
	return Callable(lc);
}

void luau_module::push_callable(lua_State *L, const Callable &p_callable) {
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 2), "push_callable(): Stack overflow. Cannot grow stack.");

	if (p_callable.is_custom()) {
		LuaCallable *lc = dynamic_cast<LuaCallable *>(p_callable.get_custom());
		if (lc) {
			ERR_FAIL_COND_MSG(!lc->is_valid(), "push_callable(): LuaCallable is invalid.");

			LuaState *callable_state = lc->get_lua_state();
			ERR_FAIL_COND_MSG(callable_state->get_main_thread()->get_lua_state() != lua_mainthread(L), "push_callable(): Cannot push a Lua value from a different Luau VM.");

			lua_getref(L, lc->get_lua_ref());
			return;
		}
	}

	void *ptr = lua_newuserdatadtor(L, sizeof(Callable), callable_dtor);
	memnew_placement(ptr, Callable(p_callable));

	push_callable_metatable(L);
	lua_setmetatable(L, -2);
}

LuaCallable::LuaCallable(LuaState *p_state, int p_lua_ref) :
		lua_state_id(p_state->get_instance_id()), lua_ref(p_lua_ref) {
}

LuaCallable::~LuaCallable() {
	LuaState *state = get_lua_state();

	if (state && lua_ref != LUA_NOREF && state->is_valid()) {
		state->unref(lua_ref);
	}
}

bool LuaCallable::get_func_info(const char *p_what, lua_Debug &r_ar) const {
	LuaState *state = get_lua_state();
	if (!state || !state->is_valid()) {
		return false;
	}

	lua_State *L = state->get_lua_state();
	ERR_FAIL_COND_V_MSG(!lua_checkstack(L, 1), false, "LuaCallable.get_func_info(): Stack overflow. Cannot grow stack.");

	if (lua_getref(L, lua_ref) != LUA_TFUNCTION) {
		lua_pop(L, 1);
		return false;
	}

	bool result = (lua_getinfo(L, -1, p_what, &r_ar) != 0);
	lua_pop(L, 1);

	return result;
}

bool LuaCallable::get_func_from_callable_table_or_userdata(lua_State *L) const {
	ERR_FAIL_COND_V_MSG(!lua_checkstack(L, 2), false, "LuaCallable.get_func_from_callable_table_or_userdata(): Stack overflow. Cannot grow stack.");
	ERR_FAIL_COND_V_MSG(lua_getmetatable(L, -1) == 0, false, vformat("LuaCallable.get_func_from_callable_table_or_userdata(): Expected userdata or table %s to have a __call metamethod, but no metatable found.", get_as_text()));

	int type = lua_getfield(L, -1, "__call");
	if (type == LUA_TFUNCTION) {
		lua_remove(L, -2);
		return true;
	} else if (type == LUA_TTABLE || type == LUA_TUSERDATA) {
		lua_remove(L, -2);
		int value_index = lua_gettop(L);
		bool result = get_func_from_callable_table_or_userdata(L);
		lua_remove(L, value_index);

		return result;
	} else {
		lua_pop(L, 2);
		return false;
	}
}

uint32_t LuaCallable::hash() const {
	uint32_t h = HASH_MURMUR3_SEED;
	h = hash_murmur3_one_64(static_cast<uint64_t>(lua_state_id), h);
	h = hash_murmur3_one_64(static_cast<uint64_t>(lua_ref), h);
	return hash_fmix32(h);
}

String LuaCallable::get_as_text() const {
	lua_Debug ar;
	if (get_func_info("n", ar)) {
		return "lua:" + String(ar.name);
	}

	LuaState *state = get_lua_state();
	if (!state || !state->is_valid()) {
		return "lua:<unknown>";
	}

	lua_State *L = state->get_lua_state();
	ERR_FAIL_COND_V_MSG(!lua_checkstack(L, 2), "<unknown>", "LuaCallable.get_as_text(): Stack overflow. Cannot grow stack.");

	lua_getref(L, lua_ref);

	size_t len;
	const char *str = luaL_tolstring(L, -1, &len);
	if (!str) {
		lua_pop(L, 2);
		return "lua:<unknown>";
	}

	String result = "lua:" + String::utf8(str, len);
	lua_pop(L, 2);

	return result;
}

CallableCustom::CompareEqualFunc LuaCallable::get_compare_equal_func() const {
	return &LuaCallable::compare_equal;
}

CallableCustom::CompareLessFunc LuaCallable::get_compare_less_func() const {
	return &LuaCallable::compare_less;
}

ObjectID LuaCallable::get_object() const {
	return lua_state_id;
}

bool LuaCallable::is_valid() const {
	LuaState *state = get_lua_state();
	return state != nullptr && state->is_valid();
}

int LuaCallable::get_argument_count(bool &r_is_valid) const {
	lua_Debug ar;
	if (get_func_info("a", ar)) {
		r_is_valid = true;
		return ar.nparams;
	} else {
		r_is_valid = false;
		return 0;
	}
}

void LuaCallable::call(const Variant **p_arguments, int p_argcount, Variant &r_return_value, Callable::CallError &r_call_error) const {
	r_call_error.error = Callable::CallError::CALL_OK;
	r_return_value = Variant();

	LuaState *state = get_lua_state();
	if (!state) {
		ERR_PRINT("LuaCallable.call(): LuaState is null");
		r_call_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
		return;
	} else if (!state->is_valid()) {
		ERR_PRINT("LuaCallable.call(): LuaState is not valid");
		r_call_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
		return;
	}

	lua_State *L = state->get_lua_state();
	if (!L) {
		ERR_PRINT("LuaCallable.call(): lua_State is null");
		r_call_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
		return;
	}

	if (!lua_checkstack(L, 1 + p_argcount)) {
		ERR_PRINT(vformat("LuaCallable.call(): Stack overflow. Cannot grow stack for %d arguments.", p_argcount));
		r_call_error.error = Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
		return;
	}

	int type = lua_getref(L, lua_ref);
	int self_arg = 0;
	switch (type) {
		case LUA_TFUNCTION:

			break;

		case LUA_TUSERDATA:
			[[fallthrough]];
		case LUA_TTABLE:
			if (!get_func_from_callable_table_or_userdata(L)) {
				ERR_PRINT(vformat("LuaCallable.call(): Expected userdata or table %s to have a __call metamethod", get_as_text()));
				lua_pop(L, 1);
				r_call_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
				return;
			}

			lua_insert(L, -2);
			self_arg = 1;
			break;

		default:
			ERR_PRINT(vformat("LuaCallable.call(): Expected function, userdata, or table, got %s.", lua_typename(L, type)));
			lua_pop(L, 1);
			r_call_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
			return;
	}

	for (int i = 0; i < p_argcount; i++) {
		push_variant(L, *(p_arguments[i]));
	}

	int status = lua_pcall(L, self_arg + p_argcount, 1, 0);
	if (status != LUA_OK) {
		const char *error_msg = lua_tostring(L, -1);
		ERR_PRINT(vformat("LuaCallable.call(): error during call to %s: %s", get_as_text(), error_msg));
		lua_pop(L, 1);

		return;
	}

	r_return_value = to_variant(L, -1);
	lua_pop(L, 1);
}

bool LuaCallable::compare_equal(const CallableCustom *p_a, const CallableCustom *p_b) {
	const LuaCallable *a = static_cast<const LuaCallable *>(p_a);
	const LuaCallable *b = static_cast<const LuaCallable *>(p_b);
	return a->lua_state_id == b->lua_state_id && a->lua_ref == b->lua_ref;
}

bool LuaCallable::compare_less(const CallableCustom *p_a, const CallableCustom *p_b) {
	const LuaCallable *a = static_cast<const LuaCallable *>(p_a);
	const LuaCallable *b = static_cast<const LuaCallable *>(p_b);

	if (a->lua_state_id != b->lua_state_id) {
		return a->lua_state_id < b->lua_state_id;
	}

	return a->lua_ref < b->lua_ref;
}

LuaState *LuaCallable::get_lua_state() const {
	Object *obj = ObjectDB::get_instance(lua_state_id);
	return Object::cast_to<LuaState>(obj);
}

int LuaCallable::get_lua_ref() const {
	return lua_ref;
}

uint32_t LuaStateBoundCallable::hash() const {
	uint32_t h = HASH_MURMUR3_SEED;
	h = hash_murmur3_one_32(callable.hash(), h);
	h = hash_murmur3_one_32(0xB17D, h);
	return hash_fmix32(h);
}

String LuaStateBoundCallable::get_as_text() const {
	return vformat("LuaStateBoundCallable(%s)", callable);
}

CallableCustom::CompareEqualFunc LuaStateBoundCallable::get_compare_equal_func() const {
	return &LuaStateBoundCallable::compare_equal;
}

CallableCustom::CompareLessFunc LuaStateBoundCallable::get_compare_less_func() const {
	return &LuaStateBoundCallable::compare_less;
}

bool LuaStateBoundCallable::is_valid() const {
	return callable.is_valid();
}

ObjectID LuaStateBoundCallable::get_object() const {
	return ObjectID(callable.get_object_id());
}

int LuaStateBoundCallable::get_argument_count(bool &r_is_valid) const {
	if (callable.is_custom()) {
		CallableCustom *custom = callable.get_custom();
		return custom->get_argument_count(r_is_valid);
	} else {
		r_is_valid = true;
		return callable.get_argument_count();
	}
}

void LuaStateBoundCallable::call(const Variant **p_arguments, int p_argcount, Variant &r_return_value, Callable::CallError &r_call_error) const {
	if (!callable.is_valid()) {
		r_call_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		ERR_FAIL_MSG("LuaStateBoundCallable.call(): Callable is not valid.");
	}

	if (p_argcount < 1) {
		r_call_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_call_error.argument = 0;
		r_call_error.expected = 1;
		ERR_FAIL_MSG("LuaStateBoundCallable.call(): At least one argument (the LuaState) is required.");
	}

	LuaState *state = Object::cast_to<LuaState>(*p_arguments[0]);
	if (!state) {
		r_call_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
		ERR_FAIL_MSG("LuaStateBoundCallable.call(): First argument must be a valid LuaState.");
	}

	Array args;
	args.resize(p_argcount);
	for (int i = 0; i < p_argcount; i++) {
		args[i] = *(p_arguments[i]);
	}

	r_return_value = callable.callv(args);
	r_call_error.error = Callable::CallError::CALL_OK;
}

bool LuaStateBoundCallable::compare_equal(const CallableCustom *p_a, const CallableCustom *p_b) {
	const LuaStateBoundCallable *a = static_cast<const LuaStateBoundCallable *>(p_a);
	const LuaStateBoundCallable *b = static_cast<const LuaStateBoundCallable *>(p_b);
	return a->callable == b->callable;
}

bool LuaStateBoundCallable::compare_less(const CallableCustom *p_a, const CallableCustom *p_b) {
	const LuaStateBoundCallable *a = static_cast<const LuaStateBoundCallable *>(p_a);
	const LuaStateBoundCallable *b = static_cast<const LuaStateBoundCallable *>(p_b);

	return Variant(a->callable) < Variant(b->callable);
}
