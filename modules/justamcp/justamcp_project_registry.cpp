/**************************************************************************/
/*  justamcp_project_registry.cpp                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#include "justamcp_project_registry.h"

#include "justamcp_pagination.h"
#include "tools/justamcp_json_rpc_helpers.h"

#include "core/io/json.h"
#include "core/variant/variant.h"

HashMap<String, JustAMCPProjectRegistry::ToolEntry> &JustAMCPProjectRegistry::tools() {
	static HashMap<String, ToolEntry> registry;
	return registry;
}

HashMap<String, JustAMCPProjectRegistry::PromptEntry> &JustAMCPProjectRegistry::prompts() {
	static HashMap<String, PromptEntry> registry;
	return registry;
}

bool JustAMCPProjectRegistry::is_valid_entry_name(const String &p_name) {
	if (p_name.is_empty() || p_name.strip_edges() != p_name) {
		return false;
	}
	if (p_name.begins_with("blazium_")) {
		return false;
	}
	for (int i = 0; i < p_name.length(); i++) {
		if (p_name[i] <= 32) {
			return false;
		}
	}
	return true;
}

void JustAMCPProjectRegistry::register_tool(const String &p_name, const String &p_description, const Dictionary &p_input_schema, const Callable &p_callable) {
	ERR_FAIL_COND_MSG(!is_valid_entry_name(p_name), "JustAMCP: tool name is empty, contains whitespace, or uses the reserved blazium_ prefix.");
	ToolEntry entry;
	entry.name = p_name;
	entry.description = p_description;
	entry.input_schema = p_input_schema;
	entry.callable = p_callable;
	tools()[p_name] = entry;
}

void JustAMCPProjectRegistry::unregister_tool(const String &p_name) {
	tools().erase(p_name);
}

bool JustAMCPProjectRegistry::has_tool(const String &p_name) {
	return tools().has(p_name);
}

Array JustAMCPProjectRegistry::list_tool_schemas() {
	Array schemas;
	for (const KeyValue<String, ToolEntry> &E : tools()) {
		Dictionary schema;
		schema["name"] = E.value.name;
		schema["description"] = E.value.description;
		Dictionary input = E.value.input_schema;
		if (input.is_empty()) {
			input["type"] = "object";
		}
		schema["inputSchema"] = input;
		schemas.push_back(schema);
	}
	return schemas;
}

Dictionary JustAMCPProjectRegistry::list_tools() {
	return justamcp_pagination_slice_array(list_tool_schemas(), String(), "tools");
}

Dictionary JustAMCPProjectRegistry::call_tool(const String &p_name, const Dictionary &p_args) {
	Dictionary ret;
	if (!tools().has(p_name)) {
		ret["ok"] = false;
		ret["error"] = "Unknown project tool: " + p_name;
		return ret;
	}
	const ToolEntry &entry = tools()[p_name];
	if (!entry.callable.is_valid()) {
		ret["ok"] = false;
		ret["error"] = "Project tool '" + p_name + "' has no callable.";
		return ret;
	}
	Variant result = entry.callable.call(p_args);
	ret["ok"] = true;
	ret["result"] = result;
	return ret;
}

Dictionary JustAMCPProjectRegistry::call_tool_mcp(const String &p_name, const Dictionary &p_args) {
	const Dictionary call = call_tool(p_name, p_args);
	if (!bool(call.get("ok", false))) {
		return JustAMCPJsonRpcHelpers::format_tool_result(false, Variant(), String(call.get("error", "Tool call failed.")));
	}
	return JustAMCPJsonRpcHelpers::format_tool_result(true, call.get("result", Variant()), String());
}

void JustAMCPProjectRegistry::register_prompt(const String &p_name, const String &p_description, const Callable &p_callable) {
	ERR_FAIL_COND_MSG(!is_valid_entry_name(p_name), "JustAMCP: prompt name is empty, contains whitespace, or uses the reserved blazium_ prefix.");
	PromptEntry entry;
	entry.name = p_name;
	entry.description = p_description;
	entry.callable = p_callable;
	prompts()[p_name] = entry;
}

void JustAMCPProjectRegistry::unregister_prompt(const String &p_name) {
	prompts().erase(p_name);
}

bool JustAMCPProjectRegistry::has_prompt(const String &p_name) {
	return prompts().has(p_name);
}

Array JustAMCPProjectRegistry::list_prompt_schemas() {
	Array schemas;
	for (const KeyValue<String, PromptEntry> &E : prompts()) {
		Dictionary schema;
		schema["name"] = E.value.name;
		schema["description"] = E.value.description;
		schemas.push_back(schema);
	}
	return schemas;
}

Dictionary JustAMCPProjectRegistry::call_prompt(const String &p_name, const Dictionary &p_args) {
	Dictionary ret;
	if (!prompts().has(p_name)) {
		ret["ok"] = false;
		ret["error"] = "Unknown project prompt: " + p_name;
		return ret;
	}
	const PromptEntry &entry = prompts()[p_name];
	if (!entry.callable.is_valid()) {
		ret["ok"] = false;
		ret["error"] = "Project prompt '" + p_name + "' has no callable.";
		return ret;
	}
	Variant result = entry.callable.call(p_args);
	ret["ok"] = true;
	if (result.get_type() == Variant::DICTIONARY) {
		Dictionary payload = result;
		if (payload.has("messages") || payload.has("description")) {
			if (payload.has("messages")) {
				ret["messages"] = payload["messages"];
			}
			if (payload.has("description")) {
				ret["description"] = payload["description"];
			}
			return ret;
		}
	}
	Dictionary message;
	message["role"] = "user";
	Array content;
	Dictionary text;
	text["type"] = "text";
	text["text"] = String(result);
	content.push_back(text);
	message["content"] = content;
	Array messages;
	messages.push_back(message);
	ret["messages"] = messages;
	return ret;
}

void JustAMCPProjectRegistry::clear() {
	tools().clear();
	prompts().clear();
}
