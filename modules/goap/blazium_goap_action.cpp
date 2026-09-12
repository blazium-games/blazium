/**************************************************************************/
/*  blazium_goap_action.cpp                                               */
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

#include "blazium_goap_action.h"
#include "blazium_goap_agent.h"
#include "blazium_goap_goal.h"

void BlaziumGoapAction::_bind_methods() {
	ClassDB::bind_method(D_METHOD("init", "actor", "world_state"), &BlaziumGoapAction::init);
	ClassDB::bind_method(D_METHOD("get_action_name"), &BlaziumGoapAction::get_action_name);

	ClassDB::bind_method(D_METHOD("is_valid"), &BlaziumGoapAction::is_valid);
	ClassDB::bind_method(D_METHOD("get_cost", "blackboard"), &BlaziumGoapAction::get_cost);
	ClassDB::bind_method(D_METHOD("get_preconditions"), &BlaziumGoapAction::get_preconditions);
	ClassDB::bind_method(D_METHOD("set_preconditions", "preconditions"), &BlaziumGoapAction::set_preconditions);
	ClassDB::bind_method(D_METHOD("get_effects"), &BlaziumGoapAction::get_effects);
	ClassDB::bind_method(D_METHOD("set_effects_dict", "effects"), &BlaziumGoapAction::set_effects);
	ClassDB::bind_method(D_METHOD("apply_effects"), &BlaziumGoapAction::apply_effects);

	ClassDB::bind_method(D_METHOD("get_base_cost"), &BlaziumGoapAction::get_base_cost);
	ClassDB::bind_method(D_METHOD("set_base_cost", "cost"), &BlaziumGoapAction::set_base_cost);

	ClassDB::bind_method(D_METHOD("prepare_action"), &BlaziumGoapAction::prepare_action);
	ClassDB::bind_method(D_METHOD("is_enabled"), &BlaziumGoapAction::is_enabled);
	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &BlaziumGoapAction::set_enabled);

	ClassDB::bind_method(D_METHOD("enter"), &BlaziumGoapAction::enter);
	ClassDB::bind_method(D_METHOD("exit"), &BlaziumGoapAction::exit);
	ClassDB::bind_method(D_METHOD("perform", "delta"), &BlaziumGoapAction::perform);

	ClassDB::bind_method(D_METHOD("get_state", "key", "default"), &BlaziumGoapAction::get_state, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("set_state", "key", "value"), &BlaziumGoapAction::set_state);

	ClassDB::bind_method(D_METHOD("get_agent"), &BlaziumGoapAction::get_agent);
	ClassDB::bind_method(D_METHOD("get_goal"), &BlaziumGoapAction::get_goal);

	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "effects"), "set_effects_dict", "get_effects");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "preconditions"), "set_preconditions", "get_preconditions");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cost"), "set_base_cost", "get_base_cost");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "is_enabled");

	GDVIRTUAL_BIND(_is_valid)
	GDVIRTUAL_BIND(_get_cost, "blackboard")
	GDVIRTUAL_BIND(_prepare_action)
	GDVIRTUAL_BIND(_enter)
	GDVIRTUAL_BIND(_exit)
	GDVIRTUAL_BIND(_perform, "delta")
}

void BlaziumGoapAction::init(Node *p_actor, Ref<BlaziumGoapWorldState> p_world_state) {
	actor = p_actor;
	world_state = p_world_state;
}

StringName BlaziumGoapAction::get_action_name() const {
	return get_name();
}

bool BlaziumGoapAction::is_valid() const {
	bool valid = enabled;
	if (GDVIRTUAL_CALL(_is_valid, valid)) {
		return valid;
	}
	return valid;
}

int BlaziumGoapAction::get_cost(const Dictionary &p_blackboard) const {
	int c = cost;
	if (GDVIRTUAL_CALL(_get_cost, p_blackboard, c)) {
		return c;
	}
	return c;
}

void BlaziumGoapAction::prepare_action() {
	GDVIRTUAL_CALL(_prepare_action);
}

void BlaziumGoapAction::apply_effects() {
	if (world_state.is_valid()) {
		Array keys = effects.keys();
		for (int i = 0; i < keys.size(); i++) {
			world_state->set_state(keys[i], effects[keys[i]]);
		}
	}
}

void BlaziumGoapAction::enter() {
	GDVIRTUAL_CALL(_enter);
}

void BlaziumGoapAction::exit() {
	GDVIRTUAL_CALL(_exit);
}

bool BlaziumGoapAction::perform(double p_delta) {
	bool ret = false;
	if (GDVIRTUAL_CALL(_perform, p_delta, ret)) {
		return ret;
	}
	return false;
}

Variant BlaziumGoapAction::get_state(const StringName &p_key, const Variant &p_default) const {
	if (world_state.is_valid()) {
		return world_state->get_state(p_key, p_default);
	}
	return p_default;
}

void BlaziumGoapAction::set_state(const StringName &p_key, const Variant &p_value) {
	if (world_state.is_valid()) {
		world_state->set_state(p_key, p_value);
	}
}

BlaziumGoapGoal *BlaziumGoapAction::get_goal() const {
	if (agent) {
		return agent->get_current_goal();
	}
	return nullptr;
}

BlaziumGoapAction::BlaziumGoapAction() {
	cost = 1;
	enabled = true;
	actor = nullptr;
	agent = nullptr;
}

BlaziumGoapAction::~BlaziumGoapAction() {
}
