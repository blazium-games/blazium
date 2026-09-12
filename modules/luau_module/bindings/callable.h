/**************************************************************************/
/*  callable.h                                                            */
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

#include "core/object/ref_counted.h"
#include "core/variant/callable.h"

#include <lua.h>

namespace luau_module {

class LuaState;

bool is_blazium_callable(lua_State *p_L, int p_index);
Callable to_callable(lua_State *p_L, int p_index);
void push_callable(lua_State *p_L, const Callable &p_callable);

class LuaCallable : public CallableCustom {
private:
	ObjectID lua_state_id;
	int lua_ref;

	bool get_func_info(const char *p_what, lua_Debug &r_ar) const;
	bool get_func_from_callable_table_or_userdata(lua_State *L) const;

public:
	LuaCallable(LuaState *p_state, int p_lua_ref);
	~LuaCallable();

	virtual uint32_t hash() const override;
	virtual String get_as_text() const override;
	virtual CompareEqualFunc get_compare_equal_func() const override;
	virtual CompareLessFunc get_compare_less_func() const override;
	virtual bool is_valid() const override;
	virtual ObjectID get_object() const override;
	virtual int get_argument_count(bool &r_is_valid) const override;
	virtual void call(const Variant **p_arguments, int p_argcount, Variant &r_return_value, Callable::CallError &r_call_error) const override;

	static bool compare_equal(const CallableCustom *p_a, const CallableCustom *p_b);
	static bool compare_less(const CallableCustom *p_a, const CallableCustom *p_b);

	LuaState *get_lua_state() const;
	int get_lua_ref() const;
};

class LuaStateBoundCallable : public CallableCustom {
private:
	Callable callable;

public:
	LuaStateBoundCallable(Callable p_callable) :
			callable(p_callable) {}

	virtual uint32_t hash() const override;
	virtual String get_as_text() const override;
	virtual CompareEqualFunc get_compare_equal_func() const override;
	virtual CompareLessFunc get_compare_less_func() const override;
	virtual bool is_valid() const override;
	virtual ObjectID get_object() const override;
	virtual int get_argument_count(bool &r_is_valid) const override;
	virtual void call(const Variant **p_arguments, int p_argcount, Variant &r_return_value, Callable::CallError &r_call_error) const override;

	static bool compare_equal(const CallableCustom *p_a, const CallableCustom *p_b);
	static bool compare_less(const CallableCustom *p_a, const CallableCustom *p_b);
};
} //namespace luau_module
