/**************************************************************************/
/*  justamcp_oauth_discovery.cpp                                          */
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

#include "justamcp_oauth_discovery.h"

#include "justamcp_server.h"
#include "tools/justamcp_settings_resolver.h"

#include "core/io/json.h"

bool JustAMCPOauthDiscovery::oauth_enabled() {
	return JustAMCPSettingsResolver::resolve_bool("blazium/justamcp/oauth_enabled", false);
}

String JustAMCPOauthDiscovery::setting_string(const String &p_name, const String &p_default) {
	return JustAMCPSettingsResolver::resolve_string(p_name, p_default);
}

int JustAMCPOauthDiscovery::listening_port() {
	if (JustAMCPServer::get_singleton() && JustAMCPServer::get_singleton()->get_listening_port() > 0) {
		return JustAMCPServer::get_singleton()->get_listening_port();
	}
	return JustAMCPSettingsResolver::resolve_server_port();
}

String JustAMCPOauthDiscovery::resource_url() {
	const String configured = setting_string("blazium/justamcp/oauth_resource");
	if (!configured.is_empty()) {
		return configured;
	}
	return "http://127.0.0.1:" + itos(listening_port()) + "/mcp";
}

String JustAMCPOauthDiscovery::issuer_url() {
	String resource = resource_url();
	if (resource.ends_with("/mcp")) {
		resource = resource.substr(0, resource.length() - 4);
	}
	while (resource.ends_with("/")) {
		resource = resource.substr(0, resource.length() - 1);
	}
	return resource;
}

Array JustAMCPOauthDiscovery::authorization_servers() {
	Array servers;
	const String configured = setting_string("blazium/justamcp/oauth_authorization_servers");
	if (!configured.is_empty()) {
		const PackedStringArray parts = configured.split(",", false);
		for (int i = 0; i < parts.size(); i++) {
			const String url = String(parts[i]).strip_edges();
			if (!url.is_empty()) {
				servers.push_back(url);
			}
		}
	}
	if (servers.is_empty()) {
		servers.push_back(issuer_url());
	}
	return servers;
}

Array JustAMCPOauthDiscovery::scopes_supported() {
	Array scopes;
	const String configured = setting_string("blazium/justamcp/oauth_scopes_supported", "mcp");
	const PackedStringArray parts = configured.split(",", false);
	for (int i = 0; i < parts.size(); i++) {
		const String scope = String(parts[i]).strip_edges();
		if (!scope.is_empty()) {
			scopes.push_back(scope);
		}
	}
	if (scopes.is_empty()) {
		scopes.push_back("mcp");
	}
	return scopes;
}

Dictionary JustAMCPOauthDiscovery::protected_resource_metadata() {
	Dictionary meta;
	meta["resource"] = resource_url();
	meta["authorization_servers"] = authorization_servers();
	meta["scopes_supported"] = scopes_supported();
	Array methods;
	methods.push_back("header");
	meta["bearer_methods_supported"] = methods;
	return meta;
}

Dictionary JustAMCPOauthDiscovery::authorization_server_metadata() {
	Dictionary meta;
	const String issuer = issuer_url();
	meta["issuer"] = issuer;
	meta["authorization_endpoint"] = issuer + "/oauth/authorize";
	meta["token_endpoint"] = issuer + "/oauth/token";
	Array grants;
	grants.push_back("client_credentials");
	meta["grant_types_supported"] = grants;
	Array auth_methods;
	auth_methods.push_back("client_secret_basic");
	auth_methods.push_back("client_secret_post");
	meta["token_endpoint_auth_methods_supported"] = auth_methods;
	meta["scopes_supported"] = scopes_supported();
	Array response_types;
	response_types.push_back("token");
	meta["response_types_supported"] = response_types;
	Array subject_types;
	subject_types.push_back("public");
	meta["subject_types_supported"] = subject_types;
	Array algs;
	algs.push_back("none");
	meta["id_token_signing_alg_values_supported"] = algs;
	return meta;
}

String JustAMCPOauthDiscovery::www_authenticate_header() {
	const String metadata = issuer_url() + "/.well-known/oauth-protected-resource/mcp";
	const Array scopes = scopes_supported();
	const String scope = scopes.is_empty() ? String("mcp") : String(scopes[0]);
	return "Bearer realm=\"mcp\", resource_metadata=\"" + metadata + "\", scope=\"" + scope + "\", error=\"insufficient_scope\"";
}

bool JustAMCPOauthDiscovery::client_id_is_cimd(const String &p_client_id) {
	return p_client_id.begins_with("https://");
}

bool JustAMCPOauthDiscovery::validate_cimd_client_id(const String &p_client_id, String &r_error) {
	if (!client_id_is_cimd(p_client_id)) {
		return true;
	}
	const String pinned = setting_string("blazium/justamcp/oauth_cimd_json");
	if (pinned.is_empty()) {
		return true;
	}
	Ref<JSON> json;
	json.instantiate();
	if (json->parse(pinned) != OK || json->get_data().get_type() != Variant::DICTIONARY) {
		r_error = "oauth_cimd_json is not a valid Client ID Metadata Document.";
		return false;
	}
	const Dictionary doc = json->get_data();
	const String doc_client_id = String(doc.get("client_id", ""));
	if (doc_client_id != p_client_id) {
		r_error = "CIMD client_id does not match the configured identifier.";
		return false;
	}
	return true;
}
