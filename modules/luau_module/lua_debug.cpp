/**************************************************************************/
/*  lua_debug.cpp                                                         */
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

#include "lua_debug.h"

using namespace luau_module;

void LuaDebug::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_name"), &LuaDebug::get_name);
	ClassDB::bind_method(D_METHOD("get_what"), &LuaDebug::get_what);
	ClassDB::bind_method(D_METHOD("get_source"), &LuaDebug::get_source);
	ClassDB::bind_method(D_METHOD("get_short_src"), &LuaDebug::get_short_src);
	ClassDB::bind_method(D_METHOD("get_line_defined"), &LuaDebug::get_line_defined);
	ClassDB::bind_method(D_METHOD("get_current_line"), &LuaDebug::get_current_line);
	ClassDB::bind_method(D_METHOD("get_nupvals"), &LuaDebug::get_nupvals);
	ClassDB::bind_method(D_METHOD("get_nparams"), &LuaDebug::get_nparams);
	ClassDB::bind_method(D_METHOD("is_vararg"), &LuaDebug::is_vararg);
}

String LuaDebug::get_name() const {
	return debug_info.name ? String(debug_info.name) : String();
}

String LuaDebug::get_what() const {
	return debug_info.what ? String(debug_info.what) : String();
}

String LuaDebug::get_source() const {
	return debug_info.source ? String(debug_info.source) : String();
}

String LuaDebug::get_short_src() const {
	return debug_info.short_src ? String(debug_info.short_src) : String();
}

int LuaDebug::get_line_defined() const {
	return debug_info.linedefined;
}

int LuaDebug::get_current_line() const {
	return debug_info.currentline;
}

uint8_t LuaDebug::get_nupvals() const {
	return static_cast<uint8_t>(debug_info.nupvals);
}

uint8_t LuaDebug::get_nparams() const {
	return static_cast<uint8_t>(debug_info.nparams);
}

bool LuaDebug::is_vararg() const {
	return debug_info.isvararg != 0;
}
