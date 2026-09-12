/**************************************************************************/
/*  autowork_stubber.cpp                                                  */
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

#include "autowork_stubber.h"

void AutoworkStubber::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_return", "object", "method_name", "return_value", "args"), &AutoworkStubber::set_return, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("get_return", "object", "method_name", "args"), &AutoworkStubber::get_return, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("should_call_super", "object", "method_name", "args"), &AutoworkStubber::should_call_super, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("clear"), &AutoworkStubber::clear);
}

AutoworkStubber::AutoworkStubber() {
}

AutoworkStubber::~AutoworkStubber() {
}

void AutoworkStubber::set_return(const Variant &p_object, const StringName &p_method_name, const Variant &p_return_value, const Variant &p_args) {
	if (!returns.has(p_object)) {
		returns[p_object] = Dictionary();
	}
	Dictionary obj_returns = returns[p_object];
	if (!obj_returns.has(p_method_name)) {
		obj_returns[p_method_name] = Dictionary();
	}
	Dictionary method_returns = obj_returns[p_method_name];
	method_returns[p_args] = p_return_value;
}

Variant AutoworkStubber::get_return(const Variant &p_object, const StringName &p_method_name, const Array &p_args) {
	if (!returns.has(p_object)) {
		return Variant();
	}
	Dictionary obj_returns = returns[p_object];
	if (!obj_returns.has(p_method_name)) {
		return Variant();
	}
	Dictionary method_returns = obj_returns[p_method_name];

	// Exact match first
	if (method_returns.has(p_args)) {
		return method_returns[p_args];
	}
	// Any-args match fallback
	if (method_returns.has(Variant())) {
		return method_returns[Variant()];
	}
	return Variant();
}

bool AutoworkStubber::should_call_super(const Variant &p_object, const StringName &p_method_name, const Array &p_args) {
	if (!returns.has(p_object)) {
		return true;
	}
	Dictionary obj_returns = returns[p_object];
	if (!obj_returns.has(p_method_name)) {
		return true;
	}
	Dictionary method_returns = obj_returns[p_method_name];

	if (method_returns.has(p_args) || method_returns.has(Variant())) {
		return false;
	}
	return true;
}

void AutoworkStubber::clear() {
	returns.clear();
	call_supers.clear();
}
