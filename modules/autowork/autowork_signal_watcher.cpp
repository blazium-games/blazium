/**************************************************************************/
/*  autowork_signal_watcher.cpp                                           */
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

#include "autowork_signal_watcher.h"

// --- AutoworkSignalHook ---

void AutoworkSignalHook::_bind_methods() {
	ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "_on_emitted", &AutoworkSignalHook::_on_emitted, MethodInfo("_on_emitted"));
}

AutoworkSignalHook::AutoworkSignalHook() {
}

AutoworkSignalHook::~AutoworkSignalHook() {
}

void AutoworkSignalHook::setup(Object *p_target, const StringName &p_signal, Ref<AutoworkSignalWatcher> p_watcher) {
	target = p_target;
	signal = p_signal;
	watcher = p_watcher;
}

Variant AutoworkSignalHook::_on_emitted(const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	if (watcher.is_valid() && target) {
		Array args;
		for (int i = 0; i < p_argcount; i++) {
			args.push_back(*p_args[i]);
		}
		watcher->record_emission(target, signal, args);
	}
	return Variant();
}

// --- AutoworkSignalWatcher ---

void AutoworkSignalWatcher::_bind_methods() {
	ClassDB::bind_method(D_METHOD("watch_signal", "object", "signal"), &AutoworkSignalWatcher::watch_signal);
	ClassDB::bind_method(D_METHOD("watch_signals", "object"), &AutoworkSignalWatcher::watch_signals);
	ClassDB::bind_method(D_METHOD("did_emit", "object", "signal"), &AutoworkSignalWatcher::did_emit);
	ClassDB::bind_method(D_METHOD("get_emissions", "object", "signal"), &AutoworkSignalWatcher::get_emissions);
	ClassDB::bind_method(D_METHOD("clear"), &AutoworkSignalWatcher::clear);
}

AutoworkSignalWatcher::AutoworkSignalWatcher() {
}

AutoworkSignalWatcher::~AutoworkSignalWatcher() {
}

void AutoworkSignalWatcher::watch_signal(Object *p_object, const StringName &p_signal) {
	ERR_FAIL_NULL_MSG(p_object, "p_object is null.");

	AutoworkSignalHook *hook = memnew(AutoworkSignalHook);
	hook->setup(p_object, p_signal, this);
	hooks.push_back(hook);

	p_object->connect(p_signal, Callable(hook, "_on_emitted"));
}

void AutoworkSignalWatcher::watch_signals(Object *p_object) {
	if (!p_object) {
		return;
	}

	List<MethodInfo> signal_list;
	p_object->get_signal_list(&signal_list);

	for (const MethodInfo &mi : signal_list) {
		watch_signal(p_object, mi.name);
	}
}

void AutoworkSignalWatcher::record_emission(Object *p_object, const StringName &p_signal, const Array &p_args) {
	if (!emissions.has(p_object)) {
		emissions[p_object] = Dictionary();
	}
	Dictionary obj_emissions = emissions[p_object];

	if (!obj_emissions.has(p_signal)) {
		obj_emissions[p_signal] = Array();
	}
	Array signal_emissions = obj_emissions[p_signal];
	signal_emissions.push_back(p_args);
}

bool AutoworkSignalWatcher::did_emit(Object *p_object, const StringName &p_signal) {
	if (!emissions.has(p_object)) {
		return false;
	}
	Dictionary obj_emissions = emissions[p_object];
	if (!obj_emissions.has(p_signal)) {
		return false;
	}
	Array signal_emissions = obj_emissions[p_signal];
	return signal_emissions.size() > 0;
}

Array AutoworkSignalWatcher::get_emissions(Object *p_object, const StringName &p_signal) {
	if (!emissions.has(p_object)) {
		return Array();
	}
	Dictionary obj_emissions = emissions[p_object];
	if (!obj_emissions.has(p_signal)) {
		return Array();
	}
	return obj_emissions[p_signal];
}

int AutoworkSignalWatcher::get_emit_count(Object *p_object, const StringName &p_signal) {
	Array ems = get_emissions(p_object, p_signal);
	return ems.size();
}

Array AutoworkSignalWatcher::get_signal_parameters(Object *p_object, const StringName &p_signal, int p_index) {
	Array ems = get_emissions(p_object, p_signal);
	if (ems.is_empty()) {
		return Array();
	}

	if (p_index == -1) {
		p_index = ems.size() - 1;
	}

	if (p_index >= 0 && p_index < ems.size()) {
		return ems[p_index];
	}
	return Array();
}

void AutoworkSignalWatcher::clear() {
	emissions.clear();
	for (int i = 0; i < hooks.size(); i++) {
		AutoworkSignalHook *hook = Object::cast_to<AutoworkSignalHook>(hooks[i]);
		if (hook) {
			memdelete(hook);
		}
	}
	hooks.clear();
}
