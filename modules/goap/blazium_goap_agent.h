/**************************************************************************/
/*  blazium_goap_agent.h                                                  */
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

#include "blazium_goap_action_planner.h"
#include "blazium_goap_world_state.h"
#include "scene/main/node.h"

class BlaziumGoapAgent : public Node {
	GDCLASS(BlaziumGoapAgent, Node);

private:
	NodePath goals_path;
	NodePath actions_path;
	bool debug_enabled;
	String debug_prefix;
	String debug_id;

	Node *actor;
	Ref<BlaziumGoapWorldState> world_state;
	Ref<BlaziumGoapActionPlanner> action_planner;

	TypedArray<BlaziumGoapGoal> goals;
	Array current_plan;

	BlaziumGoapGoal *current_goal;
	BlaziumGoapAction *previous_action;
	int current_plan_step;
	Dictionary last_blackboard;
	bool finished_last_plan;

	BlaziumGoapGoal *_get_best_goal();
	void _follow_plan(Array p_plan, double p_delta);
	bool _verify_action_effects(BlaziumGoapAction *p_action) const;

	void _send_debug(const String &p_msg_type, const Array &p_data);
	void _connect_debug_signals();
	void _send_debug_register(const TypedArray<BlaziumGoapAction> &p_actions);

	void _on_debug_goal_changed(Object *p_goal);
	void _on_debug_plan_changed(const Array &p_plan);
	void _on_debug_action_changed(Object *p_action);
	void _on_debug_world_state_updated();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_goals_path(const NodePath &p_path) { goals_path = p_path; }
	NodePath get_goals_path() const { return goals_path; }

	void set_actions_path(const NodePath &p_path) { actions_path = p_path; }
	NodePath get_actions_path() const { return actions_path; }

	void set_debug_enabled(bool p_enabled) { debug_enabled = p_enabled; }
	bool is_debug_enabled() const { return debug_enabled; }

	void set_debug_prefix(const String &p_prefix) { debug_prefix = p_prefix; }
	String get_debug_prefix() const { return debug_prefix; }

	void init(Node *p_actor);

	BlaziumGoapGoal *get_current_goal() const { return current_goal; }
	BlaziumGoapAction *get_current_action() const;
	Array get_current_plan() const { return current_plan; }
	Ref<BlaziumGoapWorldState> get_world_state() const { return world_state; }

	void debug_select();

	BlaziumGoapAgent();
	~BlaziumGoapAgent();
};
