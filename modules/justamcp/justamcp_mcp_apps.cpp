/**************************************************************************/
/*  justamcp_mcp_apps.cpp                                                 */
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

#include "justamcp_mcp_apps.h"

#include "justamcp_cli_args.h"
#include "justamcp_host_html.gen.h"
#include "justamcp_oauth_discovery.h"
#include "justamcp_server.h"
#include "tools/justamcp_mcp_client_bridge.h"

#include "core/os/os.h"

JustAMCPMCPAppsHost *JustAMCPMCPAppsHost::singleton = nullptr;

void JustAMCPMCPAppsHost::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open_app", "bridge_name", "ui_meta", "html"), &JustAMCPMCPAppsHost::open_app);
	ClassDB::bind_method(D_METHOD("list_open_apps"), &JustAMCPMCPAppsHost::list_open_apps);
	ClassDB::bind_method(D_METHOD("host_url"), &JustAMCPMCPAppsHost::host_url);
	ClassDB::bind_method(D_METHOD("proxy_tools_call", "bridge_name", "tool_name", "arguments", "user_consented"), &JustAMCPMCPAppsHost::proxy_tools_call);
}

JustAMCPMCPAppsHost *JustAMCPMCPAppsHost::get_singleton() {
	return singleton;
}

Dictionary JustAMCPMCPAppsHost::apps_extension_capability() {
	Dictionary ext;
	ext["host"] = "justamcp";
	ext["csp"] = true;
	ext["permissions"] = true;
	return ext;
}

Dictionary JustAMCPMCPAppsHost::detect_ui_meta(const Dictionary &p_tool_or_result) {
	Dictionary meta;
	if (p_tool_or_result.has("_meta") && p_tool_or_result["_meta"].get_type() == Variant::DICTIONARY) {
		meta = p_tool_or_result["_meta"];
	} else if (p_tool_or_result.has("ui") && p_tool_or_result["ui"].get_type() == Variant::DICTIONARY) {
		meta = p_tool_or_result;
	}
	Dictionary ui;
	if (meta.has("ui") && meta["ui"].get_type() == Variant::DICTIONARY) {
		ui = meta["ui"];
	} else if (meta.has("io.modelcontextprotocol/ui") && meta["io.modelcontextprotocol/ui"].get_type() == Variant::DICTIONARY) {
		ui = meta["io.modelcontextprotocol/ui"];
	}
	Dictionary out;
	out["present"] = !String(ui.get("resourceUri", "")).is_empty();
	out["resourceUri"] = ui.get("resourceUri", "");
	out["csp"] = ui.get("csp", Dictionary());
	out["permissions"] = ui.get("permissions", Dictionary());
	return out;
}

bool JustAMCPMCPAppsHost::permissions_granted(const Dictionary &p_requested, const Dictionary &p_user_grants) {
	const Array keys = p_requested.keys();
	for (int i = 0; i < keys.size(); i++) {
		const String key = String(keys[i]).to_lower();
		const bool requested = bool(p_requested[keys[i]]);
		if (!requested) {
			continue;
		}
		if (key == "camera" || key == "microphone" || key == "mic") {
			if (!bool(p_user_grants.get(key, false))) {
				return false;
			}
		} else if (!bool(p_user_grants.get(key, false))) {
			return false;
		}
	}
	return true;
}

String JustAMCPMCPAppsHost::csp_header(const Dictionary &p_csp) {
	PackedStringArray parts;
	auto directive = [&](const String &p_name, const String &p_fallback) {
		if (p_csp.has(p_name) && p_csp[p_name].get_type() == Variant::STRING && !String(p_csp[p_name]).is_empty()) {
			parts.push_back(p_name + " " + String(p_csp[p_name]));
		} else {
			parts.push_back(p_name + " " + p_fallback);
		}
	};
	directive("default-src", "'none'");
	directive("script-src", "'self'");
	directive("style-src", "'self' 'unsafe-inline'");
	directive("img-src", "'self' data:");
	directive("connect-src", "'self'");
	directive("frame-src", "'self'");
	directive("frame-ancestors", "'self'");
	parts.push_back("base-uri 'none'");
	parts.push_back("form-action 'none'");
	return String("; ").join(parts);
}

String JustAMCPMCPAppsHost::embedded_host_template() {
	return String::utf8(justamcp_host_html);
}

String JustAMCPMCPAppsHost::host_page_html(const String &p_app_html, const String &p_csp) {
	String html = embedded_host_template();
	html = html.replace("{{JUSTAMCP_CSP}}", p_csp.xml_escape());
	html = html.replace("{{JUSTAMCP_APP_HTML}}", p_app_html.xml_escape(true));
	return html;
}

String JustAMCPMCPAppsHost::ui_apps_prefix() {
	return ".blazium/apps/";
}

String JustAMCPMCPAppsHost::bundled_host_uri() {
	return "ui://" + ui_apps_prefix() + "host.html";
}

bool JustAMCPMCPAppsHost::sanitize_ui_uri(const String &p_uri, String &r_path, String &r_error) {
	if (!p_uri.begins_with("ui://")) {
		r_error = "UI resource URI must use the ui:// scheme.";
		return false;
	}
	String rest = p_uri.substr(String("ui://").length());
	if (rest.is_empty() || rest.contains("..") || rest.contains("\\") || rest.contains("//") || rest.begins_with("/")) {
		r_error = "UI resource path is outside the allow-list.";
		return false;
	}
	const String prefix = ui_apps_prefix();
	if (!rest.begins_with(prefix) || !rest.ends_with(".html")) {
		r_error = "UI resources must be ui://.blazium/apps/*.html";
		return false;
	}
	const String name = rest.substr(prefix.length());
	if (name.length() <= 5 || name.contains("/") || name.contains("\\")) {
		r_error = "UI app path must be a single .html file under ui://.blazium/apps/.";
		return false;
	}
	for (int i = 0; i < name.length() - 5; i++) {
		const char32_t c = name[i];
		const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
		if (!ok) {
			r_error = "UI app file names may only contain letters, digits, '_' or '-'.";
			return false;
		}
	}
	r_path = rest;
	return true;
}

Dictionary JustAMCPMCPAppsHost::open_app(const String &p_bridge_name, const Dictionary &p_ui_meta, const String &p_html) {
	Dictionary out;
	const String uri = String(p_ui_meta.get("resourceUri", detect_ui_meta(p_ui_meta).get("resourceUri", "")));
	if (uri.is_empty()) {
		out["ok"] = false;
		out["error"] = "Missing _meta.ui.resourceUri.";
		return out;
	}
	Dictionary app;
	app["bridge"] = p_bridge_name;
	app["resourceUri"] = uri;
	app["csp"] = csp_header(p_ui_meta.has("csp") ? Dictionary(p_ui_meta["csp"]) : Dictionary());
	app["html"] = p_html;
	{
		MutexLock lock(apps_mutex);
		open_apps[uri] = app;
	}
	const String url = host_url() + "?uri=" + uri.uri_encode();
	const bool server_listening = JustAMCPServer::get_singleton() && JustAMCPServer::get_singleton()->get_listening_port() > 0;
	const bool launch_browser = server_listening && !JustAMCPCliArgs::is_headless() && !JustAMCPCliArgs::is_unit_test();
	if (launch_browser && OS::get_singleton()) {
		OS::get_singleton()->shell_open(url);
	}
	out["ok"] = true;
	out["url"] = url;
	out["opened"] = launch_browser;
	out["resourceUri"] = uri;
	return out;
}

Dictionary JustAMCPMCPAppsHost::get_open_app(const String &p_uri) const {
	MutexLock lock(apps_mutex);
	if (!open_apps.has(p_uri)) {
		return Dictionary();
	}
	return open_apps[p_uri];
}

Array JustAMCPMCPAppsHost::list_open_apps() const {
	Array out;
	MutexLock lock(apps_mutex);
	for (const KeyValue<String, Dictionary> &E : open_apps) {
		Dictionary row = E.value.duplicate();
		row.erase("html");
		out.push_back(row);
	}
	return out;
}

String JustAMCPMCPAppsHost::host_url() const {
	return "http://127.0.0.1:" + itos(JustAMCPOauthDiscovery::listening_port()) + "/mcp-apps/host";
}

Dictionary JustAMCPMCPAppsHost::proxy_tools_call(const String &p_bridge_name, const String &p_tool_name, const Dictionary &p_arguments, bool p_user_consented) {
	Dictionary out;
	const String key = p_bridge_name + "/" + p_tool_name;
	if (!p_user_consented && !has_tool_consent(key)) {
		out["ok"] = false;
		out["error"] = "MCP App tools/call requires user consent.";
		out["needs_consent"] = true;
		out["tool"] = p_tool_name;
		return out;
	}
	if (p_user_consented) {
		grant_tool_consent(key);
	}
	if (!JustAMCPMCPClientBridge::get_singleton()) {
		out["ok"] = false;
		out["error"] = "MCP client bridge is unavailable.";
		return out;
	}
	Dictionary args;
	args["bridge_name"] = p_bridge_name;
	args["tool_name"] = p_tool_name;
	args["arguments"] = p_arguments;
	return JustAMCPMCPClientBridge::get_singleton()->call_remote_tool(args);
}

void JustAMCPMCPAppsHost::grant_tool_consent(const String &p_tool_key) {
	MutexLock lock(apps_mutex);
	consented_tools.insert(p_tool_key);
}

bool JustAMCPMCPAppsHost::has_tool_consent(const String &p_tool_key) const {
	MutexLock lock(apps_mutex);
	return consented_tools.has(p_tool_key);
}

JustAMCPMCPAppsHost::JustAMCPMCPAppsHost() {
	if (!singleton) {
		singleton = this;
	}
}

JustAMCPMCPAppsHost::~JustAMCPMCPAppsHost() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

#endif
