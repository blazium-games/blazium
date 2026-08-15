/**************************************************************************/
/*  luau_editor_plugin.cpp                                                */
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

#include "core/object/class_db.h"
#include "core/object/callable_mp.h"
#include "luau_editor_plugin.h"

#include "editor/luau_formatter.h"
#include "require/luau_package_path.h"

#include "core/config/project_settings.h"
#include "core/input/shortcut.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "scene/gui/separator.h"
#include <lua.h>

using namespace luau_module;

void LuauEditorPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_run_pressed"), &LuauEditorPlugin::_on_run_pressed);
	ClassDB::bind_method(D_METHOD("_on_run_script_pressed"), &LuauEditorPlugin::_on_run_script_pressed);
	ClassDB::bind_method(D_METHOD("_on_clear_pressed"), &LuauEditorPlugin::_on_clear_pressed);
	ClassDB::bind_method(D_METHOD("_on_format_input_pressed"), &LuauEditorPlugin::_on_format_input_pressed);
}

void LuauEditorPlugin::_append_output(const String &p_line, bool p_is_error) {
	if (!output_view) {
		return;
	}

	if (p_is_error) {
		output_view->insert_text_at_caret("[error] " + p_line + "\n");
	} else {
		output_view->insert_text_at_caret(p_line + "\n");
	}
}

void LuauEditorPlugin::_init_repl_state() {
	if (repl_state.is_valid() && repl_state->is_valid()) {
		return;
	}

	repl_state.instantiate();
	repl_state->open_libs(LuaState::LIB_ALL);
	LuauPackagePath::install_package_searchers(repl_state);
}

void LuauEditorPlugin::_on_run_pressed() {
	if (!input_view || !repl_state.is_valid()) {
		return;
	}

	const String code = input_view->get_text().strip_edges();
	if (code.is_empty()) {
		return;
	}

	_append_output("> " + code);

	const int top_before = repl_state->get_top();
	const LuaState::Status status = repl_state->do_string(code, "@LuauREPL");

	if (status == luau_module::LuaState::STATUS_OK) {
		const int results = repl_state->get_top() - top_before;
		for (int i = 0; i < results; ++i) {
			const String result = repl_state->push_as_string(-results + i);
			repl_state->pop(1);
			_append_output(result);
		}
		repl_state->set_top(top_before);
	} else {
		String err_msg;
		if (repl_state->get_top() > top_before && repl_state->is_string(-1)) {
			err_msg = repl_state->to_string_inplace(-1);
			repl_state->pop(1);
		} else {
			err_msg = "Luau execution failed.";
		}
		repl_state->set_top(top_before);
		_append_output(err_msg, true);
	}
}

void LuauEditorPlugin::_on_run_script_pressed() {
	const String code = input_view ? input_view->get_text().strip_edges() : String();
	if (code.is_empty()) {
		_append_output("Enter Luau code in the REPL input to run as a script.", true);
		return;
	}

	const String project = ProjectSettings::get_singleton()->get_resource_path();
	const String temp_path = ProjectSettings::get_singleton()->globalize_path("user://luau_run_temp.luau");
	Ref<FileAccess> file = FileAccess::open(temp_path, FileAccess::WRITE);
	if (file.is_null()) {
		_append_output("Failed to write temporary run script.", true);
		return;
	}
	file->store_string(code);
	file->close();

	const String exe = OS::get_singleton()->get_executable_path();
	const int exit_code = OS::get_singleton()->execute(exe, { "--headless", "--path", project, "-s", temp_path });
	_append_output(vformat("Run script exit code: %d", exit_code), exit_code != 0);
}

void LuauEditorPlugin::_on_clear_pressed() {
	if (output_view) {
		output_view->clear();
	}
	if (input_view) {
		input_view->clear();
	}
}

void LuauEditorPlugin::_on_format_input_pressed() {
	if (!input_view) {
		return;
	}
	const String formatted = LuauFormatter::format_source(input_view->get_text());
	input_view->set_text(formatted);
}

void LuauEditorPlugin::_setup_repl() {
	main_panel = memnew(PanelContainer);
	main_panel->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	VBoxContainer *vbox = memnew(VBoxContainer);
	vbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	vbox->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	main_panel->add_child(vbox);

	Label *title = memnew(Label);
	title->set_text("Luau REPL");
	vbox->add_child(title);

	output_view = memnew(TextEdit);
	output_view->set_editable(false);
	output_view->set_custom_minimum_size(Vector2(0, 180));
	output_view->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	output_view->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	vbox->add_child(output_view);

	HSeparator *separator = memnew(HSeparator);
	vbox->add_child(separator);

	input_view = memnew(TextEdit);
	input_view->set_custom_minimum_size(Vector2(0, 80));
	input_view->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	input_view->set_placeholder("Enter Luau code and press Run");
	vbox->add_child(input_view);

	HBoxContainer *buttons = memnew(HBoxContainer);
	Button *run_button = memnew(Button);
	run_button->set_text("Run");
	run_button->connect(SceneStringName(pressed), callable_mp(this, &LuauEditorPlugin::_on_run_pressed));
	buttons->add_child(run_button);

	Button *run_script_button = memnew(Button);
	run_script_button->set_text("Run Script");
	run_script_button->connect(SceneStringName(pressed), callable_mp(this, &LuauEditorPlugin::_on_run_script_pressed));
	buttons->add_child(run_script_button);

	Button *clear_button = memnew(Button);
	clear_button->set_text("Clear");
	clear_button->connect(SceneStringName(pressed), callable_mp(this, &LuauEditorPlugin::_on_clear_pressed));
	buttons->add_child(clear_button);

	Button *format_button = memnew(Button);
	format_button->set_text("Format");
	format_button->connect(SceneStringName(pressed), callable_mp(this, &LuauEditorPlugin::_on_format_input_pressed));
	buttons->add_child(format_button);
	vbox->add_child(buttons);

	add_control_to_bottom_panel(main_panel, "Luau REPL");
	_init_repl_state();
	_append_output("Luau REPL ready.");
}

void LuauEditorPlugin::_teardown_repl() {
	if (main_panel) {
		remove_control_from_bottom_panel(main_panel);
		memdelete(main_panel);
		main_panel = nullptr;
	}

	output_view = nullptr;
	input_view = nullptr;

	if (repl_state.is_valid()) {
		repl_state->close();
		repl_state.unref();
	}
}

void LuauEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_setup_repl();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			_teardown_repl();
		} break;
	}
}

void LuauEditorPlugin::make_visible(bool p_visible) {
	if (main_panel) {
		main_panel->set_visible(p_visible);
	}
}

LuauEditorPlugin::LuauEditorPlugin() {
}

LuauEditorPlugin::~LuauEditorPlugin() {
}

#endif
