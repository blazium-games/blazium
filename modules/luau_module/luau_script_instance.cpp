/**************************************************************************/
/*  luau_script_instance.cpp                                              */
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

#include "luau_script_instance.h"

#include "bindings/object.h"
#include "bindings/variant.h"
#include "helpers.h"
#include "luau_class_info.h"
#include "luau_script_language.h"
#include "string_cache.h"

#include "core/os/main_loop.h"
#include "core/os/time.h"
#include "scene/gui/control.h"
#include "scene/main/canvas_item.h"
#include "scene/main/node.h"
#include "scene/main/viewport.h"
#include <lualib.h>

using namespace luau_module;

namespace {

LuaState::Status traced_pcall(const Ref<LuaState> &p_state, int p_nargs, int p_nresults) {
	bool tracing = false;
	if (LuauScriptLanguage *lang = LuauScriptLanguage::get_singleton()) {
		tracing = lang->get_debugger().begin_traced_execution(p_state);
	}

	const LuaState::Status status = p_state->pcall(p_nargs, p_nresults);

	if (tracing) {
		if (LuauScriptLanguage *lang = LuauScriptLanguage::get_singleton()) {
			lang->get_debugger().end_traced_execution(p_state);
		}
	}

	return status;
}

} //namespace

LuauScriptInstance::LuauScriptInstance(Object *p_owner, const Ref<LuauScript> &p_script) :
		script(p_script), owner(p_owner) {
	ERR_FAIL_COND(p_script.is_null());

	vm_state = script->get_class_vm();
	ERR_FAIL_COND(vm_state.is_null() || !vm_state->is_valid());

	create_instance_table();

	const LuauClassInfo &info = script->get_class_info();
	for (const KeyValue<StringName, LuauClassSignal> &pair : info.signals) {
		if (owner && !owner->has_signal(pair.key)) {
			owner->add_user_signal(pair.value.info);
		}
	}
	for (const KeyValue<StringName, LuauClassProperty> &pair : info.properties) {
		if (pair.value.getter.is_empty() && pair.value.setter.is_empty() && pair.value.default_value.get_type() != Variant::NIL) {
			lua_getref(vm_state->get_lua_state(), instance_table_ref);
			push_variant(vm_state->get_lua_state(), pair.value.default_value);
			lua_setfield(vm_state->get_lua_state(), -2, char_string(pair.key).get_data());
			lua_pop(vm_state->get_lua_state(), 1);
		}
	}
}

void LuauScriptInstance::create_instance_table() {
	lua_State *L = vm_state->get_lua_state();

	lua_newtable(L);
	instance_table_ref = lua_ref(L, -1);
	lua_pop(L, 1);

	lua_getref(L, instance_table_ref);
	push_object(L, owner, LUA_NOTAG);
	lua_setfield(L, -2, "self");
	lua_pop(L, 1);

	install_super_table();
}

void LuauScriptInstance::on_script_soft_reload() {
	ERR_FAIL_COND(script.is_null());

	HashMap<StringName, Variant> saved_properties;
	const LuauClassInfo &info = script->get_class_info();
	for (const KeyValue<StringName, LuauClassProperty> &pair : info.properties) {
		Variant value;
		if (get(pair.key, value)) {
			saved_properties[pair.key] = value;
		}
	}

	if (vm_state.is_valid() && vm_state->is_valid() && instance_table_ref != LUA_NOREF) {
		lua_unref(vm_state->get_lua_state(), instance_table_ref);
		instance_table_ref = LUA_NOREF;
	}

	vm_state = script->get_class_vm();
	ERR_FAIL_COND(vm_state.is_null() || !vm_state->is_valid());

	create_instance_table();

	for (const KeyValue<StringName, Variant> &pair : saved_properties) {
		set(pair.key, pair.value);
	}
}

LuauScriptInstance::~LuauScriptInstance() {
	if (script.is_valid() && owner) {
		MutexLock lock(LuauScriptLanguage::get_singleton()->get_mutex());
		script->instances.erase(owner);
	}

	if (vm_state.is_valid() && vm_state->is_valid() && instance_table_ref != LUA_NOREF) {
		lua_State *L = vm_state->get_lua_state();
		lua_unref(L, instance_table_ref);
		instance_table_ref = LUA_NOREF;
	}
}

bool LuauScriptInstance::call_method(const StringName &p_method, const Variant **p_args, int p_argcount, Variant &r_ret, Callable::CallError &r_error, int p_expected_results) {
	r_error.error = Callable::CallError::CALL_OK;

	ERR_FAIL_COND_V(vm_state.is_null(), false);

	int class_ref = script->get_class_table_ref();
	ERR_FAIL_COND_V(class_ref == LUA_NOREF, false);

	vm_state->get_ref(class_ref);
	vm_state->push_string_name(p_method);
	vm_state->get_table(-2);
	vm_state->remove(-2);

	if (!vm_state->is_function(-1)) {
		vm_state->pop(1);
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return false;
	}

	lua_getref(vm_state->get_lua_state(), instance_table_ref);
	for (int i = 0; i < p_argcount; i++) {
		vm_state->push_variant(*p_args[i]);
	}

#ifdef DEBUG_ENABLED
	const uint64_t profile_start = Time::get_singleton()->get_ticks_usec();
	const String signature = script->get_path() + "::" + String(p_method);
	if (LuauScriptLanguage *lang = LuauScriptLanguage::get_singleton()) {
		if (lang->is_profiling_active()) {
			lang->profile_push_context(StringName(signature));
		}
	}
#endif

	LuaState::Status status = traced_pcall(vm_state, p_argcount + 1, p_expected_results);

#ifdef DEBUG_ENABLED
	if (LuauScriptLanguage *lang = LuauScriptLanguage::get_singleton()) {
		if (lang->is_profiling_active()) {
			lang->profile_record_call(StringName(signature), Time::get_singleton()->get_ticks_usec() - profile_start);
			lang->profile_pop_context();
		}
	}
#endif

	if (status != LuaState::STATUS_OK) {
		if (vm_state->get_top() > 0) {
			ERR_PRINT(String("LuauScriptInstance::call_method error in ") + p_method + ": " + vm_state->to_string_inplace(-1));
		}
		vm_state->set_top(0);
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return false;
	}

	if (p_expected_results > 0) {
		r_ret = to_variant(vm_state->get_lua_state(), -1);
		vm_state->pop(p_expected_results);
	}

	return true;
}

bool LuauScriptInstance::call_method_void(const StringName &p_method, const Variant **p_args, int p_argcount) {
	Callable::CallError err;
	Variant unused;
	return call_method(p_method, p_args, p_argcount, unused, err, 0);
}

bool LuauScriptInstance::call_super_method(const StringName &p_method, const Variant **p_args, int p_argcount, Variant &r_ret, Callable::CallError &r_error, int p_expected_results) {
	r_error.error = Callable::CallError::CALL_OK;

	Ref<Script> base = script->get_base_script();
	Ref<LuauScript> base_luau = base;
	if (base_luau.is_null()) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return false;
	}

	ERR_FAIL_COND_V(vm_state.is_null(), false);

	int class_ref = base_luau->get_class_table_ref();
	ERR_FAIL_COND_V(class_ref == LUA_NOREF, false);

	vm_state->get_ref(class_ref);
	vm_state->push_string_name(p_method);
	vm_state->get_table(-2);
	vm_state->remove(-2);

	if (!vm_state->is_function(-1)) {
		vm_state->pop(1);
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return false;
	}

	lua_getref(vm_state->get_lua_state(), instance_table_ref);
	for (int i = 0; i < p_argcount; i++) {
		vm_state->push_variant(*p_args[i]);
	}

	LuaState::Status status = traced_pcall(vm_state, p_argcount + 1, p_expected_results);
	if (status != LuaState::STATUS_OK) {
		if (vm_state->get_top() > 0) {
			ERR_PRINT(String("LuauScriptInstance::call_super_method error in ") + p_method + ": " + vm_state->to_string_inplace(-1));
		}
		vm_state->set_top(0);
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return false;
	}

	if (p_expected_results > 0) {
		r_ret = to_variant(vm_state->get_lua_state(), -1);
		vm_state->pop(p_expected_results);
	}

	return true;
}

namespace {

struct SuperBindData {
	LuauScriptInstance *instance = nullptr;
	StringName method;
};

static int super_method_trampoline(lua_State *L) {
	SuperBindData *data = static_cast<SuperBindData *>(lua_touserdata(L, lua_upvalueindex(1)));
	ERR_FAIL_NULL_V(data, 0);
	ERR_FAIL_NULL_V(data->instance, 0);

	const int arg_count = lua_gettop(L);
	Variant arg_storage[16];
	const Variant *args[16];
	const int count = MIN(arg_count, 16);
	for (int i = 0; i < count; i++) {
		arg_storage[i] = to_variant(L, i + 1);
		args[i] = &arg_storage[i];
	}

	Callable::CallError err;
	Variant ret;
	if (!data->instance->call_super_method(data->method, args, count, ret, err, 1)) {
		luaL_error(L, "super.%s failed", String(data->method).utf8().get_data());
		return 0;
	}
	push_variant(L, ret);
	return 1;
}

} //namespace

void LuauScriptInstance::install_super_table() {
	Ref<Script> base = script->get_base_script();
	Ref<LuauScript> base_luau = base;
	if (base_luau.is_null() || vm_state.is_null()) {
		return;
	}

	List<MethodInfo> methods;
	base_luau->get_script_method_list(&methods);

	lua_State *L = vm_state->get_lua_state();
	lua_getref(L, instance_table_ref);
	lua_newtable(L);

	for (const MethodInfo &method : methods) {
		void *mem = lua_newuserdata(L, sizeof(SuperBindData));
		SuperBindData *data = new (mem) SuperBindData();
		data->instance = this;
		data->method = method.name;
		lua_pushcclosurek(L, super_method_trampoline, "super", 1, 0);
		lua_setfield(L, -2, String(method.name).utf8().get_data());
	}

	lua_setfield(L, -2, "super");
	lua_pop(L, 1);
}

bool LuauScriptInstance::set(const StringName &p_name, const Variant &p_value) {
	if (script->has_method("_set")) {
		Callable::CallError err;
		const Variant key = p_name;
		const Variant *args[2] = { &key, &p_value };
		Variant result;
		if (call_method("_set", args, 2, result, err, 1) && result.get_type() == Variant::BOOL && bool(result)) {
			return true;
		}
	}

	const LuauClassInfo &info = script->get_class_info();
	if (info.properties.has(p_name)) {
		const LuauClassProperty &prop = info.properties[p_name];
		if (!prop.setter.is_empty()) {
			Callable::CallError err;
			const Variant *args[1] = { &p_value };
			Variant unused;
			return call_method(prop.setter, args, 1, unused, err, 0);
		}

		vm_state->get_ref(instance_table_ref);
		vm_state->push_string_name(p_name);
		vm_state->push_variant(p_value);
		vm_state->set_table(-3);
		vm_state->pop(1);
		return true;
	}
	return false;
}

bool LuauScriptInstance::get(const StringName &p_name, Variant &r_ret) const {
	if (script->has_method("_get")) {
		Callable::CallError err;
		const Variant key = p_name;
		const Variant *args[1] = { &key };
		LuauScriptInstance *mutable_this = const_cast<LuauScriptInstance *>(this);
		if (mutable_this->call_method("_get", args, 1, r_ret, err, 1) && r_ret.get_type() != Variant::NIL) {
			return true;
		}
	}

	const LuauClassInfo &info = script->get_class_info();
	if (info.properties.has(p_name)) {
		const LuauClassProperty &prop = info.properties[p_name];
		if (!prop.getter.is_empty()) {
			Callable::CallError err;
			const Variant **args = nullptr;
			LuauScriptInstance *mutable_this = const_cast<LuauScriptInstance *>(this);
			return mutable_this->call_method(prop.getter, args, 0, r_ret, err, 1);
		}

		LuauScriptInstance *mutable_this = const_cast<LuauScriptInstance *>(this);
		mutable_this->vm_state->get_ref(instance_table_ref);
		mutable_this->vm_state->get_field(-1, p_name);
		if (!mutable_this->vm_state->is_none_or_nil(-1)) {
			r_ret = to_variant(mutable_this->vm_state->get_lua_state(), -1);
			mutable_this->vm_state->pop(2);
			return true;
		}
		mutable_this->vm_state->pop(2);
	}
	return false;
}

void LuauScriptInstance::get_property_list(List<PropertyInfo> *p_properties) const {
	for (const KeyValue<StringName, LuauClassProperty> &pair : script->get_class_info().properties) {
		p_properties->push_back(pair.value.info);
	}
}

Variant::Type LuauScriptInstance::get_property_type(const StringName &p_name, bool *r_is_valid) const {
	if (script->get_class_info().properties.has(p_name)) {
		if (r_is_valid) {
			*r_is_valid = true;
		}
		return script->get_class_info().properties[p_name].info.type;
	}
	if (r_is_valid) {
		*r_is_valid = false;
	}
	return Variant::NIL;
}

void LuauScriptInstance::validate_property(PropertyInfo &p_property) const {
	if (!script->has_method("_validate_property")) {
		return;
	}

	Dictionary property_dict = p_property;
	const Variant property = property_dict;
	const Variant *args[1] = { &property };
	Callable::CallError err;
	Variant unused;
	LuauScriptInstance *mutable_this = const_cast<LuauScriptInstance *>(this);
	if (mutable_this->call_method("_validate_property", args, 1, unused, err, 0) && err.error == Callable::CallError::CALL_OK) {
		p_property = PropertyInfo::from_dict(property_dict);
	}
}

bool LuauScriptInstance::property_can_revert(const StringName &p_name) const {
	Variant default_value;
	return script->get_property_default_value(p_name, default_value);
}

bool LuauScriptInstance::property_get_revert(const StringName &p_name, Variant &r_ret) const {
	return script->get_property_default_value(p_name, r_ret);
}

void LuauScriptInstance::get_method_list(List<MethodInfo> *p_list) const {
	script->get_script_method_list(p_list);
}

bool LuauScriptInstance::has_method(const StringName &p_method) const {
	return script->has_method(p_method);
}

Variant LuauScriptInstance::callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	Variant ret;
	if (call_method(p_method, p_args, p_argcount, ret, r_error, 1)) {
		return ret;
	}
	return Variant();
}

void LuauScriptInstance::notification(int p_notification, bool p_reversed) {
	Node *node = Object::cast_to<Node>(owner);

	switch (p_notification) {
		case Node::NOTIFICATION_ENTER_TREE:
			if (script->has_method("_enter_tree")) {
				const Variant **args = nullptr;
				call_method_void("_enter_tree", args, 0);
			}
			break;
		case Node::NOTIFICATION_EXIT_TREE:
			if (script->has_method("_exit_tree")) {
				const Variant **args = nullptr;
				call_method_void("_exit_tree", args, 0);
			}
			break;
		case Node::NOTIFICATION_READY:
			if (node) {
				if (script->has_method("_process")) {
					node->set_process(true);
				}
				if (script->has_method("_physics_process")) {
					node->set_physics_process(true);
				}
			}
			if (script->has_method("_ready")) {
				const Variant **args = nullptr;
				call_method_void("_ready", args, 0);
			}
			break;
		case Node::NOTIFICATION_PROCESS:
			if (node && script->has_method("_process")) {
				Variant delta = node->get_process_delta_time();
				const Variant *args[1] = { &delta };
				call_method_void("_process", args, 1);
			}
			break;
		case Node::NOTIFICATION_PHYSICS_PROCESS:
			if (node && script->has_method("_physics_process")) {
				Variant delta = node->get_physics_process_delta_time();
				const Variant *args[1] = { &delta };
				call_method_void("_physics_process", args, 1);
			}
			break;
		case Node::NOTIFICATION_APPLICATION_FOCUS_IN:
			if (script->has_method("_application_focus_in")) {
				const Variant **args = nullptr;
				call_method_void("_application_focus_in", args, 0);
			}
			break;
		case Node::NOTIFICATION_APPLICATION_FOCUS_OUT:
			if (script->has_method("_application_focus_out")) {
				const Variant **args = nullptr;
				call_method_void("_application_focus_out", args, 0);
			}
			break;
		case Node::NOTIFICATION_VP_MOUSE_ENTER:
			if (script->has_method("_mouse_enter")) {
				const Variant **args = nullptr;
				call_method_void("_mouse_enter", args, 0);
			}
			break;
		case Node::NOTIFICATION_VP_MOUSE_EXIT:
			if (script->has_method("_mouse_exit")) {
				const Variant **args = nullptr;
				call_method_void("_mouse_exit", args, 0);
			}
			break;
		case Control::NOTIFICATION_MOUSE_ENTER:
			if (Object::cast_to<Control>(owner) && script->has_method("_mouse_enter")) {
				const Variant **args = nullptr;
				call_method_void("_mouse_enter", args, 0);
			}
			break;
		case Control::NOTIFICATION_MOUSE_EXIT:
			if (Object::cast_to<Control>(owner) && script->has_method("_mouse_exit")) {
				const Variant **args = nullptr;
				call_method_void("_mouse_exit", args, 0);
			}
			break;
		case Node::NOTIFICATION_WM_CLOSE_REQUEST:
			if (script->has_method("_wm_close_request")) {
				const Variant **args = nullptr;
				call_method_void("_wm_close_request", args, 0);
			}
			break;
		case CanvasItem::NOTIFICATION_DRAW:
			if (Object::cast_to<CanvasItem>(owner) && script->has_method("_draw")) {
				const Variant **args = nullptr;
				call_method_void("_draw", args, 0);
			}
			break;
		default:
			break;
	}

	Variant value = p_notification;
	const Variant *args[1] = { &value };
	if (script->has_method("_Notification")) {
		call_method_void("_Notification", args, 1);
	}
	if (script->has_method("_notification")) {
		call_method_void("_notification", args, 1);
	}
}

Ref<Script> LuauScriptInstance::get_script() const {
	return script;
}

ScriptLanguage *LuauScriptInstance::get_language() {
	return LuauScriptLanguage::get_singleton();
}
