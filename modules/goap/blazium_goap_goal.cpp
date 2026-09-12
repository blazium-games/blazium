/**************************************************************************/
/*  blazium_goap_goal.cpp                                                 */
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

#include "blazium_goap_goal.h"
#include "blazium_goap_agent.h"

void BlaziumGoapGoal::_bind_methods() {
	ClassDB::bind_method(D_METHOD("init", "actor", "world_state"), &BlaziumGoapGoal::init);
	ClassDB::bind_method(D_METHOD("get_action_name"), &BlaziumGoapGoal::get_action_name);

	ClassDB::bind_method(D_METHOD("is_valid"), &BlaziumGoapGoal::is_valid);
	ClassDB::bind_method(D_METHOD("get_priority"), &BlaziumGoapGoal::get_priority);
	ClassDB::bind_method(D_METHOD("get_base_priority"), &BlaziumGoapGoal::get_base_priority);
	ClassDB::bind_method(D_METHOD("set_base_priority", "priority"), &BlaziumGoapGoal::set_base_priority);

	ClassDB::bind_method(D_METHOD("get_desired_state"), &BlaziumGoapGoal::get_desired_state);
	ClassDB::bind_method(D_METHOD("set_desired_state", "desired_state"), &BlaziumGoapGoal::set_desired_state);

	ClassDB::bind_method(D_METHOD("get_cost", "blackboard"), &BlaziumGoapGoal::get_cost);
	ClassDB::bind_method(D_METHOD("get_base_cost"), &BlaziumGoapGoal::get_base_cost);
	ClassDB::bind_method(D_METHOD("set_base_cost", "cost"), &BlaziumGoapGoal::set_base_cost);

	ClassDB::bind_method(D_METHOD("is_enabled"), &BlaziumGoapGoal::is_enabled);
	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &BlaziumGoapGoal::set_enabled);

	ClassDB::bind_method(D_METHOD("get_default_valid_state"), &BlaziumGoapGoal::get_default_valid_state);
	ClassDB::bind_method(D_METHOD("set_default_valid_state", "default_valid_state"), &BlaziumGoapGoal::set_default_valid_state);

	ClassDB::bind_method(D_METHOD("is_performing"), &BlaziumGoapGoal::is_performing);

	ClassDB::bind_method(D_METHOD("enter"), &BlaziumGoapGoal::enter);
	ClassDB::bind_method(D_METHOD("exit"), &BlaziumGoapGoal::exit);
	ClassDB::bind_method(D_METHOD("perform", "delta"), &BlaziumGoapGoal::perform);
	ClassDB::bind_method(D_METHOD("prepare"), &BlaziumGoapGoal::prepare);
	ClassDB::bind_method(D_METHOD("on_goal_achieved"), &BlaziumGoapGoal::on_goal_achieved);
	ClassDB::bind_method(D_METHOD("on_goal_failed"), &BlaziumGoapGoal::on_goal_failed);

	ClassDB::bind_method(D_METHOD("get_agent"), &BlaziumGoapGoal::get_agent);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "priority"), "set_base_priority", "get_base_priority");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "is_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cost"), "set_base_cost", "get_base_cost");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "default_valid_state"), "set_default_valid_state", "get_default_valid_state");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "desired_state"), "set_desired_state", "get_desired_state");

	GDVIRTUAL_BIND(_is_valid)
	GDVIRTUAL_BIND(_get_priority)
	GDVIRTUAL_BIND(_get_cost, "blackboard")
	GDVIRTUAL_BIND(_enter)
	GDVIRTUAL_BIND(_exit)
	GDVIRTUAL_BIND(_perform, "delta")
	GDVIRTUAL_BIND(_prepare)
	GDVIRTUAL_BIND(_on_goal_achieved)
	GDVIRTUAL_BIND(_on_goal_failed)
}

void BlaziumGoapGoal::init(Node *p_actor, Ref<BlaziumGoapWorldState> p_world_state) {
	actor = p_actor;
	world_state = p_world_state;
}

StringName BlaziumGoapGoal::get_action_name() const {
	return get_name();
}

bool BlaziumGoapGoal::is_valid() const {
	bool valid = default_valid_state;
	if (GDVIRTUAL_CALL(_is_valid, valid)) {
		return valid;
	}
	return valid;
}

int BlaziumGoapGoal::get_cost(const Dictionary &p_blackboard) const {
	int c = cost;
	if (GDVIRTUAL_CALL(_get_cost, p_blackboard, c)) {
		return c;
	}
	return c;
}

int BlaziumGoapGoal::get_priority() const {
	int p = priority;
	if (GDVIRTUAL_CALL(_get_priority, p)) {
		return p;
	}
	return priority;
}

void BlaziumGoapGoal::enter() {
	performing = true;
	GDVIRTUAL_CALL(_enter);
}

void BlaziumGoapGoal::exit() {
	performing = false;
	GDVIRTUAL_CALL(_exit);
}

void BlaziumGoapGoal::perform(double p_delta) {
	GDVIRTUAL_CALL(_perform, p_delta);
}

void BlaziumGoapGoal::prepare() {
	GDVIRTUAL_CALL(_prepare);
}

void BlaziumGoapGoal::on_goal_achieved() {
	GDVIRTUAL_CALL(_on_goal_achieved);
}

void BlaziumGoapGoal::on_goal_failed() {
	GDVIRTUAL_CALL(_on_goal_failed);
}

BlaziumGoapGoal::BlaziumGoapGoal() {
	default_valid_state = true;
	priority = 0;
	enabled = true;
	cost = 0;
	performing = false;
	actor = nullptr;
	agent = nullptr;
}

BlaziumGoapGoal::~BlaziumGoapGoal() {
}
