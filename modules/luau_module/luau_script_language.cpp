/**************************************************************************/
/*  luau_script_language.cpp                                              */
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

#include "luau_script_language.h"

#include "analysis/luau_analysis.h"
#include "analysis/luau_typecheck.h"
#include "bindings/object.h"
#include "bindings/variant.h"
#include "editor/luau_completion.h"
#include "lua_blazium_classes.h"
#include "lua_state.h"
#include "luau.h"
#include "luau_class_info.h"
#include "luau_compile_result.h"
#include "luau_script_instance.h"
#include "require/luau_package_path.h"
#include "scheduler/luau_task_scheduler.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_settings.h"
#include "editor/luau_formatter.h"
#endif
#include "core/config/project_settings.h"
#include "core/error/error_macros.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/math/expression.h"
#include "core/object/script_language.h"
#include "core/object/script_server.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "core/variant/variant_utility.h"
#include "main/performance.h"
#include <lualib.h>

LuauScriptLanguage *LuauScriptLanguage::singleton = nullptr;

void LuauScriptLanguage::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_built_in_templates", "object"), &LuauScriptLanguage::_get_built_in_templates_bind);
	ClassDB::bind_method(D_METHOD("add_global_constant", "variable", "value"), &LuauScriptLanguage::add_global_constant);
	ClassDB::bind_method(D_METHOD("debug_should_break_at", "source", "line"), &LuauScriptLanguage::debug_should_break_at);
	ClassDB::bind_static_method("LuauScriptLanguage", D_METHOD("get_singleton"), &LuauScriptLanguage::get_singleton);
}

Array LuauScriptLanguage::_get_built_in_templates_bind(const StringName &p_object) const {
	const Vector<ScriptTemplate> templates = const_cast<LuauScriptLanguage *>(this)->get_built_in_templates(p_object);
	Array result;
	for (const ScriptTemplate &entry : templates) {
		Dictionary dict;
		dict["inherit"] = entry.inherit;
		dict["name"] = entry.name;
		dict["description"] = entry.description;
		dict["content"] = entry.content;
		dict["id"] = entry.id;
		dict["origin"] = entry.origin;
		result.push_back(dict);
	}
	return result;
}

String LuauScriptLanguage::get_name() const {
	return "Luau";
}

void LuauScriptLanguage::ensure_load_state() {
	if (load_state.is_null()) {
		load_state.instantiate();
	}
}

void LuauScriptLanguage::init() {
	luau_module::LuauPackagePath::register_project_settings();
	ensure_load_state();
}

String LuauScriptLanguage::get_type() const {
	return "LuauScript";
}

String LuauScriptLanguage::get_extension() const {
	return "luau";
}

void LuauScriptLanguage::finish() {
	if (load_state.is_valid()) {
		load_state->close();
		load_state.unref();
	}
}

void LuauScriptLanguage::get_reserved_words(List<String> *p_words) const {
	static const char *words[] = {
		"and",
		"break",
		"do",
		"else",
		"elseif",
		"end",
		"false",
		"for",
		"function",
		"if",
		"in",
		"local",
		"nil",
		"not",
		"or",
		"repeat",
		"return",
		"then",
		"true",
		"until",
		"while",
		"continue",
		"type",
		"export",
		nullptr,
	};
	for (const char **w = words; *w; w++) {
		p_words->push_back(*w);
	}
}

bool LuauScriptLanguage::is_control_flow_keyword(const String &p_string) const {
	return p_string == "break" || p_string == "do" || p_string == "else" || p_string == "elseif" ||
			p_string == "end" || p_string == "for" || p_string == "if" || p_string == "repeat" ||
			p_string == "return" || p_string == "then" || p_string == "until" || p_string == "while";
}

void LuauScriptLanguage::get_comment_delimiters(List<String> *p_delimiters) const {
	p_delimiters->push_back("--");
	p_delimiters->push_back("--[[ ]]");
}

void LuauScriptLanguage::get_doc_comment_delimiters(List<String> *p_delimiters) const {
	p_delimiters->push_back("---");
}

void LuauScriptLanguage::get_string_delimiters(List<String> *p_delimiters) const {
	p_delimiters->push_back("\" \"");
	p_delimiters->push_back("' '");
	p_delimiters->push_back("[[ ]]");
}

Ref<Script> LuauScriptLanguage::make_template(const String &p_template, const String &p_class_name, const String &p_base_class_name) const {
	Ref<LuauScript> scr;
	scr.instantiate();
	String processed = p_template;
	processed = processed.replace("_BASE_", p_base_class_name);
	processed = processed.replace("_CLASS_", p_class_name);
	scr->set_source_code(processed);
	return scr;
}

Vector<ScriptLanguage::ScriptTemplate> LuauScriptLanguage::get_built_in_templates(const StringName &p_object) {
	Vector<ScriptTemplate> templates;

	ScriptTemplate hybrid;
	hybrid.inherit = p_object;
	hybrid.name = "Hybrid DSL";
	hybrid.description = "Luau hybrid table class definition";
	hybrid.content =
			"local _CLASS_ = {\n"
			"\textends = \"_BASE_\",\n"
			"\tclass_name = \"_CLASS_\",\n"
			"\n"
			"\tfunction _CLASS_:_ready()\n"
			"\tend,\n"
			"\n"
			"\tfunction _CLASS_:_process(delta)\n"
			"\tend,\n"
			"}\n"
			"\n"
			"return _CLASS_\n";
	hybrid.origin = TEMPLATE_BUILT_IN;
	templates.push_back(hybrid);

	ScriptTemplate gdclass;
	gdclass.inherit = p_object;
	gdclass.name = "gdclass";
	gdclass.description = "Luau gdclass annotation style";
	gdclass.content =
			"--- @class _CLASS_\n"
			"--- @extends _BASE_\n"
			"local _CLASS_ = {}\n"
			"local _CLASS_C = gdclass(_CLASS_)\n"
			"\n"
			"--- @registerMethod _ready\n"
			"function _CLASS_:_ready()\n"
			"end\n"
			"\n"
			"--- @registerMethod _process\n"
			"function _CLASS_:_process(delta)\n"
			"end\n"
			"\n"
			"return _CLASS_C\n";
	gdclass.origin = TEMPLATE_BUILT_IN;
	templates.push_back(gdclass);

	if (p_object == "Object") {
		ScriptTemplate object_extends;
		object_extends.inherit = p_object;
		object_extends.name = "Object extends";
		object_extends.description = "Luau class extending Object directly";
		object_extends.content =
				"local _CLASS_ = {\n"
				"\textends = \"Object\",\n"
				"\tclass_name = \"_CLASS_\",\n"
				"\n"
				"\tfunction _CLASS_:_init()\n"
				"\tend,\n"
				"\n"
				"\tfunction _CLASS_:GetName()\n"
				"\t\treturn \"_CLASS_\"\n"
				"\tend,\n"
				"}\n"
				"\n"
				"return _CLASS_\n";
		object_extends.origin = TEMPLATE_BUILT_IN;
		templates.push_back(object_extends);
	}

	return templates;
}

bool LuauScriptLanguage::is_using_templates() {
	return true;
}

bool LuauScriptLanguage::validate(const String &p_script, const String &p_path, List<String> *r_functions, List<ScriptError> *r_errors, List<Warning> *r_warnings, HashSet<int> *r_safe_lines) const {
	const luau_module::LuauCompileResult compile_result = luau_module::Luau::compile_with_diagnostics(p_script);
	if (compile_result.is_error() || compile_result.bytecode.is_empty()) {
		if (r_errors) {
			ScriptError err;
			err.path = p_path;
			err.line = compile_result.error_line > 0 ? compile_result.error_line : 1;
			err.column = compile_result.error_column;
			err.message = compile_result.error_message.is_empty() ? "Compilation failed" : compile_result.error_message;
			r_errors->push_back(err);
		}
		return false;
	}

	bool type_ok = true;
#ifdef LUAU_MODULE_ANALYSIS_ENABLED
	type_ok = luau_module::LuauTypecheck::analyze(p_script, p_path, r_errors, r_warnings);
#endif

	if (r_functions) {
		LuauClassInfo info;
		PackedByteArray bytecode;
		Ref<luau_module::LuaState> temp_state = load_state;
		if (temp_state.is_valid()) {
			LuauClassInfo::parse_info_from_source(temp_state, p_script, p_path, bytecode, info);
			for (const KeyValue<StringName, LuauClassMethod> &pair : info.methods) {
				r_functions->push_back(pair.key);
			}
		}
	}

#ifdef LUAU_MODULE_ANALYSIS_ENABLED
	if (r_safe_lines) {
		HashSet<int> unsafe_lines;
		if (r_errors) {
			for (const ScriptError &err : *r_errors) {
				if (err.line > 0) {
					unsafe_lines.insert(err.line);
				}
			}
		}
		if (r_warnings) {
			for (const Warning &warning : *r_warnings) {
				for (int line = warning.start_line; line <= warning.end_line; line++) {
					if (line > 0) {
						unsafe_lines.insert(line);
					}
				}
			}
		}

		const int line_count = p_script.is_empty() ? 0 : p_script.split("\n").size();
		for (int line = 1; line <= line_count; line++) {
			if (!unsafe_lines.has(line)) {
				r_safe_lines->insert(line);
			}
		}
	}
#else
	(void)r_safe_lines;
#endif

	return type_ok;
}

Error LuauScriptLanguage::complete_code(const String &p_code, const String &p_path, Object *p_owner, List<CodeCompletionOption> *r_options, bool &r_force, String &r_call_hint) {
	(void)p_owner;
	r_force = false;
	r_call_hint = String();

	if (!r_options) {
		return ERR_INVALID_PARAMETER;
	}

	LuauCompletionContext ctx;
	int cursor_line = 0;
	int cursor_col = 0;
	const String source = LuauCompletionHelper::normalize_completion_source(p_code, cursor_line, cursor_col);
	ctx = LuauCompletionHelper::extract_context(source, cursor_line, cursor_col);

	auto add_option = [&](const String &p_label, CodeCompletionKind p_kind) {
		if (!LuauCompletionHelper::matches_prefix(p_label, ctx.prefix)) {
			return;
		}
		CodeCompletionOption opt;
		opt.display = p_label;
		opt.insert_text = p_label;
		opt.kind = p_kind;
		r_options->push_back(opt);
	};

	if (!ctx.wants_member) {
		List<String> words;
		get_reserved_words(&words);
		for (const String &w : words) {
			add_option(w, CODE_COMPLETION_KIND_PLAIN_TEXT);
		}

		static const char *extra[] = { "await", "wait", "wait_signal", "gdclass", "super", "require", "class", "export", "signal", nullptr };
		for (const char **w = extra; *w; w++) {
			add_option(*w, CODE_COMPLETION_KIND_PLAIN_TEXT);
		}
	}

	LuauClassInfo info;
	PackedByteArray bytecode;
	if (load_state.is_valid()) {
		LuauClassInfo::parse_info_from_source(load_state, source, p_path, bytecode, info);

		if (ctx.wants_member && !ctx.base.is_empty()) {
			if (ctx.base == "self" || ctx.base == info.class_name) {
				for (const KeyValue<StringName, LuauClassProperty> &pair : info.properties) {
					add_option(pair.key, CODE_COMPLETION_KIND_MEMBER);
				}
				for (const KeyValue<StringName, LuauClassMethod> &pair : info.methods) {
					add_option(pair.key, CODE_COMPLETION_KIND_FUNCTION);
				}
			} else if (ScriptServer::is_global_class(ctx.base)) {
				const String base_path = ScriptServer::get_global_class_path(ctx.base);
				Ref<LuauScript> base_script = ResourceLoader::load(base_path);
				if (base_script.is_valid()) {
					List<PropertyInfo> props;
					base_script->get_script_property_list(&props);
					for (const PropertyInfo &prop : props) {
						add_option(prop.name, CODE_COMPLETION_KIND_MEMBER);
					}
					List<MethodInfo> methods;
					base_script->get_script_method_list(&methods);
					for (const MethodInfo &method : methods) {
						add_option(method.name, CODE_COMPLETION_KIND_FUNCTION);
					}
				}
			}
		} else {
			for (const KeyValue<StringName, LuauClassProperty> &pair : info.properties) {
				add_option(pair.key, CODE_COMPLETION_KIND_MEMBER);
			}
			for (const KeyValue<StringName, LuauClassMethod> &pair : info.methods) {
				add_option(pair.key, CODE_COMPLETION_KIND_FUNCTION);
			}
		}

		if (!info.extends.is_empty() && !ctx.wants_member) {
			const String base_name = info.extends;
			if (ClassDB::class_exists(base_name)) {
				List<MethodInfo> methods;
				ClassDB::get_method_list(base_name, &methods);
				for (const MethodInfo &method : methods) {
					if (method.name.begins_with("_")) {
						continue;
					}
					add_option(method.name, CODE_COMPLETION_KIND_FUNCTION);
				}
			}
		}
	}

	return OK;
}

Error LuauScriptLanguage::lookup_code(const String &p_code, const String &p_symbol, const String &p_path, Object *p_owner, LookupResult &r_result) {
	(void)p_owner;

	const int line = LuauScript::find_member_line_in_source(p_code, StringName(p_symbol));
	if (line >= 0) {
		r_result.type = LOOKUP_RESULT_SCRIPT_LOCATION;
		r_result.script_path = p_path;
		r_result.location = line;
		return OK;
	}

	LuauClassInfo info;
	luau_module::LuauAnalysis::parse_annotations(p_code, &info);

	if (info.properties.has(StringName(p_symbol)) || info.methods.has(StringName(p_symbol))) {
		r_result.type = LOOKUP_RESULT_SCRIPT_LOCATION;
		r_result.script_path = p_path;
		r_result.location = line >= 0 ? line : 0;
		return OK;
	}

	if (ScriptServer::is_global_class(p_symbol)) {
		r_result.type = LOOKUP_RESULT_SCRIPT_LOCATION;
		r_result.script_path = ScriptServer::get_global_class_path(p_symbol);
		r_result.location = 0;
		return OK;
	}

	return ERR_UNAVAILABLE;
}

Script *LuauScriptLanguage::create_script() const {
	return memnew(LuauScript);
}

#ifndef DISABLE_DEPRECATED
bool LuauScriptLanguage::has_named_classes() const {
	return true;
}
#endif

bool LuauScriptLanguage::supports_builtin_mode() const {
	return false;
}

bool LuauScriptLanguage::supports_documentation() const {
	return true;
}

bool LuauScriptLanguage::can_inherit_from_file() const {
	return true;
}

int LuauScriptLanguage::find_function(const String &p_function, const String &p_code) const {
	const int line = LuauScript::find_member_line_in_source(p_code, StringName(p_function));
	if (line >= 0) {
		return line;
	}
	return -1;
}

String LuauScriptLanguage::make_function(const String &p_class, const String &p_name, const PackedStringArray &p_args) const {
	String params;
	for (int i = 0; i < p_args.size(); i++) {
		if (i > 0) {
			params += ", ";
		}
		params += p_args[i];
	}
	return vformat("function %s:%s(%s)\nend\n", p_class, p_name, params);
}

void LuauScriptLanguage::auto_indent_code(String &p_code, int p_from_line, int p_to_line) const {
#ifdef TOOLS_ENABLED
	p_code = LuauFormatter::format_lines(p_code, p_from_line, p_to_line);
#else
	(void)p_from_line;
	(void)p_to_line;
#endif
}

#ifdef TOOLS_ENABLED
Error LuauScriptLanguage::open_in_external_editor(const Ref<Script> &p_script, int p_line, int p_col) {
	ERR_FAIL_COND_V(p_script.is_null(), ERR_INVALID_PARAMETER);

	String path = EDITOR_GET("text_editor/external/exec_path");
	if (path.is_empty()) {
		return ERR_UNAVAILABLE;
	}

	String flags = EDITOR_GET("text_editor/external/exec_flags");
	String script_path = ProjectSettings::get_singleton()->globalize_path(p_script->get_path());
	String project_path = ProjectSettings::get_singleton()->get_resource_path();

	flags = flags.replacen("{line}", itos(p_line > 0 ? p_line : 1));
	flags = flags.replacen("{col}", itos(p_col));
	flags = flags.replacen("{project}", project_path);
	flags = flags.replacen("{file}", script_path);

	List<String> args;
	if (!flags.is_empty()) {
		args.push_back(flags);
	}
	if (!args.size() || !flags.contains("{file}")) {
		args.push_back(script_path);
	}

	Error err = OS::get_singleton()->create_process(path, args);
	return err == OK ? OK : ERR_UNAVAILABLE;
}
#endif

void LuauScriptLanguage::add_global_constant(const StringName &p_variable, const Variant &p_value) {
	add_named_global_constant(p_variable, p_value);
}

void LuauScriptLanguage::add_named_global_constant(const StringName &p_name, const Variant &p_value) {
	named_globals[p_name] = p_value;
}

void LuauScriptLanguage::remove_named_global_constant(const StringName &p_name) {
	named_globals.erase(p_name);
}

void LuauScriptLanguage::push_named_globals_to_state(lua_State *p_L) const {
	for (const KeyValue<StringName, Variant> &pair : named_globals) {
		CharString utf8 = pair.key.operator String().utf8();
		const Variant &value = pair.value;
		if (value.get_type() == Variant::OBJECT) {
			Object *obj = value.get_validated_object();
			if (obj) {
				luau_module::push_object(p_L, obj, LUA_NOTAG);
			} else {
				lua_pushnil(p_L);
			}
		} else {
			luau_module::push_variant(p_L, value);
		}
		lua_setglobal(p_L, utf8.get_data());
	}
}

void LuauScriptLanguage::frame() {
#ifdef DEBUG_ENABLED
	if (profiling_active) {
		for (KeyValue<StringName, ProfileEntry> &pair : profile_frame) {
			pair.value.frame_call_count = 0;
			pair.value.frame_total_time = 0;
			pair.value.frame_self_time = 0;
			pair.value.frame_native_calls.clear();
		}
	}
#endif

	double delta = Performance::get_singleton()->get_monitor(Performance::TIME_PROCESS);
	if (delta <= 0.0) {
		delta = 1.0 / 60.0;
	}
	task_scheduler.frame(delta);
}

String LuauScriptLanguage::debug_get_error() const {
	return debugger.get_error();
}

int LuauScriptLanguage::debug_get_stack_level_count() const {
	return debugger.get_stack_level_count();
}

int LuauScriptLanguage::debug_get_stack_level_line(int p_level) const {
	return debugger.get_stack_level_line(p_level);
}

String LuauScriptLanguage::debug_get_stack_level_function(int p_level) const {
	return debugger.get_stack_level_function(p_level);
}

String LuauScriptLanguage::debug_get_stack_level_source(int p_level) const {
	return debugger.get_stack_level_source(p_level);
}

void LuauScriptLanguage::debug_get_stack_level_locals(int p_level, List<String> *p_locals, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
	debugger.get_stack_level_locals(p_level, p_locals, p_values, p_max_subitems, p_max_depth);
}

void LuauScriptLanguage::debug_get_stack_level_members(int p_level, List<String> *p_members, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
	debugger.get_stack_level_members(p_level, p_members, p_values, p_max_subitems, p_max_depth);
}

ScriptInstance *LuauScriptLanguage::debug_get_stack_level_instance(int p_level) {
	return debugger.get_stack_level_instance(p_level);
}

void LuauScriptLanguage::debug_get_globals(List<String> *p_globals, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
	debugger.get_globals(p_globals, p_values, p_max_subitems, p_max_depth);
}

String LuauScriptLanguage::debug_parse_stack_level_expression(int p_level, const String &p_expression, int p_max_subitems, int p_max_depth) {
	HashMap<String, Variant> bindings;

	List<String> local_names;
	List<Variant> local_values;
	debugger.get_stack_level_locals(p_level, &local_names, &local_values, p_max_subitems, p_max_depth);
	List<Variant>::Element *local_elem = local_values.front();
	for (const String &name : local_names) {
		if (local_elem) {
			bindings[name] = local_elem->get();
			local_elem = local_elem->next();
		}
	}

	List<String> member_names;
	List<Variant> member_values;
	debugger.get_stack_level_members(p_level, &member_names, &member_values, p_max_subitems, p_max_depth);
	List<Variant>::Element *member_elem = member_values.front();
	for (const String &name : member_names) {
		if (member_elem) {
			const Variant value = member_elem->get();
			bindings[vformat("self.%s", name)] = value;
			if (!bindings.has(name)) {
				bindings[name] = value;
			}
			member_elem = member_elem->next();
		}
	}

	List<String> global_names;
	List<Variant> global_values;
	debugger.get_globals(&global_names, &global_values, p_max_subitems, p_max_depth);
	List<Variant>::Element *global_elem = global_values.front();
	for (const String &name : global_names) {
		if (global_elem) {
			const Variant value = global_elem->get();
			bindings[vformat("global.%s", name)] = value;
			if (!bindings.has(name)) {
				bindings[name] = value;
			}
			global_elem = global_elem->next();
		}
	}

	Vector<String> name_vector;
	Array value_array;
	for (const KeyValue<String, Variant> &pair : bindings) {
		name_vector.push_back(pair.key);
		value_array.push_back(pair.value);
	}

	Expression expression;
	if (expression.parse(p_expression, name_vector) == OK) {
		ScriptInstance *instance = debug_get_stack_level_instance(p_level);
		Object *base = instance ? instance->get_owner() : nullptr;
		Variant result = expression.execute(value_array, base);
		if (!(result.get_type() == Variant::NIL && expression.has_execute_failed())) {
			return result.get_construct_string();
		}
	}

	Ref<luau_module::LuaState> eval_state;
	eval_state.instantiate();
	eval_state->open_libs(luau_module::LuaState::LIB_BASE | luau_module::LuaState::LIB_MATH);
	eval_state->sandbox();
	eval_state->sandbox_thread();

	lua_State *L = eval_state->get_lua_state();
	lua_newtable(L);
	const int env_index = lua_gettop(L);
	for (const KeyValue<String, Variant> &pair : bindings) {
		CharString utf8 = pair.key.utf8();
		luau_module::push_variant(L, pair.value);
		lua_setfield(L, env_index, utf8.get_data());
	}
	lua_setglobal(L, "_ENV");

	const String wrapped = vformat("return %s", p_expression);
	const luau_module::LuaState::Status status = eval_state->do_string(wrapped, "@LuauWatch");
	if (status == luau_module::LuaState::STATUS_OK && eval_state->get_top() >= 1) {
		Variant result = eval_state->to_variant(-1);
		eval_state->pop(1);
		return result.get_construct_string();
	}

	return String();
}

void LuauScriptLanguage::get_public_functions(List<MethodInfo> *p_functions) const {
	if (!p_functions) {
		return;
	}

	static const char *helpers[] = {
		"export",
		"signal",
		"gdclass",
		"class",
		"require",
		"await",
		"wait",
		"wait_signal",
		nullptr,
	};
	for (const char **name = helpers; *name; name++) {
		MethodInfo info;
		info.name = StringName(*name);
		p_functions->push_back(info);
	}
}

void LuauScriptLanguage::get_public_constants(List<Pair<String, Variant>> *p_constants) const {
	if (!p_constants) {
		return;
	}

	for (const KeyValue<StringName, Variant> &pair : named_globals) {
		p_constants->push_back(Pair<String, Variant>(pair.key, pair.value));
	}

	MutexLock lock(const_cast<LuauScriptLanguage *>(this)->mutex);
	for (SelfList<LuauScript> *elem = const_cast<LuauScriptLanguage *>(this)->script_list.first(); elem; elem = elem->next()) {
		const LuauClassInfo &info = elem->self()->get_class_info();
		for (const KeyValue<StringName, Variant> &pair : info.constants) {
			p_constants->push_back(Pair<String, Variant>(pair.key, pair.value));
		}
	}
}

void LuauScriptLanguage::get_public_annotations(List<MethodInfo> *p_annotations) const {
	if (!p_annotations) {
		return;
	}

	static const char *tags[] = {
		"@tool",
		"@abstract",
		"@class",
		"@property",
		"@registerMethod",
		"@icon",
		"@rpc",
		nullptr,
	};
	for (const char **tag = tags; *tag; tag++) {
		MethodInfo info;
		info.name = StringName(*tag);
		p_annotations->push_back(info);
	}
}

#ifdef DEBUG_ENABLED
void LuauScriptLanguage::profile_push_context(const StringName &p_signature) {
	profile_context_signature = p_signature;
}

void LuauScriptLanguage::profile_pop_context() {
	profile_context_signature = StringName();
}

void LuauScriptLanguage::profile_record_native_call(const StringName &p_native_signature, uint64_t p_time_usec) {
	if (!profiling_active || !profile_native_calls || profile_context_signature.is_empty()) {
		return;
	}

	ProfileEntry &accumulated = profile_accumulated[profile_context_signature];
	NativeProfileEntry &native = accumulated.native_calls[p_native_signature];
	native.signature = p_native_signature;
	native.call_count++;
	native.total_time += p_time_usec;

	ProfileEntry &frame = profile_frame[profile_context_signature];
	NativeProfileEntry &frame_native = frame.frame_native_calls[p_native_signature];
	frame_native.signature = p_native_signature;
	frame_native.frame_call_count++;
	frame_native.frame_total_time += p_time_usec;
}
#endif

#ifdef DEBUG_ENABLED
void LuauScriptLanguage::profile_record_call(const StringName &p_signature, uint64_t p_time_usec) {
	if (!profiling_active) {
		return;
	}

	ProfileEntry &accumulated = profile_accumulated[p_signature];
	accumulated.signature = p_signature;
	accumulated.call_count++;
	accumulated.total_time += p_time_usec;
	accumulated.self_time += p_time_usec;

	ProfileEntry &frame = profile_frame[p_signature];
	frame.signature = p_signature;
	frame.frame_call_count++;
	frame.frame_total_time += p_time_usec;
	frame.frame_self_time += p_time_usec;
}
#endif

void LuauScriptLanguage::profiling_start() {
#ifdef DEBUG_ENABLED
	profiling_active = true;
	profile_accumulated.clear();
	profile_frame.clear();
#endif
}

void LuauScriptLanguage::profiling_stop() {
#ifdef DEBUG_ENABLED
	profiling_active = false;
#endif
}

void LuauScriptLanguage::profiling_set_save_native_calls(bool p_enable) {
#ifdef DEBUG_ENABLED
	profile_native_calls = p_enable;
#else
	(void)p_enable;
#endif
}

int LuauScriptLanguage::profiling_get_accumulated_data(ProfilingInfo *p_info_arr, int p_info_max) {
#ifdef DEBUG_ENABLED
	int count = 0;
	for (const KeyValue<StringName, ProfileEntry> &pair : profile_accumulated) {
		if (count >= p_info_max) {
			break;
		}
		const ProfileEntry &entry = pair.value;
		const int last_non_internal = count;
		p_info_arr[count].signature = entry.signature;
		p_info_arr[count].call_count = entry.call_count;
		p_info_arr[count].total_time = entry.total_time;
		p_info_arr[count].self_time = entry.self_time;
		p_info_arr[count].internal_time = 0;
		count++;

		if (profile_native_calls) {
			uint64_t native_time = 0;
			for (const KeyValue<StringName, NativeProfileEntry> &native_pair : entry.native_calls) {
				if (count >= p_info_max) {
					break;
				}
				const NativeProfileEntry &native = native_pair.value;
				p_info_arr[count].signature = native.signature;
				p_info_arr[count].call_count = native.call_count;
				p_info_arr[count].total_time = native.total_time;
				p_info_arr[count].self_time = native.total_time;
				p_info_arr[count].internal_time = native.total_time;
				native_time += native.total_time;
				count++;
			}
			p_info_arr[last_non_internal].internal_time = native_time;
		}
	}
	return count;
#else
	(void)p_info_arr;
	(void)p_info_max;
	return 0;
#endif
}

int LuauScriptLanguage::profiling_get_frame_data(ProfilingInfo *p_info_arr, int p_info_max) {
#ifdef DEBUG_ENABLED
	int count = 0;
	for (const KeyValue<StringName, ProfileEntry> &pair : profile_frame) {
		if (count >= p_info_max) {
			break;
		}
		const ProfileEntry &entry = pair.value;
		if (entry.frame_call_count == 0) {
			continue;
		}
		const int last_non_internal = count;
		p_info_arr[count].signature = entry.signature;
		p_info_arr[count].call_count = entry.frame_call_count;
		p_info_arr[count].total_time = entry.frame_total_time;
		p_info_arr[count].self_time = entry.frame_self_time;
		p_info_arr[count].internal_time = 0;
		count++;

		if (profile_native_calls) {
			uint64_t native_time = 0;
			for (const KeyValue<StringName, NativeProfileEntry> &native_pair : entry.frame_native_calls) {
				if (count >= p_info_max) {
					break;
				}
				const NativeProfileEntry &native = native_pair.value;
				if (native.frame_call_count == 0) {
					continue;
				}
				p_info_arr[count].signature = native.signature;
				p_info_arr[count].call_count = native.frame_call_count;
				p_info_arr[count].total_time = native.frame_total_time;
				p_info_arr[count].self_time = native.frame_total_time;
				p_info_arr[count].internal_time = native.frame_total_time;
				native_time += native.frame_total_time;
				count++;
			}
			p_info_arr[last_non_internal].internal_time = native_time;
		}
	}
	return count;
#else
	(void)p_info_arr;
	(void)p_info_max;
	return 0;
#endif
}

void LuauScriptLanguage::reload_all_scripts() {
	MutexLock lock(mutex);
	for (SelfList<LuauScript> *elem = script_list.first(); elem; elem = elem->next()) {
		elem->self()->reload(true);
	}
}

void LuauScriptLanguage::reload_scripts(const Array &p_scripts, bool p_soft_reload) {
	for (int i = 0; i < p_scripts.size(); i++) {
		Ref<Script> scr = p_scripts[i];
		if (scr.is_valid()) {
			scr->reload(p_soft_reload);
		}
	}
}

void LuauScriptLanguage::reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) {
	Ref<LuauScript> scr = p_script;
	if (scr.is_valid() && scr->is_tool()) {
		scr->reload(p_soft_reload);
	}
}

void LuauScriptLanguage::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("luau");
	p_extensions->push_back("lua");
	p_extensions->push_back("luauc");
}

bool LuauScriptLanguage::handles_global_class_type(const String &p_type) const {
	return p_type == "LuauScript";
}

bool LuauScriptLanguage::debug_should_break_at(const String &p_source, int p_line) const {
	return debugger.should_break_at(p_source, p_line);
}

String LuauScriptLanguage::get_global_class_name(const String &p_path, String *r_base_type, String *r_icon_path, bool *r_is_abstract, bool *r_is_tool) const {
	/* **WARNING**
	 *
	 * Do not load scripts here. EditorFileSystem calls this while scanning global
	 * classes, before dependencies exist and while the filesystem scan is active.
	 * Full ResourceLoader::load() can re-enter the scan and crash the editor.
	 */
	String source_path = p_path;
	const String ext = source_path.get_extension().to_lower();
	if (ext == "luauc") {
		const String luau_path = source_path.get_basename() + ".luau";
		if (FileAccess::exists(luau_path)) {
			source_path = luau_path;
		}
	} else if (ext != "luau" && ext != "lua") {
		return String();
	}

	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(source_path, FileAccess::READ, &err);
	if (err != OK) {
		return String();
	}

	const String source = file->get_as_text();
	LuauClassInfo info;
	LuauClassInfo::parse_global_class_metadata_from_source(source, &info);
	if (info.class_name.is_empty()) {
		return String();
	}

	if (r_base_type) {
		if (ClassDB::class_exists(info.extends)) {
			*r_base_type = info.extends;
		} else if (ScriptServer::is_global_class(info.extends)) {
			*r_base_type = ScriptServer::get_global_class_native_base(info.extends);
		} else {
			*r_base_type = info.extends;
		}
	}
	if (r_icon_path) {
		*r_icon_path = info.icon_path;
	}
	if (r_is_abstract) {
		*r_is_abstract = info.abstract;
	}
	if (r_is_tool) {
		*r_is_tool = info.tool;
	}
	return info.class_name;
}

LuauScriptLanguage::LuauScriptLanguage() {
	singleton = this;
	load_state.instantiate();
}

LuauScriptLanguage::~LuauScriptLanguage() {
	singleton = nullptr;
}
