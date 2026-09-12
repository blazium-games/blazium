/**************************************************************************/
/*  blazium_goap_action.h                                                 */
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
class BlaziumGoapGoal;

class BlaziumGoapAction : public Node {
	GDCLASS(BlaziumGoapAction, Node);

private:
	Ref<BlaziumGoapWorldState> world_state;
	Node *actor;
	BlaziumGoapAgent *agent;

	Dictionary effects;
	Dictionary preconditions;
	int cost;
	bool enabled;

protected:
	static void _bind_methods();

	GDVIRTUAL0RC(bool, _is_valid)
	GDVIRTUAL1RC(int, _get_cost, Dictionary)
	GDVIRTUAL0(_prepare_action)
	GDVIRTUAL0(_enter)
	GDVIRTUAL0(_exit)
	GDVIRTUAL1R(bool, _perform, double)

public:
	void init(Node *p_actor, Ref<BlaziumGoapWorldState> p_world_state);

	StringName get_action_name() const;

	bool is_valid() const;
	int get_cost(const Dictionary &p_blackboard) const;
	void prepare_action();

	Dictionary get_preconditions() const { return preconditions; }
	void set_preconditions(const Dictionary &p_preconditions) { preconditions = p_preconditions; }

	Dictionary get_effects() const { return effects; }
	void set_effects(const Dictionary &p_effects) { effects = p_effects; }

	void apply_effects(); // Matches gdscripts set_effects wrapper

	int get_base_cost() const { return cost; }
	void set_base_cost(int p_cost) { cost = p_cost; }

	bool is_enabled() const { return enabled; }
	void set_enabled(bool p_enabled) { enabled = p_enabled; }

	void enter();
	void exit();
	bool perform(double p_delta);

	Variant get_state(const StringName &p_key, const Variant &p_default = Variant()) const;
	void set_state(const StringName &p_key, const Variant &p_value);

	BlaziumGoapAgent *get_agent() const { return agent; }
	void set_agent(BlaziumGoapAgent *p_agent) { agent = p_agent; }

	BlaziumGoapGoal *get_goal() const;

	BlaziumGoapAction();
	~BlaziumGoapAction();
};
