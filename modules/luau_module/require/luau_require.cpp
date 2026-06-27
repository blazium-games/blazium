/**************************************************************************/
/*  luau_require.cpp                                                      */
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

#include "luau_require.h"

#include "luau.h"

#include "luau_codegen.h"

#include "core/config/project_settings.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include <lualib.h>

using namespace luau_module;

namespace {

static constexpr char PACKAGE_REGISTRY_KEY[] = "_LUAU_package";

static int file_loader(lua_State *p_L) {
	const char *filename = lua_tostring(p_L, lua_upvalueindex(1));
	if (!filename) {
		luaL_error(p_L, "error loading module: invalid filename");
		return 0;
	}

	Ref<FileAccess> file = FileAccess::open(String::utf8(filename), FileAccess::READ);
	if (file.is_null()) {
		luaL_error(p_L, "error loading module: unable to open file '%s'", filename);
		return 0;
	}

	const String source = file->get_as_text();
	if (source.is_empty() && file->get_error() != OK) {
		luaL_error(p_L, "error loading module: unable to read file '%s'", filename);
		return 0;
	}

	const PackedByteArray bytecode = Luau::compile(source);
	if (bytecode.is_empty()) {
		luaL_error(p_L, "error loading module: failed to compile '%s'", filename);
		return 0;
	}

	if (luau_load(p_L, filename, reinterpret_cast<const char *>(bytecode.ptr()), bytecode.size(), 0) != 0) {
		luaL_error(p_L, "error loading module '%s': %s", filename, lua_tostring(p_L, -1));
		return 0;
	}

	LuauCodegen::compile_loaded(p_L, -1, source);

	return 1;
}

static void push_searcher_error(lua_State *p_L, const String &p_message) {
	lua_pushstring(p_L, p_message.utf8().get_data());
}

} //namespace

String LuauRequire::get_exec_dir_replacement() {
	String execdir;
	if (OS::get_singleton()->has_feature("standalone")) {
		execdir = OS::get_singleton()->get_executable_path().get_base_dir();
	} else if (ProjectSettings::get_singleton()) {
		execdir = ProjectSettings::get_singleton()->globalize_path("res://");
	}
	return execdir.trim_suffix("/");
}

bool LuauRequire::searchpath(const String &p_name, const String &p_path, String &r_filename, String *r_err_msg, const String &p_sep, const String &p_rep) {
	r_filename = String();
	String module_name = p_name;

	if (!p_sep.is_empty() && !module_name.contains("/") && !module_name.contains("\\")) {
		module_name = module_name.replace(p_sep, p_rep);
	}

	const String exec_repl = get_exec_dir_replacement();
	PackedStringArray not_found;

	const PackedStringArray templates = p_path.split(";", false);
	for (int i = 0; i < templates.size(); ++i) {
		String candidate = templates[i];
		candidate = candidate.replace("?", module_name);
		candidate = candidate.replace("!", exec_repl);

		if (FileAccess::exists(candidate)) {
			r_filename = candidate;
			return true;
		}

		not_found.push_back(vformat("\n\tno file '%s'", candidate));
	}

	if (r_err_msg) {
		*r_err_msg = String().join(not_found);
	}
	return false;
}

int LuauRequire::lua_searchpath(lua_State *p_L) {
	const char *name = luaL_checkstring(p_L, 1);
	const char *path = luaL_checkstring(p_L, 2);
	const char *sep = luaL_optstring(p_L, 3, ".");
	const char *rep = luaL_optstring(p_L, 4, "/");

	String filename;
	String err_msg;
	if (searchpath(String::utf8(name), String::utf8(path), filename, &err_msg, String::utf8(sep), String::utf8(rep))) {
		lua_pushstring(p_L, filename.utf8().get_data());
		return 1;
	}

	lua_pushnil(p_L);
	lua_pushstring(p_L, err_msg.utf8().get_data());
	return 2;
}

int LuauRequire::lua_preload_searcher(lua_State *p_L) {
	const char *name = luaL_checkstring(p_L, 1);

	lua_getfield(p_L, LUA_REGISTRYINDEX, PACKAGE_REGISTRY_KEY);
	lua_getfield(p_L, -1, "preload");
	lua_getfield(p_L, -1, name);

	if (lua_isfunction(p_L, -1)) {
		return 1;
	}

	lua_pop(p_L, 3);
	push_searcher_error(p_L, vformat("no field package.preload['%s']", name));
	return 1;
}

int LuauRequire::lua_file_searcher(lua_State *p_L) {
	const char *name = luaL_checkstring(p_L, 1);

	lua_getfield(p_L, LUA_REGISTRYINDEX, PACKAGE_REGISTRY_KEY);
	lua_getfield(p_L, -1, "path");
	const String package_path = String::utf8(luaL_checkstring(p_L, -1));
	lua_pop(p_L, 2);

	String filename;
	String err_msg;
	if (!searchpath(String::utf8(name), package_path, filename, &err_msg)) {
		push_searcher_error(p_L, err_msg);
		return 1;
	}

	lua_pushstring(p_L, filename.utf8().get_data());
	lua_pushcclosure(p_L, file_loader, "luau_module.file_loader", 1);
	lua_pushvalue(p_L, -2);
	return 2;
}

int LuauRequire::lua_require(lua_State *p_L) {
	const char *modname = luaL_checkstring(p_L, 1);

	lua_getfield(p_L, LUA_REGISTRYINDEX, PACKAGE_REGISTRY_KEY);
	lua_getfield(p_L, -1, "loaded");
	lua_getfield(p_L, -1, modname);
	if (!lua_isnil(p_L, -1)) {
		lua_replace(p_L, 1);
		lua_settop(p_L, 1);
		return 1;
	}
	lua_pop(p_L, 1);
	lua_pop(p_L, 1);

	lua_getfield(p_L, -1, "searchers");
	if (!lua_istable(p_L, -1)) {
		luaL_error(p_L, "package.searchers must be a table");
		return 0;
	}

	const int nsearchers = lua_objlen(p_L, -1);
	String last_error = "module not found";

	for (int i = 1; i <= nsearchers; ++i) {
		lua_rawgeti(p_L, -1, i);
		lua_pushstring(p_L, modname);
		lua_call(p_L, 1, 2);

		if (lua_isfunction(p_L, -2)) {
			lua_pushvalue(p_L, -2);
			lua_pushvalue(p_L, -2);
			lua_pushstring(p_L, modname);
			lua_call(p_L, 2, 1);

			const int result_index = lua_gettop(p_L);
			lua_getfield(p_L, LUA_REGISTRYINDEX, PACKAGE_REGISTRY_KEY);
			lua_getfield(p_L, -1, "loaded");
			if (!lua_isnil(p_L, result_index)) {
				lua_pushvalue(p_L, result_index);
				lua_setfield(p_L, -2, modname);
			} else {
				lua_pushboolean(p_L, 1);
				lua_setfield(p_L, -2, modname);
			}
			lua_settop(p_L, result_index);
			lua_replace(p_L, 1);
			lua_settop(p_L, 1);
			return 1;
		}

		if (lua_isstring(p_L, -1)) {
			last_error = String::utf8(lua_tostring(p_L, -1));
		}
		lua_pop(p_L, 2);
	}

	luaL_error(p_L, "module '%s' not found:%s", modname, last_error.utf8().get_data());
	return 0;
}

void LuauRequire::install_package(lua_State *p_L, const String &p_package_path) {
	ERR_FAIL_NULL(p_L);

	lua_newtable(p_L);
	lua_pushvalue(p_L, -1);
	lua_setfield(p_L, LUA_REGISTRYINDEX, PACKAGE_REGISTRY_KEY);

	lua_pushstring(p_L, p_package_path.utf8().get_data());
	lua_setfield(p_L, -2, "path");

	lua_newtable(p_L);
	lua_setfield(p_L, -2, "loaded");

	lua_newtable(p_L);
	lua_setfield(p_L, -2, "preload");

	lua_newtable(p_L);
	int searcher_index = 1;

	lua_pushcfunction(p_L, lua_preload_searcher, "luau_module.preload_searcher");
	lua_rawseti(p_L, -2, searcher_index++);

	lua_pushcfunction(p_L, lua_file_searcher, "luau_module.file_searcher");
	lua_rawseti(p_L, -2, searcher_index++);

	lua_setfield(p_L, -2, "searchers");

	lua_pushcfunction(p_L, lua_searchpath, "luau_module.searchpath");
	lua_setfield(p_L, -2, "searchpath");

	lua_setglobal(p_L, "package");

	lua_pushcfunction(p_L, lua_require, "require");
	lua_setglobal(p_L, "require");
}

void LuauRequire::invalidate_module(lua_State *p_L, const String &p_module_name) {
	ERR_FAIL_NULL(p_L);
	if (p_module_name.is_empty()) {
		return;
	}

	lua_getfield(p_L, LUA_REGISTRYINDEX, PACKAGE_REGISTRY_KEY);
	if (!lua_istable(p_L, -1)) {
		lua_pop(p_L, 1);
		return;
	}

	lua_getfield(p_L, -1, "loaded");
	if (lua_istable(p_L, -1)) {
		lua_pushnil(p_L);
		lua_setfield(p_L, -2, p_module_name.utf8().get_data());
	}
	lua_pop(p_L, 2);
}

void LuauRequire::invalidate_all(lua_State *p_L) {
	ERR_FAIL_NULL(p_L);

	lua_getfield(p_L, LUA_REGISTRYINDEX, PACKAGE_REGISTRY_KEY);
	if (!lua_istable(p_L, -1)) {
		lua_pop(p_L, 1);
		return;
	}

	lua_newtable(p_L);
	lua_setfield(p_L, -2, "loaded");
	lua_pop(p_L, 1);
}
