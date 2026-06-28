/**************************************************************************/
/*  dictionary.cpp                                                        */
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

#include "bindings/dictionary.h"
#include "bindings/variant.h"
#include "helpers.h"

#include "core/error/error_macros.h"
#include "core/variant/variant.h"
#include <lua.h>

using namespace luau_module;

Dictionary luau_module::to_dictionary(lua_State *L, int p_index, bool *r_success) {
	if (r_success) {
		*r_success = false;
	}

	ERR_FAIL_COND_V_MSG(!is_valid_index(L, p_index), Dictionary(), vformat("to_dictionary(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	if (!lua_istable(L, p_index)) {
		return Dictionary();
	}

	ERR_FAIL_COND_V_MSG(!lua_checkstack(L, 2), Dictionary(), vformat("to_dictionary(%d): Stack overflow. Cannot grow stack.", p_index));

	Dictionary dict;
	int iter = 0;
	while ((iter = lua_rawiter(L, p_index, iter)) >= 0) {
		Variant key = to_variant(L, -2);
		Variant value = to_variant(L, -1);
		lua_pop(L, 2);

		dict[key] = value;
	}

	if (r_success) {
		*r_success = true;
	}

	return dict;
}

void luau_module::push_dictionary(lua_State *L, const Dictionary &p_dict) {
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 3), "push_dictionary(): Stack overflow. Cannot grow stack.");

	lua_createtable(L, 0, p_dict.size());
	for (const Variant &key : p_dict.keys()) {
		Variant value = p_dict[key];
		push_variant(L, key);
		push_variant(L, value);
		lua_rawset(L, -3);
	}
}
