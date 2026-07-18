/**************************************************************************/
/*  justamcp_editor_tools.h                                               */
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

#include "core/object/class_db.h"
#include "core/object/object.h"

class JustAMCPEditorPlugin;

class JustAMCPEditorTools : public Object {
	GDCLASS(JustAMCPEditorTools, Object);

	JustAMCPEditorPlugin *editor_plugin = nullptr;

protected:
	static void _bind_methods();

public:
	void set_editor_plugin(JustAMCPEditorPlugin *p_plugin);

	Dictionary editor_play_scene(const Dictionary &p_args);
	Dictionary editor_play_main(const Dictionary &p_args);
	Dictionary editor_stop_play(const Dictionary &p_args);
	Dictionary editor_is_playing(const Dictionary &p_args);

	Dictionary editor_select_node(const Dictionary &p_args);
	Dictionary editor_get_selected(const Dictionary &p_args);

	Dictionary editor_undo(const Dictionary &p_args);
	Dictionary editor_redo(const Dictionary &p_args);

	Dictionary editor_take_screenshot(const Dictionary &p_args);
	Dictionary editor_set_main_screen(const Dictionary &p_args);
	Dictionary editor_open_scene(const Dictionary &p_args);
	Dictionary editor_get_settings(const Dictionary &p_args);
	Dictionary editor_set_settings(const Dictionary &p_args);
	Dictionary editor_clear_output(const Dictionary &p_args);
	Dictionary editor_screenshot_game(const Dictionary &p_args);
	Dictionary editor_get_output_log(const Dictionary &p_args);
	Dictionary editor_get_errors(const Dictionary &p_args);
	Dictionary editor_reload_project(const Dictionary &p_args);
	Dictionary editor_save_all_scenes(const Dictionary &p_args);
	Dictionary editor_get_signals(const Dictionary &p_args);

	Dictionary execute_tool(const String &p_tool_name, const Dictionary &p_args);

	JustAMCPEditorTools() {}
	~JustAMCPEditorTools() {}
};

#endif
