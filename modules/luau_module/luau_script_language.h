/**************************************************************************/
/*  luau_script_language.h                                                */
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

#include "core/object/script_language.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/templates/self_list.h"
#include "debugger/luau_script_debugger.h"
#include "lua_state.h"
#include "luau_parser_pool.h"
#include "luau_script.h"
#include "scheduler/luau_task_scheduler.h"

class LuauScriptLanguage : public ScriptLanguage {
	GDCLASS(LuauScriptLanguage, ScriptLanguage);

	static LuauScriptLanguage *singleton;

	Ref<luau_module::LuaState> load_state;
	mutable Mutex mutex;
	SelfList<LuauScript>::List script_list;
	HashMap<StringName, Variant> named_globals;
	luau_module::LuauTaskScheduler task_scheduler;
	luau_module::LuauScriptDebugger debugger;
	LuauParserPool parser_pool;

#ifdef DEBUG_ENABLED
	struct NativeProfileEntry {
		StringName signature;
		uint64_t call_count = 0;
		uint64_t total_time = 0;
		uint64_t frame_call_count = 0;
		uint64_t frame_total_time = 0;
	};

	struct ProfileEntry {
		StringName signature;
		uint64_t call_count = 0;
		uint64_t total_time = 0;
		uint64_t self_time = 0;
		uint64_t frame_call_count = 0;
		uint64_t frame_total_time = 0;
		uint64_t frame_self_time = 0;
		HashMap<StringName, NativeProfileEntry> native_calls;
		HashMap<StringName, NativeProfileEntry> frame_native_calls;
	};

	HashMap<StringName, ProfileEntry> profile_accumulated;
	HashMap<StringName, ProfileEntry> profile_frame;
	bool profiling_active = false;
	bool profile_native_calls = false;
	StringName profile_context_signature;
#endif

public:
	void ensure_load_state();
	void push_named_globals_to_state(lua_State *p_L) const;
	void add_named_global_constant(const StringName &p_name, const Variant &p_value) override;
	void remove_named_global_constant(const StringName &p_name) override;

protected:
	static void _bind_methods();

	Array _get_built_in_templates_bind(const StringName &p_object) const;

public:
	static LuauScriptLanguage *get_singleton() { return singleton; }

	Mutex &get_mutex() { return mutex; }
	SelfList<LuauScript>::List &get_script_list() { return script_list; }
	Ref<luau_module::LuaState> get_load_state() const {
		const_cast<LuauScriptLanguage *>(this)->ensure_load_state();
		return load_state;
	}
	luau_module::LuauTaskScheduler &get_task_scheduler() { return task_scheduler; }
	luau_module::LuauScriptDebugger &get_debugger() { return debugger; }
	const luau_module::LuauScriptDebugger &get_debugger() const { return debugger; }
	LuauParserPool &get_parser_pool() { return parser_pool; }
	bool debug_should_break_at(const String &p_source, int p_line) const;

#ifdef DEBUG_ENABLED
	bool is_profiling_active() const { return profiling_active; }
	bool is_profile_native_calls_enabled() const { return profile_native_calls; }
	void profile_record_call(const StringName &p_signature, uint64_t p_time_usec);
	void profile_push_context(const StringName &p_signature);
	void profile_pop_context();
	void profile_record_native_call(const StringName &p_native_signature, uint64_t p_time_usec);
#endif

	virtual String get_name() const override;
	virtual void init() override;
	virtual String get_type() const override;
	virtual String get_extension() const override;
	virtual void finish() override;

	virtual Vector<String> get_reserved_words() const override;
	virtual bool is_control_flow_keyword(const String &p_string) const override;
	virtual Vector<String> get_comment_delimiters() const override;
	virtual Vector<String> get_doc_comment_delimiters() const override;
	virtual Vector<String> get_string_delimiters() const override;

	virtual Ref<Script> make_template(const String &p_template, const String &p_class_name, const String &p_base_class_name) const override;
	virtual Vector<ScriptTemplate> get_built_in_templates(const StringName &p_object) override;
	virtual bool is_using_templates() override;
	virtual bool validate(const String &p_script, const String &p_path = "", List<String> *r_functions = nullptr, List<ScriptError> *r_errors = nullptr, List<Warning> *r_warnings = nullptr, HashSet<int> *r_safe_lines = nullptr) const override;
	virtual Error complete_code(const String &p_code, const String &p_path, Object *p_owner, List<CodeCompletionOption> *r_options, bool &r_force, String &r_call_hint) override;
	virtual Error lookup_code(const String &p_code, const String &p_symbol, const String &p_path, Object *p_owner, LookupResult &r_result) override;

	virtual bool supports_builtin_mode() const override;
	virtual bool supports_documentation() const override;
	virtual bool can_inherit_from_file() const override;
	virtual int find_function(const String &p_function, const String &p_code) const override;
	virtual String make_function(const String &p_class, const String &p_name, const PackedStringArray &p_args) const override;

	virtual void auto_indent_code(String &p_code, int p_from_line, int p_to_line) const override;
#ifdef TOOLS_ENABLED
	virtual Error open_in_external_editor(const Ref<Script> &p_script, int p_line, int p_col) override;
#endif
	virtual void add_global_constant(const StringName &p_variable, const Variant &p_value) override;
	virtual void frame() override;

	virtual String debug_get_error() const override;
	virtual int debug_get_stack_level_count() const override;
	virtual int debug_get_stack_level_line(int p_level) const override;
	virtual String debug_get_stack_level_function(int p_level) const override;
	virtual String debug_get_stack_level_source(int p_level) const override;
	virtual void debug_get_stack_level_locals(int p_level, List<String> *p_locals, List<Variant> *p_values, int p_max_subitems = -1, int p_max_depth = -1) override;
	virtual void debug_get_stack_level_members(int p_level, List<String> *p_members, List<Variant> *p_values, int p_max_subitems = -1, int p_max_depth = -1) override;
	virtual ScriptInstance *debug_get_stack_level_instance(int p_level) override;
	virtual void debug_get_globals(List<String> *p_globals, List<Variant> *p_values, int p_max_subitems = -1, int p_max_depth = -1) override;
	virtual String debug_parse_stack_level_expression(int p_level, const String &p_expression, int p_max_subitems = -1, int p_max_depth = -1) override;

	virtual void reload_all_scripts() override;
	virtual void reload_scripts(const Array &p_scripts, bool p_soft_reload) override;
	virtual void reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) override;

	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual void get_public_functions(List<MethodInfo> *p_functions) const override;
	virtual void get_public_constants(List<Pair<String, Variant>> *p_constants) const override;
	virtual void get_public_annotations(List<MethodInfo> *p_annotations) const override;

	virtual void profiling_start() override;
	virtual void profiling_stop() override;
	virtual void profiling_set_save_native_calls(bool p_enable) override;
	virtual int profiling_get_accumulated_data(ProfilingInfo *p_info_arr, int p_info_max) override;
	virtual int profiling_get_frame_data(ProfilingInfo *p_info_arr, int p_info_max) override;

	virtual bool handles_global_class_type(const String &p_type) const override;
	virtual String get_global_class_name(const String &p_path, String *r_base_type = nullptr, String *r_icon_path = nullptr, bool *r_is_abstract = nullptr, bool *r_is_tool = nullptr) const override;

	Dictionary validate_script(const String &p_script, const String &p_path, const Variant &p_functions = Variant()) const;
	Array get_public_constants_bind() const;

	LuauScriptLanguage();
	~LuauScriptLanguage();
};
