/**************************************************************************/
/*  blazium_goap_world_state.cpp                                          */
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

#include "blazium_goap_world_state.h"

void BlaziumGoapWorldState::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_elements"), &BlaziumGoapWorldState::get_elements);
	ClassDB::bind_method(D_METHOD("get_state", "state_name", "default"), &BlaziumGoapWorldState::get_state, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("set_state", "state_name", "value"), &BlaziumGoapWorldState::set_state);
	ClassDB::bind_method(D_METHOD("erase_state", "state_name"), &BlaziumGoapWorldState::erase_state);
	ClassDB::bind_method(D_METHOD("clear_state"), &BlaziumGoapWorldState::clear_state);

	ClassDB::bind_method(D_METHOD("get_state_dictionary"), &BlaziumGoapWorldState::get_state_dictionary);
	ClassDB::bind_method(D_METHOD("set_state_dictionary", "state"), &BlaziumGoapWorldState::set_state_dictionary);
	ClassDB::bind_static_method("BlaziumGoapWorldState", D_METHOD("is_satisfied", "blackboard_value", "desired_value"), &BlaziumGoapWorldState::is_satisfied);

	ADD_SIGNAL(MethodInfo("state_updated"));

	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "state"), "set_state_dictionary", "get_state_dictionary");
}

Array BlaziumGoapWorldState::get_elements() const {
	return state.keys();
}

Variant BlaziumGoapWorldState::get_state(const StringName &p_state_name, const Variant &p_default) const {
	if (state.has(p_state_name)) {
		return state[p_state_name];
	}
	return p_default;
}

void BlaziumGoapWorldState::set_state(const StringName &p_state_name, const Variant &p_value) {
	state[p_state_name] = p_value;
	emit_signal("state_updated");
}

void BlaziumGoapWorldState::erase_state(const StringName &p_state_name) {
	state.erase(p_state_name);
	emit_signal("state_updated");
}

void BlaziumGoapWorldState::clear_state() {
	state.clear();
	emit_signal("state_updated");
}

void BlaziumGoapWorldState::set_state_dictionary(const Dictionary &p_state) {
	state = p_state;
	emit_signal("state_updated");
}

bool BlaziumGoapWorldState::is_satisfied(const Variant &p_blackboard_value, const Variant &p_desired_value) {
	if (p_desired_value.get_type() == Variant::BOOL) {
		bool as_bool = false;
		if (p_blackboard_value.get_type() != Variant::NIL) {
			switch (p_blackboard_value.get_type()) {
				case Variant::BOOL:
					as_bool = p_blackboard_value;
					break;
				case Variant::INT:
				case Variant::FLOAT:
					as_bool = (double)p_blackboard_value != 0.0;
					break;
				default:
					as_bool = true;
					break;
			}
		}
		return as_bool == (bool)p_desired_value;
	}
	// For numbers, handle potential float/int type mixups in dictionary keys effectively
	if ((p_blackboard_value.get_type() == Variant::INT || p_blackboard_value.get_type() == Variant::FLOAT) &&
			(p_desired_value.get_type() == Variant::INT || p_desired_value.get_type() == Variant::FLOAT)) {
		return (double)p_blackboard_value == (double)p_desired_value;
	}

	bool valid = false;
	Variant result;
	Variant::evaluate(Variant::OP_EQUAL, p_blackboard_value, p_desired_value, result, valid);
	if (valid) {
		return result;
	}
	return false;
}

BlaziumGoapWorldState::BlaziumGoapWorldState() {
}

BlaziumGoapWorldState::~BlaziumGoapWorldState() {
}
