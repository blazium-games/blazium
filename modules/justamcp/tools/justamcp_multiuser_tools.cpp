/**************************************************************************/
/*  justamcp_multiuser_tools.cpp                                          */
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

#include "justamcp_multiuser_tools.h"

#ifdef MODULE_MULTIUSER_EDITOR_ENABLED
#ifdef TOOLS_ENABLED

#include "modules/multiuser_editor/multiuser_editor_plugin.h"

void JustAMCPMultiuserTools::_bind_methods() {
	ClassDB::bind_method(D_METHOD("multiuser_get_status", "args"), &JustAMCPMultiuserTools::multiuser_get_status);
	ClassDB::bind_method(D_METHOD("multiuser_send_chat", "args"), &JustAMCPMultiuserTools::multiuser_send_chat);
	ClassDB::bind_method(D_METHOD("multiuser_kick_peer", "args"), &JustAMCPMultiuserTools::multiuser_kick_peer);
	ClassDB::bind_method(D_METHOD("multiuser_trigger_autowork", "args"), &JustAMCPMultiuserTools::multiuser_trigger_autowork);
}

Dictionary JustAMCPMultiuserTools::multiuser_get_status(const Dictionary &p_args) {
	Dictionary result;
	if (!MultiuserEditorPlugin::get_singleton()) {
		result["ok"] = false;
		result["error"] = "MultiuserEditorPlugin is not instantiated.";
		return result;
	}

	MultiuserEditorPlugin *plugin = MultiuserEditorPlugin::get_singleton();
	result["ok"] = true;
	result["is_connected"] = plugin->is_session_connected();
	result["local_peer_id"] = plugin->get_local_peer_id();

	return result;
}

Dictionary JustAMCPMultiuserTools::multiuser_send_chat(const Dictionary &p_args) {
	Dictionary result;
	String message = p_args.get("message", "");

	if (!MultiuserEditorPlugin::get_singleton()) {
		result["ok"] = false;
		result["error"] = "MultiuserEditorPlugin is not instantiated.";
		return result;
	}
	if (!MultiuserEditorPlugin::get_singleton()->is_session_connected()) {
		result["ok"] = false;
		result["error"] = "Must be connected to a Multiuser Session to send chat.";
		return result;
	}

	MultiuserEditorPlugin::get_singleton()->send_chat(message);
	result["ok"] = true;
	result["message"] = "Sent chat message successfully.";
	return result;
}

Dictionary JustAMCPMultiuserTools::multiuser_kick_peer(const Dictionary &p_args) {
	Dictionary result;
	String target_peer_id = p_args.get("peer_id", "");

	if (!MultiuserEditorPlugin::get_singleton()) {
		result["ok"] = false;
		result["error"] = "MultiuserEditorPlugin is not instantiated.";
		return result;
	}

	MultiuserEditorPlugin::get_singleton()->kick_peer(target_peer_id);
	result["ok"] = true;
	result["message"] = "Kicked peer: " + target_peer_id + " (Action silently ignored if not Host or peer not found).";
	return result;
}

Dictionary JustAMCPMultiuserTools::multiuser_trigger_autowork(const Dictionary &p_args) {
	Dictionary result;

	if (!MultiuserEditorPlugin::get_singleton()) {
		result["ok"] = false;
		result["error"] = "MultiuserEditorPlugin is not instantiated.";
		return result;
	}

	MultiuserEditorPlugin::get_singleton()->trigger_autowork();
	result["ok"] = true;
	result["message"] = "Remotely triggered Autowork Execution across the peer mesh.";
	return result;
}

Dictionary JustAMCPMultiuserTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "multiuser_get_status") {
		return multiuser_get_status(p_args);
	}
	if (p_tool_name == "multiuser_send_chat") {
		return multiuser_send_chat(p_args);
	}
	if (p_tool_name == "multiuser_kick_peer") {
		return multiuser_kick_peer(p_args);
	}
	if (p_tool_name == "multiuser_trigger_autowork") {
		return multiuser_trigger_autowork(p_args);
	}
	return Dictionary();
}

#endif
#endif
