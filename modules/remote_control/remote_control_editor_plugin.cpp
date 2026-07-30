/**************************************************************************/
/*  remote_control_editor_plugin.cpp                                      */
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

#include "remote_control_editor_plugin.h"

#include "remote_control_server.h"

#include "editor/debugger/editor_debugger_node.h"
#include "editor/debugger/script_editor_debugger.h"

void RemoteControlEditorPlugin::_try_start() {
	if (!RemoteControlServer::get_singleton()) {
		return;
	}
	if (RemoteControlServer::get_singleton()->is_started()) {
		_connect_debugger_hooks();
		return;
	}
	if (RemoteControlServer::should_enable_from_cmdline_or_settings()) {
		RemoteControlServer::get_singleton()->start();
	}
	_connect_debugger_hooks();
}

void RemoteControlEditorPlugin::_try_stop() {
}

void RemoteControlEditorPlugin::_connect_debugger_hooks() {
	if (debugger_hooks_connected) {
		return;
	}
	EditorDebuggerNode *debugger_node = EditorDebuggerNode::get_singleton();
	if (!debugger_node) {
		return;
	}
	debugger_node->connect("breaked", callable_mp(this, &RemoteControlEditorPlugin::_on_debugger_breaked));
	debugger_hooks_connected = true;
}

void RemoteControlEditorPlugin::_on_debugger_breaked(bool p_breaked, bool p_can_debug) {
	(void)p_can_debug;
	if (!p_breaked || !RemoteControlServer::get_singleton()) {
		return;
	}
	EditorDebuggerNode *debugger_node = EditorDebuggerNode::get_singleton();
	if (!debugger_node) {
		return;
	}
	ScriptEditorDebugger *dbg = debugger_node->get_current_debugger();
	if (!dbg) {
		return;
	}
	const String reason = dbg->get_break_reason();
	// Record pauses that look like error breaks (reason typically contains Error / Exception).
	const String lower = reason.to_lower();
	if (!(lower.contains("error") || lower.contains("exception") || dbg->get_error_count() > 0)) {
		return;
	}
	RemoteControlServer::get_singleton()->record_error_break(dbg->get_stack_script_file(), dbg->get_stack_script_line(), reason);
}

void RemoteControlEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_try_start();
		} break;
		default:
			break;
	}
}

RemoteControlEditorPlugin::RemoteControlEditorPlugin() {
}

#endif
