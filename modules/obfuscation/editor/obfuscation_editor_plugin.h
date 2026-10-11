/**************************************************************************/
/*  obfuscation_editor_plugin.h                                           */
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

#include "editor/plugins/editor_plugin.h"

class Button;
class GridContainer;
class Label;
class ScrollContainer;
class LineEdit;
class TextEdit;
class TextureRect;
class VBoxContainer;

class ObfuscationEditorPlugin : public EditorPlugin {
	GDCLASS(ObfuscationEditorPlugin, EditorPlugin);

	VBoxContainer *dock = nullptr;
	Label *status = nullptr;
	LineEdit *publisher_edit = nullptr;
	LineEdit *project_edit = nullptr;
	LineEdit *author_edit = nullptr;
	LineEdit *license_edit = nullptr;
	TextEdit *notice_edit = nullptr;
	TextureRect *preview = nullptr;

	// Title bar badge and the export report in the bottom panel.
	Button *badge = nullptr;
	bool updating_badge = false;
	ScrollContainer *report_scroll = nullptr;
	VBoxContainer *report_panel = nullptr;
	Label *report_title = nullptr;
	Label *report_summary = nullptr;
	GridContainer *report_grid = nullptr;
	Label *report_notes = nullptr;

	void _update_badge();
	void _badge_pressed();
	void _report_ready(const Dictionary &p_report);
	void _add_report_tile(const String &p_label, const String &p_value, const Color &p_color);

	void _refresh();
	void _generate_pressed();
	void _load_pressed();
	void _clear_pressed();
	void _save_settings_pressed();
	void _preview_seal();

protected:
	static void _bind_methods() {}
	void _notification(int p_what);

public:
	virtual String get_plugin_name() const override { return "Obfuscation"; }
	ObfuscationEditorPlugin();
	~ObfuscationEditorPlugin();
};

#endif
