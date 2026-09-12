/**************************************************************************/
/*  array.cpp                                                             */
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

#include "bindings/array.h"

#include "bindings/variant.h"
#include "helpers.h"

#include "core/error/error_macros.h"
#include "core/variant/variant.h"
#include <lua.h>

using namespace luau_module;

bool luau_module::is_array(lua_State *L, int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid_index(L, p_index), false, vformat("is_array(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	if (!lua_istable(L, p_index)) {
		return false;
	}

	ERR_FAIL_COND_V_MSG(!lua_checkstack(L, 2), false, vformat("is_array(%d): Stack overflow. Cannot grow stack.", p_index));

	int iter = 0;
	while ((iter = lua_rawiter(L, p_index, iter)) >= 0) {
		int isnum;
		double num = lua_tonumberx(L, -2, &isnum);
		lua_pop(L, 2);

		if (!isnum || nearbyint(num) != num) {
			return false;
		}

		if (static_cast<int>(num) != iter) {
			return false;
		}
	}

	return true;
}

Array luau_module::to_array(lua_State *L, int p_index, bool *r_is_array) {
	if (r_is_array) {
		*r_is_array = false;
	}

	ERR_FAIL_COND_V_MSG(!is_valid_index(L, p_index), Array(), vformat("to_array(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	if (!lua_istable(L, p_index)) {
		return Array();
	}

	ERR_FAIL_COND_V_MSG(!lua_checkstack(L, 2), Array(), vformat("to_array(%d): Stack overflow. Cannot grow stack.", p_index));

	Array arr;
	int iter = 0;
	bool is_array = true;
	while ((iter = lua_rawiter(L, p_index, iter)) >= 0) {
		int isnum;
		double num = lua_tonumberx(L, -2, &isnum);

		if (!isnum || nearbyint(num) != num || static_cast<int>(num) != iter) {
			is_array = false;
			lua_pop(L, 2);
			break;
		}

		Variant value = to_variant(L, -1);
		arr.push_back(value);
		lua_pop(L, 2);
	}

	if (r_is_array) {
		*r_is_array = is_array;
	}

	return arr;
}

void luau_module::push_array(lua_State *L, const Array &p_arr) {
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 2), "LuaState.push_array(): Stack overflow. Cannot grow stack.");

	lua_createtable(L, p_arr.size(), 0);
	for (int i = 0; i < p_arr.size(); i++) {
		push_variant(L, p_arr[i]);
		lua_rawseti(L, -2, i + 1);
	}
}
