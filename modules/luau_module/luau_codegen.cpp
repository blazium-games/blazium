/**************************************************************************/
/*  luau_codegen.cpp                                                      */
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

#include "luau_codegen.h"

#include "core/error/error_macros.h"

#include <lua.h>

#ifdef LUAU_MODULE_CODEGEN_ENABLED
#include <luacodegen.h>
#endif

using namespace luau_module;

namespace {

#ifdef LUAU_MODULE_CODEGEN_ENABLED
constexpr char k_codegen_registry_key[] = "luau_module_codegen_initialized";
#endif

} //namespace

bool LuauCodegen::is_enabled() {
#ifdef LUAU_MODULE_CODEGEN_ENABLED
	return luau_codegen_supported() != 0;
#else
	return false;
#endif
}

bool LuauCodegen::is_supported() {
	return is_enabled();
}

bool LuauCodegen::source_requests_native(const String &p_source) {
	return p_source.contains("--!native");
}

void LuauCodegen::ensure_vm(lua_State *p_L) {
#ifdef LUAU_MODULE_CODEGEN_ENABLED
	ERR_FAIL_NULL(p_L);
	if (!is_enabled()) {
		return;
	}

	lua_pushlightuserdata(p_L, (void *)k_codegen_registry_key);
	lua_rawget(p_L, LUA_REGISTRYINDEX);
	const bool initialized = lua_toboolean(p_L, -1) != 0;
	lua_pop(p_L, 1);
	if (initialized) {
		return;
	}

	luau_codegen_create(p_L);

	lua_pushlightuserdata(p_L, (void *)k_codegen_registry_key);
	lua_pushboolean(p_L, 1);
	lua_rawset(p_L, LUA_REGISTRYINDEX);
#endif
}

void LuauCodegen::compile_loaded(lua_State *p_L, int p_idx, const String &p_source) {
#ifdef LUAU_MODULE_CODEGEN_ENABLED
	ERR_FAIL_NULL(p_L);
	if (!is_enabled() || !source_requests_native(p_source)) {
		return;
	}

	ensure_vm(p_L);
	luau_codegen_compile(p_L, p_idx);
#else
	(void)p_L;
	(void)p_idx;
	(void)p_source;
#endif
}
