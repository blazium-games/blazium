/**************************************************************************/
/*  justamcp_input_tools.h                                                */
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

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/main/node.h"

class JustAMCPInputTools : public Object {
	GDCLASS(JustAMCPInputTools, Object);

private:
	EditorPlugin *editor_plugin = nullptr;
	const String COMMANDS_PATH = "user://mcp_input_commands";

	void _write_commands(const Array &p_events);

	Dictionary _simulate_key(const Dictionary &p_params);
	Dictionary _simulate_mouse_click(const Dictionary &p_params);
	Dictionary _simulate_mouse_move(const Dictionary &p_params);
	Dictionary _simulate_action(const Dictionary &p_params);
	Dictionary _simulate_touch(const Dictionary &p_params);
	Dictionary _simulate_gamepad(const Dictionary &p_args);
	Dictionary _simulate_sequence(const Dictionary &p_params);
	Dictionary _input_record(const Dictionary &p_args);
	Dictionary _input_replay(const Dictionary &p_args);

protected:
	static void _bind_methods();

public:
	void set_editor_plugin(EditorPlugin *p_plugin) { editor_plugin = p_plugin; }
	Dictionary execute_tool(const String &p_tool_name, const Dictionary &p_args);

	JustAMCPInputTools();
	~JustAMCPInputTools();
};
