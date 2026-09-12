/**************************************************************************/
/*  blazium_goap_debugger_plugin.cpp                                      */
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

#include "blazium_goap_debugger_plugin.h"

#ifdef TOOLS_ENABLED

// --- BlaziumGoapPlannerExplorer ---

BlaziumGoapPlannerExplorer::BlaziumGoapPlannerExplorer() {
	title_label = memnew(Label);
	title_label->set_text("Registry");
	add_child(title_label);

	registry_tree = memnew(Tree);
	registry_tree->set_v_size_flags(SIZE_EXPAND_FILL);
	add_child(registry_tree);
}

void BlaziumGoapPlannerExplorer::set_registry(const Array &p_goals, const Array &p_actions) {
	current_goals = p_goals;
	current_actions = p_actions;

	registry_tree->clear();
	TreeItem *root = registry_tree->create_item();

	TreeItem *goals_item = registry_tree->create_item(root);
	goals_item->set_text(0, "Goals");
	for (int i = 0; i < p_goals.size(); i++) {
		Dictionary g = p_goals[i];
		TreeItem *child_item = registry_tree->create_item(goals_item);
		child_item->set_text(0, String(g["name"]) + " (Priority: " + itos(g["priority"]) + ")");
	}

	TreeItem *actions_item = registry_tree->create_item(root);
	actions_item->set_text(0, "Actions");
	for (int i = 0; i < p_actions.size(); i++) {
		Dictionary a = p_actions[i];
		TreeItem *child_item = registry_tree->create_item(actions_item);
		child_item->set_text(0, String(a["name"]) + " (Cost: " + itos(a["cost"]) + ")");
	}
}

void BlaziumGoapPlannerExplorer::clear() {
	registry_tree->clear();
}

// --- BlaziumGoapEditorDebugPanel ---

BlaziumGoapEditorDebugPanel::BlaziumGoapEditorDebugPanel() {
	color_goal = Color(0.8, 0.4, 0.2);
	color_action_active = Color(0.2, 0.8, 0.4);
	color_action_inactive = Color(0.3, 0.4, 0.3);
	color_world_state = Color(0.2, 0.5, 0.8);

	goal_label = memnew(Label);
	goal_label->set_text("Current Goal: None");
	add_child(goal_label);

	step_progress = memnew(ProgressBar);
	add_child(step_progress);

	action_label = memnew(Label);
	action_label->set_text("Current Action: None");
	add_child(action_label);

	graph_container = memnew(HBoxContainer);
	graph_container->set_v_size_flags(SIZE_EXPAND_FILL);
	add_child(graph_container);

	world_state_tree = memnew(Tree);
	world_state_tree->set_v_size_flags(SIZE_EXPAND_FILL);
	add_child(world_state_tree);
}

void BlaziumGoapEditorDebugPanel::on_goal_received(const String &p_name, int p_priority, const Dictionary &p_desired_state) {
	goal_label->set_text("Current Goal: " + p_name + " (Priority: " + itos(p_priority) + ")");
}

void BlaziumGoapEditorDebugPanel::on_plan_received(const Array &p_plan) {
	// Rebuild graph
	for (int i = 0; i < graph_container->get_child_count(); i++) {
		Node *child = graph_container->get_child(i);
		graph_container->remove_child(child);
		child->queue_free();
	}

	for (int i = 0; i < p_plan.size(); i++) {
		Dictionary item = p_plan[i];
		PanelContainer *pc = memnew(PanelContainer);
		Label *l = memnew(Label);
		l->set_text(item["name"]);
		pc->add_child(l);
		graph_container->add_child(pc);
	}
}

void BlaziumGoapEditorDebugPanel::on_action_received(const String &p_name) {
	action_label->set_text("Current Action: " + p_name);
}

void BlaziumGoapEditorDebugPanel::on_step_received(int p_current, int p_total) {
	step_progress->set_max(p_total);
	step_progress->set_value(p_current);
}

void BlaziumGoapEditorDebugPanel::on_world_state_received(const Dictionary &p_state) {
	world_state_tree->clear();
	TreeItem *root = world_state_tree->create_item();
	Array keys = p_state.keys();
	for (int i = 0; i < keys.size(); i++) {
		TreeItem *child_item = world_state_tree->create_item(root);
		child_item->set_text(0, String(keys[i]) + ": " + String(Variant(p_state[keys[i]])));
	}
}

void BlaziumGoapEditorDebugPanel::clear() {
	goal_label->set_text("Current Goal: None");
	action_label->set_text("Current Action: None");
	step_progress->set_max(1);
	step_progress->set_value(0);
	world_state_tree->clear();
	for (int i = 0; i < graph_container->get_child_count(); i++) {
		Node *child = graph_container->get_child(i);
		graph_container->remove_child(child);
		child->queue_free();
	}
}

// --- BlaziumGoapBottomPanel ---

BlaziumGoapBottomPanel::BlaziumGoapBottomPanel() {
	set_name("GOAP");
	HBoxContainer *main_hbox = memnew(HBoxContainer);
	add_child(main_hbox);

	VBoxContainer *left_vbox = memnew(VBoxContainer);
	left_vbox->set_custom_minimum_size(Size2(200, 0));
	main_hbox->add_child(left_vbox);

	agent_selector = memnew(ItemList);
	agent_selector->set_v_size_flags(SIZE_EXPAND_FILL);
	left_vbox->add_child(agent_selector);

	debug_panel = memnew(BlaziumGoapEditorDebugPanel);
	debug_panel->set_h_size_flags(SIZE_EXPAND_FILL);
	main_hbox->add_child(debug_panel);

	explorer = memnew(BlaziumGoapPlannerExplorer);
	explorer->set_custom_minimum_size(Size2(300, 0));
	main_hbox->add_child(explorer);
}

void BlaziumGoapBottomPanel::clear() {
	if (agent_selector) {
		agent_selector->clear();
	}
	if (debug_panel) {
		debug_panel->clear();
	}
	if (explorer) {
		explorer->clear();
	}
}

// --- BlaziumGoapEditorDebuggerPlugin ---

void BlaziumGoapEditorDebuggerPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_agent_selected", "idx"), &BlaziumGoapEditorDebuggerPlugin::_on_agent_selected);
}

BlaziumGoapEditorDebuggerPlugin::BlaziumGoapEditorDebuggerPlugin() {
	bottom_panel = nullptr;
}

BlaziumGoapEditorDebuggerPlugin::~BlaziumGoapEditorDebuggerPlugin() {
}

void BlaziumGoapEditorDebuggerPlugin::bind_bottom_panel(BlaziumGoapBottomPanel *p_panel) {
	bottom_panel = p_panel;
	if (bottom_panel && bottom_panel->get_agent_selector()) {
		bottom_panel->get_agent_selector()->connect("item_selected", callable_mp(this, &BlaziumGoapEditorDebuggerPlugin::_on_agent_selected));
	}
}

void BlaziumGoapEditorDebuggerPlugin::setup_session(int p_session_id) {
	agents.clear();
	selected_agent_id = "";
	if (bottom_panel) {
		bottom_panel->clear();
	}
}

bool BlaziumGoapEditorDebuggerPlugin::has_capture(const String &p_capture) const {
	return p_capture == "goap";
}

bool BlaziumGoapEditorDebuggerPlugin::capture(const String &p_message, const Array &p_data, int p_session_id) {
	if (p_data.size() < 1) {
		return false;
	}
	String agent_id = p_data[0];
	String action = p_message.substr(5); // "goap:xxx" -> "xxx"

	if (!agents.has(agent_id)) {
		Dictionary new_agent;
		new_agent["goals"] = Array();
		new_agent["actions"] = Array();
		new_agent["goal_name"] = "";
		new_agent["goal_priority"] = 0;
		new_agent["goal_desired_state"] = Dictionary();
		new_agent["plan"] = Array();
		new_agent["action_name"] = "";
		new_agent["step_current"] = 0;
		new_agent["step_total"] = 0;
		new_agent["world_state"] = Dictionary();
		agents[agent_id] = new_agent;

		_refresh_agent_list();
		if (selected_agent_id == "") {
			_select_agent(agent_id);
		}
	}

	Dictionary agent_data = agents[agent_id];

	if (action == "registry" && p_data.size() >= 3) {
		agent_data["goals"] = p_data[1];
		agent_data["actions"] = p_data[2];
		if (agent_id == selected_agent_id && bottom_panel->get_explorer()) {
			bottom_panel->get_explorer()->set_registry(p_data[1], p_data[2]);
		}
	} else if (action == "goal" && p_data.size() >= 4) {
		agent_data["goal_name"] = p_data[1];
		agent_data["goal_priority"] = p_data[2];
		agent_data["goal_desired_state"] = p_data[3];
		if (agent_id == selected_agent_id) {
			_push_to_panel(agent_data, "goal");
		}
	} else if (action == "plan" && p_data.size() >= 2) {
		agent_data["plan"] = p_data[1];
		if (agent_id == selected_agent_id) {
			_push_to_panel(agent_data, "plan");
		}
	} else if (action == "action" && p_data.size() >= 2) {
		agent_data["action_name"] = p_data[1];
		if (agent_id == selected_agent_id) {
			_push_to_panel(agent_data, "action");
		}
	} else if (action == "step" && p_data.size() >= 3) {
		agent_data["step_current"] = p_data[1];
		agent_data["step_total"] = p_data[2];
		if (agent_id == selected_agent_id) {
			_push_to_panel(agent_data, "step");
		}
	} else if (action == "world_state" && p_data.size() >= 2) {
		agent_data["world_state"] = p_data[1];
		if (agent_id == selected_agent_id) {
			_push_to_panel(agent_data, "world_state");
		}
	}

	return true;
}

void BlaziumGoapEditorDebuggerPlugin::_refresh_agent_list() {
	if (!bottom_panel || !bottom_panel->get_agent_selector()) {
		return;
	}
	bottom_panel->get_agent_selector()->clear();
	Array keys = agents.keys();
	for (int i = 0; i < keys.size(); i++) {
		String id = keys[i];
		String label = _get_agent_display_name(id);
		bottom_panel->get_agent_selector()->add_item(label);
		bottom_panel->get_agent_selector()->set_item_metadata(i, id);
		if (id == selected_agent_id) {
			bottom_panel->get_agent_selector()->select(i);
		}
	}
}

String BlaziumGoapEditorDebuggerPlugin::_get_agent_display_name(const String &p_agent_id) {
	Vector<String> parts = p_agent_id.split("/");
	if (parts.size() >= 2) {
		return parts[parts.size() - 2] + "/" + parts[parts.size() - 1];
	}
	return p_agent_id;
}

void BlaziumGoapEditorDebuggerPlugin::_select_agent(const String &p_agent_id) {
	selected_agent_id = p_agent_id;
	if (!agents.has(p_agent_id)) {
		return;
	}
	Dictionary agent_data = agents[p_agent_id];

	if (bottom_panel && bottom_panel->get_debug_panel()) {
		bottom_panel->get_debug_panel()->clear();
	}

	if (bottom_panel && bottom_panel->get_explorer()) {
		bottom_panel->get_explorer()->set_registry(agent_data["goals"], agent_data["actions"]);
	}

	_push_to_panel(agent_data, "goal");
	_push_to_panel(agent_data, "plan");
	_push_to_panel(agent_data, "action");
	_push_to_panel(agent_data, "step");
	_push_to_panel(agent_data, "world_state");
}

void BlaziumGoapEditorDebuggerPlugin::_push_to_panel(const Dictionary &p_agent_data, const String &p_msg_type) {
	if (!bottom_panel || !bottom_panel->get_debug_panel()) {
		return;
	}

	BlaziumGoapEditorDebugPanel *panel = bottom_panel->get_debug_panel();

	if (p_msg_type == "goal") {
		panel->on_goal_received(p_agent_data["goal_name"], p_agent_data["goal_priority"], p_agent_data["goal_desired_state"]);
	} else if (p_msg_type == "plan") {
		panel->on_plan_received(p_agent_data["plan"]);
	} else if (p_msg_type == "action") {
		panel->on_action_received(p_agent_data["action_name"]);
	} else if (p_msg_type == "step") {
		panel->on_step_received(p_agent_data["step_current"], p_agent_data["step_total"]);
	} else if (p_msg_type == "world_state") {
		panel->on_world_state_received(p_agent_data["world_state"]);
	}
}

void BlaziumGoapEditorDebuggerPlugin::_on_agent_selected(int p_idx) {
	if (bottom_panel && bottom_panel->get_agent_selector()) {
		String id = bottom_panel->get_agent_selector()->get_item_metadata(p_idx);
		_select_agent(id);
	}
}

#endif // TOOLS_ENABLED
