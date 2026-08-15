/**************************************************************************/
/*  luau_script.h                                                         */
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
#include "core/templates/hash_set.h"
#include "core/templates/self_list.h"
#include "lua_state.h"
#include "luau_class_info.h"
#include <lua.h>

class LuauScriptLanguage;
class LuauScriptInstance;

class LuauScript : public Script {
	GDCLASS(LuauScript, Script);

	friend class LuauScriptLanguage;
	friend class LuauScriptInstance;

	bool tool = false;
	bool valid = false;
	bool reloading = false;

	String source;
	PackedByteArray bytecode;
	LuauClassInfo class_info;
	int class_table_ref = LUA_NOREF;
	Ref<luau_module::LuaState> class_vm;

	HashSet<Object *> instances;
	HashSet<PlaceHolderScriptInstance *> placeholders;

	SelfList<LuauScript> script_list;

	bool placeholder_fallback_enabled = false;

protected:
	static void _bind_methods();

	Variant _new(const Variant **p_args, int p_argcount, Callable::CallError &r_error);

public:
	Error load_source_code(const String &p_path);
	Error load_bytecode_file(const String &p_path);

	virtual bool can_instantiate() const override;
	virtual Ref<Script> get_base_script() const override;
	virtual StringName get_global_name() const override;
	virtual bool inherits_script(const Ref<Script> &p_script) const override;
	virtual StringName get_instance_base_type() const override;
	virtual ScriptInstance *instance_create(Object *p_this) override;
#ifdef TOOLS_ENABLED
	virtual PlaceHolderScriptInstance *placeholder_instance_create(Object *p_this) override;
#endif
	virtual bool has_source_code() const override;
	virtual String get_source_code() const override;
	virtual void set_source_code(const String &p_code) override;
	PackedByteArray compile(bool p_force_recompile = false);
	virtual Error reload(bool p_keep_state = false) override;

#ifdef TOOLS_ENABLED
	virtual StringName get_doc_class_name() const override;
	virtual Vector<DocData::ClassDoc> get_documentation() const override;
	virtual String get_class_icon_path() const override;
#endif

	virtual bool has_method(const StringName &p_method) const override;
	virtual MethodInfo get_method_info(const StringName &p_method) const override;

	virtual bool is_tool() const override;
	virtual bool is_valid() const override;
	virtual bool is_abstract() const override;

	virtual ScriptLanguage *get_language() const override;

	virtual bool has_script_signal(const StringName &p_signal) const override;
	virtual void get_script_signal_list(List<MethodInfo> *r_signals) const override;

	virtual bool get_property_default_value(const StringName &p_property, Variant &r_value) const override;

	virtual void get_script_method_list(List<MethodInfo> *r_list) const override;
	virtual void get_script_property_list(List<PropertyInfo> *r_list) const override;

	virtual const Variant get_rpc_config() const override;

	void _placeholder_erased(PlaceHolderScriptInstance *p_placeholder) override;

	int get_member_line(const StringName &p_member) const override;
	static int find_member_line_in_source(const String &p_source, const StringName &p_member);

#ifdef TOOLS_ENABLED
	virtual bool is_placeholder_fallback_enabled() const override;
#endif

	const LuauClassInfo &get_class_info() const { return class_info; }
	const PackedByteArray &get_bytecode() const { return bytecode; }
	int get_class_table_ref() const { return class_table_ref; }
	Ref<luau_module::LuaState> get_class_vm() const { return class_vm; }

	void unref_class_table(const Ref<luau_module::LuaState> &p_state);

	LuauScript();
	~LuauScript();
};
