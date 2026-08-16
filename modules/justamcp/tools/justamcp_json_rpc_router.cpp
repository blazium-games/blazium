/**************************************************************************/
/*  justamcp_json_rpc_router.cpp                                          */
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

#include "justamcp_json_rpc_router.h"

#include "../justamcp_log_levels.h"
#include "../justamcp_server.h"
#include "../justamcp_session_manager.h"
#include "justamcp_prompt_executor.h"
#include "justamcp_resource_executor.h"
#include "justamcp_task_manager.h"
#include "justamcp_tool_executor.h"

#include "core/os/mutex.h"

String JustAMCPJsonRpcRouter::extract_list_cursor(const Dictionary &p_payload) {
	if (!p_payload.has("params") || p_payload["params"].get_type() != Variant::DICTIONARY) {
		return String();
	}
	const Dictionary params = p_payload["params"];
	if (!params.has("cursor")) {
		return String();
	}
	return String(params["cursor"]);
}

Dictionary JustAMCPJsonRpcRouter::finalize_list_result(const Dictionary &p_result, const Variant &p_req_id) {
	if (p_result.has("ok") && !bool(p_result.get("ok", true))) {
		Dictionary err;
		err["handled"] = true;
		err["jsonrpc"] = "2.0";
		err["id"] = p_req_id;
		Dictionary error_dict;
		error_dict["code"] = p_result.get("error_code", -32602);
		error_dict["message"] = p_result.get("error", "Invalid params.");
		err["error"] = error_dict;
		return err;
	}

	Dictionary out = p_result.duplicate();
	out.erase("ok");
	Dictionary rpc_result;
	rpc_result["handled"] = true;
	rpc_result["jsonrpc"] = "2.0";
	rpc_result["id"] = p_req_id;
	rpc_result["result"] = out;
	return rpc_result;
}

Dictionary JustAMCPJsonRpcRouter::make_invalid_params(const Variant &p_req_id, const String &p_message) {
	Dictionary err;
	err["handled"] = true;
	err["jsonrpc"] = "2.0";
	err["id"] = p_req_id;
	Dictionary error_dict;
	error_dict["code"] = -32602;
	error_dict["message"] = p_message;
	err["error"] = error_dict;
	return err;
}

Dictionary JustAMCPJsonRpcRouter::finalize_action_result(const Dictionary &p_result, const Variant &p_req_id) {
	Dictionary rpc_result;
	rpc_result["handled"] = true;
	rpc_result["jsonrpc"] = "2.0";
	rpc_result["id"] = p_req_id;
	if (p_result.get("ok", false)) {
		Dictionary out = p_result.duplicate();
		out.erase("ok");
		rpc_result["result"] = out;
	} else {
		Dictionary error_dict;
		error_dict["code"] = p_result.get("error_code", -32603);
		error_dict["message"] = p_result.get("error", "Request failed.");
		rpc_result["error"] = error_dict;
	}
	return rpc_result;
}

static Dictionary _handle_resource_subscription(const String &p_method, const Dictionary &p_payload, const Variant &p_req_id_var) {
	Dictionary empty;
	empty["handled"] = false;
	if (p_method != "resources/subscribe" && p_method != "resources/unsubscribe") {
		return empty;
	}

	if (!p_payload.has("params") || p_payload["params"].get_type() != Variant::DICTIONARY) {
		return JustAMCPJsonRpcRouter::make_invalid_params(p_req_id_var, p_method + " requires 'params' object.");
	}

	const Dictionary params = p_payload["params"];
	if (!params.has("uri") || params["uri"].get_type() != Variant::STRING) {
		return JustAMCPJsonRpcRouter::make_invalid_params(p_req_id_var, p_method + " requires 'uri' string.");
	}

	Dictionary rpc_result;
	rpc_result["handled"] = true;
	rpc_result["jsonrpc"] = "2.0";
	rpc_result["id"] = p_req_id_var;
	rpc_result["result"] = Dictionary();
	rpc_result["subscription_uri"] = String(params["uri"]);
	rpc_result["subscription_action"] = p_method == "resources/subscribe" ? "subscribe" : "unsubscribe";
	return rpc_result;
}

static Dictionary _handle_resources_read(const Dictionary &p_payload, const Variant &p_req_id_var, JustAMCPResourceExecutor *p_resources) {
	if (!p_payload.has("params") || p_payload["params"].get_type() != Variant::DICTIONARY) {
		return JustAMCPJsonRpcRouter::make_invalid_params(p_req_id_var, "resources/read requires 'params' object.");
	}
	const Dictionary params = p_payload["params"];
	if (!params.has("uri") || params["uri"].get_type() != Variant::STRING) {
		return JustAMCPJsonRpcRouter::make_invalid_params(p_req_id_var, "resources/read requires 'uri' string.");
	}
	const String uri = params["uri"];
	Dictionary result;
	if (p_resources) {
		result = p_resources->read_resource(uri);
	}
	Dictionary rpc_result;
	rpc_result["handled"] = true;
	rpc_result["jsonrpc"] = "2.0";
	rpc_result["id"] = p_req_id_var;
	const bool ok = result.get("ok", false);
	if (ok) {
		result.erase("ok");
		rpc_result["result"] = result;
	} else {
		Dictionary error_dict;
		error_dict["code"] = result.get("error_code", -32603);
		error_dict["message"] = result.get("error", "Failed to retrieve resource " + uri);
		rpc_result["error"] = error_dict;
	}
	return rpc_result;
}

static Dictionary _handle_tasks_get(const Dictionary &p_payload, const Variant &p_req_id_var, JustAMCPTaskManager *p_tasks) {
	if (!p_payload.has("params") || p_payload["params"].get_type() != Variant::DICTIONARY) {
		return JustAMCPJsonRpcRouter::make_invalid_params(p_req_id_var, "tasks/get requires 'params' object.");
	}
	const Dictionary params = p_payload["params"];
	if (!params.has("taskId") || params["taskId"].get_type() != Variant::STRING) {
		return JustAMCPJsonRpcRouter::make_invalid_params(p_req_id_var, "tasks/get requires 'taskId' string.");
	}
	const String task_id = params["taskId"];
	Dictionary result;
	if (p_tasks) {
		result = p_tasks->get_task(task_id);
	}
	Dictionary rpc_result;
	rpc_result["handled"] = true;
	rpc_result["jsonrpc"] = "2.0";
	rpc_result["id"] = p_req_id_var;
	const bool ok = result.get("ok", false);
	if (ok) {
		result.erase("ok");
		rpc_result["result"] = result;
	} else {
		Dictionary error_dict;
		error_dict["code"] = result.get("error_code", -32603);
		error_dict["message"] = result.get("error", "Failed to retrieve task " + task_id);
		rpc_result["error"] = error_dict;
	}
	return rpc_result;
}

static Dictionary _handle_tasks_result(const Dictionary &p_payload, const Variant &p_req_id_var, JustAMCPTaskManager *p_tasks) {
	if (!p_payload.has("params") || p_payload["params"].get_type() != Variant::DICTIONARY) {
		return JustAMCPJsonRpcRouter::make_invalid_params(p_req_id_var, "tasks/result requires 'params' object.");
	}
	const Dictionary params = p_payload["params"];
	if (!params.has("taskId") || params["taskId"].get_type() != Variant::STRING) {
		return JustAMCPJsonRpcRouter::make_invalid_params(p_req_id_var, "tasks/result requires 'taskId' string.");
	}
	const String task_id = params["taskId"];
	const bool wait = bool(params.get("wait", false));
	Dictionary result;
	if (p_tasks) {
		result = p_tasks->get_task_result(task_id, wait);
	}
	return JustAMCPJsonRpcRouter::finalize_action_result(result, p_req_id_var);
}

static Dictionary _handle_tools_call_validation(const Dictionary &p_payload, const Variant &p_req_id_var) {
	if (!p_payload.has("params") || p_payload["params"].get_type() != Variant::DICTIONARY) {
		return JustAMCPJsonRpcRouter::make_invalid_params(p_req_id_var, "tools/call requires 'params' object.");
	}
	const Dictionary params = p_payload["params"];
	if (!params.has("name") || params["name"].get_type() != Variant::STRING) {
		return JustAMCPJsonRpcRouter::make_invalid_params(p_req_id_var, "tools/call requires 'name' string.");
	}
	Dictionary rpc_result;
	rpc_result["handled"] = true;
	rpc_result["validated"] = true;
	rpc_result["tool_name"] = String(params["name"]);
	rpc_result["arguments"] = params.has("arguments") && params["arguments"].get_type() == Variant::DICTIONARY ? Dictionary(params["arguments"]) : Dictionary();
	rpc_result["params"] = params;
	return rpc_result;
}

Dictionary JustAMCPJsonRpcRouter::route(const String &p_method, const Dictionary &p_payload, const Variant &p_req_id_var, JustAMCPResourceExecutor *p_resources, JustAMCPTaskManager *p_tasks) {
	Dictionary empty;
	empty["handled"] = false;

	if (p_method == "resources/list") {
		const String cursor = extract_list_cursor(p_payload);
		if (p_resources) {
			return finalize_list_result(p_resources->list_resources(cursor), p_req_id_var);
		}
		Dictionary fallback;
		fallback["ok"] = true;
		fallback["resources"] = Array();
		return finalize_list_result(fallback, p_req_id_var);
	}

	if (p_method == "resources/templates/list") {
		const String cursor = extract_list_cursor(p_payload);
		if (p_resources) {
			return finalize_list_result(p_resources->list_resource_templates(cursor), p_req_id_var);
		}
		Dictionary fallback;
		fallback["ok"] = true;
		fallback["resourceTemplates"] = Array();
		return finalize_list_result(fallback, p_req_id_var);
	}

	if (p_method == "resources/read") {
		return _handle_resources_read(p_payload, p_req_id_var, p_resources);
	}

	if (p_method == "tasks/list") {
		const String cursor = extract_list_cursor(p_payload);
		if (p_tasks) {
			return finalize_list_result(p_tasks->list_tasks(cursor), p_req_id_var);
		}
		Dictionary fallback;
		fallback["ok"] = true;
		fallback["tasks"] = Array();
		return finalize_list_result(fallback, p_req_id_var);
	}

	if (p_method == "tasks/get") {
		return _handle_tasks_get(p_payload, p_req_id_var, p_tasks);
	}

	if (p_method == "tasks/result") {
		return _handle_tasks_result(p_payload, p_req_id_var, p_tasks);
	}

	if (p_method == "tools/call") {
		return _handle_tools_call_validation(p_payload, p_req_id_var);
	}

	return _handle_resource_subscription(p_method, p_payload, p_req_id_var);
}

Dictionary JustAMCPJsonRpcRouter::route_tools_list(const String &p_cursor, const Variant &p_req_id_var) {
	Dictionary result = JustAMCPToolExecutor::list_tools(p_cursor);
	return finalize_list_result(result, p_req_id_var);
}

Dictionary JustAMCPJsonRpcRouter::route_prompts_list(const String &p_cursor, const Variant &p_req_id_var, JustAMCPPromptExecutor *p_prompts) {
	if (p_prompts) {
		return finalize_list_result(p_prompts->list_prompts(p_cursor), p_req_id_var);
	}
	Dictionary empty;
	empty["ok"] = true;
	empty["prompts"] = Array();
	return finalize_list_result(empty, p_req_id_var);
}

Dictionary JustAMCPJsonRpcRouter::route_prompts_get(const Dictionary &p_payload, const Variant &p_req_id_var, JustAMCPPromptExecutor *p_prompts) {
	if (!p_payload.has("params") || p_payload["params"].get_type() != Variant::DICTIONARY) {
		return make_invalid_params(p_req_id_var, "prompts/get requires 'params' object.");
	}
	const Dictionary params = p_payload["params"];
	if (!params.has("name") || params["name"].get_type() != Variant::STRING) {
		return make_invalid_params(p_req_id_var, "prompts/get requires 'name' string.");
	}
	const String prompt_name = params["name"];
	const Dictionary args = params.has("arguments") && params["arguments"].get_type() == Variant::DICTIONARY ? Dictionary(params["arguments"]) : Dictionary();
	Dictionary result;
	if (p_prompts) {
		result = p_prompts->get_prompt(prompt_name, args);
	}
	return finalize_action_result(result, p_req_id_var);
}

Dictionary JustAMCPJsonRpcRouter::route_initialize(JustAMCPServer *p_server, const Dictionary &p_payload, const Variant &p_req_id_var) {
	Dictionary empty;
	empty["handled"] = false;
	if (!p_server || !p_payload.has("params") || p_payload["params"].get_type() != Variant::DICTIONARY) {
		return empty;
	}
	const Dictionary params = p_payload["params"];
	String client_protocol = MCPSessionManager::latest_protocol_version();
	if (params.has("protocolVersion") && params["protocolVersion"].get_type() == Variant::STRING) {
		client_protocol = String(params["protocolVersion"]);
	}
	p_server->transport_negotiated_protocol = MCPSessionManager::negotiate_legacy_initialize(client_protocol);

	Dictionary result;
	result["protocolVersion"] = p_server->transport_negotiated_protocol;
	Dictionary capabilities;
	Dictionary tools_cap;
	tools_cap["listChanged"] = true;
	capabilities["tools"] = tools_cap;
	Dictionary prompts_cap;
	prompts_cap["listChanged"] = true;
	capabilities["prompts"] = prompts_cap;
	Dictionary resources_cap;
	resources_cap["listChanged"] = true;
	resources_cap["subscribe"] = true;
	capabilities["resources"] = resources_cap;
	capabilities["logging"] = Dictionary();
	capabilities["completions"] = Dictionary();
	Dictionary tasks_cap;
	tasks_cap["list"] = Dictionary();
	tasks_cap["cancel"] = Dictionary();
	Dictionary requests;
	Dictionary tools;
	tools["call"] = Dictionary();
	requests["tools"] = tools;
	tasks_cap["requests"] = requests;
	capabilities["tasks"] = tasks_cap;
	if (p_server->transport_negotiated_protocol == "2025-11-25" || MCPSessionManager::is_modern_protocol_version(p_server->transport_negotiated_protocol)) {
		Dictionary elicitation_cap;
		elicitation_cap["form"] = Dictionary();
		elicitation_cap["url"] = Dictionary();
		capabilities["elicitation"] = elicitation_cap;
	}
	result["capabilities"] = capabilities;
	result["instructions"] = "Use blazium_* tools and blazium:// resources. Prefer editor tools for scene/resource edits, runtime_* tools only when a game bridge is active, and guide resources such as blazium://guide/tool-index for workflow orientation.";
	Dictionary serverInfo;
	serverInfo["name"] = "blazium-mcp-server";
	serverInfo["title"] = "Blazium MCP";
	serverInfo["version"] = "1.0.0";
	serverInfo["websiteUrl"] = "https://blazium.app";
	result["serverInfo"] = serverInfo;

	Dictionary rpc_result;
	rpc_result["handled"] = true;
	rpc_result["jsonrpc"] = "2.0";
	rpc_result["id"] = p_req_id_var;
	rpc_result["result"] = result;
	return rpc_result;
}

Dictionary JustAMCPJsonRpcRouter::route_discover(JustAMCPServer *p_server, const Variant &p_req_id_var) {
	Dictionary result;
	Array supported;
	const Vector<String> versions = MCPSessionManager::accepted_protocol_versions();
	for (int i = 0; i < versions.size(); i++) {
		supported.push_back(versions[i]);
	}
	result["supportedVersions"] = supported;

	Dictionary capabilities;
	Dictionary tools_cap;
	tools_cap["listChanged"] = true;
	capabilities["tools"] = tools_cap;
	Dictionary prompts_cap;
	prompts_cap["listChanged"] = true;
	capabilities["prompts"] = prompts_cap;
	Dictionary resources_cap;
	resources_cap["listChanged"] = true;
	capabilities["resources"] = resources_cap;
	capabilities["completions"] = Dictionary();
	Dictionary elicitation_cap;
	elicitation_cap["form"] = Dictionary();
	elicitation_cap["url"] = Dictionary();
	capabilities["elicitation"] = elicitation_cap;
	Dictionary extensions;
	if (p_server && p_server->task_manager) {
		extensions["io.modelcontextprotocol/tasks"] = Dictionary();
	}
	capabilities["extensions"] = extensions;
	result["capabilities"] = capabilities;
	result["instructions"] = "Use blazium_* tools and blazium:// resources. Prefer editor tools for scene/resource edits, runtime_* tools only when a game bridge is active, and guide resources such as blazium://guide/tool-index for workflow orientation.";
	result["ttlMs"] = 3600000;
	result["cacheScope"] = "public";
	result["resultType"] = "complete";
	Dictionary meta;
	meta["io.modelcontextprotocol/serverInfo"] = MCPSessionManager::mcp_server_info();
	result["_meta"] = meta;

	Dictionary rpc_result;
	rpc_result["handled"] = true;
	rpc_result["jsonrpc"] = "2.0";
	rpc_result["id"] = p_req_id_var;
	rpc_result["result"] = result;
	return rpc_result;
}

Dictionary JustAMCPJsonRpcRouter::route_ping(const Variant &p_req_id_var) {
	Dictionary rpc_result;
	rpc_result["handled"] = true;
	rpc_result["jsonrpc"] = "2.0";
	rpc_result["id"] = p_req_id_var;
	rpc_result["result"] = Dictionary();
	return rpc_result;
}

Dictionary JustAMCPJsonRpcRouter::route_logging_set_level(JustAMCPServer *p_server, const Dictionary &p_payload, const Variant &p_req_id_var) {
	Dictionary empty;
	empty["handled"] = false;
	if (!p_server || !p_payload.has("params") || p_payload["params"].get_type() != Variant::DICTIONARY) {
		return empty;
	}
	const Dictionary params = p_payload["params"];
	if (!params.has("level") || params["level"].get_type() != Variant::STRING) {
		return empty;
	}
	const String level = justamcp_log_level_canonical(String(params["level"]));
	if (!justamcp_log_level_is_valid(level)) {
		return empty;
	}
	{
		MutexLock lock(p_server->minimum_log_level_mutex);
		p_server->minimum_log_level = level;
	}
	Dictionary rpc_result;
	rpc_result["handled"] = true;
	rpc_result["jsonrpc"] = "2.0";
	rpc_result["id"] = p_req_id_var;
	rpc_result["result"] = Dictionary();
	return rpc_result;
}

Dictionary JustAMCPJsonRpcRouter::route_tasks_cancel(JustAMCPServer *p_server, const Dictionary &p_payload, const Variant &p_req_id_var) {
	Dictionary empty;
	empty["handled"] = false;
	if (!p_server || !p_payload.has("params") || p_payload["params"].get_type() != Variant::DICTIONARY) {
		return empty;
	}
	const Dictionary params = p_payload["params"];
	if (!params.has("taskId") || params["taskId"].get_type() != Variant::STRING) {
		return empty;
	}
	const String task_id = params["taskId"];
	Dictionary result;
	if (p_server->task_manager) {
		result = p_server->task_manager->cancel_task(task_id);
		if (result.get("ok", false)) {
			p_server->request_task_queue_cancel(task_id);
		}
	}
	Dictionary rpc_result;
	rpc_result["handled"] = true;
	rpc_result["jsonrpc"] = "2.0";
	rpc_result["id"] = p_req_id_var;
	if (result.get("ok", false)) {
		result.erase("ok");
		rpc_result["result"] = result;
	} else {
		Dictionary error_dict;
		error_dict["code"] = result.get("error_code", -32603);
		error_dict["message"] = result.get("error", "Task error.");
		rpc_result["error"] = error_dict;
	}
	return rpc_result;
}

Dictionary JustAMCPJsonRpcRouter::route_completion_complete(JustAMCPServer *p_server, const Dictionary &p_payload, const Variant &p_req_id_var) {
	Dictionary empty;
	empty["handled"] = false;
	if (!p_server) {
		return empty;
	}
	const Dictionary params = p_payload.has("params") ? Dictionary(p_payload["params"]) : Dictionary();
	const Dictionary ref = params.has("ref") ? Dictionary(params["ref"]) : Dictionary();
	const Dictionary argument = params.has("argument") ? Dictionary(params["argument"]) : Dictionary();
	Dictionary result;
	if (p_server->prompt_executor) {
		result = p_server->prompt_executor->complete_prompt(ref, argument);
	}
	Dictionary rpc_result;
	rpc_result["handled"] = true;
	rpc_result["jsonrpc"] = "2.0";
	rpc_result["id"] = p_req_id_var;
	rpc_result["result"] = result;
	return rpc_result;
}

#endif
