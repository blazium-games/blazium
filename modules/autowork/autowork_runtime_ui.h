/**************************************************************************/
/*  autowork_runtime_ui.h                                                 */
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

#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/control.h"
#include "scene/gui/rich_text_label.h"

class AutoworkRuntimeUI : public Control {
	GDCLASS(AutoworkRuntimeUI, Control);

protected:
	static void _bind_methods();
	void _notification(int p_what);

private:
	Object *logger = nullptr;
	Object *runner = nullptr;

	VBoxContainer *main_container;
	HBoxContainer *toolbar;
	Button *btn_run;
	Button *btn_clear;
	RichTextLabel *rich_text;

	void _on_run_pressed();
	void _on_clear_pressed();

public:
	AutoworkRuntimeUI();
	~AutoworkRuntimeUI();

	void set_logger(Object *p_logger);
	void set_runner(Object *p_runner);

	// Callable from Logger
	void add_text(const String &p_text);
	void add_color_text(const String &p_text, const Color &p_color);
};
