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

#include "../justamcp_mcp_apps.h"
#include "../justamcp_mcp_client_http.h"
#include "../justamcp_mcp_spec.h"
#include "../justamcp_server.h"
#include "../justamcp_session_manager.h"
#include "../justamcp_tool_context.h"
#include "justamcp_settings_resolver.h"
#include "justamcp_tool_schema_builder.h"

#include "core/config/project_settings.h"
#include "core/object/worker_thread_pool.h"
#include "core/os/thread.h"
#include "editor/editor_settings.h"

JustAMCPMCPClientBridge *JustAMCPMCPClientBridge::singleton = nullptr;

void JustAMCPMCPClientBridge::_bind_methods() {
	ClassDB::bind_method(D_METHOD("provide_tool_schemas", "register_only", "ignore_settings", "include_disabled_tools"), &JustAMCPMCPClientBridge::provide_tool_schemas, DEFVAL(false), DEFVAL(false), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("execute_tool", "tool_name", "arguments"), &JustAMCPMCPClientBridge::execute_tool);
	ClassDB::bind_method(D_METHOD("list_bridges", "args"), &JustAMCPMCPClientBridge::list_bridges, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("add_bridge", "args"), &JustAMCPMCPClientBridge::add_bridge);
	ClassDB::bind_method(D_METHOD("update_bridge", "args"), &JustAMCPMCPClientBridge::update_bridge);
	ClassDB::bind_method(D_METHOD("remove_bridge", "args"), &JustAMCPMCPClientBridge::remove_bridge);
	ClassDB::bind_method(D_METHOD("connect_bridge", "args"), &JustAMCPMCPClientBridge::connect_bridge);
	ClassDB::bind_method(D_METHOD("disconnect_bridge", "args"), &JustAMCPMCPClientBridge::disconnect_bridge);
	ClassDB::bind_method(D_METHOD("status_bridge", "args"), &JustAMCPMCPClientBridge::status_bridge);
	ClassDB::bind_method(D_METHOD("list_remote_tools", "args"), &JustAMCPMCPClientBridge::list_remote_tools);
	ClassDB::bind_method(D_METHOD("call_remote_tool", "args"), &JustAMCPMCPClientBridge::call_remote_tool);
	ClassDB::bind_method(D_METHOD("read_remote_resource", "args"), &JustAMCPMCPClientBridge::read_remote_resource);
	ClassDB::bind_method(D_METHOD("auto_connect_enabled_bridges"), &JustAMCPMCPClientBridge::auto_connect_enabled_bridges);
}

JustAMCPMCPClientBridge *JustAMCPMCPClientBridge::get_singleton() {
	return singleton;
}

Array JustAMCPMCPClientBridge::_load_bridges() const {
	return JustAMCPSettingsResolver::resolve_array("blazium/justamcp/mcp_clients");
}

void JustAMCPMCPClientBridge::_save_bridges(const Array &p_bridges) const {
	JustAMCPSettingsResolver::set_array("blazium/justamcp/mcp_clients", p_bridges);
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

Dictionary JustAMCPMCPClientBridge::_redact_bridge(const Dictionary &p_bridge) const {
	Dictionary redacted = p_bridge.duplicate();
	const bool has_token = redacted.has("auth_token") && !String(redacted.get("auth_token", "")).is_empty();
	const bool has_secret = redacted.has("client_secret") && !String(redacted.get("client_secret", "")).is_empty();
	redacted.erase("auth_token");
	redacted.erase("client_secret");
	redacted["has_auth_token"] = has_token;
	redacted["has_client_secret"] = has_secret;
	return redacted;
}

Dictionary JustAMCPMCPClientBridge::_apply_bridge_fields(Dictionary p_bridge, const Dictionary &p_args, bool p_create) const {
	static const char *keys[] = {
		"name", "url", "protocol_version", "auth_token", "timeout_ms", "headers",
		"enabled", "auto_connect", "expose_remote_tools", "oauth_mode",
		"client_id", "client_secret", "cimd_url", "scopes"
	};
	for (int i = 0; i < (int)(sizeof(keys) / sizeof(keys[0])); i++) {
		if (p_args.has(keys[i])) {
			p_bridge[keys[i]] = p_args[keys[i]];
		}
	}
	if (p_create) {
		if (!p_bridge.has("protocol_version") || String(p_bridge["protocol_version"]).is_empty()) {
			p_bridge["protocol_version"] = MCPSessionManager::latest_protocol_version();
		}
		if (!p_bridge.has("enabled")) {
			p_bridge["enabled"] = true;
		}
		if (!p_bridge.has("auto_connect")) {
			p_bridge["auto_connect"] = false;
		}
		if (!p_bridge.has("expose_remote_tools")) {
			p_bridge["expose_remote_tools"] = false;
		}
		if (!p_bridge.has("oauth_mode") || String(p_bridge["oauth_mode"]).is_empty()) {
			p_bridge["oauth_mode"] = "none";
		}
		if (!p_bridge.has("timeout_ms")) {
			p_bridge["timeout_ms"] = 30000;
		}
	}
	return p_bridge;
}

Ref<JustAMCPMCPClient> JustAMCPMCPClientBridge::_client_for(const String &p_bridge_name) const {
	MutexLock lock(clients_mutex);
	if (clients.has(p_bridge_name) && clients[p_bridge_name].is_valid()) {
		clients[p_bridge_name]->configure(_get_bridge_config(p_bridge_name));
		return clients[p_bridge_name];
	}
	const Dictionary config = _get_bridge_config(p_bridge_name);
	if (config.is_empty()) {
		return Ref<JustAMCPMCPClient>();
	}
	Ref<JustAMCPMCPClient> client;
	client.instantiate();
	client->configure(config);
	clients[p_bridge_name] = client;
	return client;
}

Error JustAMCPMCPClientBridge::_ensure_initialized(const String &p_bridge_name) const {
	{
		MutexLock lock(initialized_bridges_mutex);
		if (initialized_bridges.has(p_bridge_name)) {
			return OK;
		}
	}
	Ref<JustAMCPMCPClient> client = _client_for(p_bridge_name);
	if (client.is_null()) {
		return ERR_DOES_NOT_EXIST;
	}
	Dictionary result = client->connect_remote();
	if (!result.get("ok", false)) {
		return ERR_CANT_CONNECT;
	}
	{
		MutexLock lock(initialized_bridges_mutex);
		initialized_bridges.insert(p_bridge_name);
	}
	return OK;
}

Dictionary JustAMCPMCPClientBridge::_rpc_request(const String &p_bridge_name, const String &p_method, const Dictionary &p_params) const {
	return _rpc_request_sync(p_bridge_name, p_method, p_params);
}

Dictionary JustAMCPMCPClientBridge::_rpc_request_sync(const String &p_bridge_name, const String &p_method, const Dictionary &p_params) const {
	Ref<JustAMCPMCPClient> client = _client_for(p_bridge_name);
	if (client.is_null()) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "Unknown bridge: " + p_bridge_name;
		return err;
	}
	return client->rpc(p_method, p_params);
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
			Vector<String>{ "name", "string", "url", "string", "protocol_version", "string", "auth_token", "string", "oauth_mode", "string", "auto_connect", "boolean", "expose_remote_tools", "boolean" },
			Vector<String>{ "name", "url" });
	add_schema("mcp_client_update_bridge", "Updates fields on an existing outbound MCP bridge.",
			Vector<String>{ "name", "string", "url", "string", "protocol_version", "string", "auth_token", "string", "oauth_mode", "string", "auto_connect", "boolean", "expose_remote_tools", "boolean", "enabled", "boolean" },
			Vector<String>{ "name" });
	add_schema("mcp_client_remove_bridge", "Removes a configured outbound MCP bridge.",
			Vector<String>{ "name", "string" }, Vector<String>{ "name" });
	add_schema("mcp_client_connect", "Connects an outbound MCP bridge (discover or initialize).",
			Vector<String>{ "name", "string" }, Vector<String>{ "name" }, "required");
	add_schema("mcp_client_disconnect", "Disconnects an outbound MCP bridge.",
			Vector<String>{ "name", "string" }, Vector<String>{ "name" });
	add_schema("mcp_client_status", "Returns connection status for an outbound MCP bridge.",
			Vector<String>{ "name", "string" }, Vector<String>{ "name" });
	add_schema("mcp_client_list_remote_tools", "Proxies tools/list from a configured bridge.",
			Vector<String>{ "bridge_name", "string" }, Vector<String>{ "bridge_name" }, "required");
	add_schema("mcp_client_call_remote_tool", "Proxies tools/call to a configured bridge.",
			Vector<String>{ "bridge_name", "string", "tool_name", "string", "arguments", "object" },
			Vector<String>{ "bridge_name", "tool_name" }, "required");
	add_schema("mcp_client_read_remote_resource", "Proxies resources/read to a configured bridge.",
			Vector<String>{ "bridge_name", "string", "uri", "string" }, Vector<String>{ "bridge_name", "uri" }, "required");

	if (!p_register_only && singleton) {
		MutexLock lock(singleton->clients_mutex);
		for (const KeyValue<String, Array> &E : singleton->exposed_remote_tools) {
			const Array remote = E.value;
			for (int i = 0; i < remote.size(); i++) {
				if (remote[i].get_type() != Variant::DICTIONARY) {
					continue;
				}
				Dictionary remote_tool = remote[i];
				String remote_name = String(remote_tool.get("name", ""));
				if (remote_name.is_empty()) {
					continue;
				}
				String exposed = "mcp_" + E.key + "_" + remote_name;
				exposed = exposed.to_lower().replace("-", "_").replace(" ", "_");
				if (exposed.length() > 64) {
					exposed = exposed.substr(0, 64);
				}
				if (!justamcp_is_valid_mcp_tool_name(exposed)) {
					continue;
				}
				remote_tool["name"] = "blazium_" + exposed;
				Dictionary meta = remote_tool.has("_meta") ? Dictionary(remote_tool["_meta"]) : Dictionary();
				meta["category"] = current_category;
				meta["exposed_from"] = E.key;
				remote_tool["_meta"] = meta;
				tools.push_back(remote_tool);
			}
		}
	}

	return tools;
}

bool JustAMCPMCPClientBridge::_is_remote_network_tool(const String &p_tool_name) const {
	return p_tool_name == "mcp_client_list_remote_tools" ||
			p_tool_name == "mcp_client_call_remote_tool" ||
			p_tool_name == "mcp_client_read_remote_resource" ||
			p_tool_name == "mcp_client_connect" ||
			p_tool_name == "mcp_client_list_remote_prompts" ||
			p_tool_name == "mcp_client_get_remote_prompt" ||
			p_tool_name == "mcp_client_list_remote_resources" ||
			p_tool_name == "mcp_client_complete_remote" ||
			p_tool_name.begins_with("mcp_");
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
	if (tool_name == "mcp_client_update_bridge") {
		return update_bridge(p_args);
	}
	if (tool_name == "mcp_client_remove_bridge") {
		return remove_bridge(p_args);
	}
	if (tool_name == "mcp_client_disconnect") {
		return disconnect_bridge(p_args);
	}
	if (tool_name == "mcp_client_status") {
		return status_bridge(p_args);
	}
	if (_is_remote_network_tool(tool_name)) {
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
	if (p_tool_name == "mcp_client_connect") {
		return connect_bridge(p_args);
	}
	if (p_tool_name == "mcp_client_list_remote_prompts") {
		return list_remote_prompts(p_args);
	}
	if (p_tool_name == "mcp_client_get_remote_prompt") {
		return get_remote_prompt(p_args);
	}
	if (p_tool_name == "mcp_client_list_remote_resources") {
		return list_remote_resources(p_args);
	}
	if (p_tool_name == "mcp_client_complete_remote") {
		return complete_remote(p_args);
	}
	if (p_tool_name.begins_with("mcp_")) {
		const String rest = p_tool_name.substr(4);
		const int sep = rest.find("_");
		if (sep > 0) {
			Dictionary call_args;
			call_args["bridge_name"] = rest.substr(0, sep);
			call_args["tool_name"] = rest.substr(sep + 1);
			call_args["arguments"] = p_args;
			return call_remote_tool(call_args);
		}
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
	const WorkerThreadPool::TaskID task_id = pool->add_native_task(&_justamcp_bridge_remote_worker, job, true, "JustAMCPBridgeRPC");
	if (task_id != WorkerThreadPool::INVALID_TASK_ID) {
		MutexLock lock(pending_remote_mutex);
		pending_remote_tasks.push_back(task_id);
	}
	r_pending["ok"] = true;
	r_pending["_justamcp_async_pending"] = true;
	return true;
}

void JustAMCPMCPClientBridge::wait_pending_remote_tasks() {
	Vector<WorkerThreadPool::TaskID> ids;
	{
		MutexLock lock(pending_remote_mutex);
		ids = pending_remote_tasks;
		pending_remote_tasks.clear();
	}
	WorkerThreadPool *pool = WorkerThreadPool::get_singleton();
	if (!pool) {
		return;
	}
	for (int i = 0; i < ids.size(); i++) {
		pool->wait_for_task_completion(ids[i]);
	}
}

Dictionary JustAMCPMCPClientBridge::list_bridges(const Dictionary &p_args) {
	(void)p_args;
	Array raw = _load_bridges();
	Array redacted;
	for (int i = 0; i < raw.size(); i++) {
		redacted.push_back(_redact_bridge(Dictionary(raw[i])));
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
	if (!JustAMCPMCPClientHTTP::url_allowed(url, allow_error)) {
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
	Dictionary bridge = _apply_bridge_fields(Dictionary(), p_args, true);
	bridges.push_back(bridge);
	_save_bridges(bridges);
	Dictionary result;
	result["ok"] = true;
	result["bridge"] = _redact_bridge(bridge);
	return result;
}

Dictionary JustAMCPMCPClientBridge::update_bridge(const Dictionary &p_args) {
	const String name = p_args.get("name", "");
	if (name.is_empty()) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "name is required";
		return err;
	}
	if (p_args.has("url")) {
		String allow_error;
		if (!JustAMCPMCPClientHTTP::url_allowed(String(p_args.get("url", "")), allow_error)) {
			Dictionary err;
			err["ok"] = false;
			err["error"] = allow_error;
			return err;
		}
	}
	Array bridges = _load_bridges();
	bool found = false;
	Dictionary updated;
	for (int i = 0; i < bridges.size(); i++) {
		Dictionary bridge = bridges[i];
		if (String(bridge.get("name", "")) == name) {
			updated = _apply_bridge_fields(bridge, p_args, false);
			bridges[i] = updated;
			found = true;
			break;
		}
	}
	if (!found) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "Unknown bridge: " + name;
		return err;
	}
	_save_bridges(bridges);
	{
		MutexLock lock(clients_mutex);
		if (clients.has(name) && clients[name].is_valid()) {
			clients[name]->configure(updated);
		}
	}
	Dictionary result;
	result["ok"] = true;
	result["bridge"] = _redact_bridge(updated);
	return result;
}

Dictionary JustAMCPMCPClientBridge::remove_bridge(const Dictionary &p_args) {
	const String name = p_args.get("name", "");
	if (name.is_empty()) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "name is required";
		return err;
	}
	Array bridges = _load_bridges();
	Array kept;
	bool found = false;
	for (int i = 0; i < bridges.size(); i++) {
		if (String(Dictionary(bridges[i]).get("name", "")) == name) {
			found = true;
			continue;
		}
		kept.push_back(bridges[i]);
	}
	if (!found) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "Unknown bridge: " + name;
		return err;
	}
	_save_bridges(kept);
	{
		MutexLock lock(clients_mutex);
		if (clients.has(name) && clients[name].is_valid()) {
			clients[name]->disconnect_remote();
		}
		clients.erase(name);
		exposed_remote_tools.erase(name);
	}
	{
		MutexLock lock(initialized_bridges_mutex);
		initialized_bridges.erase(name);
	}
	Dictionary result;
	result["ok"] = true;
	result["removed"] = name;
	return result;
}

Dictionary JustAMCPMCPClientBridge::connect_bridge(const Dictionary &p_args) {
	const String name = String(p_args.get("name", p_args.get("bridge_name", "")));
	Ref<JustAMCPMCPClient> client = _client_for(name);
	if (client.is_null()) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "Unknown bridge: " + name;
		return err;
	}
	Dictionary result = client->connect_remote();
	if (result.get("ok", false)) {
		MutexLock lock(initialized_bridges_mutex);
		initialized_bridges.insert(name);
		if (bool(_get_bridge_config(name).get("expose_remote_tools", false))) {
			Dictionary listed = client->tools_list();
			if (listed.get("ok", false) && listed.has("result")) {
				const Dictionary inner = listed["result"];
				MutexLock tools_lock(clients_mutex);
				exposed_remote_tools[name] = inner.get("tools", Array());
			}
		}
	}
	return result;
}

Dictionary JustAMCPMCPClientBridge::disconnect_bridge(const Dictionary &p_args) {
	const String name = String(p_args.get("name", p_args.get("bridge_name", "")));
	Ref<JustAMCPMCPClient> client = _client_for(name);
	if (client.is_valid()) {
		client->disconnect_remote();
	}
	{
		MutexLock lock(initialized_bridges_mutex);
		initialized_bridges.erase(name);
	}
	Dictionary result;
	result["ok"] = true;
	result["name"] = name;
	result["status"] = "disconnected";
	return result;
}

Dictionary JustAMCPMCPClientBridge::status_bridge(const Dictionary &p_args) {
	const String name = String(p_args.get("name", p_args.get("bridge_name", "")));
	Ref<JustAMCPMCPClient> client = _client_for(name);
	if (client.is_null()) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "Unknown bridge: " + name;
		return err;
	}
	return client->status();
}

void JustAMCPMCPClientBridge::auto_connect_enabled_bridges() {
	const Array bridges = _load_bridges();
	for (int i = 0; i < bridges.size(); i++) {
		const Dictionary bridge = bridges[i];
		if (!bool(bridge.get("enabled", true)) || !bool(bridge.get("auto_connect", false))) {
			continue;
		}
		Dictionary args;
		args["name"] = bridge.get("name", "");
		connect_bridge(args);
	}
}

Dictionary JustAMCPMCPClientBridge::list_remote_tools(const Dictionary &p_args) {
	const String bridge_name = p_args.get("bridge_name", "");
	if (_ensure_initialized(bridge_name) != OK) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "Failed to initialize MCP bridge session";
		return err;
	}
	Ref<JustAMCPMCPClient> client = _client_for(bridge_name);
	Dictionary listed = client.is_valid() ? client->tools_list() : _rpc_request(bridge_name, "tools/list", Dictionary());
	if (listed.get("ok", false) && JustAMCPMCPAppsHost::get_singleton() && listed.has("result")) {
		const Array tools = Dictionary(listed["result"]).get("tools", Array());
		for (int i = 0; i < tools.size(); i++) {
			if (tools[i].get_type() != Variant::DICTIONARY) {
				continue;
			}
			Dictionary ui = JustAMCPMCPAppsHost::detect_ui_meta(tools[i]);
			if (bool(ui.get("present", false))) {
				Dictionary read_args;
				read_args["uri"] = ui.get("resourceUri", "");
				Dictionary html = client->resources_read(read_args);
				String text;
				if (html.get("ok", false) && html.has("result")) {
					const Array contents = Dictionary(html["result"]).get("contents", Array());
					if (!contents.is_empty() && contents[0].get_type() == Variant::DICTIONARY) {
						text = String(Dictionary(contents[0]).get("text", ""));
					}
				}
				JustAMCPMCPAppsHost::get_singleton()->open_app(bridge_name, ui, text);
			}
		}
	}
	return listed;
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

Dictionary JustAMCPMCPClientBridge::list_remote_prompts(const Dictionary &p_args) {
	const String bridge_name = p_args.get("bridge_name", "");
	if (_ensure_initialized(bridge_name) != OK) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "Failed to initialize MCP bridge session";
		return err;
	}
	return _rpc_request(bridge_name, "prompts/list", Dictionary());
}

Dictionary JustAMCPMCPClientBridge::get_remote_prompt(const Dictionary &p_args) {
	const String bridge_name = p_args.get("bridge_name", "");
	if (_ensure_initialized(bridge_name) != OK) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "Failed to initialize MCP bridge session";
		return err;
	}
	Dictionary params;
	params["name"] = p_args.get("prompt_name", p_args.get("name", ""));
	params["arguments"] = p_args.get("arguments", Dictionary());
	return _rpc_request(bridge_name, "prompts/get", params);
}

Dictionary JustAMCPMCPClientBridge::list_remote_resources(const Dictionary &p_args) {
	const String bridge_name = p_args.get("bridge_name", "");
	if (_ensure_initialized(bridge_name) != OK) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "Failed to initialize MCP bridge session";
		return err;
	}
	return _rpc_request(bridge_name, "resources/list", Dictionary());
}

Dictionary JustAMCPMCPClientBridge::complete_remote(const Dictionary &p_args) {
	const String bridge_name = p_args.get("bridge_name", "");
	if (_ensure_initialized(bridge_name) != OK) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "Failed to initialize MCP bridge session";
		return err;
	}
	Dictionary params = p_args.duplicate();
	params.erase("bridge_name");
	return _rpc_request(bridge_name, "completion/complete", params);
}

JustAMCPMCPClientBridge::JustAMCPMCPClientBridge() {
	if (!singleton) {
		singleton = this;
	}
}

JustAMCPMCPClientBridge::~JustAMCPMCPClientBridge() {
	wait_pending_remote_tasks();
	if (singleton == this) {
		singleton = nullptr;
	}
}

#endif
