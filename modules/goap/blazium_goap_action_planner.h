/**************************************************************************/
/*  blazium_goap_action_planner.h                                         */
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

#include "blazium_goap_action.h"
#include "blazium_goap_goal.h"
#include "core/object/ref_counted.h"

class BlaziumGoapActionPlanner;

struct BlaziumGoapPlanStep {
	Node *action;
	Dictionary state;
	Vector<BlaziumGoapPlanStep *> children;

	~BlaziumGoapPlanStep() {
		for (int i = 0; i < children.size(); i++) {
			memdelete(children[i]);
		}
	}
};

struct BlaziumGoapPlanResult {
	Array actions;
	int cost;
};

class BlaziumGoapActionPlanner : public RefCounted {
	GDCLASS(BlaziumGoapActionPlanner, RefCounted);

private:
	TypedArray<BlaziumGoapAction> actions;

	Array _find_best_plan(BlaziumGoapGoal *p_goal, const Dictionary &p_desired_state, const Dictionary &p_blackboard);
	Array _get_cheapest_plan(const Vector<BlaziumGoapPlanResult> &p_plans);
	bool _build_plans(BlaziumGoapPlanStep *p_step, const Dictionary &p_blackboard);
	void _transform_tree_into_array(BlaziumGoapPlanStep *p_step, const Dictionary &p_blackboard, Vector<BlaziumGoapPlanResult> &r_plans);

protected:
	static void _bind_methods();

public:
	void set_actions(const TypedArray<BlaziumGoapAction> &p_actions);
	Array get_plan(BlaziumGoapGoal *p_goal, const Dictionary &p_blackboard = Dictionary());

	BlaziumGoapActionPlanner();
	~BlaziumGoapActionPlanner();
};
