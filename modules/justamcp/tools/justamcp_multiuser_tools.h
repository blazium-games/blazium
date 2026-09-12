/**************************************************************************/
/*  justamcp_multiuser_tools.h                                            */
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

#include "modules/modules_enabled.gen.h"

#ifdef MODULE_MULTIUSER_EDITOR_ENABLED

#include "core/object/ref_counted.h"

class JustAMCPEditorPlugin;

class JustAMCPMultiuserTools : public RefCounted {
	GDCLASS(JustAMCPMultiuserTools, RefCounted);

private:
	JustAMCPEditorPlugin *editor_plugin = nullptr;

protected:
	static void _bind_methods();

public:
	void set_editor_plugin(JustAMCPEditorPlugin *p_plugin) { editor_plugin = p_plugin; }

	Dictionary multiuser_get_status(const Dictionary &p_args);
	Dictionary multiuser_send_chat(const Dictionary &p_args);
	Dictionary multiuser_kick_peer(const Dictionary &p_args);
	Dictionary multiuser_trigger_autowork(const Dictionary &p_args);

	Dictionary execute_tool(const String &p_tool_name, const Dictionary &p_args);

	JustAMCPMultiuserTools() {}
	~JustAMCPMultiuserTools() {}
};

#endif
