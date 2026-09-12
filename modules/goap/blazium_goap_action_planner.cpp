/**************************************************************************/
/*  blazium_goap_action_planner.cpp                                       */
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

#include "blazium_goap_action_planner.h"
#include "blazium_goap_world_state.h"

void BlaziumGoapActionPlanner::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_actions", "actions"), &BlaziumGoapActionPlanner::set_actions);
	ClassDB::bind_method(D_METHOD("get_plan", "goal", "blackboard"), &BlaziumGoapActionPlanner::get_plan, DEFVAL(Dictionary()));
}

void BlaziumGoapActionPlanner::set_actions(const TypedArray<BlaziumGoapAction> &p_actions) {
	actions = p_actions;
}

Array BlaziumGoapActionPlanner::get_plan(BlaziumGoapGoal *p_goal, const Dictionary &p_blackboard) {
	if (!p_goal) {
		return Array();
	}

	for (int i = 0; i < actions.size(); ++i) {
		BlaziumGoapAction *action = Object::cast_to<BlaziumGoapAction>(actions[i]);
		if (action) {
			action->prepare_action();
		}
	}

	Dictionary desired_state = p_goal->get_desired_state().duplicate();

	if (desired_state.is_empty()) {
		return Array();
	}

	return _find_best_plan(p_goal, desired_state, p_blackboard);
}

Array BlaziumGoapActionPlanner::_find_best_plan(BlaziumGoapGoal *p_goal, const Dictionary &p_desired_state, const Dictionary &p_blackboard) {
	BlaziumGoapPlanStep *root = memnew(BlaziumGoapPlanStep);
	root->action = p_goal;
	root->state = p_desired_state;

	if (_build_plans(root, p_blackboard)) {
		Vector<BlaziumGoapPlanResult> plans;
		_transform_tree_into_array(root, p_blackboard, plans);
		Array best_plan = _get_cheapest_plan(plans);
		memdelete(root);
		return best_plan;
	}

	memdelete(root);
	return Array();
}

Array BlaziumGoapActionPlanner::_get_cheapest_plan(const Vector<BlaziumGoapPlanResult> &p_plans) {
	if (p_plans.is_empty()) {
		return Array();
	}

	int best_idx = -1;
	int best_cost = -1;

	for (int i = 0; i < p_plans.size(); i++) {
		if (best_idx == -1 || p_plans[i].cost < best_cost) {
			best_idx = i;
			best_cost = p_plans[i].cost;
		}
	}

	if (best_idx != -1) {
		return p_plans[best_idx].actions;
	}

	return Array();
}

bool BlaziumGoapActionPlanner::_build_plans(BlaziumGoapPlanStep *p_step, const Dictionary &p_blackboard) {
	bool has_followup = false;

	Dictionary state = p_step->state.duplicate();

	Array keys = p_step->state.keys();
	for (int i = 0; i < keys.size(); i++) {
		StringName s = keys[i];
		Variant blackboard_value = p_blackboard.get(s, Variant());
		Variant desired_value = state[s];

		bool is_state_satisfied = BlaziumGoapWorldState::is_satisfied(blackboard_value, desired_value);

		if (is_state_satisfied) {
			state.erase(s);
		}
	}

	if (state.is_empty()) {
		return true;
	}

	for (int i = 0; i < actions.size(); i++) {
		BlaziumGoapAction *action = Object::cast_to<BlaziumGoapAction>(actions[i]);
		if (!action || !action->is_valid()) {
			continue;
		}

		bool should_use_action = false;
		Dictionary effects = action->get_effects();
		Dictionary desired_state = state.duplicate();

		Array desired_keys = state.keys();
		for (int j = 0; j < desired_keys.size(); j++) {
			StringName s = desired_keys[j];
			if (effects.has(s) && desired_state[s] == effects[s]) {
				desired_state.erase(s);
				should_use_action = true;
			}
		}

		if (should_use_action) {
			Dictionary preconditions = action->get_preconditions();
			Array pre_keys = preconditions.keys();
			for (int j = 0; j < pre_keys.size(); j++) {
				StringName p = pre_keys[j];
				desired_state[p] = preconditions[p];
			}

			BlaziumGoapPlanStep *s = memnew(BlaziumGoapPlanStep);
			s->action = action;
			s->state = desired_state;

			if (desired_state.is_empty() || _build_plans(s, p_blackboard.duplicate())) {
				p_step->children.push_back(s);
				has_followup = true;
			} else {
				memdelete(s);
			}
		}
	}

	return has_followup;
}

void BlaziumGoapActionPlanner::_transform_tree_into_array(BlaziumGoapPlanStep *p_step, const Dictionary &p_blackboard, Vector<BlaziumGoapPlanResult> &r_plans) {
	if (p_step->children.is_empty()) {
		int cost = 0;
		if (p_step->action) {
			if (BlaziumGoapAction *a = Object::cast_to<BlaziumGoapAction>(p_step->action)) {
				cost = a->get_cost(p_blackboard);
			} else if (BlaziumGoapGoal *g = Object::cast_to<BlaziumGoapGoal>(p_step->action)) {
				cost = g->get_cost(p_blackboard);
			}
		}

		BlaziumGoapPlanResult res;
		res.actions.push_back(p_step->action);
		res.cost = cost;
		r_plans.push_back(res);
		return;
	}

	for (int i = 0; i < p_step->children.size(); i++) {
		Vector<BlaziumGoapPlanResult> child_plans;
		_transform_tree_into_array(p_step->children[i], p_blackboard, child_plans);

		for (int j = 0; j < child_plans.size(); j++) {
			BlaziumGoapPlanResult cp = child_plans[j];
			if (p_step->action) {
				cp.actions.push_back(p_step->action);
				if (BlaziumGoapAction *a = Object::cast_to<BlaziumGoapAction>(p_step->action)) {
					cp.cost += a->get_cost(p_blackboard);
				} else if (BlaziumGoapGoal *g = Object::cast_to<BlaziumGoapGoal>(p_step->action)) {
					cp.cost += g->get_cost(p_blackboard);
				}
			}
			r_plans.push_back(cp);
		}
	}
}

BlaziumGoapActionPlanner::BlaziumGoapActionPlanner() {
}

BlaziumGoapActionPlanner::~BlaziumGoapActionPlanner() {
}
