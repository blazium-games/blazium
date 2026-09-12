/**************************************************************************/
/*  luau_package_path.cpp                                                 */
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

#include "luau_package_path.h"

#include "lua_state.h"
#include "require/luau_require.h"

#include "core/config/project_settings.h"

using namespace luau_module;

void LuauPackagePath::register_project_settings() {
	ProjectSettings *settings = ProjectSettings::get_singleton();
	ERR_FAIL_NULL(settings);

	if (!settings->has_setting(SETTING_PACKAGE_PATH)) {
		settings->set_setting(SETTING_PACKAGE_PATH, DEFAULT_PACKAGE_PATH);
	}
	settings->set_initial_value(SETTING_PACKAGE_PATH, DEFAULT_PACKAGE_PATH);
	settings->set_builtin_order(SETTING_PACKAGE_PATH);
	settings->set_as_basic(SETTING_PACKAGE_PATH, true);
}

String LuauPackagePath::get_package_path() {
	ProjectSettings *settings = ProjectSettings::get_singleton();
	if (!settings) {
		return DEFAULT_PACKAGE_PATH;
	}
	return settings->get_setting(SETTING_PACKAGE_PATH);
}

void LuauPackagePath::install_package_searchers(const Ref<LuaState> &p_state) {
	ERR_FAIL_COND(p_state.is_null());
	ERR_FAIL_COND(!p_state->is_valid());

	LuauRequire::install_package(p_state->get_lua_state(), get_package_path());
}
