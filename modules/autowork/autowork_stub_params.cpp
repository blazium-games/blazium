/**************************************************************************/
/*  autowork_stub_params.cpp                                              */
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

#include "autowork_stub_params.h"

void AutoworkStubParams::_bind_methods() {
	ClassDB::bind_method(D_METHOD("to_return", "value"), &AutoworkStubParams::to_return);
	ClassDB::bind_method(D_METHOD("to_do_nothing"), &AutoworkStubParams::to_do_nothing);
	ClassDB::bind_method(D_METHOD("to_call_super"), &AutoworkStubParams::to_call_super);
	ClassDB::bind_method(D_METHOD("when_passed", "args"), &AutoworkStubParams::when_passed);
}

AutoworkStubParams::AutoworkStubParams() {
}

AutoworkStubParams::~AutoworkStubParams() {
}

void AutoworkStubParams::init(Ref<AutoworkStubber> p_stubber, const Variant &p_object, const StringName &p_method) {
	stubber = p_stubber;
	target_object = p_object;
	target_method = p_method;
	target_args = Variant(); // Any args initially
}

Ref<AutoworkStubParams> AutoworkStubParams::to_return(const Variant &p_value) {
	if (stubber.is_valid()) {
		stubber->set_return(target_object, target_method, p_value, target_args);
	}
	return Ref<AutoworkStubParams>(this);
}

Ref<AutoworkStubParams> AutoworkStubParams::to_do_nothing() {
	if (stubber.is_valid()) {
		stubber->set_return(target_object, target_method, Variant(), target_args);
	}
	return Ref<AutoworkStubParams>(this);
}

Ref<AutoworkStubParams> AutoworkStubParams::to_call_super() {
	// Current stubber implementation uses absence of return to fallthrough to super
	// Actually we need a specific way to mark "call_super" true, but for now our doubler
	// defaults to not calling super unless specified, OR defaults to super unless stubbed.
	// In GUT, you stub it to do nothing OR to call super.
	if (stubber.is_valid()) {
		// Clear specifically this stub match to let it fallback to super if configured
		stubber->set_return(target_object, target_method, Variant(), target_args);
	}
	return Ref<AutoworkStubParams>(this);
}

Ref<AutoworkStubParams> AutoworkStubParams::when_passed(const Variant &p_args) {
	target_args = p_args;
	return Ref<AutoworkStubParams>(this);
}
