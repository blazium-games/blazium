/**************************************************************************/
/*  luau_script_debugger.h                                                */
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

#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/list.h"
#include <lua.h>

class LuauScriptInstance;

namespace luau_module {
class LuaState;
}

namespace luau_module {

class LuauScriptDebugger {
	struct BreakFrame {
		lua_Debug ar;
		LuauScriptInstance *instance = nullptr;
	};

	String last_error;
	HashMap<int, HashMap<int, bool>> breakpoints;
	bool is_break_active = false;
	lua_State *break_thread = nullptr;
	List<BreakFrame> break_stack;

	LuauScriptInstance *resolve_instance_at_level(lua_State *p_L, int p_level) const;
	static String normalize_source_path(const String &p_source);

public:
	void debug_break(lua_State *p_L, lua_Debug *p_ar);
	void check_line_hook(lua_State *p_L, lua_Debug *p_ar);
	void clear_break();
	bool is_active() const { return is_break_active; }

	void set_breakpoint(const String &p_path, int p_line, bool p_enabled);
	void clear_breakpoints(const String &p_path);
	bool should_break_at(const String &p_source, int p_line) const;
	bool begin_traced_execution(const Ref<luau_module::LuaState> &p_state);
	void end_traced_execution(const Ref<luau_module::LuaState> &p_state);

	String get_error() const { return last_error; }
	int get_stack_level_count() const;
	int get_stack_level_line(int p_level) const;
	String get_stack_level_function(int p_level) const;
	String get_stack_level_source(int p_level) const;
	LuauScriptInstance *get_stack_level_instance(int p_level) const;
	void get_stack_level_locals(int p_level, List<String> *p_locals, List<Variant> *p_values, int p_max_subitems = -1, int p_max_depth = -1);
	void get_stack_level_members(int p_level, List<String> *p_members, List<Variant> *p_values, int p_max_subitems = -1, int p_max_depth = -1);
	void get_globals(List<String> *p_globals, List<Variant> *p_values, int p_max_subitems = -1, int p_max_depth = -1);
};

} //namespace luau_module
