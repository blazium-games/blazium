/**************************************************************************/
/*  justamcp_mcp_client.cpp                                               */
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

#include "justamcp_mcp_client.h"

#include "justamcp_mcp_apps.h"
#include "justamcp_mcp_client_http.h"
#include "justamcp_mcp_client_oauth.h"
#include "justamcp_mcp_spec.h"
#include "justamcp_session_manager.h"
#include "tools/justamcp_settings_resolver.h"

#include "core/io/json.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "core/version.h"

static uint64_t g_justamcp_client_rpc_id = 1;

void JustAMCPMCPClient::_bind_methods() {
	ClassDB::bind_method(D_METHOD("configure", "config"), &JustAMCPMCPClient::configure);
	ClassDB::bind_method(D_METHOD("connect_remote"), &JustAMCPMCPClient::connect_remote);
	ClassDB::bind_method(D_METHOD("disconnect_remote"), &JustAMCPMCPClient::disconnect_remote);
	ClassDB::bind_method(D_METHOD("status"), &JustAMCPMCPClient::status);
	ClassDB::bind_method(D_METHOD("discover"), &JustAMCPMCPClient::discover);
	ClassDB::bind_method(D_METHOD("tools_list", "params"), &JustAMCPMCPClient::tools_list, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("tools_call", "params"), &JustAMCPMCPClient::tools_call);
	ClassDB::bind_method(D_METHOD("prompts_list", "params"), &JustAMCPMCPClient::prompts_list, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("prompts_get", "params"), &JustAMCPMCPClient::prompts_get);
	ClassDB::bind_method(D_METHOD("resources_list", "params"), &JustAMCPMCPClient::resources_list, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("resources_templates_list", "params"), &JustAMCPMCPClient::resources_templates_list, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("resources_read", "params"), &JustAMCPMCPClient::resources_read);
	ClassDB::bind_method(D_METHOD("completion_complete", "params"), &JustAMCPMCPClient::completion_complete);
	ClassDB::bind_method(D_METHOD("rpc", "method", "params"), &JustAMCPMCPClient::rpc);
}

Dictionary JustAMCPMCPClient::client_capabilities() {
	Dictionary caps;
	Dictionary list_changed;
	list_changed["listChanged"] = true;
	caps["tools"] = list_changed;
	caps["prompts"] = list_changed.duplicate();
	caps["resources"] = list_changed.duplicate();
	Dictionary elicitation;
	elicitation["form"] = Dictionary();
	elicitation["url"] = Dictionary();
	caps["elicitation"] = elicitation;
	Dictionary extensions;
	extensions["io.modelcontextprotocol/ui"] = JustAMCPMCPAppsHost::apps_extension_capability();
	caps["extensions"] = extensions;
	return caps;
}

Dictionary JustAMCPMCPClient::client_info() {
	Dictionary info;
	info["name"] = "JustAMCP";
	info["title"] = "JustAMCP";
	info["version"] = String(VERSION_FULL_NAME);
	info["websiteUrl"] = "https://blazium.app";
	return info;
}

void JustAMCPMCPClient::attach_client_meta(Dictionary &r_params, const String &p_protocol, bool p_debug_log) {
	Dictionary meta = r_params.has("_meta") && r_params["_meta"].get_type() == Variant::DICTIONARY ? Dictionary(r_params["_meta"]) : Dictionary();
	meta["io.modelcontextprotocol/protocolVersion"] = p_protocol;
	meta["io.modelcontextprotocol/clientCapabilities"] = client_capabilities();
	meta["io.modelcontextprotocol/clientInfo"] = client_info();
	if (p_debug_log) {
		meta["io.modelcontextprotocol/logLevel"] = "debug";
	}
	r_params["_meta"] = meta;
}

Dictionary JustAMCPMCPClient::build_json_rpc(const String &p_method, const Dictionary &p_params, int64_t p_id) {
	Dictionary request;
	request["jsonrpc"] = "2.0";
	if (!p_method.begins_with("notifications/")) {
		request["id"] = p_id;
	}
	request["method"] = p_method;
	request["params"] = p_params;
	return request;
}

void JustAMCPMCPClient::configure(const Dictionary &p_config) {
	config = p_config.duplicate();
	negotiated_protocol = String(config.get("protocol_version", MCPSessionManager::latest_protocol_version()));
	if (negotiated_protocol.is_empty()) {
		negotiated_protocol = "2026-07-28";
	}
	modern_era = justamcp_protocol_at_least(negotiated_protocol, "2026-07-28");
}

String JustAMCPMCPClient::get_name() const {
	return String(config.get("name", ""));
}

String JustAMCPMCPClient::get_url() const {
	return String(config.get("url", ""));
}

void JustAMCPMCPClient::_attach_client_meta(Dictionary &r_params) const {
	if (!modern_era) {
		return;
	}
	const bool debug = JustAMCPSettingsResolver::resolve_bool("blazium/justamcp/enable_debug_logging", false);
	attach_client_meta(r_params, negotiated_protocol, debug);
}

Dictionary JustAMCPMCPClient::_rpc_once(const String &p_method, const Dictionary &p_params) {
	Dictionary params = p_params.duplicate();
	_attach_client_meta(params);

	const String url = get_url();
	const int timeout_ms = int(config.get("timeout_ms", 30000));
	Dictionary extra_headers = config.has("headers") && config["headers"].get_type() == Variant::DICTIONARY ? Dictionary(config["headers"]) : Dictionary();

	String auth_token = String(config.get("auth_token", ""));
	if (auth_token.is_empty()) {
		const Dictionary stored = JustAMCPMCPClientOAuth::load_tokens(String(config.get("oauth_issuer", "")));
		auth_token = String(stored.get("access_token", ""));
	}

	String mcp_name;
	if (p_method == "tools/call" || p_method == "prompts/get") {
		mcp_name = String(params.get("name", ""));
	} else if (p_method == "resources/read") {
		mcp_name = String(params.get("uri", ""));
	}

	const Vector<String> headers = JustAMCPMCPClientHTTP::streamable_headers(p_method, negotiated_protocol, auth_token, extra_headers, modern_era, mcp_name);
	const Dictionary request = build_json_rpc(p_method, params, (int64_t)(++g_justamcp_client_rpc_id));
	Dictionary http = JustAMCPMCPClientHTTP::request(url, HTTPClient::METHOD_POST, headers, JSON::stringify(request), timeout_ms);
	if (http.get("oauth_required", false) && String(config.get("oauth_mode", "none")) != "none" && String(config.get("oauth_mode", "none")) != "bearer") {
		Dictionary oauth = JustAMCPMCPClientOAuth::resolve_bearer_token(config, http);
		if (oauth.get("ok", false) && !String(oauth.get("access_token", "")).is_empty()) {
			auth_token = String(oauth["access_token"]);
			const Vector<String> retry_headers = JustAMCPMCPClientHTTP::streamable_headers(p_method, negotiated_protocol, auth_token, extra_headers, modern_era, mcp_name);
			http = JustAMCPMCPClientHTTP::request(url, HTTPClient::METHOD_POST, retry_headers, JSON::stringify(request), timeout_ms);
		} else {
			return oauth;
		}
	}
	if (!http.get("ok", false) && !http.has("body")) {
		return http;
	}

	Dictionary parsed = JustAMCPMCPClientHTTP::parse_json_or_sse(String(http.get("body", "")), String(http.get("content_type", "")));
	if (!parsed.get("ok", false)) {
		if (!http.get("ok", false)) {
			return http;
		}
		return parsed;
	}

	Dictionary payload = parsed.get("payload", Dictionary());
	Dictionary result;
	result["ok"] = true;
	result["bridge"] = get_name();
	result["http_status"] = http.get("status", 0);
	if (parsed.get("payload", Variant()).get_type() == Variant::DICTIONARY) {
		const Dictionary rpc = payload;
		if (rpc.has("result")) {
			result["result"] = rpc["result"];
			if (rpc["result"].get_type() == Variant::DICTIONARY) {
				const Dictionary inner = rpc["result"];
				if (inner.has("resultType")) {
					result["resultType"] = inner["resultType"];
				}
				if (inner.has("ttlMs")) {
					result["ttlMs"] = inner["ttlMs"];
				}
				if (inner.has("cacheScope")) {
					result["cacheScope"] = inner["cacheScope"];
				}
			}
		} else if (rpc.has("error")) {
			result["ok"] = false;
			result["error"] = rpc["error"];
		} else {
			result["result"] = payload;
		}
	} else {
		result["result"] = payload;
	}
	if (!http.get("ok", false) && !result.has("error")) {
		result["ok"] = false;
		result["error"] = http.get("error", "HTTP error");
	}
	return result;
}

Dictionary JustAMCPMCPClient::_handle_input_required(const String &p_method, const Dictionary &p_params, const Dictionary &p_result) {
	const Dictionary inner = p_result.has("result") && p_result["result"].get_type() == Variant::DICTIONARY ? Dictionary(p_result["result"]) : Dictionary();
	if (String(inner.get("resultType", p_result.get("resultType", ""))) != "input_required") {
		return p_result;
	}
	Dictionary elicitation = inner.has("elicitation") ? Dictionary(inner["elicitation"]) : Dictionary();
	const String mode = String(elicitation.get("mode", "form"));
	Dictionary responses;
	if (mode == "url") {
		const String url = String(elicitation.get("url", ""));
		Dictionary prompt;
		prompt["ok"] = false;
		prompt["error"] = "URL elicitation requires user action; the client will not auto-fetch the URL.";
		prompt["elicitation_url"] = url;
		prompt["resultType"] = "input_required";
		prompt["elicitation"] = elicitation;
		return prompt;
	}

	Dictionary schema = elicitation.has("requestedSchema") ? Dictionary(elicitation["requestedSchema"]) : (elicitation.has("schema") ? Dictionary(elicitation["schema"]) : justamcp_confirm_enum_schema());
	Dictionary content;
	content["confirmed"] = "yes";
	String schema_error;
	if (!justamcp_validate_elicit_content(schema, content, schema_error)) {
		content = justamcp_apply_schema_defaults(schema, Dictionary());
	}
	Dictionary retry_params = p_params.duplicate();
	Dictionary input_responses;
	input_responses["action"] = "accept";
	input_responses["content"] = content;
	retry_params["inputResponses"] = input_responses;
	return _rpc_once(p_method, retry_params);
}

Dictionary JustAMCPMCPClient::_rpc(const String &p_method, const Dictionary &p_params, bool p_use_cache) {
	if (p_use_cache) {
		const String key = p_method + ":" + JSON::stringify(p_params);
		MutexLock lock(cache_mutex);
		if (result_cache.has(key)) {
			const CacheEntry &entry = result_cache[key];
			const uint64_t now = Time::get_singleton() ? Time::get_singleton()->get_ticks_usec() : 0;
			if (entry.expires_usec == 0 || now < entry.expires_usec) {
				return entry.payload;
			}
			result_cache.erase(key);
		}
	}

	Dictionary result = _rpc_once(p_method, p_params);
	if (result.get("ok", false) && String(Dictionary(result.get("result", Dictionary())).get("resultType", result.get("resultType", ""))) == "input_required") {
		result = _handle_input_required(p_method, p_params, result);
	}

	if (p_use_cache && result.get("ok", false)) {
		const int ttl_ms = int(result.get("ttlMs", Dictionary(result.get("result", Dictionary())).get("ttlMs", 0)));
		if (ttl_ms > 0) {
			CacheEntry entry;
			entry.payload = result;
			entry.expires_usec = (Time::get_singleton() ? Time::get_singleton()->get_ticks_usec() : 0) + uint64_t(ttl_ms) * 1000ULL;
			MutexLock lock(cache_mutex);
			result_cache[p_method + ":" + JSON::stringify(p_params)] = entry;
		}
	}
	return result;
}

Dictionary JustAMCPMCPClient::rpc(const String &p_method, const Dictionary &p_params) {
	return _rpc(p_method, p_params, false);
}

Dictionary JustAMCPMCPClient::connect_remote() {
	last_status = "connecting";
	Dictionary discovered = discover();
	if (discovered.get("ok", false)) {
		const Dictionary result = discovered.has("result") && discovered["result"].get_type() == Variant::DICTIONARY ? Dictionary(discovered["result"]) : Dictionary();
		if (result.has("supportedVersions") && result["supportedVersions"].get_type() == Variant::ARRAY) {
			const Array versions = result["supportedVersions"];
			String preferred = String(config.get("protocol_version", "2026-07-28"));
			bool found = false;
			for (int i = 0; i < versions.size(); i++) {
				if (String(versions[i]) == preferred) {
					found = true;
					break;
				}
			}
			if (!found && versions.size() > 0) {
				preferred = String(versions[0]);
			}
			negotiated_protocol = preferred;
			modern_era = justamcp_protocol_at_least(negotiated_protocol, "2026-07-28");
		}
		connected = true;
		last_status = "connected";
		last_error = String();
		return discovered;
	}

	const Variant err = discovered.get("error", Variant());
	const String err_text = err.get_type() == Variant::DICTIONARY ? String(Dictionary(err).get("message", "")) : String(err);
	if (err_text.contains("not found") || err_text.contains("-32601") || err_text.contains("Method")) {
		modern_era = false;
		negotiated_protocol = String(config.get("protocol_version", "2025-11-25"));
		Dictionary init_params;
		init_params["protocolVersion"] = negotiated_protocol;
		init_params["capabilities"] = client_capabilities();
		init_params["clientInfo"] = client_info();
		Dictionary init = _rpc_once("initialize", init_params);
		if (!init.get("ok", false)) {
			last_status = "error";
			last_error = String(init.get("error", "Failed to initialize MCP bridge session"));
			return init;
		}
		if (init.has("result") && Dictionary(init["result"]).has("protocolVersion")) {
			negotiated_protocol = String(Dictionary(init["result"])["protocolVersion"]);
			modern_era = justamcp_protocol_at_least(negotiated_protocol, "2026-07-28");
		}
		_rpc_once("notifications/initialized", Dictionary());
		connected = true;
		last_status = "connected";
		last_error = String();
		return init;
	}

	last_status = "error";
	last_error = err_text.is_empty() ? String(discovered.get("error", "Failed to connect")) : err_text;
	return discovered;
}

void JustAMCPMCPClient::disconnect_remote() {
	stop_subscriptions_listen();
	connected = false;
	last_status = "disconnected";
	MutexLock lock(cache_mutex);
	result_cache.clear();
}

Dictionary JustAMCPMCPClient::status() const {
	Dictionary out;
	out["ok"] = true;
	out["name"] = get_name();
	out["url"] = get_url();
	out["connected"] = connected;
	out["status"] = last_status;
	out["protocol_version"] = negotiated_protocol;
	out["modern"] = modern_era;
	if (!last_error.is_empty()) {
		out["error"] = last_error;
	}
	return out;
}

Dictionary JustAMCPMCPClient::discover() {
	return _rpc("server/discover", Dictionary(), true);
}

Dictionary JustAMCPMCPClient::tools_list(const Dictionary &p_params) {
	return _rpc("tools/list", p_params, true);
}

Dictionary JustAMCPMCPClient::tools_call(const Dictionary &p_params) {
	return _rpc("tools/call", p_params, false);
}

Dictionary JustAMCPMCPClient::prompts_list(const Dictionary &p_params) {
	return _rpc("prompts/list", p_params, true);
}

Dictionary JustAMCPMCPClient::prompts_get(const Dictionary &p_params) {
	return _rpc("prompts/get", p_params, false);
}

Dictionary JustAMCPMCPClient::resources_list(const Dictionary &p_params) {
	return _rpc("resources/list", p_params, true);
}

Dictionary JustAMCPMCPClient::resources_templates_list(const Dictionary &p_params) {
	return _rpc("resources/templates/list", p_params, true);
}

Dictionary JustAMCPMCPClient::resources_read(const Dictionary &p_params) {
	return _rpc("resources/read", p_params, false);
}

Dictionary JustAMCPMCPClient::completion_complete(const Dictionary &p_params) {
	return _rpc("completion/complete", p_params, false);
}

Dictionary JustAMCPMCPClient::notifications_cancelled(const Dictionary &p_params) {
	return _rpc_once("notifications/cancelled", p_params);
}

void JustAMCPMCPClient::_listen_thread_cb(void *p_userdata) {
	JustAMCPMCPClient *self = static_cast<JustAMCPMCPClient *>(p_userdata);
	if (self) {
		self->_listen_worker();
	}
}

void JustAMCPMCPClient::_listen_worker() {
	listen_running.set();
	Dictionary params;
	_rpc_once("subscriptions/listen", params);
	listen_running.clear();
}

Dictionary JustAMCPMCPClient::start_subscriptions_listen() {
	if (listen_running.is_set()) {
		Dictionary ok;
		ok["ok"] = true;
		ok["listening"] = true;
		return ok;
	}
	listen_stop.clear();
	listen_thread.start(_listen_thread_cb, this);
	Dictionary ok;
	ok["ok"] = true;
	ok["listening"] = true;
	return ok;
}

void JustAMCPMCPClient::stop_subscriptions_listen() {
	listen_stop.set();
	if (listen_thread.is_started()) {
		listen_thread.wait_to_finish();
	}
	listen_running.clear();
}

JustAMCPMCPClient::JustAMCPMCPClient() {
	negotiated_protocol = "2026-07-28";
	modern_era = true;
}

JustAMCPMCPClient::~JustAMCPMCPClient() {
	disconnect_remote();
}

#endif
