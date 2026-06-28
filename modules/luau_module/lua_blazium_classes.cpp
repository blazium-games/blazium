/**************************************************************************/
/*  lua_blazium_classes.cpp                                               */
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

#include "lua_blazium_classes.h"
#include "luau_class_info.h"

#include "string_cache.h"

#include "core/error/error_macros.h"
#include "core/object/class_db.h"
#include <lua.h>

using namespace luau_module;

namespace {

static const char *const k_essential_classes[] = {
	"Object",
	"RefCounted",
	"Node",
	"Node2D",
	"Node3D",
	"CanvasItem",
	"Control",
	"Sprite2D",
	"Resource",
	nullptr,
};

static void register_class_global(lua_State *p_L, const StringName &p_class_name) {
	if (!ClassDB::class_exists(p_class_name)) {
		return;
	}

	CharString utf8 = char_string(p_class_name);
	lua_createtable(p_L, 0, 1);
	lua_pushlstring(p_L, utf8.get_data(), utf8.length());
	lua_setfield(p_L, -2, LUAU_CLASS_KEY_BLAZIUM_CLASS);
	lua_setglobal(p_L, utf8.get_data());
}

} //namespace

void luau_module::install_blazium_class_globals(lua_State *p_L) {
	ERR_FAIL_NULL(p_L);
	ERR_FAIL_COND_MSG(!lua_checkstack(p_L, 3), "install_blazium_class_globals(): Stack overflow.");

	for (const char *const *name = k_essential_classes; *name != nullptr; name++) {
		register_class_global(p_L, StringName(*name));
	}
}
