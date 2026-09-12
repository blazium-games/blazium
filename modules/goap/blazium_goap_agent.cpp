/**************************************************************************/
/*  blazium_goap_agent.cpp                                                */
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

#include "blazium_goap_agent.h"
#include "blazium_goap_action.h"
#include "blazium_goap_goal.h"
#include "core/debugger/engine_debugger.h"

void BlaziumGoapAgent::_bind_methods() {
	ClassDB::bind_method(D_METHOD("init", "actor"), &BlaziumGoapAgent::init);

	ClassDB::bind_method(D_METHOD("get_current_goal"), &BlaziumGoapAgent::get_current_goal);
	ClassDB::bind_method(D_METHOD("get_current_action"), &BlaziumGoapAgent::get_current_action);
	ClassDB::bind_method(D_METHOD("get_current_plan"), &BlaziumGoapAgent::get_current_plan);
	ClassDB::bind_method(D_METHOD("get_world_state"), &BlaziumGoapAgent::get_world_state);

	ClassDB::bind_method(D_METHOD("set_goals_path", "path"), &BlaziumGoapAgent::set_goals_path);
	ClassDB::bind_method(D_METHOD("get_goals_path"), &BlaziumGoapAgent::get_goals_path);

	ClassDB::bind_method(D_METHOD("set_actions_path", "path"), &BlaziumGoapAgent::set_actions_path);
	ClassDB::bind_method(D_METHOD("get_actions_path"), &BlaziumGoapAgent::get_actions_path);

	ClassDB::bind_method(D_METHOD("set_debug_enabled", "enabled"), &BlaziumGoapAgent::set_debug_enabled);
	ClassDB::bind_method(D_METHOD("is_debug_enabled"), &BlaziumGoapAgent::is_debug_enabled);

	ClassDB::bind_method(D_METHOD("set_debug_prefix", "prefix"), &BlaziumGoapAgent::set_debug_prefix);
	ClassDB::bind_method(D_METHOD("get_debug_prefix"), &BlaziumGoapAgent::get_debug_prefix);

	ClassDB::bind_method(D_METHOD("debug_select"), &BlaziumGoapAgent::debug_select);

	ClassDB::bind_method(D_METHOD("_on_debug_goal_changed", "goal"), &BlaziumGoapAgent::_on_debug_goal_changed);
	ClassDB::bind_method(D_METHOD("_on_debug_plan_changed", "plan"), &BlaziumGoapAgent::_on_debug_plan_changed);
	ClassDB::bind_method(D_METHOD("_on_debug_action_changed", "action"), &BlaziumGoapAgent::_on_debug_action_changed);
	ClassDB::bind_method(D_METHOD("_on_debug_world_state_updated"), &BlaziumGoapAgent::_on_debug_world_state_updated);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "goals_node"), "set_goals_path", "get_goals_path");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "actions_node"), "set_actions_path", "get_actions_path");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_enabled"), "set_debug_enabled", "is_debug_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "debug_prefix"), "set_debug_prefix", "get_debug_prefix");

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "current_goal", PROPERTY_HINT_RESOURCE_TYPE, "BlaziumGoapGoal"), "", "get_current_goal");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "current_plan"), "", "get_current_plan");

	ADD_SIGNAL(MethodInfo("goal_changed", PropertyInfo(Variant::OBJECT, "goal", PROPERTY_HINT_RESOURCE_TYPE, "BlaziumGoapGoal")));
	ADD_SIGNAL(MethodInfo("plan_changed", PropertyInfo(Variant::ARRAY, "plan")));
	ADD_SIGNAL(MethodInfo("action_changed", PropertyInfo(Variant::OBJECT, "action", PROPERTY_HINT_RESOURCE_TYPE, "BlaziumGoapAction")));
}

void BlaziumGoapAgent::init(Node *p_actor) {
	actor = p_actor;
	world_state.instantiate();
	if (is_inside_tree()) {
		debug_id = String(get_path());
	} else {
		debug_id = String::num_int64(get_instance_id());
	}

	TypedArray<BlaziumGoapAction> actions;

	Node *goals_n = get_node_or_null(goals_path);
	if (goals_n) {
		for (int i = 0; i < goals_n->get_child_count(); i++) {
			BlaziumGoapGoal *g = Object::cast_to<BlaziumGoapGoal>(goals_n->get_child(i));
			if (g) {
				goals.push_back(g);
				g->set_agent(this);
			}
		}
	}

	Node *actions_n = get_node_or_null(actions_path);
	if (actions_n) {
		for (int i = 0; i < actions_n->get_child_count(); i++) {
			BlaziumGoapAction *a = Object::cast_to<BlaziumGoapAction>(actions_n->get_child(i));
			if (a) {
				actions.push_back(a);
				a->set_agent(this);
			}
		}
	}

	action_planner.instantiate();
	action_planner->set_actions(actions);

	for (int i = 0; i < goals.size(); i++) {
		BlaziumGoapGoal *g = Object::cast_to<BlaziumGoapGoal>(goals[i]);
		g->init(actor, world_state);
	}

	for (int i = 0; i < actions.size(); i++) {
		BlaziumGoapAction *a = Object::cast_to<BlaziumGoapAction>(actions[i]);
		a->init(actor, world_state);
	}

	_connect_debug_signals();
	_send_debug_register(actions);
}

void BlaziumGoapAgent::_notification(int p_what) {
	if (p_what == NOTIFICATION_PROCESS) {
		if (Engine::get_singleton()->is_editor_hint()) {
			return;
		}

		double delta = get_process_delta_time();

		BlaziumGoapGoal *goal = _get_best_goal();
		if (current_goal == nullptr || goal != current_goal) {
			if (current_goal) {
				current_goal->exit();
			}
			current_goal = goal;
			if (current_goal) {
				current_goal->enter();
				current_goal->prepare();
			}
			emit_signal("goal_changed", current_goal);

			Array plan;
			if (current_goal) {
				Dictionary blackboard;
				if (actor != nullptr && actor->is_class("Node3D")) {
					blackboard["global_position"] = actor->get("global_position");
				} else if (actor != nullptr && actor->is_class("Node2D")) {
					blackboard["global_position"] = actor->get("global_position");
				}

				Dictionary st = world_state->get_state_dictionary();
				Array keys = st.keys();
				for (int i = 0; i < keys.size(); i++) {
					blackboard[keys[i]] = st[keys[i]];
				}

				plan = action_planner->get_plan(current_goal, blackboard);
				last_blackboard = blackboard;
			}

			current_plan = plan;
			emit_signal("plan_changed", current_plan);
			current_plan_step = 0;

			if (current_plan.is_empty()) {
				if (current_goal) {
					current_goal->on_goal_failed();
					current_goal->exit();
				}
				current_goal = nullptr;
			} else if (current_plan.size() == 1) { // 1 means it's just the goal node inside it natively
				if (current_goal) {
					current_goal->on_goal_achieved();
				}
				finished_last_plan = true;
			}
		} else if (current_goal != nullptr) {
			_follow_plan(current_plan, delta);
		}
	}
}

BlaziumGoapGoal *BlaziumGoapAgent::_get_best_goal() {
	BlaziumGoapGoal *highest_priority = nullptr;

	for (int i = 0; i < goals.size(); i++) {
		BlaziumGoapGoal *goal = Object::cast_to<BlaziumGoapGoal>(goals[i]);
		if (goal && goal->is_enabled() && goal->is_valid()) {
			if (highest_priority == nullptr || goal->get_priority() > highest_priority->get_priority()) {
				highest_priority = goal;
			}
		}
	}

	return highest_priority;
}

void BlaziumGoapAgent::_follow_plan(Array p_plan, double p_delta) {
	if (p_plan.is_empty()) {
		finished_last_plan = true;
		return;
	}

	if (current_goal) {
		current_goal->perform(p_delta);
	}

	if (current_plan_step < p_plan.size() - 1) {
		BlaziumGoapAction *current_action = Object::cast_to<BlaziumGoapAction>(p_plan[current_plan_step]);
		if (!current_action) {
			return; // Defensive catch in case tree passed goal explicitly natively
		}

		if (!current_action->has_meta("_entered") || !current_action->get_meta("_entered")) {
			if (!current_action->is_valid()) {
				current_plan.clear();
				current_plan_step = 0;
				if (current_goal) {
					current_goal->on_goal_failed();
				}
				current_goal = nullptr;
				return;
			}
			current_action->enter();
			emit_signal("action_changed", current_action);
			current_action->set_meta("_entered", true);
		}

		if (!current_action->is_valid()) {
			current_plan.clear();
			current_plan_step = 0;
			if (current_goal) {
				current_goal->on_goal_failed();
			}
			current_goal = nullptr;
			return;
		}

		bool is_step_complete = current_action->perform(p_delta);
		if (is_step_complete) {
			previous_action = current_action;
			emit_signal("action_changed", Variant((Object *)nullptr));
			current_action->exit();
			current_action->set_meta("_entered", false);

			current_action->apply_effects();

			if (!_verify_action_effects(current_action)) {
				Dictionary blackboard;
				if (actor != nullptr && actor->is_class("Node3D")) {
					blackboard["global_position"] = actor->get("global_position");
				} else if (actor != nullptr && actor->is_class("Node2D")) {
					blackboard["global_position"] = actor->get("global_position");
				}

				Dictionary st = world_state->get_state_dictionary();
				Array keys = st.keys();
				for (int i = 0; i < keys.size(); i++) {
					blackboard[keys[i]] = st[keys[i]];
				}

				current_plan = action_planner->get_plan(current_goal, blackboard);
				emit_signal("plan_changed", current_plan);
				current_plan_step = 0;

				if (current_plan.is_empty()) {
					if (current_goal) {
						current_goal->on_goal_failed();
						current_goal->exit();
					}
					current_goal = nullptr;
					current_plan.clear();
					current_plan_step = 0;
				}
				return;
			}

			current_plan_step += 1;

			if (current_goal) {
				Dictionary desired_state = current_goal->get_desired_state();
				bool is_goal_satisfied = true;
				Array d_keys = desired_state.keys();
				for (int i = 0; i < d_keys.size(); i++) {
					StringName k = d_keys[i];
					if (world_state->get_state(k) != desired_state[k]) {
						is_goal_satisfied = false;
						break;
					}
				}

				if (is_goal_satisfied) {
					current_goal->on_goal_achieved();
					finished_last_plan = true;
					return;
				}
			}
		}
	}

	if (current_plan_step >= p_plan.size() - 1) {
		if (current_goal) {
			Dictionary desired_state = current_goal->get_desired_state();
			bool is_goal_satisfied = true;
			Array d_keys = desired_state.keys();
			for (int i = 0; i < d_keys.size(); i++) {
				StringName k = d_keys[i];
				if (world_state->get_state(k) != desired_state[k]) {
					is_goal_satisfied = false;
					break;
				}
			}

			if (is_goal_satisfied) {
				current_goal->on_goal_achieved();
				finished_last_plan = true;
			} else {
				current_goal = nullptr;
				current_plan.clear();
				current_plan_step = 0;
			}
		}
	}
}

bool BlaziumGoapAgent::_verify_action_effects(BlaziumGoapAction *p_action) const {
	Dictionary effects = p_action->get_effects();
	Array keys = effects.keys();
	for (int i = 0; i < keys.size(); i++) {
		StringName k = keys[i];
		Variant expected_value = effects[k];
		Variant actual_value = world_state->get_state(k);
		if (actual_value != expected_value) {
			return false;
		}
	}
	return true;
}

BlaziumGoapAction *BlaziumGoapAgent::get_current_action() const {
	if (current_plan.size() > 0 && current_plan_step < current_plan.size()) {
		return Object::cast_to<BlaziumGoapAction>(current_plan[current_plan_step]);
	}
	return nullptr;
}

void BlaziumGoapAgent::debug_select() {
	_send_debug("select", Array());
}

void BlaziumGoapAgent::_send_debug(const String &p_msg_type, const Array &p_data) {
	if (!debug_enabled || !EngineDebugger::get_singleton()->is_active()) {
		return;
	}
	Array msg;
	msg.push_back(debug_id);
	msg.append_array(p_data);
	EngineDebugger::get_singleton()->send_message(debug_prefix + ":" + p_msg_type, msg);
}

void BlaziumGoapAgent::_connect_debug_signals() {
	connect("goal_changed", callable_mp(this, &BlaziumGoapAgent::_on_debug_goal_changed));
	connect("plan_changed", callable_mp(this, &BlaziumGoapAgent::_on_debug_plan_changed));
	connect("action_changed", callable_mp(this, &BlaziumGoapAgent::_on_debug_action_changed));
	if (world_state.is_valid()) {
		world_state->connect("state_updated", callable_mp(this, &BlaziumGoapAgent::_on_debug_world_state_updated));
	}
}

void BlaziumGoapAgent::_send_debug_register(const TypedArray<BlaziumGoapAction> &p_actions) {
	Array goal_data;
	for (int i = 0; i < goals.size(); i++) {
		if (BlaziumGoapGoal *g = Object::cast_to<BlaziumGoapGoal>(goals[i])) {
			Dictionary dict;
			dict["name"] = g->get_action_name();
			dict["priority"] = g->get_priority();
			dict["desired_state"] = g->get_desired_state();
			dict["cost"] = g->get_cost(Dictionary());
			goal_data.push_back(dict);
		}
	}

	Array action_data;
	for (int i = 0; i < p_actions.size(); i++) {
		if (BlaziumGoapAction *a = Object::cast_to<BlaziumGoapAction>(p_actions[i])) {
			Dictionary dict;
			dict["name"] = a->get_action_name();
			dict["cost"] = a->get_base_cost();
			dict["preconditions"] = a->get_preconditions();
			dict["effects"] = a->get_effects();
			action_data.push_back(dict);
		}
	}

	Array res;
	res.push_back(goal_data);
	res.push_back(action_data);
	_send_debug("registry", res);
}

void BlaziumGoapAgent::_on_debug_goal_changed(Object *p_goal) {
	Array msg_data;
	BlaziumGoapGoal *g = Object::cast_to<BlaziumGoapGoal>(p_goal);
	if (g) {
		msg_data.push_back(g->get_action_name());
		msg_data.push_back(g->get_priority());
		msg_data.push_back(g->get_desired_state());
	} else {
		msg_data.push_back("");
		msg_data.push_back(0);
		msg_data.push_back(Dictionary());
	}
	_send_debug("goal", msg_data);
}

void BlaziumGoapAgent::_on_debug_plan_changed(const Array &p_plan) {
	Array plan_data;
	for (int i = 0; i < p_plan.size(); i++) {
		if (BlaziumGoapAction *a = Object::cast_to<BlaziumGoapAction>(p_plan[i])) {
			Dictionary dict;
			dict["name"] = a->get_action_name();
			dict["cost"] = a->get_base_cost();
			dict["preconditions"] = a->get_preconditions();
			dict["effects"] = a->get_effects();
			plan_data.push_back(dict);
		} else if (BlaziumGoapGoal *g = Object::cast_to<BlaziumGoapGoal>(p_plan[i])) {
			Dictionary dict;
			dict["name"] = g->get_action_name();
			dict["cost"] = g->get_base_cost();
			plan_data.push_back(dict);
		}
	}
	Array msg;
	msg.push_back(plan_data);
	_send_debug("plan", msg);
}

void BlaziumGoapAgent::_on_debug_action_changed(Object *p_action) {
	StringName name = "";
	if (BlaziumGoapAction *a = Object::cast_to<BlaziumGoapAction>(p_action)) {
		name = a->get_action_name();
	}
	Array msg1;
	msg1.push_back(name);
	_send_debug("action", msg1);

	Array msg2;
	msg2.push_back(current_plan_step);
	msg2.push_back(current_plan.size());
	_send_debug("step", msg2);
}

void BlaziumGoapAgent::_on_debug_world_state_updated() {
	if (world_state.is_valid()) {
		Array msg;
		msg.push_back(world_state->get_state_dictionary().duplicate());
		_send_debug("world_state", msg);
	}
}

BlaziumGoapAgent::BlaziumGoapAgent() {
	debug_enabled = false;
	debug_prefix = "goap";
	actor = nullptr;
	current_goal = nullptr;
	previous_action = nullptr;
	current_plan_step = 0;
	finished_last_plan = false;
	set_process(true);
}

BlaziumGoapAgent::~BlaziumGoapAgent() {
}
