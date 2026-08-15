/**************************************************************************/
/*  luau_script_debugger.cpp                                              */
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

#include "debugger/luau_script_debugger.h"

#include "bindings/object.h"
#include "bindings/variant.h"
#include "luau_script.h"
#include "luau_script_instance.h"
#include "luau_script_language.h"

#include "core/debugger/engine_debugger.h"
#include "core/debugger/script_debugger.h"
#include "core/error/error_macros.h"
#include "core/variant/variant.h"
#include "lua_state.h"
#include <lualib.h>

using namespace luau_module;

namespace {

Variant lua_to_debug_variant(lua_State *L, int p_index, int p_depth, int p_max_depth, int p_max_subitems, int &r_subitems) {
	if (p_max_depth >= 0 && p_depth >= p_max_depth) {
		return "...";
	}
	if (p_max_subitems >= 0 && r_subitems >= p_max_subitems) {
		return "...";
	}
	r_subitems++;

	const int type = lua_type(L, p_index);
	switch (type) {
		case LUA_TNIL:
			return Variant();
		case LUA_TBOOLEAN:
			return Variant((bool)lua_toboolean(L, p_index));
		case LUA_TNUMBER:
			return Variant(lua_tonumber(L, p_index));
		case LUA_TSTRING: {
			size_t len = 0;
			const char *str = lua_tolstring(L, p_index, &len);
			return Variant(String::utf8(str, len));
		}
		case LUA_TTABLE: {
			Dictionary dict;
			Array array;
			bool is_array = true;
			int expected_index = 1;
			lua_pushnil(L);
			while (lua_next(L, p_index) != 0) {
				Variant key = to_variant(L, -2);
				Variant value = lua_to_debug_variant(L, -1, p_depth + 1, p_max_depth, p_max_subitems, r_subitems);
				if (key.get_type() == Variant::INT && int(key) == expected_index) {
					array.push_back(value);
					expected_index++;
				} else {
					is_array = false;
					dict[key] = value;
				}
				lua_pop(L, 1);
			}
			return is_array && dict.is_empty() ? Variant(array) : Variant(dict);
		}
		default:
			return to_variant(L, p_index);
	}
}

} //namespace

String LuauScriptDebugger::normalize_source_path(const String &p_source) {
	String path = p_source;
	if (path.begins_with("@")) {
		path = path.substr(1);
	}
	return path;
}

bool LuauScriptDebugger::should_break_at(const String &p_source, int p_line) const {
	if (p_line <= 0) {
		return false;
	}

	const String normalized = normalize_source_path(p_source);

	if (ScriptDebugger *script_debugger = EngineDebugger::get_script_debugger()) {
		const String resolved = script_debugger->breakpoint_find_source(normalized);
		const StringName source_name(resolved);
		if (script_debugger->is_breakpoint(p_line, source_name)) {
			return true;
		}
	}

	const uint32_t path_hash = normalized.hash();
	if (!breakpoints.has(path_hash)) {
		return false;
	}

	const HashMap<int, bool> &lines = breakpoints[path_hash];
	if (!lines.has(p_line)) {
		return false;
	}

	return lines[p_line];
}

bool LuauScriptDebugger::begin_traced_execution(const Ref<LuaState> &p_state) {
	if (p_state.is_null() || !p_state->is_valid() || !EngineDebugger::is_active()) {
		return false;
	}

	ScriptDebugger *script_debugger = EngineDebugger::get_script_debugger();
	if (!script_debugger) {
		return false;
	}

	if (script_debugger->get_lines_left() > 0) {
		p_state->set_single_step(true);
		return true;
	}

	if (!script_debugger->get_breakpoints().is_empty()) {
		p_state->set_single_step(true);
		return true;
	}

	return false;
}

void LuauScriptDebugger::end_traced_execution(const Ref<LuaState> &p_state) {
	if (p_state.is_valid() && p_state->is_valid()) {
		p_state->set_single_step(false);
	}
}

void LuauScriptDebugger::check_line_hook(lua_State *p_L, lua_Debug *p_ar) {
	if (!p_ar || !p_ar->source) {
		return;
	}

	bool do_break = false;

	if (EngineDebugger::is_active()) {
		ScriptDebugger *script_debugger = EngineDebugger::get_script_debugger();
		if (script_debugger) {
			if (script_debugger->is_skipping_breakpoints()) {
				return;
			}

			if (script_debugger->get_lines_left() > 0) {
				if (script_debugger->get_depth() <= 0) {
					script_debugger->set_lines_left(script_debugger->get_lines_left() - 1);
				}
				if (script_debugger->get_lines_left() <= 0) {
					do_break = true;
				}
			}
		}
	}

	if (!do_break && should_break_at(String(p_ar->source), p_ar->currentline)) {
		do_break = true;
	}

	if (do_break) {
		debug_break(p_L, p_ar);
	}

	if (EngineDebugger::is_active()) {
		EngineDebugger::get_singleton()->line_poll();
	}
}

LuauScriptInstance *LuauScriptDebugger::resolve_instance_at_level(lua_State *p_L, int p_level) const {
	for (int i = 1;; i++) {
		const char *name = lua_getlocal(p_L, p_level, i);
		if (!name) {
			break;
		}

		LuauScriptInstance *resolved = nullptr;
		if (strcmp(name, "self") == 0 && lua_istable(p_L, -1)) {
			lua_getfield(p_L, -1, "self");
			if (lua_isuserdata(p_L, -1) || lua_islightuserdata(p_L, -1)) {
				Object *owner = to_object(p_L, -1);
				if (owner) {
					ScriptInstance *script_instance = owner->get_script_instance();
					if (script_instance) {
						resolved = static_cast<LuauScriptInstance *>(script_instance);
					}
				}
			}
			lua_pop(p_L, 1);
		}

		lua_pop(p_L, 1);
		if (resolved) {
			return resolved;
		}
	}
	return nullptr;
}

void LuauScriptDebugger::debug_break(lua_State *p_L, lua_Debug *p_ar) {
	is_break_active = true;
	break_thread = p_L;
	break_stack.clear();

	lua_Debug ar;
	for (int level = 0; lua_getinfo(p_L, level, "sln", &ar); level++) {
		BreakFrame frame;
		frame.ar = ar;
		frame.instance = resolve_instance_at_level(p_L, level);
		break_stack.push_back(frame);
	}

	if (p_ar && p_ar->source) {
		last_error = String("Breakpoint at ") + String(p_ar->source) + ":" + itos(p_ar->currentline);
	} else {
		last_error = "Debugger break";
	}

	if (EngineDebugger::is_active()) {
		if (LuauScriptLanguage *lang = LuauScriptLanguage::get_singleton()) {
			EngineDebugger::get_script_debugger()->debug(lang, true, false);
		}
	}

	clear_break();
}

void LuauScriptDebugger::clear_break() {
	is_break_active = false;
	break_thread = nullptr;
	break_stack.clear();
}

void LuauScriptDebugger::set_breakpoint(const String &p_path, int p_line, bool p_enabled) {
	breakpoints[normalize_source_path(p_path).hash()][p_line] = p_enabled;
}

void LuauScriptDebugger::clear_breakpoints(const String &p_path) {
	breakpoints.erase(normalize_source_path(p_path).hash());
}

int LuauScriptDebugger::get_stack_level_count() const {
	return break_stack.size();
}

int LuauScriptDebugger::get_stack_level_line(int p_level) const {
	if (p_level < 0 || p_level >= break_stack.size()) {
		return 0;
	}
	int idx = 0;
	for (const BreakFrame &frame : break_stack) {
		if (idx == p_level) {
			return frame.ar.currentline;
		}
		idx++;
	}
	return 0;
}

String LuauScriptDebugger::get_stack_level_function(int p_level) const {
	if (p_level < 0 || p_level >= break_stack.size()) {
		return String();
	}
	int idx = 0;
	for (const BreakFrame &frame : break_stack) {
		if (idx == p_level) {
			return frame.ar.name ? String(frame.ar.name) : String("<anonymous>");
		}
		idx++;
	}
	return String();
}

String LuauScriptDebugger::get_stack_level_source(int p_level) const {
	if (p_level < 0 || p_level >= break_stack.size()) {
		return String();
	}
	int idx = 0;
	for (const BreakFrame &frame : break_stack) {
		if (idx == p_level) {
			return frame.ar.source ? String(frame.ar.source) : String();
		}
		idx++;
	}
	return String();
}

LuauScriptInstance *LuauScriptDebugger::get_stack_level_instance(int p_level) const {
	if (p_level < 0 || p_level >= break_stack.size()) {
		return nullptr;
	}
	int idx = 0;
	for (const BreakFrame &frame : break_stack) {
		if (idx == p_level) {
			return frame.instance;
		}
		idx++;
	}
	return nullptr;
}

void LuauScriptDebugger::get_stack_level_locals(int p_level, List<String> *p_locals, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
	if (!break_thread || p_level < 0 || p_level >= break_stack.size()) {
		return;
	}

	for (int i = 1;; i++) {
		const char *name = lua_getlocal(break_thread, p_level, i);
		if (!name) {
			break;
		}
		if (p_locals) {
			p_locals->push_back(String(name));
		}
		if (p_values) {
			int subitems = 0;
			p_values->push_back(lua_to_debug_variant(break_thread, -1, 0, p_max_depth, p_max_subitems, subitems));
		}
		lua_pop(break_thread, 1);
	}
}

void LuauScriptDebugger::get_stack_level_members(int p_level, List<String> *p_members, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
	(void)p_max_subitems;
	(void)p_max_depth;

	LuauScriptInstance *instance = get_stack_level_instance(p_level);
	if (!instance) {
		return;
	}

	List<PropertyInfo> properties;
	instance->get_property_list(&properties);
	for (const PropertyInfo &info : properties) {
		Variant value;
		if (!instance->get(info.name, value)) {
			continue;
		}
		if (p_members) {
			p_members->push_back(info.name);
		}
		if (p_values) {
			p_values->push_back(value);
		}
	}

	List<MethodInfo> methods;
	instance->get_method_list(&methods);
	for (const MethodInfo &method : methods) {
		if (p_members) {
			p_members->push_back(method.name);
		}
		if (p_values) {
			p_values->push_back(Callable(instance->get_owner(), method.name));
		}
	}
}

void LuauScriptDebugger::get_globals(List<String> *p_globals, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
	if (!break_thread) {
		return;
	}

	lua_getglobal(break_thread, "_G");
	if (!lua_istable(break_thread, -1)) {
		lua_pop(break_thread, 1);
		return;
	}

	lua_pushnil(break_thread);
	while (lua_next(break_thread, -2) != 0) {
		if (lua_isstring(break_thread, -2)) {
			size_t len = 0;
			const char *key = lua_tolstring(break_thread, -2, &len);
			if (p_globals) {
				p_globals->push_back(String::utf8(key, len));
			}
			if (p_values) {
				int subitems = 0;
				p_values->push_back(lua_to_debug_variant(break_thread, -1, 0, p_max_depth, p_max_subitems, subitems));
			}
		}
		lua_pop(break_thread, 1);
	}
	lua_pop(break_thread, 1);
}
