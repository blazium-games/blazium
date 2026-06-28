/**************************************************************************/
/*  luau.cpp                                                              */
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

#include "luau.h"
#include "helpers.h"
#include "lua_compileoptions.h"
#include "luau_codegen.h"
#include "luau_compile_result.h"

#include "core/object/class_db.h"
#include <luacode.h>
#include <stdlib.h>

using namespace luau_module;

#define BIND_LUAU_ENUM(m_name) ClassDB::bind_integer_constant(get_class_static(), "", #m_name, m_name)

void Luau::_bind_methods() {
	ClassDB::bind_integer_constant(get_class_static(), "", "LUA_MULTRET", LUA_MULTRET);
	ClassDB::bind_integer_constant(get_class_static(), "", "LUA_REGISTRYINDEX", LUA_REGISTRYINDEX);
	ClassDB::bind_integer_constant(get_class_static(), "", "LUA_ENVIRONINDEX", LUA_ENVIRONINDEX);
	ClassDB::bind_integer_constant(get_class_static(), "", "LUA_GLOBALSINDEX", LUA_GLOBALSINDEX);

	BIND_LUAU_ENUM(LUA_OK);
	BIND_LUAU_ENUM(LUA_YIELD);
	BIND_LUAU_ENUM(LUA_ERRRUN);
	BIND_LUAU_ENUM(LUA_ERRSYNTAX);
	BIND_LUAU_ENUM(LUA_ERRMEM);
	BIND_LUAU_ENUM(LUA_ERRERR);
	BIND_LUAU_ENUM(LUA_BREAK);

	BIND_LUAU_ENUM(LUA_CORUN);
	BIND_LUAU_ENUM(LUA_COSUS);
	BIND_LUAU_ENUM(LUA_CONOR);
	BIND_LUAU_ENUM(LUA_COFIN);
	BIND_LUAU_ENUM(LUA_COERR);

	ClassDB::bind_integer_constant(get_class_static(), "", "LUA_TNONE", LUA_TNONE);

	BIND_LUAU_ENUM(LUA_TNIL);
	BIND_LUAU_ENUM(LUA_TBOOLEAN);
	BIND_LUAU_ENUM(LUA_TLIGHTUSERDATA);
	BIND_LUAU_ENUM(LUA_TNUMBER);
	BIND_LUAU_ENUM(LUA_TVECTOR);
	BIND_LUAU_ENUM(LUA_TSTRING);
	BIND_LUAU_ENUM(LUA_TTABLE);
	BIND_LUAU_ENUM(LUA_TFUNCTION);
	BIND_LUAU_ENUM(LUA_TUSERDATA);
	BIND_LUAU_ENUM(LUA_TTHREAD);
	BIND_LUAU_ENUM(LUA_TBUFFER);
	BIND_LUAU_ENUM(LUA_TPROTO);
	BIND_LUAU_ENUM(LUA_TUPVAL);
	BIND_LUAU_ENUM(LUA_TDEADKEY);

	ClassDB::bind_integer_constant(get_class_static(), "", "LUA_NOREF", LUA_NOREF);
	ClassDB::bind_integer_constant(get_class_static(), "", "LUA_REFNIL", LUA_REFNIL);
	ClassDB::bind_integer_constant(get_class_static(), "", "LUA_NOTAG", LUA_NOTAG);

	ClassDB::bind_static_method("Luau", D_METHOD("compile", "source_code", "options"), &Luau::compile);
	ClassDB::bind_static_method("Luau", D_METHOD("upvalue_index", "upvalue"), &Luau::upvalue_index);
	ClassDB::bind_static_method("Luau", D_METHOD("is_pseudo", "index"), &Luau::is_pseudo);
	ClassDB::bind_static_method("Luau", D_METHOD("clock"), &Luau::clock);
}

LuauCompileResult Luau::compile_with_diagnostics(const String &p_source_code, const LuaCompileOptions *p_options) {
	LuauCompileResult result;
	if (LuauCodegen::source_requests_native(p_source_code) && !LuauCodegen::is_enabled()) {
	}
	CharString utf8 = p_source_code.utf8();
	lua_CompileOptions options = p_options ? p_options->get_options() : LuaCompileOptions::default_options();

	size_t bytecode_size = 0;
	char *bytecode = luau_compile(utf8.get_data(), utf8.length(), &options, &bytecode_size);

	if (!bytecode || bytecode_size == 0) {
		result.error_message = "Luau compilation failed";
		result.error_line = 1;
		return result;
	}

	result.bytecode.resize(bytecode_size);
	memcpy(result.bytecode.ptrw(), bytecode, bytecode_size);
	free(bytecode);

	if (result.is_error() && result.bytecode.size() > 1) {
		String message = String::utf8(reinterpret_cast<const char *>(result.bytecode.ptr() + 1), result.bytecode.size() - 1);
		result.error_message = message;
		result.error_line = 1;

		const int colon = message.find(":");
		if (colon > 0) {
			const String line_str = message.substr(0, colon);
			if (line_str.is_valid_int()) {
				result.error_line = line_str.to_int();
				const int msg_start = message.find(":", colon + 1);
				if (msg_start >= 0) {
					result.error_message = message.substr(msg_start + 1).strip_edges();
				} else {
					result.error_message = message.substr(colon + 1).strip_edges();
				}
			}
		}
	}

	return result;
}

PackedByteArray Luau::compile(const String &p_source_code, const LuaCompileOptions *p_options) {
	const LuauCompileResult result = compile_with_diagnostics(p_source_code, p_options);
	if (result.succeeded()) {
		return result.bytecode;
	}
	return PackedByteArray();
}

int Luau::upvalue_index(int p_upvalue) {
	return lua_upvalueindex(p_upvalue);
}

bool Luau::is_pseudo(int p_index) {
	return lua_ispseudo(p_index) != 0;
}

double Luau::clock() {
	return lua_clock();
}
