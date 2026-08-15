/**************************************************************************/
/*  autowork_signal_watcher.h                                             */
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

#include "core/object/ref_counted.h"

class AutoworkSignalWatcher;

class AutoworkSignalHook : public Object {
	GDCLASS(AutoworkSignalHook, Object);

	Object *target = nullptr;
	StringName signal;
	Ref<AutoworkSignalWatcher> watcher;

protected:
	static void _bind_methods();

public:
	AutoworkSignalHook();
	~AutoworkSignalHook();

	void setup(Object *p_target, const StringName &p_signal, Ref<AutoworkSignalWatcher> p_watcher);
	Variant _on_emitted(const Variant **p_args, int p_argcount, Callable::CallError &r_error);
};

class AutoworkSignalWatcher : public RefCounted {
	GDCLASS(AutoworkSignalWatcher, RefCounted);

protected:
	static void _bind_methods();

	// Structure: Object (Variant) -> Signal Name (StringName) -> Array of Emissions (each is an Array of args)
	Dictionary emissions;

	// List of hook objects to keep them alive
	Array hooks;

public:
	AutoworkSignalWatcher();
	~AutoworkSignalWatcher();

	void watch_signal(Object *p_object, const StringName &p_signal);
	void watch_signals(Object *p_object);
	void record_emission(Object *p_object, const StringName &p_signal, const Array &p_args);

	bool did_emit(Object *p_object, const StringName &p_signal);
	Array get_emissions(Object *p_object, const StringName &p_signal);
	int get_emit_count(Object *p_object, const StringName &p_signal);
	Array get_signal_parameters(Object *p_object, const StringName &p_signal, int p_index = -1);
	void clear();
};
