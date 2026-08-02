/**************************************************************************/
/*  justamcp_remote_control_tools.cpp                                     */
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

#include "modules/modules_enabled.gen.h"

#ifdef MODULE_REMOTE_CONTROL_ENABLED

#include "justamcp_remote_control_tools.h"

#include "justamcp_tool_schema_builder.h"

#include "modules/remote_control/remote_control_registry.h"
#include "modules/remote_control/remote_control_server.h"

#include "core/config/project_settings.h"
#include "editor/editor_settings.h"

void JustAMCPRemoteControlTools::_bind_methods() {
	ClassDB::bind_method(D_METHOD("provide_tool_schemas", "register_only", "ignore_settings", "include_disabled_tools"), &JustAMCPRemoteControlTools::provide_tool_schemas, DEFVAL(false), DEFVAL(false), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("execute_tool", "tool_name", "arguments"), &JustAMCPRemoteControlTools::execute_tool);
}

Dictionary JustAMCPRemoteControlTools::_make_error(const String &p_message) const {
	Dictionary err;
	err["ok"] = false;
	err["error"] = p_message;
	return err;
}

Array JustAMCPRemoteControlTools::provide_tool_schemas(bool p_register_only, bool p_ignore_settings, bool p_include_disabled_tools) {
	return get_tool_schemas(p_register_only, p_ignore_settings, p_include_disabled_tools);
}

Array JustAMCPRemoteControlTools::get_tool_schemas(bool p_register_only, bool p_ignore_settings, bool p_include_disabled_tools) {
	Array tools;
	const String current_category = "remote_control_tools";
	const bool is_core = false;

	auto add_schema = [&](const String &p_name, const String &p_desc, const Vector<String> &p_props, const Vector<String> &p_req) {
		const String full_name = "blazium_" + p_name;
		if (p_register_only) {
			JustAMCPToolSchemaBuilder::register_tool_settings(current_category, full_name, is_core);
			return;
		}
		bool cat_enabled = true;
		bool tool_enabled = true;
		if (!p_ignore_settings) {
			if (!JustAMCPToolSchemaBuilder::resolve_tool_enabled(current_category, full_name, p_ignore_settings, p_include_disabled_tools, cat_enabled, tool_enabled)) {
				return;
			}
		}
		tools.push_back(JustAMCPToolSchemaBuilder::build_tool_schema(full_name, p_desc, current_category, cat_enabled && tool_enabled, p_props, p_req));
	};

	add_schema("remote_control_status", "Return in-process remote_control server status.",
			Vector<String>{}, Vector<String>{});
	add_schema("remote_control_exec", "Execute a remote_control builtin/command in-process.",
			Vector<String>{ "command", "string", "arguments", "object" }, Vector<String>{ "command" });
	add_schema("remote_control_eval", "Evaluate an expression via remote_control (respects allow_eval).",
			Vector<String>{ "expression", "string", "language", "string" }, Vector<String>{ "expression" });
	add_schema("remote_control_instance", "Return remote_control instance id/port/token summary.",
			Vector<String>{}, Vector<String>{});

	return tools;
}

Dictionary JustAMCPRemoteControlTools::remote_control_status(const Dictionary &p_args) {
	(void)p_args;
	RemoteControlServer *server = RemoteControlServer::get_singleton();
	if (!server) {
		return _make_error("RemoteControlServer not available");
	}
	Dictionary result = server->get_status();
	result["ok"] = true;
	return result;
}

Dictionary JustAMCPRemoteControlTools::remote_control_exec(const Dictionary &p_args) {
	const String command = p_args.get("command", "");
	if (command.is_empty()) {
		return _make_error("command is required");
	}
	if (!RemoteControlRegistry::get_singleton()) {
		return _make_error("RemoteControlRegistry not available");
	}
	Dictionary args = p_args.get("arguments", Dictionary());
	Dictionary result = RemoteControlRegistry::get_singleton()->execute(command, args);
	if (!result.has("ok")) {
		result["ok"] = true;
	}
	return result;
}

Dictionary JustAMCPRemoteControlTools::remote_control_eval(const Dictionary &p_args) {
	const String expression = p_args.get("expression", "");
	if (expression.is_empty()) {
		return _make_error("expression is required");
	}
	RemoteControlServer *server = RemoteControlServer::get_singleton();
	if (!server) {
		return _make_error("RemoteControlServer not available");
	}
	if (!server->get_allow_eval()) {
		return _make_error("Eval is disabled. Enable blazium/remote_control/allow_eval.");
	}
	const String language = p_args.get("language", "gdscript");
	Dictionary result = server->eval_expression(expression, language);
	if (!result.has("ok")) {
		result["ok"] = true;
	}
	return result;
}

Dictionary JustAMCPRemoteControlTools::remote_control_instance(const Dictionary &p_args) {
	(void)p_args;
	RemoteControlServer *server = RemoteControlServer::get_singleton();
	if (!server) {
		return _make_error("RemoteControlServer not available");
	}
	Dictionary result;
	result["ok"] = true;
	result["started"] = server->is_started();
	result["port"] = server->get_port();
	result["instance_id"] = server->get_remote_instance_id();
	result["allow_eval"] = server->get_allow_eval();

	return result;
}

Dictionary JustAMCPRemoteControlTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	String tool_name = p_tool_name;
	if (tool_name.begins_with("blazium_")) {
		tool_name = tool_name.substr(8);
	}
	if (tool_name == "remote_control_status") {
		return remote_control_status(p_args);
	}
	if (tool_name == "remote_control_exec") {
		return remote_control_exec(p_args);
	}
	if (tool_name == "remote_control_eval") {
		return remote_control_eval(p_args);
	}
	if (tool_name == "remote_control_instance") {
		return remote_control_instance(p_args);
	}
	return _make_error("Unknown remote_control tool: " + p_tool_name);
}

JustAMCPRemoteControlTools::JustAMCPRemoteControlTools() {
}

JustAMCPRemoteControlTools::~JustAMCPRemoteControlTools() {
}

#endif
#endif
