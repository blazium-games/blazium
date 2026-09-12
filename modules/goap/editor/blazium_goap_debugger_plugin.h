/**************************************************************************/
/*  blazium_goap_debugger_plugin.h                                        */
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

#ifdef TOOLS_ENABLED

#include "core/input/shortcut.h"
#include "core/object/script_language.h"
#include "editor/plugins/editor_debugger_plugin.h"
#include "scene/gui/box_container.h"
#include "scene/gui/check_box.h"
#include "scene/gui/item_list.h"
#include "scene/gui/label.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/progress_bar.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/tree.h"
#include "scene/resources/style_box_flat.h"

// --- Components ---

class BlaziumGoapPlannerExplorer : public VBoxContainer {
	GDCLASS(BlaziumGoapPlannerExplorer, VBoxContainer);

private:
	Label *title_label;
	Tree *registry_tree;
	Array current_goals;
	Array current_actions;

protected:
	static void _bind_methods() {}

public:
	void set_registry(const Array &p_goals, const Array &p_actions);
	void clear();
	BlaziumGoapPlannerExplorer();
};

class BlaziumGoapEditorDebugPanel : public VBoxContainer {
	GDCLASS(BlaziumGoapEditorDebugPanel, VBoxContainer);

private:
	HBoxContainer *graph_container;
	Label *goal_label;
	Label *action_label;
	ProgressBar *step_progress;
	Tree *world_state_tree;

	Color color_goal;
	Color color_action_active;
	Color color_action_inactive;
	Color color_world_state;

protected:
	static void _bind_methods() {}

public:
	void on_goal_received(const String &p_name, int p_priority, const Dictionary &p_desired_state);
	void on_plan_received(const Array &p_plan);
	void on_action_received(const String &p_name);
	void on_step_received(int p_current, int p_total);
	void on_world_state_received(const Dictionary &p_state);
	void clear();

	BlaziumGoapEditorDebugPanel();
};

class BlaziumGoapBottomPanel : public MarginContainer {
	GDCLASS(BlaziumGoapBottomPanel, MarginContainer);

private:
	ItemList *agent_selector;
	BlaziumGoapEditorDebugPanel *debug_panel;
	BlaziumGoapPlannerExplorer *explorer;

protected:
	static void _bind_methods() {}

public:
	ItemList *get_agent_selector() { return agent_selector; }
	BlaziumGoapEditorDebugPanel *get_debug_panel() { return debug_panel; }
	BlaziumGoapPlannerExplorer *get_explorer() { return explorer; }
	void clear();
	BlaziumGoapBottomPanel();
};

// --- Plugin ---

class BlaziumGoapEditorDebuggerPlugin : public EditorDebuggerPlugin {
	GDCLASS(BlaziumGoapEditorDebuggerPlugin, EditorDebuggerPlugin);

private:
	BlaziumGoapBottomPanel *bottom_panel;
	Dictionary agents;
	String selected_agent_id;

	void _refresh_agent_list();
	String _get_agent_display_name(const String &p_agent_id);
	void _select_agent(const String &p_agent_id);
	void _push_to_panel(const Dictionary &p_agent_data, const String &p_msg_type);
	void _on_agent_selected(int p_idx);

protected:
	static void _bind_methods();
	virtual void setup_session(int p_session_id) override;
	virtual bool capture(const String &p_message, const Array &p_data, int p_session_id) override;
	virtual bool has_capture(const String &p_capture) const override;

public:
	void bind_bottom_panel(BlaziumGoapBottomPanel *p_panel);
	BlaziumGoapEditorDebuggerPlugin();
	~BlaziumGoapEditorDebuggerPlugin();
};

#endif // TOOLS_ENABLED
