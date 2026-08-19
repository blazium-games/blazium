/**************************************************************************/
/*  justamcp_mcp_client_bridge.cpp                                        */
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

#include "justamcp_mcp_client_bridge.h"

#include "../justamcp_server.h"
#include "../justamcp_session_manager.h"
#include "../justamcp_tool_context.h"
#include "justamcp_tool_schema_builder.h"

#include "core/config/project_settings.h"
#include "core/io/http_client.h"
#include "core/io/json.h"
#include "core/object/worker_thread_pool.h"
#include "core/os/thread.h"
#include "editor/editor_settings.h"

JustAMCPMCPClientBridge *JustAMCPMCPClientBridge::singleton = nullptr;
static uint64_t g_mcp_client_rpc_id = 1;

static bool _justamcp_bridge_host_allowed(const String &p_host) {
	const String host = p_host.strip_edges().to_lower();
	if (host == "127.0.0.1" || host == "localhost" || host == "::1" || host == "[::1]") {
		return true;
	}
	if (!ProjectSettings::get_singleton() || !ProjectSettings::get_singleton()->has_setting("blazium/justamcp/bridge_url_allow_hosts")) {
		return false;
	}
	const Array allow = GLOBAL_GET("blazium/justamcp/bridge_url_allow_hosts");
	for (int i = 0; i < allow.size(); i++) {
		if (String(allow[i]).strip_edges().to_lower() == host) {
			return true;
		}
	}
	return false;
}

static String _justamcp_normalize_url_scheme(const String &p_scheme) {
	String scheme = p_scheme.strip_edges().to_lower();
	if (scheme.ends_with("://")) {
		scheme = scheme.substr(0, scheme.length() - 3);
	}
	return scheme;
}

static bool _justamcp_bridge_url_allowed(const String &p_url, String &r_error) {
	String scheme;
	String host;
	int port = 0;
	String path;
	String fragment;
	if (p_url.parse_url(scheme, host, port, path, fragment) != OK || host.is_empty()) {
		r_error = "Invalid bridge URL";
		return false;
	}
	scheme = _justamcp_normalize_url_scheme(scheme);
	if (scheme != "http" && scheme != "https") {
		r_error = "Bridge URL scheme must be http or https";
		return false;
	}
	if (!_justamcp_bridge_host_allowed(host)) {
		r_error = "Bridge URL host is not allow-listed (default: localhost / 127.0.0.1 / ::1). Extend blazium/justamcp/bridge_url_allow_hosts.";
		return false;
	}
	return true;
}

void JustAMCPMCPClientBridge::_bind_methods() {
	ClassDB::bind_method(D_METHOD("provide_tool_schemas", "register_only", "ignore_settings", "include_disabled_tools"), &JustAMCPMCPClientBridge::provide_tool_schemas, DEFVAL(false), DEFVAL(false), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("execute_tool", "tool_name", "arguments"), &JustAMCPMCPClientBridge::execute_tool);
}

JustAMCPMCPClientBridge *JustAMCPMCPClientBridge::get_singleton() {
	return singleton;
}

Array JustAMCPMCPClientBridge::_load_bridges() const {
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/justamcp/mcp_clients")) {
		return GLOBAL_GET("blazium/justamcp/mcp_clients");
	}
	return Array();
}

void JustAMCPMCPClientBridge::_save_bridges(const Array &p_bridges) const {
	ProjectSettings::get_singleton()->set_setting("blazium/justamcp/mcp_clients", p_bridges);
	ProjectSettings::get_singleton()->save();
}

Dictionary JustAMCPMCPClientBridge::_get_bridge_config(const String &p_bridge_name) const {
	const Array bridges = _load_bridges();
	for (int i = 0; i < bridges.size(); i++) {
		Dictionary bridge = bridges[i];
		if (String(bridge.get("name", "")) == p_bridge_name) {
			return bridge;
		}
	}
	return Dictionary();
}

Error JustAMCPMCPClientBridge::_ensure_initialized(const String &p_bridge_name) const {
	{
		MutexLock lock(initialized_bridges_mutex);
		if (initialized_bridges.has(p_bridge_name)) {
			return OK;
		}
	}
	Dictionary init_params;
	init_params["protocolVersion"] = _get_bridge_config(p_bridge_name).get("protocol_version", MCPSessionManager::latest_protocol_version());
	Dictionary init_result = _rpc_request(p_bridge_name, "initialize", init_params);
	if (!init_result.get("ok", false)) {
		return ERR_CANT_CONNECT;
	}
	{
		MutexLock lock(initialized_bridges_mutex);
		initialized_bridges.insert(p_bridge_name);
	}
	_rpc_request(p_bridge_name, "notifications/initialized", Dictionary());
	return OK;
}

Dictionary JustAMCPMCPClientBridge::_rpc_request(const String &p_bridge_name, const String &p_method, const Dictionary &p_params) const {
	return _rpc_request_sync(p_bridge_name, p_method, p_params);
}

Dictionary JustAMCPMCPClientBridge::_rpc_request_sync(const String &p_bridge_name, const String &p_method, const Dictionary &p_params) const {
	Dictionary bridge = _get_bridge_config(p_bridge_name);
	if (bridge.is_empty()) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "Unknown bridge: " + p_bridge_name;
		return err;
	}

	const String url = bridge.get("url", "");
	if (url.is_empty()) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "Bridge URL is empty";
		return err;
	}

	String scheme;
	String host;
	int port = 0;
	String path;
	String fragment;
	if (url.parse_url(scheme, host, port, path, fragment) != OK || host.is_empty()) {
		Dictionary result;
		result["ok"] = false;
		result["error"] = "Invalid bridge URL";
		return result;
	}
	scheme = _justamcp_normalize_url_scheme(scheme);
	String allow_error;
	if (!_justamcp_bridge_url_allowed(url, allow_error)) {
		Dictionary result;
		result["ok"] = false;
		result["error"] = allow_error;
		return result;
	}
	if (path.is_empty()) {
		path = "/mcp";
	}
	if (port <= 0) {
		port = scheme == "https" ? 443 : 80;
	}

	const String protocol = bridge.get("protocol_version", MCPSessionManager::latest_protocol_version());

	Ref<HTTPClient> client = HTTPClient::create();
	Ref<TLSOptions> tls;
	if (scheme == "https") {
		tls = TLSOptions::client();
	}
	Error err = client->connect_to_host(host, port, tls);
	if (err != OK) {
		Dictionary result;
		result["ok"] = false;
		result["error"] = "Failed to connect to bridge host";
		return result;
	}

	client->set_blocking_mode(true);
	const uint64_t deadline_ms = OS::get_singleton()->get_ticks_msec() + 30000;
	while (client->get_status() == HTTPClient::STATUS_CONNECTING || client->get_status() == HTTPClient::STATUS_RESOLVING) {
		if (OS::get_singleton()->get_ticks_msec() >= deadline_ms) {
			Dictionary result;
			result["ok"] = false;
			result["error"] = "MCP bridge connection timed out";
			return result;
		}
		client->poll();
	}

	Dictionary request;
	request["jsonrpc"] = "2.0";
	request["id"] = (int64_t)(++g_mcp_client_rpc_id);
	request["method"] = p_method;
	request["params"] = p_params;

	Vector<String> headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back("Accept: application/json");
	headers.push_back("MCP-Protocol-Version: " + protocol);
	if (bridge.has("auth_token")) {
		const String token = bridge.get("auth_token", "");
		if (!token.is_empty()) {
			headers.push_back("Authorization: Bearer " + token);
		}
	}

	const String body = JSON::stringify(request);
	const Vector<uint8_t> body_bytes = body.to_utf8_buffer();
	err = client->request(HTTPClient::METHOD_POST, path, headers, body_bytes.ptr(), body_bytes.size());
	if (err != OK) {
		Dictionary result;
		result["ok"] = false;
		result["error"] = "Failed to send MCP request";
		return result;
	}

	while (client->get_status() == HTTPClient::STATUS_REQUESTING) {
		if (OS::get_singleton()->get_ticks_msec() >= deadline_ms) {
			Dictionary result;
			result["ok"] = false;
			result["error"] = "MCP bridge request timed out";
			return result;
		}
		client->poll();
	}

	if (client->get_status() < HTTPClient::STATUS_BODY || client->get_status() > HTTPClient::STATUS_DISCONNECTED) {
		Dictionary result;
		result["ok"] = false;
		result["error"] = "MCP request failed with status " + itos(client->get_status());
		return result;
	}

	PackedByteArray response_body;
	while (client->get_status() == HTTPClient::STATUS_BODY) {
		if (OS::get_singleton()->get_ticks_msec() >= deadline_ms) {
			Dictionary result;
			result["ok"] = false;
			result["error"] = "MCP bridge response timed out";
			return result;
		}
		client->poll();
		response_body.append_array(client->read_response_body_chunk());
	}

	Ref<JSON> json;
	json.instantiate();
	const String response_text = String::utf8((const char *)response_body.ptr(), response_body.size());
	if (json->parse(response_text) != OK) {
		Dictionary result;
		result["ok"] = false;
		result["error"] = "Invalid JSON response from bridge";
		return result;
	}

	Dictionary payload = json->get_data();
	Dictionary result;
	result["ok"] = true;
	result["bridge"] = p_bridge_name;
	if (payload.has("result")) {
		result["result"] = payload["result"];
	} else if (payload.has("error")) {
		result["ok"] = false;
		result["error"] = payload["error"];
	} else {
		result["result"] = payload;
	}
	return result;
}

Array JustAMCPMCPClientBridge::provide_tool_schemas(bool p_register_only, bool p_ignore_settings, bool p_include_disabled_tools) {
	return get_tool_schemas(p_register_only, p_ignore_settings, p_include_disabled_tools);
}

Array JustAMCPMCPClientBridge::get_tool_schemas(bool p_register_only, bool p_ignore_settings, bool p_include_disabled_tools) {
	Array tools;
	String current_category = "mcp_client_tools";
	bool is_core = false;

	auto add_schema = [&](const String &p_name, const String &p_desc, const Vector<String> &p_props, const Vector<String> &p_req, const String &p_task_support = "forbidden") {
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
		Dictionary t;
		t["name"] = full_name;
		t["description"] = p_desc;
		Dictionary meta;
		meta["category"] = current_category;
		meta["enabled"] = cat_enabled && tool_enabled;
		t["_meta"] = meta;
		Dictionary schema;
		schema["type"] = "object";
		Dictionary props;
		for (int i = 0; i < p_props.size(); i += 2) {
			Dictionary prop;
			prop["type"] = p_props[i + 1];
			props[p_props[i]] = prop;
		}
		schema["properties"] = props;
		if (!p_req.is_empty()) {
			Array req;
			for (int i = 0; i < p_req.size(); i++) {
				req.push_back(p_req[i]);
			}
			schema["required"] = req;
		}
		t["inputSchema"] = schema;
		if (!p_task_support.is_empty() && p_task_support != "forbidden") {
			Dictionary execution;
			execution["taskSupport"] = p_task_support;
			t["execution"] = execution;
		}
		tools.push_back(t);
	};

	add_schema("mcp_client_list_bridges", "Lists configured outbound MCP client bridges.",
			Vector<String>{}, Vector<String>{});
	add_schema("mcp_client_add_bridge", "Adds an outbound MCP bridge (name, url, protocol_version, optional auth_token).",
			Vector<String>{ "name", "string", "url", "string", "protocol_version", "string", "auth_token", "string" },
			Vector<String>{ "name", "url" });
	add_schema("mcp_client_list_remote_tools", "Proxies tools/list from a configured bridge.",
			Vector<String>{ "bridge_name", "string" }, Vector<String>{ "bridge_name" }, "required");
	add_schema("mcp_client_call_remote_tool", "Proxies tools/call to a configured bridge.",
			Vector<String>{ "bridge_name", "string", "tool_name", "string", "arguments", "object" },
			Vector<String>{ "bridge_name", "tool_name" }, "required");
	add_schema("mcp_client_read_remote_resource", "Proxies resources/read to a configured bridge.",
			Vector<String>{ "bridge_name", "string", "uri", "string" }, Vector<String>{ "bridge_name", "uri" }, "required");

	return tools;
}

Dictionary JustAMCPMCPClientBridge::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	String tool_name = p_tool_name;
	if (tool_name.begins_with("blazium_")) {
		tool_name = tool_name.substr(8);
	}
	if (tool_name == "mcp_client_list_bridges") {
		return list_bridges(p_args);
	}
	if (tool_name == "mcp_client_add_bridge") {
		return add_bridge(p_args);
	}
	if (tool_name == "mcp_client_list_remote_tools" ||
			tool_name == "mcp_client_call_remote_tool" ||
			tool_name == "mcp_client_read_remote_resource") {
		Dictionary pending;
		if (_try_schedule_remote_tool(tool_name, p_args, pending)) {
			return pending;
		}

		if (Thread::is_main_thread()) {
			Dictionary err;
			err["ok"] = false;
			err["error"] = "MCP bridge remote tools require async WorkerThreadPool scheduling on the main thread";
			err["_justamcp_async_required"] = true;
			return err;
		}
		return _execute_remote_tool_sync(tool_name, p_args);
	}
	Dictionary err;
	err["ok"] = false;
	err["error"] = "Unknown MCP client tool: " + p_tool_name;
	return err;
}

Dictionary JustAMCPMCPClientBridge::_execute_remote_tool_sync(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "mcp_client_list_remote_tools") {
		return list_remote_tools(p_args);
	}
	if (p_tool_name == "mcp_client_call_remote_tool") {
		return call_remote_tool(p_args);
	}
	if (p_tool_name == "mcp_client_read_remote_resource") {
		return read_remote_resource(p_args);
	}
	Dictionary err;
	err["ok"] = false;
	err["error"] = "Unknown MCP client tool: " + p_tool_name;
	return err;
}

struct JustAMCPBridgeRemoteJob {
	JustAMCPMCPClientBridge *bridge = nullptr;
	String tool_name;
	Dictionary args;
	Variant request_id;
};

static void _justamcp_bridge_remote_worker(void *p_userdata) {
	JustAMCPBridgeRemoteJob *job = static_cast<JustAMCPBridgeRemoteJob *>(p_userdata);
	Dictionary result;
	if (job && job->bridge) {
		result = job->bridge->_execute_remote_tool_sync(job->tool_name, job->args);
	} else {
		result["ok"] = false;
		result["error"] = "Bridge remote job missing bridge instance";
	}
	const Variant request_id = job ? job->request_id : Variant();
	if (job) {
		memdelete(job);
	}
	JustAMCPServer *server = JustAMCPServer::get_singleton();
	if (server && request_id.get_type() != Variant::NIL) {
		server->call_deferred(SNAME("_deferred_complete_tool_dict"), request_id, result);
	}
}

bool JustAMCPMCPClientBridge::_try_schedule_remote_tool(const String &p_tool_name, const Dictionary &p_args, Dictionary &r_pending) const {
	if (!Thread::is_main_thread()) {
		return false;
	}
	const Variant request_id = justamcp_get_active_tool_request_id();
	if (request_id.get_type() == Variant::NIL) {
		return false;
	}
	WorkerThreadPool *pool = WorkerThreadPool::get_singleton();
	if (!pool) {
		return false;
	}
	JustAMCPBridgeRemoteJob *job = memnew(JustAMCPBridgeRemoteJob);
	job->bridge = const_cast<JustAMCPMCPClientBridge *>(this);
	job->tool_name = p_tool_name;
	job->args = p_args;
	job->request_id = request_id;
	pool->add_native_task(&_justamcp_bridge_remote_worker, job, true, "JustAMCPBridgeRPC");
	r_pending["ok"] = true;
	r_pending["_justamcp_async_pending"] = true;
	return true;
}

Dictionary JustAMCPMCPClientBridge::list_bridges(const Dictionary &p_args) {
	(void)p_args;
	Array raw = _load_bridges();
	Array redacted;
	for (int i = 0; i < raw.size(); i++) {
		Dictionary bridge = Dictionary(raw[i]).duplicate();
		const bool has_token = bridge.has("auth_token") && !String(bridge.get("auth_token", "")).is_empty();
		bridge.erase("auth_token");
		bridge["has_auth_token"] = has_token;
		redacted.push_back(bridge);
	}
	Dictionary result;
	result["ok"] = true;
	result["bridges"] = redacted;
	result["count"] = redacted.size();
	return result;
}

Dictionary JustAMCPMCPClientBridge::add_bridge(const Dictionary &p_args) {
	const String name = p_args.get("name", "");
	const String url = p_args.get("url", "");
	if (name.is_empty() || url.is_empty()) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "name and url are required";
		return err;
	}
	String allow_error;
	if (!_justamcp_bridge_url_allowed(url, allow_error)) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = allow_error;
		return err;
	}
	Array bridges = _load_bridges();
	for (int i = 0; i < bridges.size(); i++) {
		if (String(Dictionary(bridges[i]).get("name", "")) == name) {
			Dictionary err;
			err["ok"] = false;
			err["error"] = "Bridge already exists: " + name;
			return err;
		}
	}
	Dictionary bridge;
	bridge["name"] = name;
	bridge["url"] = url;
	bridge["protocol_version"] = p_args.get("protocol_version", MCPSessionManager::latest_protocol_version());
	if (p_args.has("auth_token")) {
		bridge["auth_token"] = p_args.get("auth_token", "");
	}
	bridges.push_back(bridge);
	_save_bridges(bridges);
	Dictionary redacted = bridge.duplicate();
	const bool has_token = redacted.has("auth_token") && !String(redacted.get("auth_token", "")).is_empty();
	redacted.erase("auth_token");
	redacted["has_auth_token"] = has_token;
	Dictionary result;
	result["ok"] = true;
	result["bridge"] = redacted;
	return result;
}

Dictionary JustAMCPMCPClientBridge::list_remote_tools(const Dictionary &p_args) {
	const String bridge_name = p_args.get("bridge_name", "");
	if (_ensure_initialized(bridge_name) != OK) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "Failed to initialize MCP bridge session";
		return err;
	}
	return _rpc_request(bridge_name, "tools/list", Dictionary());
}

Dictionary JustAMCPMCPClientBridge::call_remote_tool(const Dictionary &p_args) {
	const String bridge_name = p_args.get("bridge_name", "");
	if (_ensure_initialized(bridge_name) != OK) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "Failed to initialize MCP bridge session";
		return err;
	}
	Dictionary params;
	params["name"] = p_args.get("tool_name", "");
	params["arguments"] = p_args.get("arguments", Dictionary());
	return _rpc_request(bridge_name, "tools/call", params);
}

Dictionary JustAMCPMCPClientBridge::read_remote_resource(const Dictionary &p_args) {
	const String bridge_name = p_args.get("bridge_name", "");
	if (_ensure_initialized(bridge_name) != OK) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "Failed to initialize MCP bridge session";
		return err;
	}
	Dictionary params;
	params["uri"] = p_args.get("uri", "");
	return _rpc_request(bridge_name, "resources/read", params);
}

JustAMCPMCPClientBridge::JustAMCPMCPClientBridge() {
	if (!singleton) {
		singleton = this;
	}
}

JustAMCPMCPClientBridge::~JustAMCPMCPClientBridge() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

#endif
