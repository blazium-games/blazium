/**************************************************************************/
/*  lua_state.cpp                                                         */
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

#include "lua_state.h"

#include "bindings/array.h"
#include "bindings/callable.h"
#include "bindings/dictionary.h"
#include "bindings/object.h"
#include "bindings/variant.h"
#include "lua_blaziumlib.h"
#include "lua_debug.h"
#include "luau.h"
#include "luau_codegen.h"
#include "luau_script_language.h"
#include "require/luau_package_path.h"
#include "static_strings.h"
#include "string_cache.h"

#include "core/object/class_db.h"
#include "core/variant/variant.h"

VARIANT_ENUM_CAST(luau_module::LuaState::ValueType);
VARIANT_ENUM_CAST(luau_module::LuaState::Status);
VARIANT_ENUM_CAST(luau_module::LuaState::CoStatus);
VARIANT_ENUM_CAST(luau_module::LuaState::GCOp);
#include "core/error/error_macros.h"
#include "core/object/object.h"
#include "core/templates/local_vector.h"
#include <lualib.h>

using namespace luau_module;

static void callback_interrupt(lua_State *L, int gc) {
	LuaState *state = LuaState::find_lua_state(L);
	if (!state) {
		return;
	}

	state->emit_signal(static_strings->interrupt, state, gc);
}

static void callback_panic(lua_State *L, int errcode) {
	const char *error_msg = lua_gettop(L) > 0 ? lua_tostring(L, -1) : "Unknown error";
	const char *traceback = lua_debugtrace(L);
	ERR_PRINT(vformat("Luau panic! Error %d: %s. LuaState is now invalid and cannot be used further. Traceback:\n%s", errcode, error_msg, traceback));

	LuaState *state = LuaState::find_lua_state(L);
	if (!state) {
		return;
	}

	state->close();
}

static void callback_userthread(lua_State *parent, lua_State *L) {
	if (parent) {
		return;
	}

	LuaState *state = LuaState::find_lua_state(L);
	if (state) {
		state->close();
	}
}

static void callback_debugbreak(lua_State *L, lua_Debug *ar) {
	LuaState *state = LuaState::find_lua_state(L);
	if (!state) {
		return;
	}

	Ref<LuaDebug> debug_info(memnew(LuaDebug(*ar)));
	state->emit_signal(static_strings->debugbreak, state, debug_info);

	if (LuauScriptLanguage *lang = LuauScriptLanguage::get_singleton()) {
		lang->get_debugger().debug_break(L, ar);
	}
}

static void callback_debugstep(lua_State *L, lua_Debug *ar) {
	LuaState *state = LuaState::find_lua_state(L);
	if (!state) {
		return;
	}

	state->emit_signal(static_strings->debugstep, state);

	if (LuauScriptLanguage *lang = LuauScriptLanguage::get_singleton()) {
		lang->get_debugger().check_line_hook(L, ar);
	}
}

static int cpcall_wrapper(lua_State *L) {
	ERR_FAIL_COND_V_MSG(lua_gettop(L) != 1, LUA_ERRMEM, "LuaState.cpcall: called function does not have exactly one stack value.");

	void *ud = lua_tolightuserdata(L, 1);
	Callable *callable = static_cast<Callable *>(ud);
	Variant result = callable->call();
	if (result.get_type() != Variant::NIL) {
		WARN_PRINT("LuaState.cpcall(): Callable returned a non-nil value. This value will be ignored.");
	}

	return 0;
}

static void coverage_wrapper(void *context, const char *function, int linedefined, int depth, const int *hits, size_t size) {
	Callable *callable = static_cast<Callable *>(context);
	String function_str(function);

	PackedInt32Array hits_array;
	hits_array.resize(size);

	for (size_t i = 0; i < size; i++) {
		hits_array.set(i, hits[i]);
	}

	callable->call(function_str, linedefined, depth, hits_array);
}

static int callable_metamethod_wrapper(lua_State *L) {
	luaL_checkstack(L, 1, "LuaState.callable_metamethod_wrapper: Stack overflow. Cannot grow stack.");

	int nargs = lua_gettop(L);

	lua_pushvalue(L, lua_upvalueindex(1));
	lua_insert(L, 1);

	lua_call(L, nargs, LUA_MULTRET);

	return lua_gettop(L);
}

void LuaState::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_valid"), &LuaState::is_valid);
	ClassDB::bind_method(D_METHOD("is_main_thread"), &LuaState::is_main_thread);

	ClassDB::bind_method(D_METHOD("close"), &LuaState::close);
	ClassDB::bind_method(D_METHOD("new_thread"), &LuaState::new_thread);
	ClassDB::bind_method(D_METHOD("get_main_thread"), &LuaState::get_main_thread);
	ClassDB::bind_method(D_METHOD("reset_thread"), &LuaState::reset_thread);
	ClassDB::bind_method(D_METHOD("is_thread_reset"), &LuaState::is_thread_reset);

	ClassDB::bind_method(D_METHOD("abs_index", "index"), &LuaState::abs_index);
	ClassDB::bind_method(D_METHOD("get_top"), &LuaState::get_top);
	ClassDB::bind_method(D_METHOD("set_top", "index"), &LuaState::set_top);
	ClassDB::bind_method(D_METHOD("pop", "count"), &LuaState::pop);
	ClassDB::bind_method(D_METHOD("push_value", "index"), &LuaState::push_value);
	ClassDB::bind_method(D_METHOD("remove", "index"), &LuaState::remove);
	ClassDB::bind_method(D_METHOD("insert", "index"), &LuaState::insert);
	ClassDB::bind_method(D_METHOD("replace", "index"), &LuaState::replace);
	ClassDB::bind_method(D_METHOD("check_stack", "size"), &LuaState::check_stack);
	ClassDB::bind_method(D_METHOD("raw_check_stack", "size"), &LuaState::raw_check_stack);
	ClassDB::bind_method(D_METHOD("xmove", "to_state", "count"), &LuaState::xmove);
	ClassDB::bind_method(D_METHOD("xpush", "to_state", "index"), &LuaState::xpush);

	ClassDB::bind_method(D_METHOD("is_number", "index"), &LuaState::is_number);
	ClassDB::bind_method(D_METHOD("is_string", "index"), &LuaState::is_string);
	ClassDB::bind_method(D_METHOD("is_c_function", "index"), &LuaState::is_c_function);
	ClassDB::bind_method(D_METHOD("is_lua_function", "index"), &LuaState::is_lua_function);
	ClassDB::bind_method(D_METHOD("is_userdata", "index"), &LuaState::is_userdata);
	ClassDB::bind_method(D_METHOD("type", "index"), &LuaState::type);
	ClassDB::bind_method(D_METHOD("is_function", "index"), &LuaState::is_function);
	ClassDB::bind_method(D_METHOD("is_table", "index"), &LuaState::is_table);
	ClassDB::bind_method(D_METHOD("is_full_userdata", "index"), &LuaState::is_full_userdata);
	ClassDB::bind_method(D_METHOD("is_light_userdata", "index"), &LuaState::is_light_userdata);
	ClassDB::bind_method(D_METHOD("is_nil", "index"), &LuaState::is_nil);
	ClassDB::bind_method(D_METHOD("is_boolean", "index"), &LuaState::is_boolean);
	ClassDB::bind_method(D_METHOD("is_vector", "index"), &LuaState::is_vector);
	ClassDB::bind_method(D_METHOD("is_thread", "index"), &LuaState::is_thread);
	ClassDB::bind_method(D_METHOD("is_buffer", "index"), &LuaState::is_buffer);
	ClassDB::bind_method(D_METHOD("is_none", "index"), &LuaState::is_none);
	ClassDB::bind_method(D_METHOD("is_none_or_nil", "index"), &LuaState::is_none_or_nil);
	ClassDB::bind_method(D_METHOD("type_name", "type_id"), &LuaState::type_name);

	ClassDB::bind_method(D_METHOD("equal", "index1", "index2"), &LuaState::equal);
	ClassDB::bind_method(D_METHOD("raw_equal", "index1", "index2"), &LuaState::raw_equal);
	ClassDB::bind_method(D_METHOD("less_than", "index1", "index2"), &LuaState::less_than);

	ClassDB::bind_method(D_METHOD("to_number", "index"), &LuaState::to_number);
	ClassDB::bind_method(D_METHOD("to_integer", "index"), &LuaState::to_integer);
	ClassDB::bind_method(D_METHOD("to_vector3", "index"), &LuaState::to_vector3);
	ClassDB::bind_method(D_METHOD("to_boolean", "index"), &LuaState::to_boolean);
	ClassDB::bind_method(D_METHOD("stack_to_string", "index"), &LuaState::to_string_inplace);
	ClassDB::bind_method(D_METHOD("to_string_inplace", "index"), &LuaState::to_string_inplace);
	ClassDB::bind_method(D_METHOD("to_string_name", "index"), &LuaState::to_string_name);
	ClassDB::bind_method(D_METHOD("get_namecall"), &LuaState::get_namecall);
	ClassDB::bind_method(D_METHOD("obj_len", "index"), &LuaState::obj_len);
	ClassDB::bind_method(D_METHOD("to_light_userdata", "index", "tag"), &LuaState::to_light_userdata, DEFVAL(LUA_NOTAG));
	ClassDB::bind_method(D_METHOD("to_full_userdata", "index", "tag"), &LuaState::to_full_userdata, DEFVAL(LUA_NOTAG));
	ClassDB::bind_method(D_METHOD("to_object", "index", "tag"), &LuaState::to_object, DEFVAL(LUA_NOTAG));
	ClassDB::bind_method(D_METHOD("light_userdata_tag", "index"), &LuaState::light_userdata_tag);
	ClassDB::bind_method(D_METHOD("full_userdata_tag", "index"), &LuaState::full_userdata_tag);
	ClassDB::bind_method(D_METHOD("to_thread", "index"), &LuaState::to_thread);
	ClassDB::bind_method(D_METHOD("to_buffer", "index"), &LuaState::to_buffer);
	ClassDB::bind_method(D_METHOD("to_pointer", "index"), &LuaState::to_pointer);

	ClassDB::bind_method(D_METHOD("push_nil"), &LuaState::push_nil);
	ClassDB::bind_method(D_METHOD("push_number", "num"), &LuaState::push_number);
	ClassDB::bind_method(D_METHOD("push_integer", "num"), &LuaState::push_integer);
	ClassDB::bind_method(D_METHOD("push_vector3", "vec"), &LuaState::push_vector3);
	ClassDB::bind_method(D_METHOD("push_string", "str"), &LuaState::push_string);
	ClassDB::bind_method(D_METHOD("push_string_name", "string_name"), &LuaState::push_string_name);
	ClassDB::bind_method(D_METHOD("push_boolean", "b"), &LuaState::push_boolean);
	ClassDB::bind_method(D_METHOD("push_thread"), &LuaState::push_thread);
	ClassDB::bind_method(D_METHOD("push_light_userdata", "obj", "tag"), &LuaState::push_light_userdata, DEFVAL(LUA_NOTAG));
	ClassDB::bind_method(D_METHOD("push_full_userdata", "obj", "tag"), &LuaState::push_full_userdata, DEFVAL(LUA_NOTAG));
	ClassDB::bind_method(D_METHOD("push_object", "obj", "tag"), &LuaState::push_object, DEFVAL(LUA_NOTAG));

	ClassDB::bind_method(D_METHOD("get_table", "index"), &LuaState::get_table);
	ClassDB::bind_method(D_METHOD("get_field", "index", "key"), &LuaState::get_field);
	ClassDB::bind_method(D_METHOD("get_global", "key"), &LuaState::get_global);
	ClassDB::bind_method(D_METHOD("raw_get_field", "index", "key"), &LuaState::raw_get_field);
	ClassDB::bind_method(D_METHOD("raw_get", "index"), &LuaState::raw_get);
	ClassDB::bind_method(D_METHOD("raw_geti", "index", "n"), &LuaState::raw_geti);
	ClassDB::bind_method(D_METHOD("create_table", "narr", "nrec"), &LuaState::create_table, DEFVAL(0), DEFVAL(0));

	ClassDB::bind_method(D_METHOD("set_read_only", "index", "enabled"), &LuaState::set_read_only);
	ClassDB::bind_method(D_METHOD("get_read_only", "index"), &LuaState::get_read_only);
	ClassDB::bind_method(D_METHOD("set_safe_env", "index", "enabled"), &LuaState::set_safe_env);

	ClassDB::bind_method(D_METHOD("get_metatable", "index"), &LuaState::get_metatable);
	ClassDB::bind_method(D_METHOD("get_fenv", "index"), &LuaState::get_fenv);

	ClassDB::bind_method(D_METHOD("set_table", "index"), &LuaState::set_table);
	ClassDB::bind_method(D_METHOD("set_field", "index", "key"), &LuaState::set_field);
	ClassDB::bind_method(D_METHOD("set_global", "key"), &LuaState::set_global);
	ClassDB::bind_method(D_METHOD("raw_set_field", "index", "key"), &LuaState::raw_set_field);
	ClassDB::bind_method(D_METHOD("raw_set", "index"), &LuaState::raw_set);
	ClassDB::bind_method(D_METHOD("raw_seti", "index", "n"), &LuaState::raw_seti);
	ClassDB::bind_method(D_METHOD("set_metatable", "index"), &LuaState::set_metatable);
	ClassDB::bind_method(D_METHOD("set_fenv", "index"), &LuaState::set_fenv);

	ClassDB::bind_method(D_METHOD("load_bytecode", "bytecode", "chunk_name", "env"), &LuaState::load_bytecode, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("pcall", "nargs", "nresults", "errfunc"), &LuaState::pcall, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("cpcall", "callable"), &LuaState::cpcall);

	ClassDB::bind_method(D_METHOD("yield", "nresults"), &LuaState::yield);
	ClassDB::bind_method(D_METHOD("break_execution"), &LuaState::break_execution);
	ClassDB::bind_method(D_METHOD("resume", "narg", "from"), &LuaState::resume, DEFVAL(0), DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("resume_error", "from"), &LuaState::resume_error, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("status"), &LuaState::status);
	ClassDB::bind_method(D_METHOD("is_yieldable"), &LuaState::is_yieldable);
	ClassDB::bind_method(D_METHOD("co_status", "co"), &LuaState::co_status);

	ClassDB::bind_method(D_METHOD("gc", "what", "data"), &LuaState::gc);

	ClassDB::bind_method(D_METHOD("set_memory_category", "category"), &LuaState::set_memory_category);
	ClassDB::bind_method(D_METHOD("get_total_bytes", "category"), &LuaState::get_total_bytes);

	ClassDB::bind_method(D_METHOD("error"), &LuaState::error);

	ClassDB::bind_method(D_METHOD("next", "index"), &LuaState::next);
	ClassDB::bind_method(D_METHOD("raw_iter", "index", "iter"), &LuaState::raw_iter);

	ClassDB::bind_method(D_METHOD("concat", "count"), &LuaState::concat);

	ClassDB::bind_method(D_METHOD("set_full_userdata_tag", "index", "tag"), &LuaState::set_full_userdata_tag);
	ClassDB::bind_method(D_METHOD("set_full_userdata_metatable", "tag"), &LuaState::set_full_userdata_metatable);
	ClassDB::bind_method(D_METHOD("get_full_userdata_metatable", "tag"), &LuaState::get_full_userdata_metatable);

	ClassDB::bind_method(D_METHOD("set_light_userdata_name", "tag", "name"), &LuaState::set_light_userdata_name);
	ClassDB::bind_method(D_METHOD("get_light_userdata_name", "tag"), &LuaState::get_light_userdata_name);

	ClassDB::bind_method(D_METHOD("clone_function", "index"), &LuaState::clone_function);

	ClassDB::bind_method(D_METHOD("clear_table", "index"), &LuaState::clear_table);
	ClassDB::bind_method(D_METHOD("clone_table", "index"), &LuaState::clone_table);

	ClassDB::bind_method(D_METHOD("ref", "index"), &LuaState::ref);
	ClassDB::bind_method(D_METHOD("get_ref", "ref"), &LuaState::get_ref);
	ClassDB::bind_method(D_METHOD("unref", "ref"), &LuaState::unref);

	ClassDB::bind_method(D_METHOD("get_stack_depth"), &LuaState::get_stack_depth);
	ClassDB::bind_method(D_METHOD("get_info", "level", "what"), &LuaState::get_info);
	ClassDB::bind_method(D_METHOD("get_argument", "level", "narg"), &LuaState::get_argument);
	ClassDB::bind_method(D_METHOD("get_local", "level", "nlocal"), &LuaState::get_local);
	ClassDB::bind_method(D_METHOD("set_local", "level", "nlocal"), &LuaState::set_local);
	ClassDB::bind_method(D_METHOD("get_upvalue", "funcindex", "nupvalue"), &LuaState::get_upvalue);
	ClassDB::bind_method(D_METHOD("set_upvalue", "funcindex", "nupvalue"), &LuaState::set_upvalue);
	ClassDB::bind_method(D_METHOD("set_single_step", "enabled"), &LuaState::set_single_step);
	ClassDB::bind_method(D_METHOD("set_interrupts", "enabled"), &LuaState::set_interrupts);
	ClassDB::bind_method(D_METHOD("set_breakpoint", "funcindex", "nline", "enabled"), &LuaState::set_breakpoint);
	ClassDB::bind_method(D_METHOD("get_coverage", "funcindex", "callback"), &LuaState::get_coverage);
	ClassDB::bind_method(D_METHOD("debug_trace"), &LuaState::debug_trace);

	ClassDB::bind_method(D_METHOD("register_library", "lib_name", "functions"), &LuaState::register_library);
	ClassDB::bind_method(D_METHOD("get_meta_field", "index", "field"), &LuaState::get_meta_field);
	ClassDB::bind_method(D_METHOD("call_meta", "index", "field"), &LuaState::call_meta);
	ClassDB::bind_method(D_METHOD("type_error", "index", "expected"), &LuaState::type_error);
	ClassDB::bind_method(D_METHOD("arg_error", "index", "message"), &LuaState::arg_error);
	ClassDB::bind_method(D_METHOD("enforce_string_inplace", "index"), &LuaState::enforce_string_inplace);
	ClassDB::bind_method(D_METHOD("opt_string_inplace", "index", "default"), &LuaState::opt_string_inplace);
	ClassDB::bind_method(D_METHOD("enforce_string_name", "index"), &LuaState::enforce_string_name);
	ClassDB::bind_method(D_METHOD("opt_string_name", "index", "default"), &LuaState::opt_string_name);
	ClassDB::bind_method(D_METHOD("enforce_number", "index"), &LuaState::enforce_number);
	ClassDB::bind_method(D_METHOD("opt_number", "index", "default"), &LuaState::opt_number);
	ClassDB::bind_method(D_METHOD("enforce_integer", "index"), &LuaState::enforce_integer);
	ClassDB::bind_method(D_METHOD("opt_integer", "index", "default"), &LuaState::opt_integer);
	ClassDB::bind_method(D_METHOD("enforce_vector3", "index"), &LuaState::enforce_vector3);
	ClassDB::bind_method(D_METHOD("opt_vector3", "index", "default"), &LuaState::opt_vector3);
	ClassDB::bind_method(D_METHOD("enforce_stack", "size", "message"), &LuaState::enforce_stack);
	ClassDB::bind_method(D_METHOD("enforce_type", "index", "type"), &LuaState::enforce_type);
	ClassDB::bind_method(D_METHOD("enforce_any", "index"), &LuaState::enforce_any);
	ClassDB::bind_method(D_METHOD("new_metatable_named", "tname"), &LuaState::new_metatable_named);
	ClassDB::bind_method(D_METHOD("get_metatable_named", "tname"), &LuaState::get_metatable_named);
	ClassDB::bind_method(D_METHOD("enforce_full_userdata", "index", "tname"), &LuaState::enforce_full_userdata);
	ClassDB::bind_method(D_METHOD("enforce_buffer", "index"), &LuaState::enforce_buffer);
	ClassDB::bind_method(D_METHOD("print_where", "level"), &LuaState::print_where);
	ClassDB::bind_method(D_METHOD("enforce_option", "index", "options", "default"), &LuaState::enforce_option, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("push_as_string", "index"), &LuaState::push_as_string);
	ClassDB::bind_method(D_METHOD("type_name_for_value", "index"), &LuaState::type_name_for_value);

	ClassDB::bind_method(D_METHOD("open_libs", "libs"), &LuaState::open_libs, DEFVAL(LuaState::LIB_ALL));
	ClassDB::bind_method(D_METHOD("sandbox"), &LuaState::sandbox);
	ClassDB::bind_method(D_METHOD("sandbox_thread"), &LuaState::sandbox_thread);

	ClassDB::bind_method(D_METHOD("is_array", "index"), &LuaState::is_array);
	ClassDB::bind_method(D_METHOD("is_object", "index", "tag"), &LuaState::is_object, DEFVAL(LUA_NOTAG));
	ClassDB::bind_method(D_METHOD("to_array", "index"), &LuaState::to_array);
	ClassDB::bind_method(D_METHOD("to_callable", "index"), &LuaState::to_callable);
	ClassDB::bind_method(D_METHOD("to_dictionary", "index"), &LuaState::to_dictionary);
	ClassDB::bind_method(D_METHOD("to_variant", "index"), &LuaState::to_variant);
	ClassDB::bind_method(D_METHOD("push_array", "value"), &LuaState::push_array);
	ClassDB::bind_method(D_METHOD("push_callable", "value"), &LuaState::push_callable);
	ClassDB::bind_method(D_METHOD("push_dictionary", "value"), &LuaState::push_dictionary);
	ClassDB::bind_method(D_METHOD("push_variant", "value"), &LuaState::push_variant);
	ClassDB::bind_method(D_METHOD("push_default_object_metatable"), &LuaState::push_default_object_metatable);

	ClassDB::bind_method(D_METHOD("load_string", "code", "chunk_name", "env"), &LuaState::load_string, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("do_string", "code", "chunk_name", "env", "nargs", "nresults", "errfunc"), &LuaState::do_string, DEFVAL(String()), DEFVAL(0), DEFVAL(0), DEFVAL(LUA_MULTRET), DEFVAL(0));
	ClassDB::bind_static_method(LuaState::get_class_static(), D_METHOD("bind_callable", "callable"), &LuaState::bind_callable);
	ClassDB::bind_method(D_METHOD("set_call_metamethod", "metatable_index", "callable"), &LuaState::set_call_metamethod);
	ClassDB::bind_method(D_METHOD("set_index_metamethod", "metatable_index", "callable"), &LuaState::set_index_metamethod);
	ClassDB::bind_method(D_METHOD("set_newindex_metamethod", "metatable_index", "callable"), &LuaState::set_newindex_metamethod);

	BIND_BITFIELD_FLAG(LIB_BASE);
	BIND_BITFIELD_FLAG(LIB_COROUTINE);
	BIND_BITFIELD_FLAG(LIB_TABLE);
	BIND_BITFIELD_FLAG(LIB_OS);
	BIND_BITFIELD_FLAG(LIB_STRING);
	BIND_BITFIELD_FLAG(LIB_BIT32);
	BIND_BITFIELD_FLAG(LIB_BUFFER);
	BIND_BITFIELD_FLAG(LIB_UTF8);
	BIND_BITFIELD_FLAG(LIB_MATH);
	BIND_BITFIELD_FLAG(LIB_DEBUG);
	BIND_BITFIELD_FLAG(LIB_VECTOR);
	BIND_BITFIELD_FLAG(LIB_BLAZIUM);
	BIND_BITFIELD_FLAG(LIB_ALL);

	BIND_ENUM_CONSTANT(VALUE_NONE);
	BIND_ENUM_CONSTANT(VALUE_NIL);
	BIND_ENUM_CONSTANT(VALUE_BOOLEAN);
	BIND_ENUM_CONSTANT(VALUE_LIGHTUSERDATA);
	BIND_ENUM_CONSTANT(VALUE_NUMBER);
	BIND_ENUM_CONSTANT(VALUE_VECTOR);
	BIND_ENUM_CONSTANT(VALUE_STRING);
	BIND_ENUM_CONSTANT(VALUE_TABLE);
	BIND_ENUM_CONSTANT(VALUE_FUNCTION);
	BIND_ENUM_CONSTANT(VALUE_USERDATA);
	BIND_ENUM_CONSTANT(VALUE_THREAD);
	BIND_ENUM_CONSTANT(VALUE_BUFFER);
	BIND_ENUM_CONSTANT(VALUE_PROTO);
	BIND_ENUM_CONSTANT(VALUE_UPVAL);
	BIND_ENUM_CONSTANT(VALUE_DEADKEY);

	BIND_ENUM_CONSTANT(STATUS_OK);
	BIND_ENUM_CONSTANT(STATUS_YIELD);
	BIND_ENUM_CONSTANT(STATUS_ERRRUN);
	BIND_ENUM_CONSTANT(STATUS_ERRSYNTAX);
	BIND_ENUM_CONSTANT(STATUS_ERRMEM);
	BIND_ENUM_CONSTANT(STATUS_ERRERR);
	BIND_ENUM_CONSTANT(STATUS_BREAK);

	BIND_ENUM_CONSTANT(CO_RUN);
	BIND_ENUM_CONSTANT(CO_SUSPENDED);
	BIND_ENUM_CONSTANT(CO_NORMAL);
	BIND_ENUM_CONSTANT(CO_FINISHED);
	BIND_ENUM_CONSTANT(CO_ERROR);

	BIND_ENUM_CONSTANT(GC_STOP);
	BIND_ENUM_CONSTANT(GC_RESTART);
	BIND_ENUM_CONSTANT(GC_COLLECT);
	BIND_ENUM_CONSTANT(GC_COUNT);
	BIND_ENUM_CONSTANT(GC_COUNTB);
	BIND_ENUM_CONSTANT(GC_ISRUNNING);
	BIND_ENUM_CONSTANT(GC_STEP);
	BIND_ENUM_CONSTANT(GC_SETGOAL);
	BIND_ENUM_CONSTANT(GC_SETSTEPMUL);
	BIND_ENUM_CONSTANT(GC_SETSTEPSIZE);

	ADD_SIGNAL(MethodInfo("interrupt", PropertyInfo(Variant::OBJECT, "state", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "LuaState"), PropertyInfo(Variant::INT, "gc_state")));
	ADD_SIGNAL(MethodInfo("debugbreak", PropertyInfo(Variant::OBJECT, "state", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "LuaState"), PropertyInfo(Variant::OBJECT, "debug_info", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "LuaDebug")));
	ADD_SIGNAL(MethodInfo("debugstep", PropertyInfo(Variant::OBJECT, "state", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "LuaState")));
}

LuaState::LuaState() :
		LuaState(luaL_newstate()) {
}

LuaState::LuaState(lua_State *p_L) :
		L(p_L) {
	ERR_FAIL_NULL_MSG(L, "lua_State* is null.");

	lua_setthreaddata(L, this);

	setup_vm();
}

LuaState::LuaState(lua_State *p_thread_L, const Ref<LuaState> &p_main_thread) :
		L(p_thread_L), main_thread(p_main_thread) {
	ERR_FAIL_NULL_MSG(p_thread_L, "Thread lua_State* is null.");
	ERR_FAIL_COND_MSG(!p_main_thread.is_valid(), "Main LuaState is not valid.");

	lua_setthreaddata(L, this);
}

LuaState::~LuaState() {
	close();
}

void LuaState::setup_vm() {
	lua_Callbacks *callbacks = lua_callbacks(L);
	callbacks->panic = callback_panic;
	callbacks->userthread = callback_userthread;
	callbacks->useratom = create_atom;
	callbacks->debugbreak = callback_debugbreak;
	callbacks->debugstep = callback_debugstep;

	LuauCodegen::ensure_vm(L);
}

Ref<LuaState> LuaState::bind_thread(lua_State *p_thread_L) {
	Ref<LuaState> state;
	state.reference_ptr(memnew(LuaState(p_thread_L, get_main_thread())));
	return state;
}

bool LuaState::is_valid_index(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot check stack index.");
	return luau_module::is_valid_index(L, p_index);
}

void LuaState::close() {
	if (!is_valid()) {
		return;
	}

	lua_setthreaddata(L, nullptr);

	if (is_main_thread()) {
		lua_close(L);
	}

	L = nullptr;
}

Ref<LuaState> LuaState::new_thread() {
	ERR_FAIL_COND_V_MSG(!is_valid(), Ref<LuaState>(), "Lua state is invalid. Cannot create thread.");
	ERR_FAIL_COND_V_MSG(!check_stack(1), Ref<LuaState>(), "Lua stack overflow. Cannot create thread.");

	lua_State *thread_L = lua_newthread(L);
	ERR_FAIL_NULL_V_MSG(thread_L, Ref<LuaState>(), "Failed to create new Lua thread.");

	Ref<LuaState> thread = bind_thread(thread_L);
	lua_pop(L, 1);
	return thread;
}

void LuaState::reset_thread() {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot reset thread.");
	lua_resetthread(L);
}

bool LuaState::is_thread_reset() {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot check if thread is reset.");
	return lua_isthreadreset(L) != 0;
}

int LuaState::abs_index(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), 0, "Lua state is invalid. Cannot convert to absolute index.");
	return lua_absindex(L, p_index);
}

int LuaState::get_top() {
	ERR_FAIL_COND_V_MSG(!is_valid(), 0, "Lua state is invalid. Cannot get stack top.");
	return lua_gettop(L);
}

void LuaState::set_top(int p_index) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot set stack top.");
	ERR_FAIL_COND_MSG(p_index < 0 && -p_index > lua_gettop(L),
			vformat("LuaState.set_top(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	lua_settop(L, p_index);
}

void LuaState::pop(int p_count) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot pop stack.");

	int top = lua_gettop(L);
	ERR_FAIL_COND_MSG(p_count < 0, vformat("LuaState.pop(%d): Cannot pop negative number of elements.", p_count));
	ERR_FAIL_COND_MSG(top < p_count, vformat("LuaState.pop(%d): Stack underflow. Stack only has %d elements.", p_count, top));

	lua_pop(L, p_count);
}

void LuaState::push_value(int p_index) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot push value.");
	ERR_FAIL_COND_MSG(!is_valid_index(p_index), vformat("LuaState.pushvalue(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 1), "LuaState.pushvalue(): Stack overflow. Cannot grow stack.");

	lua_pushvalue(L, p_index);
}

void LuaState::remove(int p_index) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot remove value.");
	ERR_FAIL_COND_MSG(!is_valid_index(p_index), vformat("LuaState.remove(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	lua_remove(L, p_index);
}

void LuaState::insert(int p_index) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot insert value.");

	int top = lua_gettop(L);
	ERR_FAIL_COND_MSG(top == 0, "LuaState.insert(): Stack is empty. Nothing to insert.");

	ERR_FAIL_COND_MSG(p_index == 0 || (p_index > 0 && p_index > top + 1) || (p_index < 0 && -p_index > top),
			vformat("LuaState.insert(%d): Invalid stack index. Stack has %d elements.", p_index, top));

	lua_insert(L, p_index);
}

void LuaState::replace(int p_index) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot replace value.");

	int top = lua_gettop(L);
	ERR_FAIL_COND_MSG(top == 0, "LuaState.replace(): Stack is empty. Nothing to replace with.");
	ERR_FAIL_COND_MSG(!is_valid_index(p_index), vformat("LuaState.replace(%d): Invalid stack index. Stack has %d elements.", p_index, top));

	lua_replace(L, p_index);
}

bool LuaState::check_stack(int p_size) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot grow stack.");
	return lua_checkstack(L, p_size) != 0;
}

void LuaState::raw_check_stack(int p_size) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot grow stack.");
	lua_rawcheckstack(L, p_size);
}

void LuaState::xmove(LuaState *p_to_state, int p_count) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot move stack values.");
	ERR_FAIL_COND_MSG(p_count < 0, vformat("LuaState.xmove(%p, %d): count cannot be negative.", p_to_state, p_count));
	ERR_FAIL_COND_MSG(!p_to_state || !p_to_state->is_valid(), "Destination Lua state is invalid. Cannot move stack values.");
	ERR_FAIL_COND_MSG(get_main_thread() != p_to_state->get_main_thread(), "Cannot xmove between different Luau VMs.");

	int top = lua_gettop(L);
	ERR_FAIL_COND_MSG(top < p_count, vformat("LuaState.xmove(%p, %d): Not enough items on source stack. Stack has %d elements.", p_to_state, p_count, top));
	ERR_FAIL_COND_MSG(!p_to_state->check_stack(p_count), vformat("LuaState.xmove(%p, %d): Stack overflow on destination. Cannot grow stack.", p_to_state, p_count));

	lua_xmove(L, p_to_state->L, p_count);
}

void LuaState::xpush(LuaState *p_to_state, int p_index) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot push stack values.");
	ERR_FAIL_COND_MSG(!p_to_state || !p_to_state->is_valid(), "Destination Lua state is invalid. Cannot push stack values.");
	ERR_FAIL_COND_MSG(get_main_thread() != p_to_state->get_main_thread(), "Cannot xpush between different Luau VMs.");
	ERR_FAIL_COND_MSG(!is_valid_index(p_index), vformat("LuaState.xpush(%p, %d): Source index is invalid.", p_to_state, p_index));
	ERR_FAIL_COND_MSG(!p_to_state->check_stack(1), vformat("LuaState.xpush(%p, %d): Stack overflow on destination. Cannot grow stack.", p_to_state, 1));

	lua_xpush(L, p_to_state->L, p_index);
}

bool LuaState::is_number(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot check type.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), false, vformat("LuaState.is_number(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));
	return lua_isnumber(L, p_index) != 0;
}

bool LuaState::is_string(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot check type.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), false, vformat("LuaState.is_string(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));
	return lua_isstring(L, p_index) != 0;
}

bool LuaState::is_c_function(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot check type.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), false, vformat("LuaState.is_c_function(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));
	return lua_iscfunction(L, p_index) != 0;
}

bool LuaState::is_lua_function(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot check type.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), false, vformat("LuaState.is_lua_function(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));
	return lua_isLfunction(L, p_index) != 0;
}

bool LuaState::is_userdata(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot check type.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), false, vformat("LuaState.is_userdata(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));
	return lua_isuserdata(L, p_index) != 0;
}

LuaState::ValueType LuaState::type(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), static_cast<ValueType>(LUA_TNONE), "Lua state is invalid. Cannot get type.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), static_cast<ValueType>(LUA_TNONE), vformat("LuaState.type(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));
	return static_cast<ValueType>(lua_type(L, p_index));
}

bool LuaState::is_function(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot check type.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), false, vformat("LuaState.is_function(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));
	return lua_isfunction(L, p_index) != 0;
}

bool LuaState::is_table(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot check type.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), false, vformat("LuaState.is_table(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));
	return lua_istable(L, p_index) != 0;
}

bool LuaState::is_full_userdata(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot check type.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), false, vformat("LuaState.is_full_userdata(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));
	return lua_type(L, p_index) == LUA_TUSERDATA;
}

bool LuaState::is_light_userdata(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot check type.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), false, vformat("LuaState.is_light_userdata(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));
	return lua_islightuserdata(L, p_index) != 0;
}

bool LuaState::is_nil(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot check type.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), false, vformat("LuaState.is_nil(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));
	return lua_isnil(L, p_index) != 0;
}

bool LuaState::is_boolean(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot check type.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), false, vformat("LuaState.is_boolean(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));
	return lua_isboolean(L, p_index) != 0;
}

bool LuaState::is_vector(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot check type.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), false, vformat("LuaState.is_vector(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));
	return lua_isvector(L, p_index) != 0;
}

bool LuaState::is_thread(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot check type.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), false, vformat("LuaState.is_thread(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));
	return lua_isthread(L, p_index) != 0;
}

bool LuaState::is_buffer(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot check type.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), false, vformat("LuaState.is_buffer(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));
	return lua_isbuffer(L, p_index) != 0;
}

bool LuaState::is_none(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot check type.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), false, vformat("LuaState.is_none(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));
	return lua_isnone(L, p_index) != 0;
}

bool LuaState::is_none_or_nil(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot check type.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), false, vformat("LuaState.is_none_or_nil(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));
	return lua_isnoneornil(L, p_index) != 0;
}

StringName LuaState::type_name(ValueType p_type) {
	ERR_FAIL_COND_V_MSG(!is_valid(), StringName(), "Lua state is invalid. Cannot get type name.");
	return StringName(lua_typename(L, static_cast<int>(p_type)));
}

bool LuaState::equal(int p_index1, int p_index2) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot compare values.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index1), false, vformat("LuaState.equal(%d, %d): First index is invalid. Stack has %d elements.", p_index1, p_index2, lua_gettop(L)));
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index2), false, vformat("LuaState.equal(%d, %d): Second index is invalid. Stack has %d elements.", p_index1, p_index2, lua_gettop(L)));
	return lua_equal(L, p_index1, p_index2) != 0;
}

bool LuaState::raw_equal(int p_index1, int p_index2) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot compare values.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index1), false, vformat("LuaState.rawequal(%d, %d): First index is invalid. Stack has %d elements.", p_index1, p_index2, lua_gettop(L)));
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index2), false, vformat("LuaState.rawequal(%d, %d): Second index is invalid. Stack has %d elements.", p_index1, p_index2, lua_gettop(L)));
	return lua_rawequal(L, p_index1, p_index2) != 0;
}

bool LuaState::less_than(int p_index1, int p_index2) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot compare values.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index1), false, vformat("LuaState.less_than(%d, %d): First index is invalid. Stack has %d elements.", p_index1, p_index2, lua_gettop(L)));
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index2), false, vformat("LuaState.less_than(%d, %d): Second index is invalid. Stack has %d elements.", p_index1, p_index2, lua_gettop(L)));
	return lua_lessthan(L, p_index1, p_index2) != 0;
}

double LuaState::to_number(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), 0.0, "Lua state is invalid. Cannot convert to number.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), 0.0, vformat("LuaState.to_number(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	return lua_tonumber(L, p_index);
}

int LuaState::to_integer(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), 0, "Lua state is invalid. Cannot convert to integer.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), 0, vformat("LuaState.to_integer(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	return lua_tointeger(L, p_index);
}

Vector3 LuaState::to_vector3(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), Vector3(), "Lua state is invalid. Cannot convert to Vector3.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), Vector3(), vformat("LuaState.to_vector3(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	const float *vec = lua_tovector(L, p_index);
	if (vec) {
		return Vector3(vec[0], vec[1], vec[2]);
	} else {
		return Vector3();
	}
}

bool LuaState::to_boolean(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot convert to boolean.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), false, vformat("LuaState.to_boolean(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	return lua_toboolean(L, p_index) != 0;
}

String LuaState::to_string_inplace(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), String(), "Lua state is invalid. Cannot convert to string.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), String(), vformat("LuaState.to_string_inplace(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	size_t len;
	const char *str = lua_tolstring(L, p_index, &len);
	ERR_FAIL_COND_V_MSG(!str, String(), "Item from stack cannot be converted to string.");

	return String::utf8(str, len);
}

StringName LuaState::to_string_name(int p_index) {
	return opt_string_name(p_index, StringName());
}

StringName LuaState::get_namecall() {
	ERR_FAIL_COND_V_MSG(!is_valid(), StringName(), "Lua state is invalid. Cannot get namecall.");

	int atom = -1;
	const char *name = lua_namecallatom(L, &atom);
	ERR_FAIL_COND_V_MSG(!name, StringName(), "No namecall set.");

	const StringName &cached = string_name_for_atom(atom);
	if (cached.is_empty()) {
		return StringName(name);
	} else {
		return cached;
	}
}

int LuaState::obj_len(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), 0, "Lua state is invalid. Cannot get object length.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), 0, vformat("LuaState.obj_len(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));
	return lua_objlen(L, p_index);
}

Object *LuaState::to_light_userdata(int p_index, int p_tag) {
	ERR_FAIL_COND_V_MSG(!is_valid(), nullptr, "Lua state is invalid. Cannot convert to light userdata.");
	return to_light_object(L, p_index, p_tag);
}

Object *LuaState::to_full_userdata(int p_index, int p_tag) {
	ERR_FAIL_COND_V_MSG(!is_valid(), nullptr, "Lua state is invalid. Cannot convert to userdata.");
	return to_full_object(L, p_index, p_tag);
}

Object *LuaState::to_object(int p_index, int p_tag) {
	ERR_FAIL_COND_V_MSG(!is_valid(), nullptr, "Lua state is invalid. Cannot convert to object.");
	return luau_module::to_object(L, p_index, p_tag);
}

int LuaState::light_userdata_tag(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), LUA_NOTAG, "Lua state is invalid. Cannot get light userdata tag.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), LUA_NOTAG, vformat("LuaState.light_userdata_tag(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	int tag = lua_lightuserdatatag(L, p_index);
	if (tag < 0 || tag >= LUA_LUTAG_LIMIT) {
		return LUA_NOTAG;
	} else {
		return tag;
	}
}

int LuaState::full_userdata_tag(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), LUA_NOTAG, "Lua state is invalid. Cannot get full userdata tag.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), LUA_NOTAG, vformat("LuaState.full_userdata_tag(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	int tag = lua_userdatatag(L, p_index);
	if (tag < 0 || tag >= LUA_UTAG_LIMIT) {
		return LUA_NOTAG;
	} else {
		return tag;
	}
}

Ref<LuaState> LuaState::to_thread(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), Ref<LuaState>(), "Lua state is invalid. Cannot convert to thread.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), Ref<LuaState>(), vformat("LuaState.to_thread(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	lua_State *thread_L = lua_tothread(L, p_index);
	if (thread_L) {
		return find_or_create_lua_state(thread_L);
	} else {
		return Ref<LuaState>();
	}
}

PackedByteArray LuaState::to_buffer(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), PackedByteArray(), "Lua state is invalid. Cannot convert to buffer.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), PackedByteArray(), vformat("LuaState.to_buffer(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	size_t size;
	void *data = lua_tobuffer(L, p_index, &size);
	if (!data) {
		return PackedByteArray();
	}

	PackedByteArray byte_array;
	byte_array.resize(size);
	memcpy(byte_array.ptrw(), data, size);
	return byte_array;
}

uint64_t LuaState::to_pointer(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), 0, "Lua state is invalid. Cannot convert to pointer.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), 0, vformat("LuaState.to_pointer(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	return reinterpret_cast<uint64_t>(lua_topointer(L, p_index));
}

void LuaState::push_nil() {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot push nil.");
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 1), "LuaState.push_nil(): Stack overflow. Cannot grow stack.");
	lua_pushnil(L);
}

void LuaState::push_number(double p_num) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot push number.");
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 1), "LuaState.push_number(): Stack overflow. Cannot grow stack.");
	lua_pushnumber(L, p_num);
}

void LuaState::push_integer(int p_num) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot push integer.");
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 1), "LuaState.push_integer(): Stack overflow. Cannot grow stack.");
	lua_pushinteger(L, p_num);
}

void LuaState::push_vector3(const Vector3 &p_vec) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot push vector.");
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 1), "LuaState.push_vector3(): Stack overflow. Cannot grow stack.");
	lua_pushvector(L, static_cast<float>(p_vec.x), static_cast<float>(p_vec.y), static_cast<float>(p_vec.z));
}

void LuaState::push_string(const String &p_str) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot push string.");
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 1), "LuaState.push_string(): Stack overflow. Cannot grow stack.");

	CharString utf8 = p_str.utf8();
	lua_pushlstring(L, utf8.get_data(), utf8.length());
}

void LuaState::push_string_name(const StringName &p_string_name) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot push string.");
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 1), "LuaState.push_string(): Stack overflow. Cannot grow stack.");

	CharString utf8 = char_string(p_string_name);
	lua_pushlstring(L, utf8.get_data(), utf8.length());
}

void LuaState::push_boolean(bool b) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot push boolean.");
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 1), "LuaState.push_boolean(): Stack overflow. Cannot grow stack.");
	lua_pushboolean(L, b ? 1 : 0);
}

bool LuaState::push_thread() {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot push thread.");
	ERR_FAIL_COND_V_MSG(!lua_checkstack(L, 1), false, "LuaState.push_thread(): Stack overflow. Cannot grow stack.");

	bool pushed_main_thread = lua_pushthread(L) != 0;
	if (pushed_main_thread != is_main_thread()) {
		ERR_PRINT("LuaState.push_thread() inconsistency: Lua and GDExtension disagree about which is the main thread.");
	}

	return pushed_main_thread;
}

void LuaState::push_light_userdata(Object *p_obj, int p_tag) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot push light userdata.");
	ERR_FAIL_COND_MSG(p_tag < LUA_NOTAG || p_tag >= LUA_LUTAG_LIMIT, vformat("LuaState.push_light_userdata(%s, %d): Invalid tag.", p_obj, p_tag));

	push_light_object(L, p_obj, p_tag);
}

void LuaState::push_full_userdata(Object *p_obj, int p_tag) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot push full userdata.");
	ERR_FAIL_COND_MSG(p_tag < LUA_NOTAG || p_tag >= LUA_UTAG_LIMIT, vformat("LuaState.push_full_userdata(%s, %d): Invalid tag.", p_obj, p_tag));

	push_full_object(L, p_obj, p_tag);
}

void LuaState::push_object(Object *p_obj, int p_tag) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot push userdata.");
	ERR_FAIL_COND_MSG(p_tag < LUA_NOTAG || p_tag >= LUA_UTAG_LIMIT, vformat("LuaState.push_object(%s, %d): Invalid tag.", p_obj, p_tag));

	luau_module::push_object(L, p_obj, p_tag);
}

LuaState::ValueType LuaState::get_table(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), static_cast<ValueType>(LUA_TNONE), "Lua state is invalid. Cannot get table value.");

	int top = lua_gettop(L);
	ERR_FAIL_COND_V_MSG(p_index == -1 || p_index == top, static_cast<ValueType>(LUA_TNONE), vformat("LuaState.get_table(%d): Table index cannot be the key (at index %d).", p_index, top));
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), static_cast<ValueType>(LUA_TNONE), vformat("LuaState.get_table(%d): Invalid table index. Stack has %d elements.", p_index, top));

	return static_cast<ValueType>(lua_gettable(L, p_index));
}

LuaState::ValueType LuaState::get_field(int p_index, const StringName &p_key) {
	ERR_FAIL_COND_V_MSG(!is_valid(), static_cast<ValueType>(LUA_TNONE), "Lua state is invalid. Cannot get field.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), static_cast<ValueType>(LUA_TNONE), vformat("LuaState.get_field(%d, \"%s\"): Invalid table index. Stack has %d elements.", p_index, p_key, lua_gettop(L)));
	ERR_FAIL_COND_V_MSG(!lua_checkstack(L, 1), static_cast<ValueType>(LUA_TNONE), "LuaState.get_field(): Stack overflow. Cannot grow stack.");

	return static_cast<ValueType>(lua_getfield(L, p_index, char_string(p_key).get_data()));
}

LuaState::ValueType LuaState::get_global(const StringName &p_key) {
	ERR_FAIL_COND_V_MSG(!is_valid(), static_cast<ValueType>(LUA_TNONE), "Lua state is invalid. Cannot get global variable.");
	ERR_FAIL_COND_V_MSG(!lua_checkstack(L, 1), static_cast<ValueType>(LUA_TNONE), "LuaState.get_global(): Stack overflow. Cannot grow stack.");

	return static_cast<ValueType>(lua_getglobal(L, char_string(p_key).get_data()));
}

LuaState::ValueType LuaState::raw_get_field(int p_index, const StringName &p_key) {
	ERR_FAIL_COND_V_MSG(!is_valid(), static_cast<ValueType>(LUA_TNONE), "Lua state is invalid. Cannot raw get field.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), static_cast<ValueType>(LUA_TNONE), vformat("LuaState.raw_get_field(%d, \"%s\"): Invalid table index. Stack has %d elements.", p_index, p_key, lua_gettop(L)));
	ERR_FAIL_COND_V_MSG(!lua_checkstack(L, 1), static_cast<ValueType>(LUA_TNONE), "LuaState.raw_get_field(): Stack overflow. Cannot grow stack.");

	return static_cast<ValueType>(lua_rawgetfield(L, p_index, char_string(p_key).get_data()));
}

LuaState::ValueType LuaState::raw_get(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), static_cast<ValueType>(LUA_TNONE), "Lua state is invalid. Cannot raw get.");

	int top = lua_gettop(L);
	ERR_FAIL_COND_V_MSG(p_index == -1 || p_index == top, static_cast<ValueType>(LUA_TNONE), vformat("LuaState.raw_get(%d): Table index cannot be the key (at index %d).", p_index, top));
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), static_cast<ValueType>(LUA_TNONE), vformat("LuaState.raw_get(%d): Invalid table index. Stack has %d elements.", p_index, top));

	return static_cast<ValueType>(lua_rawget(L, p_index));
}

LuaState::ValueType LuaState::raw_geti(int p_stack_index, int p_table_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), static_cast<ValueType>(LUA_TNONE), "Lua state is invalid. Cannot raw get index.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_stack_index), static_cast<ValueType>(LUA_TNONE), vformat("LuaState.raw_geti(%d, %d): Invalid table index. Stack has %d elements.", p_stack_index, p_table_index, lua_gettop(L)));
	ERR_FAIL_COND_V_MSG(!lua_checkstack(L, 1), static_cast<ValueType>(LUA_TNONE), "LuaState.raw_geti(): Stack overflow. Cannot grow stack.");

	return static_cast<ValueType>(lua_rawgeti(L, p_stack_index, p_table_index));
}

void LuaState::create_table(int p_narr, int p_nrec) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot create table.");
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 1), "LuaState.create_table(): Stack overflow. Cannot grow stack.");
	lua_createtable(L, p_narr, p_nrec);
}

void LuaState::set_read_only(int p_index, bool p_enabled) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot set read-only status.");
	ERR_FAIL_COND_MSG(!is_valid_index(p_index), vformat("LuaState.set_read_only(%d, %s): Invalid stack index. Stack has %d elements.", p_index, p_enabled ? "true" : "false", lua_gettop(L)));

	lua_setreadonly(L, p_index, p_enabled ? 1 : 0);
}

bool LuaState::get_read_only(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot get read-only status.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), false, vformat("LuaState.get_read_only(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	return lua_getreadonly(L, p_index) != 0;
}

void LuaState::set_safe_env(int p_index, bool p_enabled) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot set safe environment status.");
	ERR_FAIL_COND_MSG(!is_valid_index(p_index), vformat("LuaState.set_safe_env(%d, %s): Invalid stack index. Stack has %d elements.", p_index, p_enabled ? "true" : "false", lua_gettop(L)));

	lua_setsafeenv(L, p_index, p_enabled ? 1 : 0);
}

bool LuaState::get_metatable(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot get metatable.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), false, vformat("LuaState.get_metatable(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));
	ERR_FAIL_COND_V_MSG(!lua_checkstack(L, 1), false, "LuaState.get_metatable(): Stack overflow. Cannot grow stack.");

	return lua_getmetatable(L, p_index) != 0;
}

void LuaState::get_fenv(int p_index) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot get function environment.");
	ERR_FAIL_COND_MSG(!is_valid_index(p_index), vformat("LuaState.get_fenv(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 1), "LuaState.get_fenv(): Stack overflow. Cannot grow stack.");

	lua_getfenv(L, p_index);
}

void LuaState::set_table(int p_index) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot set table value.");

	int top = lua_gettop(L);
	ERR_FAIL_COND_MSG(p_index == -1 || p_index == top, vformat("LuaState.set_table(%d): Table index cannot be the value (at index %d).", p_index, top));
	ERR_FAIL_COND_MSG(p_index == -2 || p_index == top - 1, vformat("LuaState.set_table(%d): Table index cannot be the key (at index %d).", p_index, top - 1));

	ERR_FAIL_COND_MSG(!is_valid_index(p_index), vformat("LuaState.set_table(%d): Invalid table index. Stack has %d elements.", p_index, top));

	lua_settable(L, p_index);
}

void LuaState::set_field(int p_index, const StringName &p_key) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot set field.");

	int top = lua_gettop(L);
	ERR_FAIL_COND_MSG(p_index == -1 || p_index == top, vformat("LuaState.set_field(%d, \"%s\"): Table index cannot be the value (at index %d).", p_index, p_key, top));

	ERR_FAIL_COND_MSG(!is_valid_index(p_index), vformat("LuaState.set_field(%d, \"%s\"): Invalid table index. Stack has %d elements.", p_index, p_key, top));

	lua_setfield(L, p_index, char_string(p_key).get_data());
}

void LuaState::set_global(const StringName &p_key) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot set global variable.");

	int top = lua_gettop(L);
	ERR_FAIL_COND_MSG(top < 1, vformat("LuaState.set_global(): Need value on stack. Stack has %d elements.", top));

	lua_setglobal(L, char_string(p_key).get_data());
}

void LuaState::raw_set_field(int p_index, const StringName &p_key) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot raw set field.");

	int top = lua_gettop(L);
	ERR_FAIL_COND_MSG(p_index == -1 || p_index == top, vformat("LuaState.raw_set_field(%d, \"%s\"): Table index cannot be the value (at index %d).", p_index, p_key, top));
	ERR_FAIL_COND_MSG(!is_valid_index(p_index), vformat("LuaState.raw_set_field(%d, \"%s\"): Invalid table index. Stack has %d elements.", p_index, p_key, top));

	lua_rawsetfield(L, p_index, char_string(p_key).get_data());
}

void LuaState::raw_set(int p_index) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot raw set.");

	int top = lua_gettop(L);
	ERR_FAIL_COND_MSG(p_index == -1 || p_index == top, vformat("LuaState.raw_set(%d): Table index cannot be the value (at index %d).", p_index, top));
	ERR_FAIL_COND_MSG(p_index == -2 || p_index == top - 1, vformat("LuaState.raw_set(%d): Table index cannot be the key (at index %d).", p_index, top - 1));

	ERR_FAIL_COND_MSG(!is_valid_index(p_index), vformat("LuaState.raw_set(%d): Invalid table index. Stack has %d elements.", p_index, top));

	lua_rawset(L, p_index);
}

void LuaState::raw_seti(int p_stack_index, int p_table_index) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot raw set index.");

	int top = lua_gettop(L);
	ERR_FAIL_COND_MSG(p_stack_index == -1 || p_stack_index == top, vformat("LuaState.raw_seti(%d, %d): Index of table cannot be the value (at index %d).", p_stack_index, p_table_index, top));

	ERR_FAIL_COND_MSG(!is_valid_index(p_stack_index), vformat("LuaState.raw_seti(%d, %d): Invalid stack index. Stack has %d elements.", p_stack_index, p_table_index, top));

	lua_rawseti(L, p_stack_index, p_table_index);
}

void LuaState::set_metatable(int p_index) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot set metatable.");

	int top = lua_gettop(L);
	ERR_FAIL_COND_MSG(p_index == -1 || p_index == top, vformat("LuaState.set_metatable(%d): Index cannot be the metatable (at index %d).", p_index, top));

	ERR_FAIL_COND_MSG(!is_valid_index(p_index), vformat("LuaState.set_metatable(%d): Invalid stack index. Stack has %d elements.", p_index, top));

	lua_setmetatable(L, p_index);
}

bool LuaState::set_fenv(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot set fenv.");

	int top = lua_gettop(L);
	ERR_FAIL_COND_V_MSG(p_index == -1 || p_index == top, false, vformat("LuaState.set_fenv(%d): Index cannot be the environment table (at index %d).", p_index, top));
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), false, vformat("LuaState.set_fenv(%d): Invalid stack index. Stack has %d elements.", p_index, top));

	return lua_setfenv(L, p_index) != 0;
}

bool LuaState::load_bytecode(const PackedByteArray &p_bytecode, const String &p_chunk_name, int p_env) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot load bytecode.");
	ERR_FAIL_COND_V_MSG(!lua_checkstack(L, 1), false, "LuaState.load_bytecode(): Stack overflow. Cannot grow stack.");

	return luau_load(L, p_chunk_name.utf8().get_data(), reinterpret_cast<const char *>(p_bytecode.ptr()), p_bytecode.size(), p_env) == 0;
}

LuaState::Status LuaState::pcall(int p_nargs, int p_nresults, int p_errfunc) {
	ERR_FAIL_COND_V_MSG(!is_valid(), static_cast<Status>(LUA_ERRMEM), "Lua state is invalid. Cannot pcall function.");
	ERR_FAIL_COND_V_MSG(p_nargs < 0, static_cast<Status>(LUA_ERRMEM), vformat("LuaState.pcall(%d, %d, %d): nargs cannot be negative.", p_nargs, p_nresults, p_errfunc));
	ERR_FAIL_COND_V_MSG(p_nresults > p_nargs && !lua_checkstack(L, p_nresults - p_nargs), static_cast<Status>(LUA_ERRMEM), vformat("LuaState.call(%d, %d): Stack overflow. Cannot grow stack.", p_nargs, p_nresults));

	int top = lua_gettop(L);
	ERR_FAIL_COND_V_MSG(top < p_nargs + 1, static_cast<Status>(LUA_ERRMEM), vformat("LuaState.pcall(%d, %d, %d): Need function + %d arguments on stack. Stack has %d elements.", p_nargs, p_nresults, p_errfunc, p_nargs, top));
	ERR_FAIL_COND_V_MSG(p_errfunc != 0 && !is_valid_index(p_errfunc), static_cast<Status>(LUA_ERRMEM), vformat("LuaState.pcall(%d, %d, %d): Invalid error function index. Stack has %d elements.", p_nargs, p_nresults, p_errfunc, top));

	int status = lua_pcall(L, p_nargs, p_nresults, p_errfunc);
	return static_cast<Status>(status);
}

LuaState::Status LuaState::cpcall(Callable p_callable) {
	ERR_FAIL_COND_V_MSG(!is_valid(), static_cast<Status>(LUA_ERRMEM), "Lua state is invalid. Cannot pcall function.");
	ERR_FAIL_COND_V_MSG(!p_callable.is_valid(), static_cast<Status>(LUA_ERRMEM), "LuaState.cpcall: Callable is invalid.");

	lua_pushcfunction(L, cpcall_wrapper, "cpcall");
	lua_pushlightuserdata(L, &p_callable);
	const int status = lua_pcall(L, 1, 0, 0);
	return static_cast<Status>(status);
}

void LuaState::yield(int p_nresults) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot yield.");
	ERR_FAIL_COND_MSG(lua_gettop(L) < p_nresults, "LuaState.yield(): Not enough values on the stack to yield.");
	lua_yield(L, p_nresults);
}

void LuaState::break_execution() {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot break.");
	::lua_break(L);
}

LuaState::Status LuaState::resume(int p_narg, LuaState *p_from) {
	ERR_FAIL_COND_V_MSG(!is_valid(), static_cast<Status>(LUA_ERRMEM), "Lua state is invalid. Cannot resume execution.");
	ERR_FAIL_COND_V_MSG(lua_gettop(L) < p_narg, static_cast<Status>(LUA_ERRMEM), vformat("LuaState.resume(%d): Not enough values on the stack to resume.", p_narg));

	int status = lua_resume(L, p_from ? p_from->L : nullptr, p_narg);
	return static_cast<Status>(status);
}

LuaState::Status LuaState::resume_error(LuaState *p_from) {
	ERR_FAIL_COND_V_MSG(!is_valid(), static_cast<Status>(LUA_ERRMEM), "Lua state is invalid. Cannot resume execution.");

	int status = lua_resumeerror(L, p_from ? p_from->L : nullptr);
	return static_cast<Status>(status);
}

LuaState::Status LuaState::status() {
	ERR_FAIL_COND_V_MSG(!is_valid(), static_cast<Status>(LUA_ERRMEM), "Lua state is invalid. Cannot get status.");
	return static_cast<Status>(lua_status(L));
}

bool LuaState::is_yieldable() {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot check if yieldable.");
	return lua_isyieldable(L);
}

LuaState::CoStatus LuaState::co_status(LuaState *p_co) {
	ERR_FAIL_COND_V_MSG(!is_valid(), CO_RUN, "Lua state is invalid. Cannot get coroutine status.");
	ERR_FAIL_COND_V_MSG(!p_co || !p_co->is_valid(), CO_RUN, "Coroutine Lua state is invalid. Cannot get coroutine status.");
	ERR_FAIL_COND_V_MSG(get_main_thread() != p_co->get_main_thread(), CO_RUN, "Cannot get coroutine status for a different Luau VM.");

	return static_cast<CoStatus>(lua_costatus(L, p_co->L));
}

int LuaState::gc(GCOp p_what, int p_data) {
	ERR_FAIL_COND_V_MSG(!is_valid(), 0, "Lua state is invalid. Cannot control GC.");
	return lua_gc(L, static_cast<lua_GCOp>(p_what), p_data);
}

void LuaState::set_memory_category(int p_category) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot set memory category.");
	lua_setmemcat(L, p_category);
}

uint64_t LuaState::get_total_bytes(int p_category) {
	ERR_FAIL_COND_V_MSG(!is_valid(), 0, "Lua state is invalid. Cannot get total bytes.");
	return lua_totalbytes(L, p_category);
}

void LuaState::error() {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot raise error.");
	ERR_FAIL_COND_MSG(lua_gettop(L) < 1, "LuaState.error(): Need error object on stack.");
	lua_error(L);
}

bool LuaState::next(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot get next table entry.");
	ERR_FAIL_COND_V_MSG(!lua_checkstack(L, 1), false, vformat("LuaState.next(%d): Stack overflow. Cannot grow stack.", p_index));

	int top = lua_gettop(L);
	ERR_FAIL_COND_V_MSG(top < 1, false, "LuaState.next(): Stack is empty. Need a table and key on the stack.");

	ERR_FAIL_COND_V_MSG(p_index == -1 || p_index == top, false, vformat("LuaState.next(%d): Table index cannot be the key (at index %d).", p_index, top));
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), false, vformat("LuaState.next(%d): Invalid table index. Stack has %d elements.", p_index, top));

	return lua_next(L, p_index) != 0;
}

int LuaState::raw_iter(int p_index, int p_iter) {
	ERR_FAIL_COND_V_MSG(!is_valid(), -1, "Lua state is invalid. Cannot raw iterate table.");
	ERR_FAIL_COND_V_MSG(!lua_checkstack(L, 2), false, vformat("LuaState.raw_iter(%d, %d): Stack overflow. Cannot grow stack.", p_index, p_iter));

	ERR_FAIL_COND_V_MSG(p_iter < 0, -1, vformat("LuaState.raw_iter(%d, %d): Invalid iterator.", p_index, p_iter));
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), -1, vformat("LuaState.raw_iter(%d, %d): Invalid table index. Stack has %d elements.", p_index, p_iter, lua_gettop(L)));

	return lua_rawiter(L, p_index, p_iter);
}

void LuaState::concat(int p_count) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot concatenate.");
	ERR_FAIL_COND_MSG(p_count < 0, vformat("LuaState.concat(%d): count cannot be negative.", p_count));
	ERR_FAIL_COND_MSG(p_count == 0 && !lua_checkstack(L, 1), vformat("LuaState.concat(%d): Stack overflow. Cannot grow stack.", p_count));

	int top = lua_gettop(L);
	ERR_FAIL_COND_MSG(top < p_count, vformat("LuaState.concat(%d): Not enough values on the stack to concatenate. Stack has %d elements.", p_count, top));

	lua_concat(L, p_count);
}

void LuaState::set_full_userdata_tag(int p_index, int p_tag) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot set full userdata tag.");
	ERR_FAIL_COND_MSG(!is_valid_index(p_index), vformat("LuaState.set_full_userdata_tag(%d, %d): Invalid stack index. Stack has %d elements.", p_index, p_tag, lua_gettop(L)));
	ERR_FAIL_COND_MSG(p_tag < 0 || p_tag >= LUA_UTAG_LIMIT, vformat("LuaState.set_full_userdata_tag(%d, %d): Invalid tag.", p_index, p_tag));

	update_full_object_tag(L, p_index, p_tag);
}

void LuaState::set_full_userdata_metatable(int p_tag) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot set full userdata metatable.");
	ERR_FAIL_COND_MSG(p_tag < 0 || p_tag >= LUA_UTAG_LIMIT, vformat("LuaState.set_full_userdata_metatable(%d): Invalid tag.", p_tag));

	int top = lua_gettop(L);
	ERR_FAIL_COND_MSG(top < 1, vformat("LuaState.set_full_userdata_metatable(%d): Expected metatable at top of stack. Stack has %d elements.", p_tag, top));

	lua_setuserdatametatable(L, p_tag);
}

void LuaState::get_full_userdata_metatable(int p_tag) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot get full userdata metatable.");
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 1), vformat("LuaState.get_full_userdata_metatable(%d): Stack overflow. Cannot grow stack.", p_tag));
	ERR_FAIL_COND_MSG(p_tag < 0 || p_tag >= LUA_UTAG_LIMIT, vformat("LuaState.get_full_userdata_metatable(%d): Invalid tag.", p_tag));

	lua_getuserdatametatable(L, p_tag);
}

void LuaState::set_light_userdata_name(int p_tag, const StringName &p_name) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot set light userdata name.");
	ERR_FAIL_COND_MSG(p_tag < 0 || p_tag >= LUA_LUTAG_LIMIT, vformat("LuaState.set_light_userdata_name(%d, %s): Invalid tag.", p_tag, p_name));

	lua_setlightuserdataname(L, p_tag, char_string(p_name).get_data());
}

StringName LuaState::get_light_userdata_name(int p_tag) {
	ERR_FAIL_COND_V_MSG(!is_valid(), StringName(), "Lua state is invalid. Cannot get light userdata name.");
	ERR_FAIL_COND_V_MSG(p_tag < 0 || p_tag >= LUA_LUTAG_LIMIT, StringName(), vformat("LuaState.get_light_userdata_name(%d): Invalid tag.", p_tag));

	const char *name = lua_getlightuserdataname(L, p_tag);
	return StringName(name);
}

void LuaState::clone_function(int p_index) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot clone function.");
	ERR_FAIL_COND_MSG(!is_valid_index(p_index), vformat("LuaState.clone_function(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 1), vformat("LuaState.clone_function(%d): Stack overflow. Cannot grow stack.", p_index));

	lua_clonefunction(L, p_index);
}

void LuaState::clear_table(int p_index) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot clear table.");
	ERR_FAIL_COND_MSG(!is_valid_index(p_index), vformat("LuaState.clear_table(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	lua_cleartable(L, p_index);
}

void LuaState::clone_table(int p_index) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot clone table.");
	ERR_FAIL_COND_MSG(!is_valid_index(p_index), vformat("LuaState.clone_table(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 1), vformat("LuaState.clone_table(%d): Stack overflow. Cannot grow stack.", p_index));

	lua_clonetable(L, p_index);
}

int LuaState::ref(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), LUA_NOREF, "Lua state is invalid. Cannot create reference.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), LUA_NOREF, vformat("LuaState.ref(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	return lua_ref(L, p_index);
}

void LuaState::get_ref(int p_ref) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot get reference.");
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 1), "LuaState.getref(): Stack overflow. Cannot grow stack.");

	lua_getref(L, p_ref);
}

void LuaState::unref(int p_ref) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot release reference.");
	lua_unref(L, p_ref);
}

int LuaState::get_stack_depth() {
	ERR_FAIL_COND_V_MSG(!is_valid(), -1, "Lua state is invalid. Cannot get stack depth.");
	return lua_stackdepth(L);
}

Ref<LuaDebug> LuaState::get_info(int p_level, const String &p_what) {
	ERR_FAIL_COND_V_MSG(!is_valid(), Ref<LuaDebug>(), "Lua state is invalid. Cannot get debug info.");

	Ref<LuaDebug> debug_info;
	debug_info.reference_ptr(memnew(LuaDebug));
	if (lua_getinfo(L, p_level, p_what.utf8().get_data(), debug_info->ptrw())) {
		return debug_info;
	} else {
		return Ref<LuaDebug>();
	}
}

bool LuaState::get_argument(int p_level, int p_narg) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot get argument.");
	ERR_FAIL_COND_V_MSG(!lua_checkstack(L, 1), false, vformat("LuaState.get_argument(%d, %d): Stack overflow. Cannot grow stack.", p_level, p_narg));

	return lua_getargument(L, p_level, p_narg) != 0;
}

StringName LuaState::get_local(int p_level, int p_nlocal) {
	ERR_FAIL_COND_V_MSG(!is_valid(), StringName(), "Lua state is invalid. Cannot get local variable.");
	ERR_FAIL_COND_V_MSG(!lua_checkstack(L, 1), StringName(), vformat("LuaState.get_local(%d, %d): Stack overflow. Cannot grow stack.", p_level, p_nlocal));

	const char *name = lua_getlocal(L, p_level, p_nlocal);
	if (name) {
		return StringName(name);
	} else {
		return StringName();
	}
}

StringName LuaState::set_local(int p_level, int p_nlocal) {
	ERR_FAIL_COND_V_MSG(!is_valid(), StringName(), "Lua state is invalid. Cannot set local variable.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(-1), StringName(), vformat("LuaState.set_local(%d, %d): Need value on stack. Stack has %d elements.", p_level, p_nlocal, lua_gettop(L)));

	const char *name = lua_setlocal(L, p_level, p_nlocal);
	if (name) {
		return StringName(name);
	} else {
		return StringName();
	}
}

StringName LuaState::get_upvalue(int p_funcindex, int p_nupvalue) {
	ERR_FAIL_COND_V_MSG(!is_valid(), StringName(), "Lua state is invalid. Cannot get upvalue.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_funcindex), StringName(), vformat("LuaState.get_upvalue(%d, %d): Invalid function index. Stack has %d elements.", p_funcindex, p_nupvalue, lua_gettop(L)));
	ERR_FAIL_COND_V_MSG(!lua_checkstack(L, 1), StringName(), vformat("LuaState.get_upvalue(%d, %d): Stack overflow. Cannot grow stack.", p_funcindex, p_nupvalue));

	const char *name = lua_getupvalue(L, p_funcindex, p_nupvalue);
	if (name) {
		return StringName(name);
	} else {
		return StringName();
	}
}

StringName LuaState::set_upvalue(int p_funcindex, int p_nupvalue) {
	ERR_FAIL_COND_V_MSG(!is_valid(), StringName(), "Lua state is invalid. Cannot set upvalue.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_funcindex), StringName(), vformat("LuaState.set_upvalue(%d, %d): Invalid function index. Stack has %d elements.", p_funcindex, p_nupvalue, lua_gettop(L)));
	ERR_FAIL_COND_V_MSG(!is_valid_index(-1), StringName(), vformat("LuaState.set_upvalue(%d, %d): Need value on stack. Stack has %d elements.", p_funcindex, p_nupvalue, lua_gettop(L)));

	const char *name = lua_setupvalue(L, p_funcindex, p_nupvalue);
	if (name) {
		return StringName(name);
	} else {
		return StringName();
	}
}

void LuaState::set_single_step(bool p_enabled) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot set singlestep mode.");
	lua_singlestep(L, p_enabled);
}

void LuaState::set_interrupts(bool p_enabled) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot set interrupts.");

	if (!is_main_thread()) {
		WARN_PRINT("LuaState.set_interrupts() called on a non-main Lua thread, but will affect all threads.");
	}

	lua_Callbacks *callbacks = lua_callbacks(L);
	callbacks->interrupt = p_enabled ? callback_interrupt : nullptr;
}

int LuaState::set_breakpoint(int p_funcindex, int p_nline, bool p_enabled) {
	ERR_FAIL_COND_V_MSG(!is_valid(), -1, "Lua state is invalid. Cannot set breakpoint.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_funcindex), -1, vformat("LuaState.set_breakpoint(%d, %d, %s): Invalid function index. Stack has %d elements.", p_funcindex, p_nline, p_enabled ? "true" : "false", lua_gettop(L)));

	return lua_breakpoint(L, p_funcindex, p_nline, p_enabled);
}

void LuaState::get_coverage(int p_funcindex, Callable p_callback) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot get coverage.");
	ERR_FAIL_COND_MSG(!is_valid_index(p_funcindex), vformat("LuaState.get_coverage(%d, ...): Invalid function index. Stack has %d elements.", p_funcindex, lua_gettop(L)));

	lua_getcoverage(L, p_funcindex, static_cast<void *>(&p_callback), &coverage_wrapper);
}

String LuaState::debug_trace() {
	ERR_FAIL_COND_V_MSG(!is_valid(), String(), "Lua state is invalid. Cannot get debug trace.");

	const char *trace = lua_debugtrace(L);
	if (trace) {
		return String(trace);
	} else {
		return String();
	}
}

void LuaState::register_library(const StringName &p_lib_name, const Dictionary &p_functions) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot register library.");
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 3), vformat("LuaState.register_library(%s): Stack overflow. Cannot grow stack.", p_lib_name));

	if (!p_lib_name.is_empty()) {
		luaL_Reg reg[] = {
			{ NULL, NULL }
		};
		luaL_register(L, char_string(p_lib_name).get_data(), reg);
	}

	for (const Variant &key : p_functions.keys()) {
		const Variant &value = p_functions[key];

		::push_variant(L, key);
		::push_variant(L, value);
		lua_settable(L, -3);
	}
}

bool LuaState::get_meta_field(int p_index, const StringName &p_field) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot get meta field.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), false, vformat("LuaState.get_meta_field(%d, \"%s\"): Invalid stack index. Stack has %d elements.", p_index, p_field, lua_gettop(L)));
	ERR_FAIL_COND_V_MSG(!lua_checkstack(L, 1), false, vformat("LuaState.get_meta_field(%d, \"%s\"): Stack overflow. Cannot grow stack.", p_index, p_field));

	return luaL_getmetafield(L, p_index, char_string(p_field).get_data()) != 0;
}

bool LuaState::call_meta(int p_index, const StringName &p_field) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot call meta method.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), false, vformat("LuaState.call_meta(%d, \"%s\"): Invalid stack index. Stack has %d elements.", p_index, p_field, lua_gettop(L)));

	return luaL_callmeta(L, p_index, char_string(p_field).get_data()) != 0;
}

void LuaState::type_error(int p_index, const StringName &p_expected) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot raise type error.");
	ERR_FAIL_COND_MSG(!is_valid_index(p_index), vformat("LuaState.type_error(%d, \"%s\"): Invalid stack index. Stack has %d elements.", p_index, p_expected, lua_gettop(L)));

	luaL_typeerror(L, p_index, char_string(p_expected).get_data());
}

void LuaState::arg_error(int p_index, const String &p_message) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot raise argument error.");
	ERR_FAIL_COND_MSG(!is_valid_index(p_index), vformat("LuaState.arg_error(%d, \"%s\"): Invalid stack index. Stack has %d elements.", p_index, p_message, lua_gettop(L)));

	luaL_argerror(L, p_index, p_message.utf8().get_data());
}

String LuaState::enforce_string_inplace(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), String(), "Lua state is invalid. Cannot enforce string.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), String(), vformat("LuaState.enforce_string_inplace(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	size_t len;
	const char *str = luaL_checklstring(L, p_index, &len);
	return String::utf8(str, len);
}

String LuaState::opt_string_inplace(int p_index, const String &p_default) {
	ERR_FAIL_COND_V_MSG(!is_valid(), p_default, "Lua state is invalid. Cannot get optional string.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), p_default, vformat("LuaState.opt_string(%d, \"%s\"): Invalid stack index. Stack has %d elements.", p_index, p_default, lua_gettop(L)));

	size_t len;
	const char *str = luaL_optlstring(L, p_index, NULL, &len);
	if (str) {
		return String::utf8(str, len);
	} else {
		return p_default;
	}
}

StringName LuaState::enforce_string_name(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), StringName(), "Lua state is invalid. Cannot convert to StringName.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), StringName(), vformat("LuaState.to_string_name(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	if (lua_type(L, p_index) == LUA_TSTRING) {
		return luau_module::to_string_name(L, p_index);
	} else {
		luaL_typeerror(L, p_index, "string");
	}
}

StringName LuaState::opt_string_name(int p_index, const StringName &p_default) {
	ERR_FAIL_COND_V_MSG(!is_valid(), StringName(), "Lua state is invalid. Cannot convert to StringName.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), StringName(), vformat("LuaState.to_string_name(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	if (lua_type(L, p_index) == LUA_TSTRING) {
		return luau_module::to_string_name(L, p_index);
	} else {
		return p_default;
	}
}

double LuaState::enforce_number(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), 0.0, "Lua state is invalid. Cannot enforce number.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), 0.0, vformat("LuaState.enforce_number(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	return luaL_checknumber(L, p_index);
}

double LuaState::opt_number(int p_index, double p_default) {
	ERR_FAIL_COND_V_MSG(!is_valid(), p_default, "Lua state is invalid. Cannot get optional number.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), p_default, vformat("LuaState.opt_number(%d, %f): Invalid stack index. Stack has %d elements.", p_index, p_default, lua_gettop(L)));

	return luaL_optnumber(L, p_index, p_default);
}

bool LuaState::enforce_boolean(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot enforce boolean.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), false, vformat("LuaState.enforce_boolean(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	return luaL_checkboolean(L, p_index);
}

bool LuaState::opt_boolean(int p_index, bool p_default) {
	ERR_FAIL_COND_V_MSG(!is_valid(), p_default, "Lua state is invalid. Cannot get optional boolean.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), p_default, vformat("LuaState.opt_boolean(%d, %s): Invalid stack index. Stack has %d elements.", p_index, p_default ? "true" : "false", lua_gettop(L)));

	return luaL_optboolean(L, p_index, p_default);
}

int LuaState::enforce_integer(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), 0, "Lua state is invalid. Cannot enforce integer.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), 0, vformat("LuaState.enforce_integer(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	return luaL_checkinteger(L, p_index);
}

int LuaState::opt_integer(int p_index, int p_default) {
	ERR_FAIL_COND_V_MSG(!is_valid(), p_default, "Lua state is invalid. Cannot get optional integer.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), p_default, vformat("LuaState.opt_integer(%d, %d): Invalid stack index. Stack has %d elements.", p_index, p_default, lua_gettop(L)));

	return luaL_optinteger(L, p_index, p_default);
}

Vector3 LuaState::enforce_vector3(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), Vector3(), "Lua state is invalid. Cannot enforce Vector3.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), Vector3(), vformat("LuaState.enforce_vector3(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	const float *vec = luaL_checkvector(L, p_index);
	return Vector3(vec[0], vec[1], vec[2]);
}

Vector3 LuaState::opt_vector3(int p_index, const Vector3 &p_default) {
	ERR_FAIL_COND_V_MSG(!is_valid(), p_default, "Lua state is invalid. Cannot get optional Vector3.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), p_default, vformat("LuaState.opt_vector3(%d, %s): Invalid stack index. Stack has %d elements.", p_index, p_default, lua_gettop(L)));

	const float *vec = luaL_optvector(L, p_index, NULL);
	if (vec) {
		return Vector3(vec[0], vec[1], vec[2]);
	} else {
		return p_default;
	}
}

void LuaState::enforce_stack(int p_size, const String &p_message) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot enforce stack size.");
	luaL_checkstack(L, p_size, p_message.utf8().get_data());
}

void LuaState::enforce_type(int p_index, ValueType p_type) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot enforce type.");
	ERR_FAIL_COND_MSG(!is_valid_index(p_index), vformat("LuaState.enforce_type(%d, %d): Invalid stack index. Stack has %d elements.", p_index, static_cast<int>(p_type), lua_gettop(L)));

	luaL_checktype(L, p_index, static_cast<int>(p_type));
}

void LuaState::enforce_any(int p_index) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot enforce any type.");
	ERR_FAIL_COND_MSG(!is_valid_index(p_index), vformat("LuaState.enforce_any(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	luaL_checkany(L, p_index);
}

bool LuaState::new_metatable_named(const StringName &p_name) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot create new metatable.");
	return luaL_newmetatable(L, char_string(p_name).get_data()) != 0;
}

LuaState::ValueType LuaState::get_metatable_named(const StringName &p_name) {
	ERR_FAIL_COND_V_MSG(!is_valid(), static_cast<ValueType>(LUA_TNONE), "Lua state is invalid. Cannot get metatable.");
	ERR_FAIL_COND_V_MSG(!lua_checkstack(L, 1), static_cast<ValueType>(LUA_TNONE), vformat("LuaState.get_metatable_named(\"%s\"): Stack overflow. Cannot grow stack.", p_name));

	return static_cast<ValueType>(luaL_getmetatable(L, char_string(p_name).get_data()));
}

Object *LuaState::enforce_full_userdata(int p_index, const StringName &p_name) {
	ERR_FAIL_COND_V_MSG(!is_valid(), nullptr, "Lua state is invalid. Cannot enforce userdata.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), nullptr, vformat("LuaState.enforce_userdata(%d, \"%s\"): Invalid stack index. Stack has %d elements.", p_index, p_name, lua_gettop(L)));

	void *ud = luaL_checkudata(L, p_index, char_string(p_name).get_data());
	return static_cast<Object *>(ud);
}

PackedByteArray LuaState::enforce_buffer(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), PackedByteArray(), "Lua state is invalid. Cannot enforce buffer.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), PackedByteArray(), vformat("LuaState.enforce_buffer(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	size_t size;
	void *buf = luaL_checkbuffer(L, p_index, &size);

	PackedByteArray byte_array;
	byte_array.resize(size);
	memcpy(byte_array.ptrw(), buf, size);

	return byte_array;
}

void LuaState::print_where(int p_level) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot print where.");
	luaL_where(L, p_level);
}

int LuaState::enforce_option(int p_index, const PackedStringArray &p_options, const String &p_default) {
	ERR_FAIL_COND_V_MSG(!is_valid(), -1, "Lua state is invalid. Cannot enforce option.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), -1, vformat("LuaState.enforce_option(%d, [...], \"%s\"): Invalid stack index. Stack has %d elements.", p_index, p_default, lua_gettop(L)));

	const String &name = p_default.is_empty() ? enforce_string_inplace(p_index) : opt_string_inplace(p_index, p_default);
	for (int i = 0; i < p_options.size(); i++) {
		if (p_options[i] == name) {
			return i;
		}
	}

	arg_error(p_index, vformat("invalid option '%s'", name));
	return -1;
}

String LuaState::push_as_string(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), String(), "Lua state is invalid. Cannot push as string.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), String(), vformat("LuaState.push_as_string(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));
	ERR_FAIL_COND_V_MSG(!lua_checkstack(L, 1), String(), vformat("LuaState.push_as_string(%d): Stack overflow. Cannot grow stack.", p_index));

	size_t len;
	const char *str = luaL_tolstring(L, p_index, &len);
	if (str) {
		return String::utf8(str, len);
	} else {
		return String();
	}
}

StringName LuaState::type_name_for_value(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), StringName(), "Lua state is invalid. Cannot get type name.");
	ERR_FAIL_COND_V_MSG(!is_valid_index(p_index), StringName(), vformat("LuaState.type_name_for_value(%d): Invalid stack index. Stack has %d elements.", p_index, lua_gettop(L)));

	const char *name = luaL_typename(L, p_index);
	return StringName(name);
}

void LuaState::open_libs(BitField<LuaState::LibraryFlags> libs) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot open libraries.");

	if (!is_main_thread()) {
		WARN_PRINT("LuaState.open_libs() called on a non-main Lua thread, which may affect other threads (depending on fenv and sandboxing).");
	}

	if (libs & LIB_BASE) {
		open_library(luaopen_base, "");
	}
	if (libs & LIB_COROUTINE) {
		open_library(luaopen_coroutine, LUA_COLIBNAME);
	}
	if (libs & LIB_TABLE) {
		open_library(luaopen_table, LUA_TABLIBNAME);
	}
	if (libs & LIB_OS) {
		open_library(luaopen_os, LUA_OSLIBNAME);
	}
	if (libs & LIB_STRING) {
		open_library(luaopen_string, LUA_STRLIBNAME);
	}
	if (libs & LIB_BIT32) {
		open_library(luaopen_bit32, LUA_BITLIBNAME);
	}
	if (libs & LIB_BUFFER) {
		open_library(luaopen_buffer, LUA_BUFFERLIBNAME);
	}
	if (libs & LIB_UTF8) {
		open_library(luaopen_utf8, LUA_UTF8LIBNAME);
	}
	if (libs & LIB_MATH) {
		open_library(luaopen_math, LUA_MATHLIBNAME);
	}
	if (libs & LIB_DEBUG) {
		open_library(luaopen_debug, LUA_DBLIBNAME);
	}
	if (libs & LIB_VECTOR) {
		open_library(luaopen_vector, LUA_VECLIBNAME);
	}
	if (libs & LIB_BLAZIUM) {
		open_library(luaopen_blazium, LUA_BLAZIUMLIBNAME);
		LuauPackagePath::install_package_searchers(Ref<LuaState>(this));
		if (LuauScriptLanguage *lang = LuauScriptLanguage::get_singleton()) {
			lang->push_named_globals_to_state(L);
		}
	}
}

void LuaState::sandbox() {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot sandbox.");
	ERR_FAIL_COND_MSG(!is_main_thread(), "LuaState.sandbox() should only be called on the main thread.");
	luaL_sandbox(L);
}

void LuaState::sandbox_thread() {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot sandbox thread.");
	ERR_FAIL_COND_MSG(is_main_thread(), "LuaState.sandbox_thread() should only be called on Lua threads, not the main thread.");

	luaL_sandboxthread(L);
}

bool LuaState::is_array(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot check if value is array.");
	return luau_module::is_array(L, p_index);
}

bool LuaState::is_object(int p_index, int p_tag) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot check if value is array.");
	return luau_module::is_object(L, p_index, p_tag);
}

Array LuaState::to_array(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), Array(), "Lua state is invalid. Cannot convert to Array.");
	return luau_module::to_array(L, p_index);
}

Callable LuaState::to_callable(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), Callable(), "Lua state is invalid. Cannot convert to Callable.");
	return luau_module::to_callable(L, p_index);
}

Dictionary LuaState::to_dictionary(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), Dictionary(), "Lua state is invalid. Cannot convert to Dictionary.");
	return luau_module::to_dictionary(L, p_index);
}

Variant LuaState::to_variant(int p_index) {
	ERR_FAIL_COND_V_MSG(!is_valid(), Variant(), "Lua state is invalid. Cannot convert to Variant.");
	return luau_module::to_variant(L, p_index);
}

void LuaState::push_array(const Array &p_arr) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot push Array.");
	luau_module::push_array(L, p_arr);
}

void LuaState::push_callable(const Callable &p_callable) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot push Callable.");
	luau_module::push_callable(L, p_callable);
}

void LuaState::push_dictionary(const Dictionary &p_dict) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot push Dictionary.");
	luau_module::push_dictionary(L, p_dict);
}

void LuaState::push_variant(const Variant &p_value) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot push Variant.");
	luau_module::push_variant(L, p_value);
}

void LuaState::push_default_object_metatable() {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot push object metatable.");
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 1), "LuaState.push_object_metatable(): Stack overflow. Cannot grow stack.");
	push_object_metatable(L);
}

bool LuaState::load_string(const String &p_code, const String &p_chunk_name, int p_env) {
	ERR_FAIL_COND_V_MSG(!is_valid(), false, "Lua state is invalid. Cannot load string.");
	return load_bytecode(Luau::compile(p_code), p_chunk_name, p_env);
}

LuaState::Status LuaState::do_string(const String &p_code, const String &p_chunk_name, int p_env, int p_nargs, int p_nresults, int p_errfunc) {
	if (load_string(p_code, p_chunk_name.is_empty() ? p_code : p_chunk_name, p_env)) {
		return pcall(p_nargs, p_nresults, p_errfunc);
	} else {
		const char *err_msg = lua_tostring(L, -1);
		ERR_PRINT(vformat("Failed to load Lua chunk \"%s\": %s", p_chunk_name, err_msg));
		return static_cast<Status>(LUA_ERRSYNTAX);
	}
}

Callable LuaState::bind_callable(const Callable &p_callable) {
	ERR_FAIL_COND_V_MSG(!p_callable.is_valid(), Callable(), "LuaState.bind_callable(): Callable is invalid.");

	LuaStateBoundCallable *bound = memnew(LuaStateBoundCallable(p_callable));
	return Callable(bound);
}

void LuaState::set_call_metamethod(int p_metatable_index, const Callable &p_callable) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot set metamethod.");
	ERR_FAIL_COND_MSG(!is_valid_index(p_metatable_index), vformat("LuaState.set_call_metamethod(%d): Invalid stack index. Stack has %d elements.", p_metatable_index, lua_gettop(L)));
	ERR_FAIL_COND_MSG(!lua_istable(L, p_metatable_index), vformat("LuaState.set_call_metamethod(%d): Index is not a table.", p_metatable_index));
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 1), vformat("LuaState.set_call_metamethod(%d): Stack overflow. Cannot grow stack.", p_metatable_index));

	int abs_index = lua_absindex(L, p_metatable_index);

	push_callable(p_callable);
	lua_pushcclosure(L, callable_metamethod_wrapper, "LuaState.callable_metamethod_wrapper.__call", 1);
	lua_rawsetfield(L, abs_index, "__call");
}

void LuaState::set_index_metamethod(int p_metatable_index, const Callable &p_callable) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot set metamethod.");
	ERR_FAIL_COND_MSG(!is_valid_index(p_metatable_index), vformat("LuaState.set_index_metamethod(%d): Invalid stack index. Stack has %d elements.", p_metatable_index, lua_gettop(L)));
	ERR_FAIL_COND_MSG(!lua_istable(L, p_metatable_index), vformat("LuaState.set_index_metamethod(%d): Index is not a table.", p_metatable_index));
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 1), vformat("LuaState.set_index_metamethod(%d): Stack overflow. Cannot grow stack.", p_metatable_index));

	int abs_index = lua_absindex(L, p_metatable_index);

	push_callable(p_callable);
	lua_pushcclosure(L, callable_metamethod_wrapper, "LuaState.callable_metamethod_wrapper.__index", 1);
	lua_rawsetfield(L, abs_index, "__index");
}

void LuaState::set_newindex_metamethod(int p_metatable_index, const Callable &p_callable) {
	ERR_FAIL_COND_MSG(!is_valid(), "Lua state is invalid. Cannot set metamethod.");
	ERR_FAIL_COND_MSG(!is_valid_index(p_metatable_index), vformat("LuaState.set_newindex_metamethod(%d): Invalid stack index. Stack has %d elements.", p_metatable_index, lua_gettop(L)));
	ERR_FAIL_COND_MSG(!lua_istable(L, p_metatable_index), vformat("LuaState.set_newindex_metamethod(%d): Index is not a table.", p_metatable_index));
	ERR_FAIL_COND_MSG(!lua_checkstack(L, 1), vformat("LuaState.set_newindex_metamethod(%d): Stack overflow. Cannot grow stack.", p_metatable_index));

	int abs_index = lua_absindex(L, p_metatable_index);

	push_callable(p_callable);
	lua_pushcclosure(L, callable_metamethod_wrapper, "LuaState.callable_metamethod_wrapper.__newindex", 1);
	lua_rawsetfield(L, abs_index, "__newindex");
}

Ref<LuaState> LuaState::find_or_create_lua_state(lua_State *p_L) {
	Ref<LuaState> state;
	state.reference_ptr(LuaState::find_lua_state(p_L));

	if (!state.is_valid()) {
		lua_State *main_thread_L = lua_mainthread(p_L);
		if (p_L == main_thread_L) {
			state.reference_ptr(memnew(LuaState(p_L)));
		} else {
			Ref<LuaState> main_thread_state = LuaState::find_or_create_lua_state(main_thread_L);
			state.reference_ptr(memnew(LuaState(p_L, main_thread_state)));
		}
	}

	return state;
}

void LuaState::open_library(lua_CFunction func, const char *name) {
	lua_pushcfunction(L, func, name);
	lua_pushstring(L, name);
	lua_call(L, 1, 0);
}
