/**************************************************************************/
/*  autowork_runtime_ui.cpp                                               */
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

#include "core/object/class_db.h"
#include "core/object/callable_mp.h"
#include "autowork_runtime_ui.h"
#include "modules/autowork/autowork_logger.h"
#include "modules/autowork/autowork_main.h"
#include "scene/gui/panel.h"

void AutoworkRuntimeUI::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_logger", "logger"), &AutoworkRuntimeUI::set_logger);
	ClassDB::bind_method(D_METHOD("set_runner", "runner"), &AutoworkRuntimeUI::set_runner);
	ClassDB::bind_method(D_METHOD("add_text", "text"), &AutoworkRuntimeUI::add_text);
	ClassDB::bind_method(D_METHOD("add_color_text", "text", "color"), &AutoworkRuntimeUI::add_color_text);
	ClassDB::bind_method(D_METHOD("_on_run_pressed"), &AutoworkRuntimeUI::_on_run_pressed);
	ClassDB::bind_method(D_METHOD("_on_clear_pressed"), &AutoworkRuntimeUI::_on_clear_pressed);
}

AutoworkRuntimeUI::AutoworkRuntimeUI() {
	set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);

	Panel *bg = memnew(Panel);
	bg->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	add_child(bg);

	main_container = memnew(VBoxContainer);
	main_container->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	main_container->set_offset(SIDE_LEFT, 5);
	main_container->set_offset(SIDE_TOP, 5);
	main_container->set_offset(SIDE_RIGHT, -5);
	main_container->set_offset(SIDE_BOTTOM, -5);
	add_child(main_container);

	toolbar = memnew(HBoxContainer);
	main_container->add_child(toolbar);

	btn_run = memnew(Button);
	btn_run->set_text("Run All Autowork Tests");
	btn_run->connect("pressed", callable_mp(this, &AutoworkRuntimeUI::_on_run_pressed));
	toolbar->add_child(btn_run);

	btn_clear = memnew(Button);
	btn_clear->set_text("Clear Log");
	btn_clear->connect("pressed", callable_mp(this, &AutoworkRuntimeUI::_on_clear_pressed));
	toolbar->add_child(btn_clear);

	rich_text = memnew(RichTextLabel);
	rich_text->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	rich_text->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	rich_text->set_scroll_follow(true);
	rich_text->set_use_bbcode(true);
	main_container->add_child(rich_text);
}

AutoworkRuntimeUI::~AutoworkRuntimeUI() {}

void AutoworkRuntimeUI::_notification(int p_what) {}

void AutoworkRuntimeUI::set_logger(Object *p_logger) {
	logger = p_logger;
}

void AutoworkRuntimeUI::set_runner(Object *p_runner) {
	runner = p_runner;
}

void AutoworkRuntimeUI::add_text(const String &p_text) {
	rich_text->append_text(p_text + "\n");
}

void AutoworkRuntimeUI::add_color_text(const String &p_text, const Color &p_color) {
	String bb = "[color=#" + p_color.to_html(false) + "]" + p_text + "[/color]\n";
	rich_text->append_text(bb);
}

void AutoworkRuntimeUI::_on_run_pressed() {
	if (!runner) {
		return;
	}
	Autowork *main_runner = Object::cast_to<Autowork>(runner);
	if (!main_runner) {
		return;
	}

	// Clear the current board
	_on_clear_pressed();

	// Hook logger into self
	if (logger) {
		AutoworkLogger *log = Object::cast_to<AutoworkLogger>(logger);
		if (log) {
			log->set_output_ui(this);
		}
	}

	// Execute
	main_runner->run_tests();
}

void AutoworkRuntimeUI::_on_clear_pressed() {
	rich_text->clear();
}
