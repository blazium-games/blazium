/**************************************************************************/
/*  justamcp_mcp_client_http.cpp                                          */
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

#include "justamcp_mcp_client_http.h"

#include "tools/justamcp_settings_resolver.h"

#include "core/io/json.h"
#include "core/os/os.h"

String JustAMCPMCPClientHTTP::normalize_scheme(const String &p_scheme) {
	String scheme = p_scheme.strip_edges().to_lower();
	if (scheme.ends_with("://")) {
		scheme = scheme.substr(0, scheme.length() - 3);
	}
	return scheme;
}

bool JustAMCPMCPClientHTTP::host_allowed(const String &p_host) {
	const String host = p_host.strip_edges().to_lower();
	if (host == "127.0.0.1" || host == "localhost" || host == "::1" || host == "[::1]") {
		return true;
	}
	const Array allow = JustAMCPSettingsResolver::resolve_array("blazium/justamcp/bridge_url_allow_hosts");
	for (int i = 0; i < allow.size(); i++) {
		if (String(allow[i]).strip_edges().to_lower() == host) {
			return true;
		}
	}
	return false;
}

bool JustAMCPMCPClientHTTP::url_allowed(const String &p_url, String &r_error) {
	String scheme;
	String host;
	int port = 0;
	String path;
	if (!parse_url(p_url, scheme, host, port, path)) {
		r_error = "Invalid bridge URL";
		return false;
	}
	if (scheme != "http" && scheme != "https") {
		r_error = "Bridge URL scheme must be http or https";
		return false;
	}
	if (!host_allowed(host)) {
		r_error = "Host '" + host + "' is not allow-listed (default: localhost / 127.0.0.1 / ::1). Extend blazium/justamcp/bridge_url_allow_hosts.";
		return false;
	}
	return true;
}

bool JustAMCPMCPClientHTTP::parse_url(const String &p_url, String &r_scheme, String &r_host, int &r_port, String &r_path) {
	String fragment;
	if (p_url.parse_url(r_scheme, r_host, r_port, r_path, fragment) != OK || r_host.is_empty()) {
		return false;
	}
	r_scheme = normalize_scheme(r_scheme);
	if (r_path.is_empty()) {
		r_path = "/mcp";
	}
	if (r_port <= 0) {
		r_port = r_scheme == "https" ? 443 : 80;
	}
	return true;
}

Vector<String> JustAMCPMCPClientHTTP::streamable_headers(const String &p_method, const String &p_protocol, const String &p_auth_token, const Dictionary &p_extra_headers, bool p_modern, const String &p_mcp_name) {
	Vector<String> headers;
	headers.push_back("Content-Type: application/json");
	if (p_modern) {
		headers.push_back("Accept: application/json, text/event-stream");
		if (!p_method.is_empty()) {
			headers.push_back("Mcp-Method: " + p_method);
		}
		if (!p_mcp_name.is_empty()) {
			headers.push_back("Mcp-Name: " + p_mcp_name);
		}
	} else {
		headers.push_back("Accept: application/json");
	}
	if (!p_protocol.is_empty()) {
		headers.push_back("MCP-Protocol-Version: " + p_protocol);
	}
	if (!p_auth_token.is_empty()) {
		headers.push_back("Authorization: Bearer " + p_auth_token);
	}
	const Array keys = p_extra_headers.keys();
	for (int i = 0; i < keys.size(); i++) {
		const String name = String(keys[i]).strip_edges();
		const String value = String(p_extra_headers[keys[i]]);
		if (!name.is_empty()) {
			headers.push_back(name + ": " + value);
		}
	}
	return headers;
}

Dictionary JustAMCPMCPClientHTTP::parse_json_or_sse(const String &p_body, const String &p_content_type) {
	Dictionary out;
	String text = p_body.strip_edges();
	if (p_content_type.to_lower().contains("text/event-stream")) {
		PackedStringArray lines = p_body.split("\n");
		String data;
		for (int i = 0; i < lines.size(); i++) {
			String line = lines[i].strip_edges();
			if (line.begins_with("data:")) {
				const String chunk = line.substr(5).strip_edges();
				if (!data.is_empty()) {
					data += "\n";
				}
				data += chunk;
			}
		}
		if (!data.is_empty()) {
			text = data;
		}
	}
	Ref<JSON> json;
	json.instantiate();
	if (json->parse(text) != OK) {
		out["ok"] = false;
		out["error"] = "Invalid JSON response from bridge";
		return out;
	}
	out["ok"] = true;
	out["payload"] = json->get_data();
	return out;
}

Dictionary JustAMCPMCPClientHTTP::header_value(const Dictionary &p_headers, const String &p_name) {
	if (p_headers.has(p_name)) {
		return Dictionary(p_headers);
	}
	const String lower = p_name.to_lower();
	for (const Variant &key : p_headers.keys()) {
		if (String(key).to_lower() == lower) {
			Dictionary single;
			single[p_name] = p_headers[key];
			return single;
		}
	}
	return Dictionary();
}

Dictionary JustAMCPMCPClientHTTP::request(const String &p_url, HTTPClient::Method p_method, const Vector<String> &p_headers, const String &p_body, int p_timeout_ms) {
	Dictionary result;
	String allow_error;
	if (!url_allowed(p_url, allow_error)) {
		result["ok"] = false;
		result["error"] = allow_error;
		return result;
	}

	String scheme;
	String host;
	int port = 0;
	String path;
	if (!parse_url(p_url, scheme, host, port, path)) {
		result["ok"] = false;
		result["error"] = "Invalid bridge URL";
		return result;
	}

	Ref<HTTPClient> client = HTTPClient::create();
	Ref<TLSOptions> tls;
	if (scheme == "https") {
		tls = TLSOptions::client();
	}
	Error err = client->connect_to_host(host, port, tls);
	if (err != OK) {
		result["ok"] = false;
		result["error"] = "Failed to connect to bridge host";
		return result;
	}

	const int timeout_ms = p_timeout_ms > 0 ? p_timeout_ms : 30000;
	client->set_blocking_mode(true);
	const uint64_t deadline_ms = OS::get_singleton()->get_ticks_msec() + (uint64_t)timeout_ms;
	while (client->get_status() == HTTPClient::STATUS_CONNECTING || client->get_status() == HTTPClient::STATUS_RESOLVING) {
		if (OS::get_singleton()->get_ticks_msec() >= deadline_ms) {
			client->close();
			result["ok"] = false;
			result["error"] = "MCP bridge connection timed out";
			return result;
		}
		client->poll();
	}
	if (client->get_status() != HTTPClient::STATUS_CONNECTED) {
		client->close();
		result["ok"] = false;
		result["error"] = "Failed to connect to bridge host";
		return result;
	}

	const Vector<uint8_t> body_bytes = p_body.to_utf8_buffer();
	err = client->request(p_method, path, p_headers, body_bytes.ptr(), body_bytes.size());
	if (err != OK) {
		result["ok"] = false;
		result["error"] = "Failed to send MCP request";
		return result;
	}

	while (client->get_status() == HTTPClient::STATUS_REQUESTING) {
		if (OS::get_singleton()->get_ticks_msec() >= deadline_ms) {
			result["ok"] = false;
			result["error"] = "MCP bridge request timed out";
			return result;
		}
		client->poll();
	}

	if (client->get_status() < HTTPClient::STATUS_BODY || client->get_status() > HTTPClient::STATUS_DISCONNECTED) {
		result["ok"] = false;
		result["error"] = "MCP request failed with status " + itos(client->get_status());
		return result;
	}

	List<String> raw_headers;
	client->get_response_headers(&raw_headers);
	Dictionary headers;
	String content_type;
	String www_authenticate;
	for (const String &line : raw_headers) {
		const int sep = line.find(":");
		if (sep <= 0) {
			continue;
		}
		const String name = line.substr(0, sep).strip_edges();
		const String value = line.substr(sep + 1).strip_edges();
		headers[name] = value;
		if (name.to_lower() == "content-type") {
			content_type = value;
		} else if (name.to_lower() == "www-authenticate") {
			www_authenticate = value;
		}
	}

	PackedByteArray response_body;
	while (client->get_status() == HTTPClient::STATUS_BODY) {
		if (OS::get_singleton()->get_ticks_msec() >= deadline_ms) {
			result["ok"] = false;
			result["error"] = "MCP bridge response timed out";
			return result;
		}
		client->poll();
		response_body.append_array(client->read_response_body_chunk());
	}

	const int status = client->get_response_code();
	const String response_text = String::utf8((const char *)response_body.ptr(), response_body.size());
	result["ok"] = status >= 200 && status < 300;
	result["status"] = status;
	result["headers"] = headers;
	result["body"] = response_text;
	result["content_type"] = content_type;
	if (!www_authenticate.is_empty()) {
		result["www_authenticate"] = www_authenticate;
	}
	if (status == 401) {
		result["ok"] = false;
		result["error"] = "unauthorized";
		result["oauth_required"] = true;
	}
	return result;
}

#endif
