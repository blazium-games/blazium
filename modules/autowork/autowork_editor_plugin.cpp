/**************************************************************************/
/*  autowork_editor_plugin.cpp                                            */
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

#ifdef TOOLS_ENABLED

#include "autowork_editor_plugin.h"
#include "autowork_logger.h"
#include "autowork_main.h"

#include "core/input/shortcut.h"
#include "core/os/thread.h"
#include "editor/editor_node.h"

void AutoworkEditorPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_run_tests_pressed"), &AutoworkEditorPlugin::_run_tests_pressed);
}

AutoworkEditorPlugin::AutoworkEditorPlugin() {
	main_panel = memnew(VBoxContainer);
	main_panel->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	main_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	HBoxContainer *toolbar = memnew(HBoxContainer);
	main_panel->add_child(toolbar);

	run_button = memnew(Button);
	run_button->set_text("Run All Tests");
	run_button->connect("pressed", callable_mp(this, &AutoworkEditorPlugin::_run_tests_pressed));
	toolbar->add_child(run_button);

	output_log = memnew(RichTextLabel);
	output_log->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	output_log->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	output_log->set_use_bbcode(true);
	output_log->set_scroll_follow(true);
	output_log->set_selection_enabled(true);

	main_panel->add_child(output_log);

	add_control_to_bottom_panel(main_panel, TTR("Autowork"));
}

AutoworkEditorPlugin::~AutoworkEditorPlugin() {
}

void AutoworkEditorPlugin::_run_tests_pressed() {
	output_log->clear();
	output_log->append_text("[b]Initializing Autowork Tests...[/b]\n");

	// Ensure we run the test execution sequentially or deferred.
	// Since tests can alter scene states, it is safe to instantiate
	// Autowork, add it to the editor tree, and run it.

	Autowork *aw = memnew(Autowork);
	EditorNode::get_singleton()->add_child(aw);

	// Hook the plugin's UI label to the Logger before running.
	if (aw->get_logger().is_valid()) {
		aw->get_logger()->set_output_ui(output_log);
	}

	// Path to test directory
	aw->add_directory("res://"); // Standard for project tests

	// Execute tests synchronously
	aw->run_tests();

	// Cleanup runner
	aw->queue_free();
}

#endif // TOOLS_ENABLED
