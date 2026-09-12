/**************************************************************************/
/*  blazium_goap_goal.h                                                   */
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

#include "blazium_goap_world_state.h"
#include "core/object/gdvirtual.gen.inc"
#include "scene/main/node.h"

class BlaziumGoapAgent;

class BlaziumGoapGoal : public Node {
	GDCLASS(BlaziumGoapGoal, Node);

private:
	Ref<BlaziumGoapWorldState> world_state;
	Node *actor;
	BlaziumGoapAgent *agent;

	bool default_valid_state;
	int priority;
	bool enabled;
	int cost;
	bool performing;
	Dictionary desired_state;

protected:
	static void _bind_methods();

	GDVIRTUAL0RC(bool, _is_valid)
	GDVIRTUAL0RC(int, _get_priority)
	GDVIRTUAL1RC(int, _get_cost, Dictionary)
	GDVIRTUAL0(_enter)
	GDVIRTUAL0(_exit)
	GDVIRTUAL1(_perform, double)
	GDVIRTUAL0(_prepare)
	GDVIRTUAL0(_on_goal_achieved)
	GDVIRTUAL0(_on_goal_failed)

public:
	void init(Node *p_actor, Ref<BlaziumGoapWorldState> p_world_state);

	StringName get_action_name() const;

	bool is_valid() const;
	int get_priority() const;
	int get_base_priority() const { return priority; }
	void set_base_priority(int p_priority) { priority = p_priority; }

	Dictionary get_desired_state() const { return desired_state; }
	void set_desired_state(const Dictionary &p_desired_state) { desired_state = p_desired_state; }

	int get_cost(const Dictionary &p_blackboard) const;

	int get_base_cost() const { return cost; }
	void set_base_cost(int p_cost) { cost = p_cost; }

	bool is_enabled() const { return enabled; }
	void set_enabled(bool p_enabled) { enabled = p_enabled; }

	bool get_default_valid_state() const { return default_valid_state; }
	void set_default_valid_state(bool p_default_valid) { default_valid_state = p_default_valid; }

	bool is_performing() const { return performing; }

	void enter();
	void exit();
	void perform(double p_delta);
	void prepare();
	void on_goal_achieved();
	void on_goal_failed();

	BlaziumGoapAgent *get_agent() const { return agent; }
	void set_agent(BlaziumGoapAgent *p_agent) { agent = p_agent; }

	BlaziumGoapGoal();
	~BlaziumGoapGoal();
};
