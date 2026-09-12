/**************************************************************************/
/*  autowork_spy.cpp                                                      */
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

#include "autowork_spy.h"

void AutoworkSpy::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_call", "object", "method_name", "args"), &AutoworkSpy::add_call);
	ClassDB::bind_method(D_METHOD("was_called", "object", "method_name", "args"), &AutoworkSpy::was_called, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("call_count", "object", "method_name", "args"), &AutoworkSpy::call_count, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("get_call_parameters", "object", "method_name", "index"), &AutoworkSpy::get_call_parameters, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("clear"), &AutoworkSpy::clear);
}

AutoworkSpy::AutoworkSpy() {
}

AutoworkSpy::~AutoworkSpy() {
}

void AutoworkSpy::add_call(const Variant &p_object, const StringName &p_method_name, const Array &p_args) {
	if (!calls.has(p_object)) {
		calls[p_object] = Dictionary();
	}
	Dictionary obj_calls = calls[p_object];
	if (!obj_calls.has(p_method_name)) {
		obj_calls[p_method_name] = Array();
	}
	Array method_calls = obj_calls[p_method_name];
	method_calls.push_back(p_args);
}

bool AutoworkSpy::was_called(const Variant &p_object, const StringName &p_method_name, const Variant &p_args) {
	return call_count(p_object, p_method_name, p_args) > 0;
}

int AutoworkSpy::call_count(const Variant &p_object, const StringName &p_method_name, const Variant &p_args) {
	if (!calls.has(p_object)) {
		return 0;
	}
	Dictionary obj_calls = calls[p_object];
	if (!obj_calls.has(p_method_name)) {
		return 0;
	}
	Array method_calls = obj_calls[p_method_name];

	if (p_args.get_type() == Variant::NIL) {
		return method_calls.size();
	}

	int count = 0;
	for (int i = 0; i < method_calls.size(); i++) {
		Array call_args = method_calls[i];
		if (Variant(call_args) == p_args) {
			count++;
		}
	}
	return count;
}

Array AutoworkSpy::get_call_parameters(const Variant &p_object, const StringName &p_method_name, int p_index) {
	if (!calls.has(p_object)) {
		return Array();
	}
	Dictionary obj_calls = calls[p_object];
	if (!obj_calls.has(p_method_name)) {
		return Array();
	}
	Array method_calls = obj_calls[p_method_name];

	if (method_calls.is_empty()) {
		return Array();
	}

	if (p_index < 0) {
		return method_calls[method_calls.size() - 1]; // Return latest by default
	} else if (p_index < method_calls.size()) {
		return method_calls[p_index];
	}
	return Array();
}

void AutoworkSpy::clear() {
	calls.clear();
}
