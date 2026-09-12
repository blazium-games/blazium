/**************************************************************************/
/*  justamcp_json_rpc_list.cpp                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#include "justamcp_mcp_spec.h"
#include "justamcp_server.h"
#include "justamcp_session_manager.h"
#include "tools/justamcp_json_rpc_router.h"

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
	justamcp_apply_protocol_to_list_result(out, justamcp_active_protocol_version());
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

Dictionary JustAMCPJsonRpcRouter::route_runtime_host_initialize(JustAMCPServer *p_server, const Dictionary &p_payload, const Variant &p_req_id) {
	if (!p_payload.has("params") || p_payload["params"].get_type() != Variant::DICTIONARY) {
		return make_invalid_params(p_req_id, "initialize requires 'params' object.");
	}
	const Dictionary params = p_payload["params"];
	String client_protocol = MCPSessionManager::latest_protocol_version();
	if (params.has("protocolVersion") && params["protocolVersion"].get_type() == Variant::STRING) {
		client_protocol = String(params["protocolVersion"]);
	}
	const String negotiated = MCPSessionManager::negotiate_legacy_initialize(client_protocol);
	if (p_server) {
		p_server->transport_negotiated_protocol = negotiated;
	}

	Dictionary capabilities;
	Dictionary tools_cap;
	tools_cap["listChanged"] = true;
	capabilities["tools"] = tools_cap;
	Dictionary prompts_cap;
	prompts_cap["listChanged"] = true;
	capabilities["prompts"] = prompts_cap;

	Dictionary serverInfo;
	serverInfo["name"] = "blazium-game";
	if (justamcp_protocol_supports(negotiated, JUSTAMCP_FEATURE_SERVER_TITLE)) {
		serverInfo["title"] = "Blazium Game MCP";
	}
	serverInfo["version"] = "1.0.0";

	Dictionary result;
	result["protocolVersion"] = negotiated;
	result["capabilities"] = capabilities;
	result["instructions"] = "Project-owned MCP tools and prompts registered from res://mcp. No editor catalog.";
	result["serverInfo"] = serverInfo;

	Dictionary rpc_result;
	rpc_result["handled"] = true;
	rpc_result["jsonrpc"] = "2.0";
	rpc_result["id"] = p_req_id;
	rpc_result["result"] = result;
	return rpc_result;
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
