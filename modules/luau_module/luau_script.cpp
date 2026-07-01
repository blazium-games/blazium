/**************************************************************************/
/*  luau_script.cpp                                                       */
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

#include "luau_script.h"

#include "luau.h"
#include "luau_bytecode_format.h"
#include "luau_script_instance.h"
#include "luau_script_language.h"
#include "require/luau_require.h"

using luau_module::LuaState;
using luau_module::LuauBytecodeFormat;

#include "core/error/error_macros.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "core/object/script_language.h"

namespace {

void merge_inherited_class_metadata(LuauClassInfo &r_info, const LuauClassInfo &p_base_info) {
	for (const KeyValue<StringName, LuauClassProperty> &pair : p_base_info.properties) {
		if (!r_info.properties.has(pair.key)) {
			r_info.properties[pair.key] = pair.value;
		}
	}
	for (const KeyValue<StringName, LuauClassSignal> &pair : p_base_info.signals) {
		if (!r_info.signals.has(pair.key)) {
			r_info.signals[pair.key] = pair.value;
		}
	}
}

} //namespace

void LuauScript::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load_source_code", "path"), &LuauScript::load_source_code);
	ClassDB::bind_method(D_METHOD("compile", "force_recompile"), &LuauScript::compile, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("is_placeholder_fallback_enabled"), &LuauScript::is_placeholder_fallback_enabled);
	ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "new", &LuauScript::_new, MethodInfo("new"));
}

Variant LuauScript::_new(const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	if (!valid) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return Variant();
	}

	r_error.error = Callable::CallError::CALL_OK;

	const StringName base_type = get_instance_base_type();
	Object *owner = ClassDB::instantiate(base_type);
	ERR_FAIL_NULL_V_MSG(owner, Variant(), vformat("Cannot instantiate base type '%s' for LuauScript.", base_type));

	Ref<RefCounted> ref;
	RefCounted *ref_counted = Object::cast_to<RefCounted>(owner);
	if (ref_counted) {
		ref = Ref<RefCounted>(ref_counted);
	}

	ScriptInstance *instance = instance_create(owner);
	if (!instance) {
		if (ref.is_null()) {
			memdelete(owner);
		}
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return Variant();
	}

	if (p_argcount > 0 && has_method("_init")) {
		LuauScriptInstance *luau_instance = static_cast<LuauScriptInstance *>(instance);
		luau_instance->callp("_init", p_args, p_argcount, r_error);
		if (r_error.error != Callable::CallError::CALL_OK) {
			if (ref.is_null()) {
				memdelete(owner);
			}
			return Variant();
		}
	}

	if (ref.is_valid()) {
		return ref;
	}
	return owner;
}

Error LuauScript::load_source_code(const String &p_path) {
	Error err;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Cannot open Luau script '%s'.", p_path));
	bytecode.clear();
	set_source_code(file->get_as_text());
	set_path(p_path);
	return reload(false);
}

Error LuauScript::load_bytecode_file(const String &p_path) {
	Error err;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Cannot open Luau bytecode '%s'.", p_path));

	const Vector<uint8_t> raw = file->get_buffer(file->get_length());
	ERR_FAIL_COND_V_MSG(raw.is_empty(), ERR_FILE_CORRUPT, vformat("Luau bytecode file is empty: '%s'.", p_path));
	const Vector<uint8_t> decrypted = LuauBytecodeFormat::decrypt_export_data(raw);
	bytecode = LuauBytecodeFormat::unwrap(decrypted);
	ERR_FAIL_COND_V_MSG(bytecode.is_empty(), ERR_FILE_CORRUPT, vformat("Luau bytecode file is corrupt: '%s'.", p_path));

	const String source_path = p_path.get_basename() + ".luau";
	if (FileAccess::exists(source_path)) {
		set_source_code(FileAccess::get_file_as_string(source_path));
	} else {
		source = String();
		valid = false;
	}

	set_path(p_path);
	return reload(false);
}

bool LuauScript::can_instantiate() const {
	return valid && (tool || ScriptServer::is_scripting_enabled());
}

Ref<Script> LuauScript::get_base_script() const {
	if (class_info.extends.is_empty()) {
		return Ref<Script>();
	}
	if (ScriptServer::is_global_class(class_info.extends)) {
		const String path = ScriptServer::get_global_class_path(class_info.extends);
		if (!path.is_empty()) {
			return ResourceLoader::load(path);
		}
	}
	return Ref<Script>();
}

StringName LuauScript::get_global_name() const {
	return class_info.class_name;
}

bool LuauScript::inherits_script(const Ref<Script> &p_script) const {
	Ref<LuauScript> luau_script = p_script;
	return luau_script.is_valid() && luau_script.ptr() == this;
}

StringName LuauScript::get_instance_base_type() const {
	if (class_info.extends.is_empty()) {
		return StringName("RefCounted");
	}
	if (ClassDB::class_exists(class_info.extends)) {
		return class_info.extends;
	}
	if (ScriptServer::is_global_class(class_info.extends)) {
		return ScriptServer::get_global_class_native_base(class_info.extends);
	}
	Ref<Script> base = get_base_script();
	if (base.is_valid() && base->is_valid()) {
		return base->get_instance_base_type();
	}
	return StringName("RefCounted");
}

ScriptInstance *LuauScript::instance_create(Object *p_this) {
	ERR_FAIL_COND_V(!valid, nullptr);

	LuauScriptInstance *instance = memnew(LuauScriptInstance(p_this, Ref<LuauScript>(this)));
	p_this->set_script_instance(instance);

	MutexLock lock(LuauScriptLanguage::get_singleton()->get_mutex());
	instances.insert(p_this);

	return instance;
}

#ifdef TOOLS_ENABLED
PlaceHolderScriptInstance *LuauScript::placeholder_instance_create(Object *p_this) {
	PlaceHolderScriptInstance *instance = memnew(PlaceHolderScriptInstance(LuauScriptLanguage::get_singleton(), Ref<Script>(this), p_this));
	placeholders.insert(instance);
	return instance;
}
#endif

void LuauScript::_placeholder_erased(PlaceHolderScriptInstance *p_placeholder) {
	placeholders.erase(p_placeholder);
}

bool LuauScript::instance_has(const Object *p_this) const {
	MutexLock lock(LuauScriptLanguage::get_singleton()->get_mutex());
	return instances.has((Object *)p_this);
}

bool LuauScript::has_source_code() const {
	return !source.is_empty();
}

String LuauScript::get_source_code() const {
	return source;
}

void LuauScript::set_source_code(const String &p_code) {
	if (source == p_code) {
		return;
	}
	source = p_code;
	bytecode.clear();
	valid = false;
}

PackedByteArray LuauScript::compile(bool p_force_recompile) {
	if (bytecode.is_empty() || p_force_recompile) {
		bytecode = luau_module::Luau::compile(source);
	}
	return bytecode;
}

Error LuauScript::reload(bool p_keep_state) {
	if (reloading) {
		return OK;
	}
	reloading = true;

	{
		MutexLock lock(LuauScriptLanguage::get_singleton()->get_mutex());
		ERR_FAIL_COND_V(!p_keep_state && !instances.is_empty(), ERR_ALREADY_IN_USE);
	}

	valid = false;

	LuauScriptLanguage *lang = LuauScriptLanguage::get_singleton();
	ERR_FAIL_NULL_V(lang, ERR_UNAVAILABLE);

	Ref<LuaState> load_state = lang->get_load_state();
	ERR_FAIL_COND_V(load_state.is_null(), ERR_UNAVAILABLE);

	// Default: isolated parser VM per script. Only share the parent's class VM when
	// extending another Luau global class (super calls require the same lua_State).
	Ref<LuaState> parse_vm;
	{
		LuauClassInfo extends_info;
		LuauClassInfo::parse_global_class_metadata_from_source(source, &extends_info);
		if (!extends_info.extends.is_empty() && ScriptServer::is_global_class(extends_info.extends)) {
			const String base_path = ScriptServer::get_global_class_path(extends_info.extends);
			if (!base_path.is_empty()) {
				Ref<LuauScript> base_luau = ResourceLoader::load(base_path);
				if (base_luau.is_valid() && base_luau->is_valid() && base_luau->get_class_vm().is_valid()) {
					parse_vm = base_luau->get_class_vm();
				}
			}
		}
	}

	unref_class_table(load_state);

	LuauClassInfo new_info;
	Ref<LuaState> new_class_vm;
	int new_table_ref = LUA_NOREF;
	const bool parsed_into_parent_vm = parse_vm.is_valid();

	Error err = LuauClassInfo::parse_from_source(parse_vm, source, get_path(), bytecode, new_info, new_class_vm, &new_table_ref);
	if (err != OK) {
#ifdef TOOLS_ENABLED
		if (tool || source.contains("@tool")) {
			placeholder_fallback_enabled = true;
		}
#endif
		reloading = false;
		return err;
	}

	placeholder_fallback_enabled = false;

	const StringName old_global_name = class_info.class_name;

	class_info = new_info;

	if (!class_info.extends.is_empty() && ScriptServer::is_global_class(class_info.extends)) {
		const String base_path = ScriptServer::get_global_class_path(class_info.extends);
		if (!base_path.is_empty()) {
			Ref<LuauScript> base_luau = ResourceLoader::load(base_path);
			if (base_luau.is_valid() && base_luau->is_valid()) {
				merge_inherited_class_metadata(class_info, base_luau->get_class_info());
			}
		}
	}

	class_vm = new_class_vm;
	class_table_ref = new_table_ref;
	tool = class_info.tool;
	valid = true;

	if (old_global_name != class_info.class_name) {
		if (old_global_name != StringName()) {
			ScriptServer::remove_global_class(old_global_name);
		}
	}

	if (class_info.class_name != StringName()) {
		ScriptServer::add_global_class(class_info.class_name, get_instance_base_type(), lang->get_name(), get_path(), is_abstract(), is_tool());
	} else if (old_global_name != StringName()) {
		ScriptServer::remove_global_class(old_global_name);
	}

	// Do not call ScriptServer::save_global_classes() here. Reload can run while
	// ResourceLoader is active; persisting global classes re-enters project I/O
	// and matches GDScript (EditorFileSystem owns cache updates during scan).

	// Child scripts parsed into the parent's class VM share require state; clearing
	// package.loaded would break the parent and any live instances on that VM.
	if (class_vm.is_valid() && class_vm->is_valid() && !parsed_into_parent_vm) {
		luau_module::LuauRequire::invalidate_all(class_vm->get_lua_state());
	}

	if (p_keep_state) {
		MutexLock lock(LuauScriptLanguage::get_singleton()->get_mutex());
		for (Object *owner : instances) {
			if (owner && owner->get_script_instance()) {
				LuauScriptInstance *instance = static_cast<LuauScriptInstance *>(owner->get_script_instance());
				instance->on_script_soft_reload();
			}
		}
	}

	reloading = false;
	return OK;
}

#ifdef TOOLS_ENABLED
#include "editor/doc_tools.h"

StringName LuauScript::get_doc_class_name() const {
	return class_info.class_name;
}

Vector<DocData::ClassDoc> LuauScript::get_documentation() const {
	Vector<DocData::ClassDoc> docs;
	if (class_info.class_name.is_empty()) {
		return docs;
	}

	DocData::ClassDoc doc;
	doc.name = class_info.class_name;
	doc.is_script_doc = true;
	doc.script_path = get_path();
	doc.inherits = class_info.extends;
	doc.description = class_info.class_description;

	for (const KeyValue<StringName, LuauClassProperty> &pair : class_info.properties) {
		DocData::PropertyDoc prop_doc;
		prop_doc.name = pair.key;
		prop_doc.type = Variant::get_type_name(pair.value.info.type);
		prop_doc.description = pair.value.description;
		doc.properties.push_back(prop_doc);
	}

	for (const KeyValue<StringName, LuauClassMethod> &pair : class_info.methods) {
		DocData::MethodDoc method_doc;
		method_doc.name = pair.key;
		method_doc.description = pair.value.description;
		doc.methods.push_back(method_doc);
	}

	docs.push_back(doc);
	return docs;
}

String LuauScript::get_class_icon_path() const {
	return class_info.icon_path;
}
#endif

bool LuauScript::has_method(const StringName &p_method) const {
	return class_info.methods.has(p_method);
}

MethodInfo LuauScript::get_method_info(const StringName &p_method) const {
	if (class_info.methods.has(p_method)) {
		return class_info.methods[p_method].info;
	}
	return MethodInfo();
}

bool LuauScript::is_tool() const {
	return tool;
}

bool LuauScript::is_valid() const {
	return valid;
}

bool LuauScript::is_abstract() const {
	return class_info.abstract;
}

ScriptLanguage *LuauScript::get_language() const {
	return LuauScriptLanguage::get_singleton();
}

bool LuauScript::has_script_signal(const StringName &p_signal) const {
	return class_info.signals.has(p_signal);
}

void LuauScript::get_script_signal_list(List<MethodInfo> *r_signals) const {
	for (const KeyValue<StringName, LuauClassSignal> &pair : class_info.signals) {
		r_signals->push_back(pair.value.info);
	}
}

bool LuauScript::get_property_default_value(const StringName &p_property, Variant &r_value) const {
	if (class_info.properties.has(p_property)) {
		const LuauClassProperty &prop = class_info.properties[p_property];
		if (prop.default_value.get_type() != Variant::NIL) {
			r_value = prop.default_value;
			return true;
		}
	}
	Ref<LuauScript> base_luau = get_base_script();
	if (base_luau.is_valid() && base_luau->is_valid()) {
		return base_luau->get_property_default_value(p_property, r_value);
	}
	return false;
}

void LuauScript::get_script_method_list(List<MethodInfo> *r_list) const {
	for (const KeyValue<StringName, LuauClassMethod> &pair : class_info.methods) {
		r_list->push_back(pair.value.info);
	}
}

void LuauScript::get_script_property_list(List<PropertyInfo> *r_list) const {
	for (const KeyValue<StringName, LuauClassProperty> &pair : class_info.properties) {
		r_list->push_back(pair.value.info);
	}
}

int LuauScript::find_member_line_in_source(const String &p_source, const StringName &p_member) {
	const String member = p_member;
	if (member.is_empty() || p_source.is_empty()) {
		return -1;
	}

	const PackedStringArray lines = p_source.split("\n");
	for (int i = 0; i < lines.size(); i++) {
		const String &line = lines[i];
		if (line.contains("@registerMethod " + member) || line.contains("@property " + member)) {
			return i;
		}
		if (line.contains("function ") && (line.contains(":" + member + "(") || line.contains("." + member + "(") || line.contains("function " + member + "("))) {
			return i;
		}
		if (line.contains(member + " = export") || line.contains(member + " = signal")) {
			return i;
		}
	}
	return -1;
}

int LuauScript::get_member_line(const StringName &p_member) const {
	return find_member_line_in_source(source, p_member);
}

Variant LuauScript::get_rpc_config() const {
	return class_info.rpc_config;
}

#ifdef TOOLS_ENABLED
bool LuauScript::is_placeholder_fallback_enabled() const {
	return placeholder_fallback_enabled;
}
#endif

void LuauScript::unref_class_table(const Ref<LuaState> &p_state) {
	if (class_table_ref != LUA_NOREF && class_vm.is_valid() && class_vm->is_valid()) {
		lua_State *L = class_vm->get_lua_state();
		lua_unref(L, class_table_ref);
	}
	class_table_ref = LUA_NOREF;
	class_vm.unref();
}

LuauScript::LuauScript() :
		script_list(this) {
	if (LuauScriptLanguage::get_singleton()) {
		MutexLock lock(LuauScriptLanguage::get_singleton()->get_mutex());
		LuauScriptLanguage::get_singleton()->get_script_list().add(&script_list);
	}
}

LuauScript::~LuauScript() {
	if (LuauScriptLanguage::get_singleton()) {
		Ref<LuaState> load_state = LuauScriptLanguage::get_singleton()->get_load_state();
		unref_class_table(load_state);
	}
}
