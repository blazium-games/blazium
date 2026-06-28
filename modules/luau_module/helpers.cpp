/**************************************************************************/
/*  helpers.cpp                                                           */
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

#include "helpers.h"

#include "string_cache.h"

#include "core/error/error_macros.h"
#include "core/variant/variant.h"
#include <lua.h>
#include <lualib.h>

using namespace luau_module;

int luau_module::generic_lua_concat(lua_State *L) {
	luaL_checklstring(L, 1, NULL);
	luaL_checklstring(L, 2, NULL);
	lua_concat(L, 2);
	return 1;
}

bool luau_module::is_valid_index(lua_State *L, int p_index) {
	if (p_index == 0) {
		return false;
	} else if (lua_ispseudo(p_index)) {
		return true;
	}

	int top = lua_gettop(L);

	if (p_index > 0) {
		return p_index <= top;
	} else {
		return p_index >= -top;
	}
}

StringName luau_module::to_string_name(lua_State *L, int p_index) {
	size_t len;
	int atom = -1;
	const char *str = lua_tolstringatom(L, p_index, &len, &atom);
	if (!str) {
		return StringName();
	}

	const StringName &cached = string_name_for_atom(atom);
	if (cached.is_empty()) {
		return StringName(String::utf8(str, len));
	} else {
		return cached;
	}
}
