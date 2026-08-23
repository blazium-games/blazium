/**************************************************************************/
/*  justamcp_mcp_client_oauth.cpp                                         */
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

#include "justamcp_mcp_client_oauth.h"

#include "justamcp_mcp_client_http.h"
#include "justamcp_oauth_discovery.h"
#include "justamcp_server.h"

#include "core/crypto/crypto.h"
#include "core/crypto/crypto_core.h"
#include "core/io/json.h"
#include "core/math/math_funcs.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/templates/hash_map.h"
#include "editor/editor_settings.h"

static Mutex g_oauth_pending_mutex;
static HashMap<String, Dictionary> g_oauth_pending;

String JustAMCPMCPClientOAuth::parse_www_authenticate_metadata_url(const String &p_header) {
	const String header = p_header.strip_edges();
	const int idx = header.find("resource_metadata=");
	if (idx < 0) {
		return String();
	}
	String rest = header.substr(idx + String("resource_metadata=").length()).strip_edges();
	if (rest.begins_with("\"")) {
		rest = rest.substr(1);
		const int end = rest.find("\"");
		if (end >= 0) {
			rest = rest.substr(0, end);
		}
	} else {
		const int end = rest.find(",");
		if (end >= 0) {
			rest = rest.substr(0, end);
		}
		rest = rest.strip_edges();
	}
	return rest;
}

Dictionary JustAMCPMCPClientOAuth::parse_protected_resource_metadata(const Dictionary &p_json) {
	Dictionary out;
	out["resource"] = p_json.get("resource", "");
	out["authorization_servers"] = p_json.get("authorization_servers", Array());
	out["scopes_supported"] = p_json.get("scopes_supported", Array());
	out["bearer_methods_supported"] = p_json.get("bearer_methods_supported", Array());
	return out;
}

Dictionary JustAMCPMCPClientOAuth::parse_authorization_server_metadata(const Dictionary &p_json) {
	Dictionary out;
	out["issuer"] = p_json.get("issuer", "");
	out["authorization_endpoint"] = p_json.get("authorization_endpoint", "");
	out["token_endpoint"] = p_json.get("token_endpoint", "");
	out["registration_endpoint"] = p_json.get("registration_endpoint", "");
	out["client_id_metadata_document_supported"] = p_json.get("client_id_metadata_document_supported", false);
	out["grant_types_supported"] = p_json.get("grant_types_supported", Array());
	out["code_challenge_methods_supported"] = p_json.get("code_challenge_methods_supported", Array());
	return out;
}

Dictionary JustAMCPMCPClientOAuth::build_dcr_request(const String &p_redirect_uri, const String &p_application_type, const String &p_token_auth_method) {
	Dictionary body;
	body["application_type"] = p_application_type;
	body["token_endpoint_auth_method"] = p_token_auth_method;
	body["client_name"] = "JustAMCP";
	Array grants;
	grants.push_back("authorization_code");
	grants.push_back("refresh_token");
	body["grant_types"] = grants;
	Array response_types;
	response_types.push_back("code");
	body["response_types"] = response_types;
	Array redirects;
	redirects.push_back(p_redirect_uri);
	body["redirect_uris"] = redirects;
	return body;
}

bool JustAMCPMCPClientOAuth::validate_cimd_url(const String &p_url, String &r_error) {
	if (p_url.is_empty()) {
		r_error = "CIMD URL is empty.";
		return false;
	}
	if (!p_url.begins_with("https://") && !p_url.begins_with("http://127.0.0.1") && !p_url.begins_with("http://localhost")) {
		r_error = "CIMD client_id must be an HTTPS URL (loopback HTTP is allowed for local testing).";
		return false;
	}
	return JustAMCPOauthDiscovery::validate_cimd_client_id(p_url, r_error);
}

bool JustAMCPMCPClientOAuth::validate_iss(const String &p_expected_issuer, const String &p_returned_iss, String &r_error) {
	if (p_returned_iss.is_empty()) {
		return true;
	}
	if (p_expected_issuer.is_empty()) {
		return true;
	}
	String expected = p_expected_issuer.strip_edges();
	String got = p_returned_iss.strip_edges();
	while (expected.ends_with("/")) {
		expected = expected.substr(0, expected.length() - 1);
	}
	while (got.ends_with("/")) {
		got = got.substr(0, got.length() - 1);
	}
	if (expected != got) {
		r_error = "Authorization response iss does not match the authorization server issuer.";
		return false;
	}
	return true;
}

String JustAMCPMCPClientOAuth::issuer_storage_key(const String &p_issuer) {
	String issuer = p_issuer.strip_edges().to_lower();
	while (issuer.ends_with("/")) {
		issuer = issuer.substr(0, issuer.length() - 1);
	}
	unsigned char hash[32];
	const CharString utf8 = issuer.utf8();
	CryptoCore::sha256((const uint8_t *)utf8.get_data(), utf8.length(), hash);
	return CryptoCore::b64_encode_str(hash, 32).replace("/", "_").replace("+", "-").replace("=", "");
}

String JustAMCPMCPClientOAuth::to_base64url(const String &p_b64) {
	return p_b64.replace("+", "-").replace("/", "_").trim_suffix("=").trim_suffix("=");
}

String JustAMCPMCPClientOAuth::pkce_challenge_s256(const String &p_verifier) {
	unsigned char hash[32];
	const CharString utf8 = p_verifier.utf8();
	CryptoCore::sha256((const uint8_t *)utf8.get_data(), utf8.length(), hash);
	return to_base64url(CryptoCore::b64_encode_str(hash, 32));
}

static String _random_urlsafe(int p_bytes) {
	PackedByteArray bytes;
	Ref<Crypto> crypto = Crypto::create();
	if (crypto.is_valid()) {
		bytes = crypto->generate_random_bytes(p_bytes);
	} else {
		bytes.resize(p_bytes);
		uint8_t *w = bytes.ptrw();
		for (int i = 0; i < p_bytes; i++) {
			w[i] = (uint8_t)Math::rand();
		}
	}
	return JustAMCPMCPClientOAuth::to_base64url(CryptoCore::b64_encode_str(bytes.ptr(), bytes.size()));
}

String JustAMCPMCPClientOAuth::generate_pkce_verifier() {
	return _random_urlsafe(32);
}

String JustAMCPMCPClientOAuth::generate_state() {
	return _random_urlsafe(16);
}

String JustAMCPMCPClientOAuth::editor_token_path(const String &p_issuer) {
	return "blazium/justamcp/oauth_tokens/" + issuer_storage_key(p_issuer);
}

Dictionary JustAMCPMCPClientOAuth::load_tokens(const String &p_issuer) {
	if (p_issuer.is_empty() || !EditorSettings::get_singleton()) {
		return Dictionary();
	}
	const String path = editor_token_path(p_issuer);
	if (!EditorSettings::get_singleton()->has_setting(path)) {
		return Dictionary();
	}
	const Variant stored = EditorSettings::get_singleton()->get_setting(path);
	if (stored.get_type() == Variant::DICTIONARY) {
		return stored;
	}
	if (stored.get_type() == Variant::STRING) {
		Ref<JSON> json;
		json.instantiate();
		if (json->parse(String(stored)) == OK && json->get_data().get_type() == Variant::DICTIONARY) {
			return json->get_data();
		}
	}
	return Dictionary();
}

void JustAMCPMCPClientOAuth::store_tokens(const String &p_issuer, const Dictionary &p_tokens) {
	if (p_issuer.is_empty() || !EditorSettings::get_singleton()) {
		return;
	}
	EditorSettings::get_singleton()->set_setting(editor_token_path(p_issuer), p_tokens.duplicate());
}

void JustAMCPMCPClientOAuth::clear_tokens(const String &p_issuer) {
	if (p_issuer.is_empty() || !EditorSettings::get_singleton()) {
		return;
	}
	EditorSettings::get_singleton()->set_setting(editor_token_path(p_issuer), Dictionary());
}

Dictionary JustAMCPMCPClientOAuth::client_metadata_document(int p_loopback_port, const String &p_cimd_url) {
	Dictionary doc;
	const String redirect = "http://127.0.0.1:" + itos(p_loopback_port > 0 ? p_loopback_port : 6506) + "/oauth/callback";
	const String client_id = p_cimd_url.is_empty() ? ("http://127.0.0.1:" + itos(p_loopback_port > 0 ? p_loopback_port : 6506) + "/oauth/client-metadata.json") : p_cimd_url;
	doc["client_id"] = client_id;
	doc["client_name"] = "JustAMCP";
	Array redirects;
	redirects.push_back(redirect);
	doc["redirect_uris"] = redirects;
	Array grants;
	grants.push_back("authorization_code");
	grants.push_back("refresh_token");
	doc["grant_types"] = grants;
	Array response_types;
	response_types.push_back("code");
	doc["response_types"] = response_types;
	doc["token_endpoint_auth_method"] = "none";
	doc["application_type"] = "native";
	return doc;
}

bool JustAMCPMCPClientOAuth::register_pending_auth(const Dictionary &p_pending) {
	const String state = String(p_pending.get("state", ""));
	if (state.is_empty()) {
		return false;
	}
	MutexLock lock(g_oauth_pending_mutex);
	g_oauth_pending[state] = p_pending.duplicate();
	return true;
}

Dictionary JustAMCPMCPClientOAuth::take_pending_auth(const String &p_state) {
	MutexLock lock(g_oauth_pending_mutex);
	if (!g_oauth_pending.has(p_state)) {
		return Dictionary();
	}
	Dictionary pending = g_oauth_pending[p_state];
	g_oauth_pending.erase(p_state);
	return pending;
}

Dictionary JustAMCPMCPClientOAuth::handle_loopback_callback(const Dictionary &p_query) {
	Dictionary out;
	const String state = String(p_query.get("state", ""));
	const String code = String(p_query.get("code", ""));
	const String iss = String(p_query.get("iss", ""));
	const String error = String(p_query.get("error", ""));
	if (!error.is_empty()) {
		out["ok"] = false;
		out["error"] = error;
		out["error_description"] = p_query.get("error_description", "");
		return out;
	}
	Dictionary pending = take_pending_auth(state);
	if (pending.is_empty()) {
		out["ok"] = false;
		out["error"] = "Unknown or expired OAuth state.";
		return out;
	}
	String iss_error;
	if (!validate_iss(String(pending.get("issuer", "")), iss, iss_error)) {
		out["ok"] = false;
		out["error"] = iss_error;
		return out;
	}
	if (code.is_empty()) {
		out["ok"] = false;
		out["error"] = "Authorization callback missing code.";
		return out;
	}
	return exchange_code(pending, code);
}

Dictionary JustAMCPMCPClientOAuth::exchange_code(const Dictionary &p_pending, const String &p_code) {
	Dictionary out;
	const String token_endpoint = String(p_pending.get("token_endpoint", ""));
	if (token_endpoint.is_empty()) {
		out["ok"] = false;
		out["error"] = "Missing token_endpoint.";
		return out;
	}
	String allow_error;
	if (!JustAMCPMCPClientHTTP::url_allowed(token_endpoint, allow_error)) {
		out["ok"] = false;
		out["error"] = "OAuth token endpoint is blocked: " + allow_error;
		return out;
	}
	Dictionary form;
	form["grant_type"] = "authorization_code";
	form["code"] = p_code;
	form["redirect_uri"] = p_pending.get("redirect_uri", "");
	form["client_id"] = p_pending.get("client_id", "");
	form["code_verifier"] = p_pending.get("code_verifier", "");
	const String secret = String(p_pending.get("client_secret", ""));
	if (!secret.is_empty()) {
		form["client_secret"] = secret;
	}

	Vector<String> headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back("Accept: application/json");
	Dictionary http = JustAMCPMCPClientHTTP::request(token_endpoint, HTTPClient::METHOD_POST, headers, JSON::stringify(form), 15000);
	if (!http.get("ok", false)) {
		return http;
	}
	Ref<JSON> json;
	json.instantiate();
	if (json->parse(String(http.get("body", ""))) != OK || json->get_data().get_type() != Variant::DICTIONARY) {
		out["ok"] = false;
		out["error"] = "Invalid token response.";
		return out;
	}
	Dictionary tokens = json->get_data();
	tokens["issuer"] = p_pending.get("issuer", "");
	store_tokens(String(p_pending.get("issuer", "")), tokens);
	out["ok"] = true;
	out["access_token"] = tokens.get("access_token", "");
	out["issuer"] = tokens.get("issuer", "");
	return out;
}

Dictionary JustAMCPMCPClientOAuth::refresh_access_token(const Dictionary &p_tokens, const Dictionary &p_as_metadata, const Dictionary &p_bridge_config) {
	Dictionary out;
	const String refresh = String(p_tokens.get("refresh_token", ""));
	const String token_endpoint = String(p_as_metadata.get("token_endpoint", ""));
	if (refresh.is_empty() || token_endpoint.is_empty()) {
		out["ok"] = false;
		out["error"] = "Refresh token or token endpoint missing.";
		return out;
	}
	String allow_error;
	if (!JustAMCPMCPClientHTTP::url_allowed(token_endpoint, allow_error)) {
		out["ok"] = false;
		out["error"] = "OAuth token endpoint is blocked: " + allow_error;
		return out;
	}
	Dictionary form;
	form["grant_type"] = "refresh_token";
	form["refresh_token"] = refresh;
	form["client_id"] = p_bridge_config.get("client_id", "");
	Vector<String> headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back("Accept: application/json");
	Dictionary http = JustAMCPMCPClientHTTP::request(token_endpoint, HTTPClient::METHOD_POST, headers, JSON::stringify(form), 15000);
	if (!http.get("ok", false)) {
		return http;
	}
	Ref<JSON> json;
	json.instantiate();
	if (json->parse(String(http.get("body", ""))) != OK || json->get_data().get_type() != Variant::DICTIONARY) {
		out["ok"] = false;
		out["error"] = "Invalid refresh response.";
		return out;
	}
	Dictionary tokens = json->get_data();
	const String issuer = String(p_as_metadata.get("issuer", p_tokens.get("issuer", "")));
	tokens["issuer"] = issuer;
	store_tokens(issuer, tokens);
	out["ok"] = true;
	out["access_token"] = tokens.get("access_token", "");
	return out;
}

Dictionary JustAMCPMCPClientOAuth::begin_authorization(const Dictionary &p_bridge_config, const Dictionary &p_as_metadata) {
	Dictionary out;
	const String issuer = String(p_as_metadata.get("issuer", ""));
	const String authorize = String(p_as_metadata.get("authorization_endpoint", ""));
	const String token_endpoint = String(p_as_metadata.get("token_endpoint", ""));
	if (authorize.is_empty() || token_endpoint.is_empty()) {
		out["ok"] = false;
		out["error"] = "Authorization server metadata is missing authorization or token endpoint.";
		return out;
	}

	const int port = JustAMCPOauthDiscovery::listening_port();
	const String redirect = "http://127.0.0.1:" + itos(port) + "/oauth/callback";
	String client_id = String(p_bridge_config.get("client_id", ""));
	String client_secret = String(p_bridge_config.get("client_secret", ""));
	String cimd_url = String(p_bridge_config.get("cimd_url", ""));

	if (client_id.is_empty() && bool(p_as_metadata.get("client_id_metadata_document_supported", false))) {
		if (cimd_url.is_empty()) {
			cimd_url = "http://127.0.0.1:" + itos(port) + "/oauth/client-metadata.json";
		}
		String cimd_error;
		if (validate_cimd_url(cimd_url, cimd_error)) {
			client_id = cimd_url;
		}
	}

	if (client_id.is_empty()) {
		const String registration = String(p_as_metadata.get("registration_endpoint", ""));
		String registration_allow_error;
		if (!registration.is_empty() && !JustAMCPMCPClientHTTP::url_allowed(registration, registration_allow_error)) {
			out["ok"] = false;
			out["error"] = "OAuth registration endpoint is blocked: " + registration_allow_error;
			return out;
		}
		if (!registration.is_empty()) {
			Dictionary dcr = build_dcr_request(redirect, "native", "none");
			Vector<String> headers;
			headers.push_back("Content-Type: application/json");
			headers.push_back("Accept: application/json");
			Dictionary http = JustAMCPMCPClientHTTP::request(registration, HTTPClient::METHOD_POST, headers, JSON::stringify(dcr), 15000);
			if (!http.get("ok", false)) {
				dcr = build_dcr_request(redirect, "web", "none");
				http = JustAMCPMCPClientHTTP::request(registration, HTTPClient::METHOD_POST, headers, JSON::stringify(dcr), 15000);
			}
			if (http.get("ok", false)) {
				Ref<JSON> json;
				json.instantiate();
				if (json->parse(String(http.get("body", ""))) == OK && json->get_data().get_type() == Variant::DICTIONARY) {
					const Dictionary registered = json->get_data();
					client_id = String(registered.get("client_id", ""));
					client_secret = String(registered.get("client_secret", client_secret));
				}
			}
		}
	}

	if (client_id.is_empty()) {
		out["ok"] = false;
		out["error"] = "OAuth client registration failed. Provide client_id / client_secret or a CIMD URL.";
		out["needs_user_credentials"] = true;
		return out;
	}

	const String verifier = generate_pkce_verifier();
	const String challenge = pkce_challenge_s256(verifier);
	const String state = generate_state();
	const String scopes = String(p_bridge_config.get("scopes", "mcp"));

	Dictionary pending;
	pending["state"] = state;
	pending["code_verifier"] = verifier;
	pending["issuer"] = issuer;
	pending["token_endpoint"] = token_endpoint;
	pending["redirect_uri"] = redirect;
	pending["client_id"] = client_id;
	pending["client_secret"] = client_secret;
	register_pending_auth(pending);

	String url = authorize;
	url += authorize.contains("?") ? "&" : "?";
	url += "response_type=code";
	url += "&client_id=" + client_id.uri_encode();
	url += "&redirect_uri=" + redirect.uri_encode();
	url += "&state=" + state.uri_encode();
	url += "&code_challenge=" + challenge.uri_encode();
	url += "&code_challenge_method=S256";
	if (!scopes.is_empty()) {
		url += "&scope=" + scopes.uri_encode();
	}

	if (OS::get_singleton()) {
		OS::get_singleton()->shell_open(url);
	}

	out["ok"] = true;
	out["pending"] = true;
	out["authorize_url"] = url;
	out["issuer"] = issuer;
	return out;
}

Dictionary JustAMCPMCPClientOAuth::resolve_bearer_token(const Dictionary &p_bridge_config, const Dictionary &p_http_response) {
	Dictionary out;
	const String mode = String(p_bridge_config.get("oauth_mode", "auto"));
	if (mode == "none") {
		out["ok"] = false;
		out["error"] = "OAuth is disabled for this bridge.";
		return out;
	}
	const String static_token = String(p_bridge_config.get("auth_token", ""));
	if (!static_token.is_empty() || mode == "bearer") {
		out["ok"] = !static_token.is_empty();
		out["access_token"] = static_token;
		if (static_token.is_empty()) {
			out["error"] = "oauth_mode=bearer requires auth_token.";
		}
		return out;
	}

	const String www = String(p_http_response.get("www_authenticate", ""));
	String metadata_url = parse_www_authenticate_metadata_url(www);
	if (metadata_url.is_empty()) {
		String scheme;
		String host;
		int port = 0;
		String path;
		if (JustAMCPMCPClientHTTP::parse_url(String(p_bridge_config.get("url", "")), scheme, host, port, path)) {
			metadata_url = scheme + "://" + host + ":" + itos(port) + "/.well-known/oauth-protected-resource";
		}
	}
	String metadata_allow_error;
	if (!metadata_url.is_empty() && !JustAMCPMCPClientHTTP::url_allowed(metadata_url, metadata_allow_error)) {
		out["ok"] = false;
		out["error"] = "OAuth metadata URL is blocked: " + metadata_allow_error;
		return out;
	}

	Vector<String> headers;
	headers.push_back("Accept: application/json");
	Dictionary prm_http = JustAMCPMCPClientHTTP::request(metadata_url, HTTPClient::METHOD_GET, headers, String(), 10000);
	Dictionary prm;
	if (prm_http.get("ok", false)) {
		Ref<JSON> json;
		json.instantiate();
		if (json->parse(String(prm_http.get("body", ""))) == OK && json->get_data().get_type() == Variant::DICTIONARY) {
			prm = parse_protected_resource_metadata(json->get_data());
		}
	}

	String issuer;
	const Array servers = prm.has("authorization_servers") ? Array(prm["authorization_servers"]) : Array();
	if (!servers.is_empty()) {
		issuer = String(servers[0]);
	}
	if (issuer.is_empty()) {
		issuer = String(p_bridge_config.get("oauth_issuer", ""));
	}

	Dictionary stored = load_tokens(issuer);
	if (!String(stored.get("access_token", "")).is_empty()) {
		out["ok"] = true;
		out["access_token"] = stored.get("access_token", "");
		out["issuer"] = issuer;
		return out;
	}

	String as_url = issuer;
	if (!as_url.ends_with("/.well-known/oauth-authorization-server") && !as_url.ends_with("/.well-known/openid-configuration")) {
		while (as_url.ends_with("/")) {
			as_url = as_url.substr(0, as_url.length() - 1);
		}
		as_url += "/.well-known/oauth-authorization-server";
	}
	String as_allow_error;
	if (!as_url.is_empty() && !JustAMCPMCPClientHTTP::url_allowed(as_url, as_allow_error)) {
		out["ok"] = false;
		out["error"] = "OAuth authorization server URL is blocked: " + as_allow_error;
		return out;
	}
	Dictionary as_http = JustAMCPMCPClientHTTP::request(as_url, HTTPClient::METHOD_GET, headers, String(), 10000);
	Dictionary as_meta;
	if (as_http.get("ok", false)) {
		Ref<JSON> json;
		json.instantiate();
		if (json->parse(String(as_http.get("body", ""))) == OK && json->get_data().get_type() == Variant::DICTIONARY) {
			as_meta = parse_authorization_server_metadata(json->get_data());
		}
	}
	if (String(as_meta.get("issuer", "")).is_empty()) {
		as_meta["issuer"] = issuer;
	}

	if (!String(stored.get("refresh_token", "")).is_empty()) {
		Dictionary refreshed = refresh_access_token(stored, as_meta, p_bridge_config);
		if (refreshed.get("ok", false)) {
			return refreshed;
		}
	}

	return begin_authorization(p_bridge_config, as_meta);
}

#endif
