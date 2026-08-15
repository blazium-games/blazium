/**************************************************************************/
/*  lua_state.h                                                           */
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
#include "core/variant/type_info.h"
#include <lua.h>

#include "helpers.h"

namespace luau_module {

class LuaDebug;

class LuaState : public RefCounted {
	GDCLASS(LuaState, RefCounted)

private:
	lua_State *L;
	Ref<LuaState> main_thread;

	LuaState(lua_State *p_L);

	LuaState(lua_State *p_thread_L, const Ref<LuaState> &p_main_thread);

	Ref<LuaState> bind_thread(lua_State *p_thread_L);
	void setup_vm();

	bool is_valid_index(int p_index);

protected:
	static void _bind_methods();

public:
	enum LibraryFlags {
		LIB_BASE = 1 << 0,
		LIB_COROUTINE = 1 << 1,
		LIB_TABLE = 1 << 2,
		LIB_OS = 1 << 3,
		LIB_STRING = 1 << 4,
		LIB_BIT32 = 1 << 5,
		LIB_BUFFER = 1 << 6,
		LIB_UTF8 = 1 << 7,
		LIB_MATH = 1 << 8,
		LIB_DEBUG = 1 << 9,
		LIB_VECTOR = 1 << 10,
		LIB_BLAZIUM = 1 << 11,
		LIB_ALL = LIB_BASE | LIB_COROUTINE | LIB_TABLE | LIB_OS | LIB_STRING |
				LIB_BIT32 | LIB_BUFFER | LIB_UTF8 | LIB_MATH | LIB_DEBUG | LIB_VECTOR | LIB_BLAZIUM
	};

	// Godot-facing enum wrappers (avoid exposing raw Luau C API type names to bindings).
	enum ValueType {
		VALUE_NONE = -1,
		VALUE_NIL = 0,
		VALUE_BOOLEAN = 1,
		VALUE_LIGHTUSERDATA,
		VALUE_NUMBER,
		VALUE_VECTOR,
		VALUE_STRING,
		VALUE_TABLE,
		VALUE_FUNCTION,
		VALUE_USERDATA,
		VALUE_THREAD,
		VALUE_BUFFER,
		VALUE_PROTO,
		VALUE_UPVAL,
		VALUE_DEADKEY,
	};

	enum Status {
		STATUS_OK = 0,
		STATUS_YIELD,
		STATUS_ERRRUN,
		STATUS_ERRSYNTAX,
		STATUS_ERRMEM,
		STATUS_ERRERR,
		STATUS_BREAK,
	};

	enum CoStatus {
		CO_RUN = 0,
		CO_SUSPENDED,
		CO_NORMAL,
		CO_FINISHED,
		CO_ERROR,
	};

	enum GCOp {
		GC_STOP,
		GC_RESTART,
		GC_COLLECT,
		GC_COUNT,
		GC_COUNTB,
		GC_ISRUNNING,
		GC_STEP,
		GC_SETGOAL,
		GC_SETSTEPMUL,
		GC_SETSTEPSIZE,
	};

	LuaState();
	~LuaState();

	bool is_valid() const {
		if (!L) {
			return false;
		} else if (!is_main_thread()) {
			return main_thread->L != nullptr;
		} else {
			return true;
		}
	}

	bool is_main_thread() const {
		return !main_thread.is_valid();
	}

	void close();
	Ref<LuaState> new_thread();
	Ref<LuaState> get_main_thread() {
		return is_main_thread() ? Ref<LuaState>(this) : main_thread;
	}
	void reset_thread();
	bool is_thread_reset();

	int abs_index(int p_index);
	int get_top();
	void set_top(int p_index);
	void pop(int p_count);
	void push_value(int p_index);
	void remove(int p_index);
	void insert(int p_index);
	void replace(int p_index);
	bool check_stack(int p_size);
	void raw_check_stack(int p_size);

	void xmove(LuaState *p_to_state, int p_count);
	void xpush(LuaState *p_to_state, int p_index);

	bool is_number(int p_index);
	bool is_string(int p_index);
	bool is_c_function(int p_index);
	bool is_lua_function(int p_index);
	bool is_userdata(int p_index);
	ValueType type(int p_index);
	bool is_function(int p_index);
	bool is_table(int p_index);
	bool is_full_userdata(int p_index);
	bool is_light_userdata(int p_index);
	bool is_nil(int p_index);
	bool is_boolean(int p_index);
	bool is_vector(int p_index);
	bool is_thread(int p_index);
	bool is_buffer(int p_index);
	bool is_none(int p_index);
	bool is_none_or_nil(int p_index);
	StringName type_name(ValueType p_type);

	bool equal(int p_index1, int p_index2);
	bool raw_equal(int p_index1, int p_index2);
	bool less_than(int p_index1, int p_index2);

	double to_number(int p_index);
	int to_integer(int p_index);
	Vector3 to_vector3(int p_index);
	bool to_boolean(int p_index);
	String to_string_inplace(int p_index);
	StringName to_string_name(int p_index);
	StringName get_namecall();
	int obj_len(int p_index);
	Object *to_light_userdata(int p_index, int p_tag = LUA_NOTAG);
	Object *to_full_userdata(int p_index, int p_tag = LUA_NOTAG);
	Object *to_object(int p_index, int p_tag = LUA_NOTAG);
	int light_userdata_tag(int p_index);
	int full_userdata_tag(int p_index);
	Ref<LuaState> to_thread(int p_index);
	PackedByteArray to_buffer(int p_index);
	uint64_t to_pointer(int p_index);

	void push_nil();
	void push_number(double p_num);
	void push_integer(int p_num);
	void push_vector3(const Vector3 &p_vec);
	void push_string(const String &p_str);
	void push_string_name(const StringName &p_string_name);
	void push_boolean(bool p_b);
	bool push_thread();
	void push_light_userdata(Object *p_obj, int p_tag = LUA_NOTAG);
	void push_full_userdata(Object *p_obj, int p_tag = LUA_NOTAG);
	void push_object(Object *p_obj, int p_tag = LUA_NOTAG);

	ValueType get_table(int p_index);
	ValueType get_field(int p_index, const StringName &p_key);
	ValueType get_global(const StringName &p_key);
	ValueType raw_get_field(int p_index, const StringName &p_key);
	ValueType raw_get(int p_index);
	ValueType raw_geti(int p_stack_index, int p_table_index);
	void create_table(int p_narr = 0, int p_nrec = 0);

	void set_read_only(int p_index, bool p_enabled);
	bool get_read_only(int p_index);
	void set_safe_env(int p_index, bool p_enabled);

	bool get_metatable(int p_index);
	void get_fenv(int p_index);

	void set_table(int p_index);
	void set_field(int p_index, const StringName &p_key);
	void set_global(const StringName &p_key);
	void raw_set_field(int p_index, const StringName &p_key);
	void raw_set(int p_index);
	void raw_seti(int p_stack_index, int p_table_index);
	void set_metatable(int p_index);
	bool set_fenv(int p_index);

	bool load_bytecode(const PackedByteArray &p_bytecode, const String &p_chunk_name, int p_env = 0);
	Status pcall(int p_nargs, int p_nresults, int p_errfunc = 0);
	Status cpcall(Callable p_callable);

	void yield(int p_nresults);
	void break_execution();
	Status resume(int p_narg = 0, LuaState *p_from = nullptr);
	Status resume_error(LuaState *p_from = nullptr);
	Status status();
	bool is_yieldable();
	CoStatus co_status(LuaState *p_co);

	int gc(GCOp p_what, int p_data);

	void set_memory_category(int p_category);
	uint64_t get_total_bytes(int p_category);

	void error();

	bool next(int p_index);
	int raw_iter(int p_index, int p_iter);

	void concat(int p_count);

	void set_full_userdata_tag(int p_index, int p_tag);
	void set_full_userdata_metatable(int p_tag);
	void get_full_userdata_metatable(int p_tag);

	void set_light_userdata_name(int p_tag, const StringName &p_name);
	StringName get_light_userdata_name(int p_tag);

	void clone_function(int p_index);

	void clear_table(int p_index);
	void clone_table(int p_index);

	int ref(int p_index);
	void get_ref(int p_ref);
	void unref(int p_ref);

	int get_stack_depth();
	Ref<LuaDebug> get_info(int p_level, const String &p_what);
	bool get_argument(int p_level, int p_narg);
	StringName get_local(int p_level, int p_nlocal);
	StringName set_local(int p_level, int p_nlocal);
	StringName get_upvalue(int p_funcindex, int p_nupvalue);
	StringName set_upvalue(int p_funcindex, int p_nupvalue);
	void set_single_step(bool p_enabled);
	void set_interrupts(bool p_enabled);
	int set_breakpoint(int p_funcindex, int p_nline, bool p_enabled);
	void get_coverage(int p_funcindex, Callable p_callback);
	String debug_trace();

	void register_library(const StringName &p_lib_name, const Dictionary &p_functions);
	bool get_meta_field(int p_index, const StringName &p_field);
	bool call_meta(int p_index, const StringName &p_field);
	void type_error(int p_index, const StringName &p_expected);
	void arg_error(int p_index, const String &p_message);
	String enforce_string_inplace(int p_index);
	String opt_string_inplace(int p_index, const String &p_default);
	StringName enforce_string_name(int p_index);
	StringName opt_string_name(int p_index, const StringName &p_default);
	double enforce_number(int p_index);
	double opt_number(int p_index, double p_default);
	bool enforce_boolean(int p_index);
	bool opt_boolean(int p_index, bool p_default);
	int enforce_integer(int p_index);
	int opt_integer(int p_index, int p_default);
	Vector3 enforce_vector3(int p_index);
	Vector3 opt_vector3(int p_index, const Vector3 &p_default);
	void enforce_stack(int p_size, const String &p_message);
	void enforce_type(int p_index, ValueType p_type);
	void enforce_any(int p_index);
	bool new_metatable_named(const StringName &p_tname);
	ValueType get_metatable_named(const StringName &p_tname);
	Object *enforce_full_userdata(int p_index, const StringName &p_tname);
	PackedByteArray enforce_buffer(int p_index);
	void print_where(int p_level);
	int enforce_option(int p_index, const PackedStringArray &p_options, const String &p_default = String());
	String push_as_string(int p_index);
	StringName type_name_for_value(int p_index);

	void open_libs(BitField<LibraryFlags> p_libs = LIB_ALL);
	void sandbox();
	void sandbox_thread();

	bool is_array(int p_index);
	bool is_object(int p_index, int p_tag = LUA_NOTAG);
	Array to_array(int p_index);
	Callable to_callable(int p_index);
	Dictionary to_dictionary(int p_index);
	Variant to_variant(int p_index);
	void push_array(const Array &p_arr);
	void push_callable(const Callable &p_callable);
	void push_dictionary(const Dictionary &p_dict);
	void push_variant(const Variant &p_value);
	void push_default_object_metatable();

	bool load_string(const String &p_code, const String &p_chunk_name, int p_env = 0);
	Status do_string(const String &p_code, const String &p_chunk_name = String(), int p_env = 0, int p_nargs = 0, int p_nresults = LUA_MULTRET, int p_errfunc = 0);
	static Callable bind_callable(const Callable &p_callable);
	void set_call_metamethod(int p_metatable_index, const Callable &p_callable);
	void set_index_metamethod(int p_metatable_index, const Callable &p_callable);
	void set_newindex_metamethod(int p_metatable_index, const Callable &p_callable);

	lua_State *get_lua_state() {
		return L;
	}

	static LuaState *find_lua_state(lua_State *p_L) {
		return static_cast<LuaState *>(lua_getthreaddata(p_L));
	}

	static Ref<LuaState> find_or_create_lua_state(lua_State *p_L);

	void open_library(lua_CFunction p_func, const char *p_name);
};
} //namespace luau_module

VARIANT_BITFIELD_CAST(luau_module::LuaState::LibraryFlags);
