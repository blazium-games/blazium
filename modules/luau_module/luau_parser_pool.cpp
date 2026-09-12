/**************************************************************************/
/*  luau_parser_pool.cpp                                                  */
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

#include "luau_parser_pool.h"

#include "bindings/luau_blazium_types.h"
#include "lua_blazium_classes.h"
#include "luau_class_info.h"
#include "luau_script_language.h"
#include "require/luau_package_path.h"
#include "scheduler/luau_task_scheduler.h"

void LuauParserPool::configure_parser_vm(const Ref<luau_module::LuaState> &p_state) {
	ERR_FAIL_COND(p_state.is_null() || !p_state->is_valid());

	p_state->open_libs(luau_module::LuaState::LIB_BASE | luau_module::LuaState::LIB_STRING | luau_module::LuaState::LIB_TABLE | luau_module::LuaState::LIB_MATH);
	LuauClassInfo::register_script_helpers(p_state);
	luau_module::LuauPackagePath::install_package_searchers(p_state);
	luau_module::install_blazium_class_globals(p_state->get_lua_state());
	if (LuauScriptLanguage *lang = LuauScriptLanguage::get_singleton()) {
		lang->push_named_globals_to_state(p_state->get_lua_state());
		luau_module::install_scheduler_libs(p_state->get_lua_state(), &lang->get_task_scheduler());
	}
	luau_module::install_script_math_globals(p_state->get_lua_state());
	p_state->sandbox();
}

Ref<luau_module::LuaState> LuauParserPool::acquire() {
	if (!idle.is_empty()) {
		Ref<luau_module::LuaState> state = idle[idle.size() - 1];
		idle.remove_at(idle.size() - 1);
		state->set_top(0);
		return state;
	}

	Ref<luau_module::LuaState> state;
	state.instantiate();
	configure_parser_vm(state);
	configured_count++;
	return state;
}

void LuauParserPool::release(const Ref<luau_module::LuaState> &p_state) {
	if (p_state.is_null() || !p_state->is_valid()) {
		return;
	}

	p_state->set_top(0);
	if (idle.size() < k_max_pool_size) {
		idle.push_back(p_state);
	}
}

void LuauParserPool::clear() {
	idle.clear();
	configured_count = 0;
}
