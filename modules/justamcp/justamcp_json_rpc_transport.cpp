/**************************************************************************/
/*  justamcp_json_rpc_transport.cpp                                       */
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

#include "modules/modules_enabled.gen.h"

#if defined(MODULE_HTTPSERVER_ENABLED)

#include "justamcp_json_rpc_transport.h"

#include "justamcp_log_levels.h"
#include "justamcp_server.h"
#include "justamcp_session_manager.h"
#include "tools/justamcp_json_rpc_helpers.h"
#include "tools/justamcp_json_rpc_router.h"
#include "tools/justamcp_prompt_executor.h"
#include "tools/justamcp_resource_executor.h"
#include "tools/justamcp_task_manager.h"
#include "tools/justamcp_tool_schema_cache.h"

#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "core/math/math_funcs.h"
#include "core/os/mutex.h"
#include "core/os/os.h"

bool JustAMCPJsonRpcTransport::_is_http_transport(Ref<HTTPResponse> p_response) {
	return p_response.is_valid();
}

Dictionary JustAMCPJsonRpcTransport::sanitize_wire_rpc(const Dictionary &p_rpc) {
	if (p_rpc.is_empty()) {
		return p_rpc;
	}
	Dictionary out = p_rpc.duplicate();
	out.erase("handled");
	out.erase("subscription_uri");
	out.erase("subscription_action");
	return out;
}

Dictionary JustAMCPJsonRpcTransport::handle_json_rpc(JustAMCPServer *p_server, const String &p_body, Ref<HTTPResponse> p_response, const String &p_caller_session_id) {
	if (!p_server) {
		return Dictionary();
	}
	Ref<JSON> json;
	json.instantiate();

	if (json->parse(p_body) != OK) {
		ERR_PRINT("JustAMCP: Failed to parse MCP JSON-RPC Payload: " + p_body);
		Dictionary err;
		err["jsonrpc"] = "2.0";
		Dictionary error_dict;
		error_dict["code"] = -32700;
		error_dict["message"] = "Invalid JSON";
		err["error"] = error_dict;
		return err;
	}

	Variant parsed = json->get_data();
	if (parsed.get_type() == Variant::ARRAY) {
		if (p_server->transport_negotiated_protocol == "2025-06-18" || p_server->transport_negotiated_protocol == "2025-11-25" || MCPSessionManager::is_modern_protocol_version(p_server->transport_negotiated_protocol)) {
			Dictionary err;
			err["jsonrpc"] = "2.0";
			Dictionary error_dict;
			error_dict["code"] = -32600;
			error_dict["message"] = "JSON-RPC batch requests are not supported for protocol " + p_server->transport_negotiated_protocol;
			err["error"] = error_dict;
			if (p_response.is_valid() && !p_response->is_sent()) {
				p_response->set_status(400);
				p_response->set_json(err);
			}
			return err;
		}
	}

	if (parsed.get_type() != Variant::DICTIONARY) {
		Dictionary err;
		err["jsonrpc"] = "2.0";
		Dictionary error_dict;
		error_dict["code"] = -32600;
		error_dict["message"] = "Invalid Request: JSON-RPC payload must be an object";
		err["error"] = error_dict;
		if (p_response.is_valid() && !p_response->is_sent()) {
			p_response->set_status(400);
			p_response->set_json(err);
		}
		return err;
	}

	return handle_json_rpc_parsed(p_server, parsed, p_response, p_caller_session_id);
}

Dictionary JustAMCPJsonRpcTransport::handle_json_rpc_parsed(JustAMCPServer *p_server, const Dictionary &p_payload, Ref<HTTPResponse> p_response, const String &p_caller_session_id) {
	if (!p_server) {
		return Dictionary();
	}
	Dictionary result = _handle_json_rpc_payload(p_server, p_payload, p_response, p_caller_session_id);
	if (MCPSessionManager::is_modern_protocol_version(p_server->transport_negotiated_protocol)) {
		const String method = p_payload.has("method") ? String(p_payload["method"]) : String();
		if (result.has("error") && result["error"].get_type() == Variant::DICTIONARY && int(Dictionary(result["error"]).get("code", 0)) == -32601) {
			if (p_response.is_valid() && !p_response->is_sent()) {
				p_response->set_status(404);
			}
		} else {
			MCPSessionManager::decorate_modern_rpc(result, method);
		}
	}
	return result;
}

Dictionary JustAMCPJsonRpcTransport::_handle_json_rpc_payload(JustAMCPServer *p_server, const Dictionary &p_payload, Ref<HTTPResponse> p_response, const String &p_caller_session_id) {
	if (!p_server) {
		return Dictionary();
	}

	Dictionary payload = p_payload;
	if (!payload.has("method")) {
		if (payload.has("id") && payload.has("result") && payload["result"].get_type() == Variant::DICTIONARY) {
			p_server->handle_client_rpc_result(p_caller_session_id, payload);
		}
		return Dictionary();
	}

	String method = payload["method"];
	String request_id = payload.has("id") ? String(Variant(payload["id"])) : "";

	Variant req_id_var;
	if (payload.has("id")) {
		req_id_var = payload["id"];
		if (req_id_var.get_type() == Variant::NIL) {
			Dictionary err;
			err["jsonrpc"] = "2.0";
			err["id"] = Variant();
			Dictionary error_dict;
			error_dict["code"] = -32600;
			error_dict["message"] = "Invalid Request: id must not be null";
			err["error"] = error_dict;
			if (p_response.is_valid() && !p_response->is_sent()) {
				p_response->set_status(400);
				p_response->set_json(err);
			}
			return err;
		}
		if (req_id_var.get_type() == Variant::FLOAT) {
			double d = req_id_var;
			if (Math::is_equal_approx(d, Math::round(d))) {
				req_id_var = (int64_t)Math::round(d);
			}
		}
		const Variant::Type id_type = req_id_var.get_type();
		if (id_type != Variant::INT && id_type != Variant::STRING) {
			Dictionary err;
			err["jsonrpc"] = "2.0";
			err["id"] = req_id_var;
			Dictionary error_dict;
			error_dict["code"] = -32600;
			error_dict["message"] = "Invalid Request: id must be a string or integer";
			err["error"] = error_dict;
			if (p_response.is_valid() && !p_response->is_sent()) {
				p_response->set_status(400);
				p_response->set_json(err);
			}
			return err;
		}
	}

	const bool modern = MCPSessionManager::is_modern_protocol_version(p_server->transport_negotiated_protocol);
	if (modern && payload.has("params") && payload["params"].get_type() == Variant::DICTIONARY) {
		const Dictionary params = payload["params"];
		if (params.has("_meta") && params["_meta"].get_type() == Variant::DICTIONARY) {
			const Dictionary meta = params["_meta"];
			if (meta.has("io.modelcontextprotocol/logLevel")) {
				const String level = justamcp_log_level_canonical(String(meta["io.modelcontextprotocol/logLevel"]));
				if (justamcp_log_level_is_valid(level)) {
					MutexLock lock(p_server->minimum_log_level_mutex);
					p_server->minimum_log_level = level;
				}
			}
		}
	}

	const bool debug_logging = GLOBAL_GET("blazium/justamcp/enable_debug_logging");
	if (debug_logging) {
		p_server->_mcp_debug_log("Executing JSON-RPC Method: " + method + " (ID: " + request_id + ")");
	}

	auto invalid_params = [&](const String &msg) -> Dictionary {
		if (debug_logging) {
			p_server->_mcp_debug_log("Payload Validation Failed for " + method + ": " + msg);
		}
		Dictionary err;
		err["jsonrpc"] = "2.0";
		err["id"] = req_id_var;
		Dictionary error_dict;
		error_dict["code"] = -32602;
		error_dict["message"] = msg;
		err["error"] = error_dict;
		return err;
	};

	if (method == "notifications/initialized") {
		if (p_server->session_manager && !p_caller_session_id.is_empty()) {
			p_server->session_manager->mark_session_initialized(p_caller_session_id);
		}
		return Dictionary();
	}

	if (method == "notifications/roots/list_changed") {
		const Dictionary params = payload.has("params") && payload["params"].get_type() == Variant::DICTIONARY ? Dictionary(payload["params"]) : Dictionary();
		if (p_server->session_manager) {
			p_server->session_manager->handle_roots_list_changed(p_caller_session_id, params);
		}
		return Dictionary();
	}

	if (method == "server/discover") {
#ifdef TOOLS_ENABLED
		Dictionary routed = JustAMCPJsonRpcRouter::route_discover(p_server, req_id_var);
		routed.erase("handled");
		return routed;
#else
		return invalid_params("server/discover is unavailable.");
#endif
	}

	if (modern && (method == "ping" || method == "logging/setLevel" || method == "resources/subscribe" || method == "resources/unsubscribe")) {
		Dictionary err;
		err["jsonrpc"] = "2.0";
		err["id"] = req_id_var;
		Dictionary error_dict;
		error_dict["code"] = -32601;
		error_dict["message"] = "Method not found: " + method;
		err["error"] = error_dict;
		if (p_response.is_valid() && !p_response->is_sent()) {
			p_response->set_status(404);
		}
		return err;
	}

#ifdef TOOLS_ENABLED
	{
		const bool route_resources = method == "resources/list" || method == "resources/templates/list" || method == "resources/read" || method == "resources/subscribe" || method == "resources/unsubscribe";
		const bool route_tasks = method == "tasks/list" || method == "tasks/get" || method == "tasks/result";
		if (method == "tasks/result") {
			const Dictionary params = payload.has("params") && payload["params"].get_type() == Variant::DICTIONARY ? Dictionary(payload["params"]) : Dictionary();
			if (bool(params.get("wait", false))) {
				return invalid_params("tasks/result with wait=true is not supported on HTTP transport. Poll with wait=false or use task notifications.");
			}
		}
		if (route_resources || route_tasks) {
			Dictionary routed = JustAMCPJsonRpcRouter::route(method, payload, req_id_var, p_server->resource_executor, p_server->task_manager);
			if (routed.get("handled", false)) {
				if (method == "resources/subscribe" || method == "resources/unsubscribe") {
					const String uri = routed.get("subscription_uri", "");
					const String action = routed.get("subscription_action", "");
					if (!uri.is_empty()) {
						if (action == "subscribe") {
							p_server->subscribe_resource(uri);
						} else {
							p_server->unsubscribe_resource(uri);
						}
					}
					routed.erase("subscription_uri");
					routed.erase("subscription_action");
				}
				routed.erase("handled");
				return routed;
			}
		}
	}
#endif

	if (method == "initialize") {
#ifdef TOOLS_ENABLED
		Dictionary routed = JustAMCPJsonRpcRouter::route_initialize(p_server, payload, req_id_var);
		if (routed.get("handled", false)) {
			routed.erase("handled");
			return routed;
		}
#endif
		return invalid_params("initialize requires 'params' object.");
	}

	if (method == "ping") {
#ifdef TOOLS_ENABLED
		Dictionary routed = JustAMCPJsonRpcRouter::route_ping(req_id_var);
		routed.erase("handled");
		return routed;
#else
		Dictionary rpc_result;
		rpc_result["jsonrpc"] = "2.0";
		rpc_result["id"] = req_id_var;
		rpc_result["result"] = Dictionary();
		return rpc_result;
#endif
	}

	if (method == "tools/list") {
		const String cursor = JustAMCPJsonRpcRouter::extract_list_cursor(payload);
#ifdef TOOLS_ENABLED
		Dictionary routed = JustAMCPJsonRpcRouter::route_tools_list(cursor, req_id_var);
		routed.erase("handled");
		return routed;
#else
		Dictionary empty;
		empty["ok"] = true;
		empty["tools"] = Array();
		Dictionary routed = JustAMCPJsonRpcRouter::finalize_list_result(empty, req_id_var);
		routed.erase("handled");
		return routed;
#endif
	}

	if (method == "prompts/list") {
		const String cursor = JustAMCPJsonRpcRouter::extract_list_cursor(payload);
#ifdef TOOLS_ENABLED
		Dictionary routed = JustAMCPJsonRpcRouter::route_prompts_list(cursor, req_id_var, p_server->prompt_executor);
		routed.erase("handled");
		return routed;
#else
		Dictionary empty;
		empty["ok"] = true;
		empty["prompts"] = Array();
		Dictionary routed = JustAMCPJsonRpcRouter::finalize_list_result(empty, req_id_var);
		routed.erase("handled");
		return routed;
#endif
	}

	if (method == "prompts/get") {
#ifdef TOOLS_ENABLED
		Dictionary routed = JustAMCPJsonRpcRouter::route_prompts_get(payload, req_id_var, p_server->prompt_executor);
		routed.erase("handled");
		return routed;
#else
		return invalid_params("prompts/get is unavailable.");
#endif
	}

	if (method == "notifications/cancelled") {
		Dictionary params = payload.has("params") ? Dictionary(payload["params"]) : Dictionary();
		Variant req_id;
		if (params.has("requestId")) {
			req_id = params["requestId"];
			if (req_id.get_type() == Variant::FLOAT) {
				double d = req_id;
				if (Math::is_equal_approx(d, Math::round(d))) {
					req_id = (int64_t)Math::round(d);
				}
			}
		}
		String reason = params.has("reason") ? String(Variant(params["reason"])) : "";
		p_server->call_deferred(SNAME("emit_signal"), "request_cancelled", req_id, reason, p_caller_session_id);
		return Dictionary();
	}

	if (method == "tasks/result") {
#ifdef TOOLS_ENABLED
		Dictionary routed = JustAMCPJsonRpcRouter::route("tasks/result", payload, req_id_var, nullptr, p_server->task_manager);
		if (routed.get("handled", false)) {
			routed.erase("handled");
			return routed;
		}
#endif
		return invalid_params("tasks/result is unavailable.");
	}

	if (method == "tasks/cancel") {
#ifdef TOOLS_ENABLED
		Dictionary routed = JustAMCPJsonRpcRouter::route_tasks_cancel(p_server, payload, req_id_var);
		if (routed.get("handled", false)) {
			routed.erase("handled");
			return routed;
		}
#endif
		return invalid_params("tasks/cancel requires 'params' object.");
	}

	if (method == "notifications/elicitation/complete") {
		Dictionary params = payload.has("params") ? Dictionary(payload["params"]) : Dictionary();
		String req_id = params.has("requestId") ? String(Variant(params["requestId"])) : "";
		Dictionary elicitation_result = params.has("result") ? Dictionary(params["result"]) : Dictionary();
		if (elicitation_result.is_empty() && params.has("action")) {
			elicitation_result = params;
		}
		p_server->complete_elicitation(req_id, elicitation_result);
		return Dictionary();
	}

	if (method == "logging/setLevel") {
#ifdef TOOLS_ENABLED
		Dictionary routed = JustAMCPJsonRpcRouter::route_logging_set_level(p_server, payload, req_id_var);
		if (routed.get("handled", false)) {
			routed.erase("handled");
			return routed;
		}
#endif
		return invalid_params("logging/setLevel requires 'params' object.");
	}

	if (method == "completion/complete") {
#ifdef TOOLS_ENABLED
		Dictionary routed = JustAMCPJsonRpcRouter::route_completion_complete(p_server, payload, req_id_var);
		if (routed.get("handled", false)) {
			routed.erase("handled");
			return routed;
		}
#endif
		return invalid_params("completion/complete is unavailable.");
	}

	if (method == "tools/call") {
		Dictionary params;
		String tool_name;
		Dictionary args;
#ifdef TOOLS_ENABLED
		Dictionary validated = JustAMCPJsonRpcRouter::route("tools/call", payload, req_id_var, nullptr, nullptr);
		if (!validated.get("handled", false)) {
			return invalid_params("tools/call requires 'params' object.");
		}
		if (validated.has("error")) {
			validated.erase("handled");
			return validated;
		}
		params = validated.get("params", Dictionary());
		tool_name = validated.get("tool_name", "");
		args = validated.get("arguments", Dictionary());
#else
		if (!payload.has("params") || payload["params"].get_type() != Variant::DICTIONARY) {
			return invalid_params("tools/call requires 'params' object.");
		}
		params = payload["params"];
		if (!params.has("name") || params["name"].get_type() != Variant::STRING) {
			return invalid_params("tools/call requires 'name' string.");
		}
		tool_name = params["name"];
		args = params.has("arguments") && params["arguments"].get_type() == Variant::DICTIONARY ? Dictionary(params["arguments"]) : Dictionary();
#endif

#ifdef TOOLS_ENABLED
		const String task_support = JustAMCPJsonRpcHelpers::get_tool_task_support(tool_name);
		const bool has_task_param = params.has("task") && params["task"].get_type() == Variant::DICTIONARY;
		if (task_support == "forbidden" && has_task_param) {
			Dictionary err;
			err["jsonrpc"] = "2.0";
			err["id"] = req_id_var;
			Dictionary error_dict;
			error_dict["code"] = -32601;
			error_dict["message"] = vformat("Tool '%s' does not support task-augmented execution.", tool_name);
			err["error"] = error_dict;
			return err;
		}
		if (task_support == "required" && !has_task_param) {
			Dictionary err;
			err["jsonrpc"] = "2.0";
			err["id"] = req_id_var;
			Dictionary error_dict;
			error_dict["code"] = -32601;
			error_dict["message"] = vformat("Tool '%s' requires task-augmented execution.", tool_name);
			err["error"] = error_dict;
			return err;
		}
#endif

		Dictionary enqueue_options;
		String progress_token = JustAMCPJsonRpcHelpers::progress_token_from_meta(JustAMCPJsonRpcHelpers::extract_request_meta(params));
		if (!progress_token.is_empty()) {
			enqueue_options["progress_token"] = progress_token;
		}

		auto wait_stateless_result = [&](MCPToolQueueEntry *p_entry, bool p_task_augmented) -> Dictionary {
			if (!p_entry || !p_entry->has_stateless_response) {
				return Dictionary();
			}
			const bool on_http = _is_http_transport(p_response);
			if (p_task_augmented || on_http) {
				if (!p_task_augmented && on_http && ProjectSettings::get_singleton() &&
						ProjectSettings::get_singleton()->has_setting("blazium/justamcp/stateless_tool_blocking") &&
						bool(GLOBAL_GET("blazium/justamcp/stateless_tool_blocking"))) {
					return invalid_params("stateless_tool_blocking is not supported on HTTP transport. Use SSE/streamable responses or poll tasks/result with wait=false.");
				}
				return Dictionary();
			}
			bool blocking = false;
			if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/justamcp/stateless_tool_blocking")) {
				blocking = bool(GLOBAL_GET("blazium/justamcp/stateless_tool_blocking"));
			}
			if (!blocking) {
				return Dictionary();
			}
			int timeout_ms = 120000;
			if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/justamcp/stateless_tool_timeout_ms")) {
				timeout_ms = int(GLOBAL_GET("blazium/justamcp/stateless_tool_timeout_ms"));
			}
			if (p_server->_wait_for_stateless_tool_entry(p_entry, timeout_ms)) {
				return p_entry->rpc_result;
			}
			return p_server->_stateless_tool_timeout_error(req_id_var);
		};

#ifdef TOOLS_ENABLED
		if (has_task_param) {
			int default_ttl = 600000;
			int default_poll = 1000;
			if (ProjectSettings::get_singleton()) {
				if (ProjectSettings::get_singleton()->has_setting("blazium/justamcp/task_default_ttl_ms")) {
					default_ttl = int(GLOBAL_GET("blazium/justamcp/task_default_ttl_ms"));
				}
				if (ProjectSettings::get_singleton()->has_setting("blazium/justamcp/task_poll_interval_ms")) {
					default_poll = int(GLOBAL_GET("blazium/justamcp/task_poll_interval_ms"));
				}
			}
			const Dictionary task_params = params["task"];
			const int ttl_ms = task_params.has("ttl") ? int(task_params["ttl"]) : default_ttl;
			const int poll_ms = task_params.has("pollInterval") ? int(task_params["pollInterval"]) : default_poll;

			if (!p_server->task_manager) {
				return invalid_params("Task manager unavailable.");
			}

			enqueue_options["is_task_augmented"] = true;
			enqueue_options["pending_task_dispatch"] = true;
			enqueue_options["task_ttl_ms"] = ttl_ms;
			enqueue_options["task_poll_ms"] = poll_ms;

			Dictionary queue_full_error;
			MCPToolQueueEntry *entry = p_server->_enqueue_tool_request(req_id_var, tool_name, args, p_response, queue_full_error, enqueue_options);
			if (!entry) {
				return queue_full_error;
			}

			p_server->call_deferred(SNAME("_dispatch_task_augmented_tools_call"), req_id_var);
			return wait_stateless_result(entry, true);
		}
#endif

		Dictionary queue_full_error;
		MCPToolQueueEntry *entry = p_server->_enqueue_tool_request(req_id_var, tool_name, args, p_response, queue_full_error, enqueue_options);
		if (!entry) {
			return queue_full_error;
		}

		if (!progress_token.is_empty()) {
			p_server->_register_progress_token(progress_token, String(), req_id_var);
		}

		p_server->_schedule_process_pending_tools();
		return wait_stateless_result(entry, false);
	}

	if (!payload.has("id")) {
		return Dictionary();
	}

	Dictionary err;
	err["jsonrpc"] = "2.0";
	err["id"] = req_id_var;
	Dictionary error_dict;
	error_dict["code"] = -32601;
	error_dict["message"] = "Method not found: " + method;
	err["error"] = error_dict;
	return err;
}

#endif
