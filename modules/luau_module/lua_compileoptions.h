/**************************************************************************/
/*  lua_compileoptions.h                                                  */
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

#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include <luacode.h>

namespace luau_module {

class LuaCompileOptions : public RefCounted {
	GDCLASS(LuaCompileOptions, RefCounted)

private:
	lua_CompileOptions options;

protected:
	static void _bind_methods();

public:
	LuaCompileOptions();

	void set_optimization_level(int p_level);
	int get_optimization_level() const;

	void set_debug_level(int p_level);
	int get_debug_level() const;

	void set_type_info_level(int p_level);
	int get_type_info_level() const;

	void set_coverage_level(int p_level);
	int get_coverage_level() const;

	static lua_CompileOptions default_options() {
		lua_CompileOptions options{};
		options.optimizationLevel = 1;
		options.debugLevel = 1;
		options.typeInfoLevel = 0;
		options.coverageLevel = 0;
		options.vectorLib = nullptr;
		options.vectorCtor = "Vector3";
		options.vectorType = "Vector3";
		options.mutableGlobals = nullptr;
		options.userdataTypes = nullptr;
		options.librariesWithKnownMembers = nullptr;
		options.libraryMemberTypeCb = nullptr;
		options.libraryMemberConstantCb = nullptr;
		options.disabledBuiltins = nullptr;
		return options;
	}

	const lua_CompileOptions &get_options() const { return options; }
};
} //namespace luau_module
