/**************************************************************************/
/*  analytics_http.cpp                                                    */
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

#include "analytics_http.h"

#include "core/io/http_client.h"
#include "core/os/os.h"

#ifdef ANALYTICS_ENABLED

String AnalyticsHTTP::normalize_endpoint(const String &p_endpoint) {
	String endpoint = p_endpoint.strip_edges();
	if (endpoint.is_empty()) {
		return endpoint;
	}
	while (endpoint.ends_with("/")) {
		endpoint = endpoint.substr(0, endpoint.length() - 1);
	}
	if (!endpoint.contains("/v1/events")) {
		endpoint += "/v1/events";
	}
	return endpoint;
}

AnalyticsHTTPResult AnalyticsHTTP::post_events(const String &p_endpoint, const String &p_api_key, const String &p_user_agent, const Vector<uint8_t> &p_body, int p_timeout_sec, bool p_verify_tls) {
	AnalyticsHTTPResult result;
	const String endpoint = normalize_endpoint(p_endpoint);
	String scheme;
	String host;
	String path;
	String fragment;
	int port = 0;
	Error err = endpoint.parse_url(scheme, host, port, path, fragment);
	if (err != OK || host.is_empty()) {
		result.error = ERR_INVALID_PARAMETER;
		result.message = "Invalid endpoint URL.";
		return result;
	}
	const bool ssl = scheme == "https://";
	if (path.is_empty()) {
		path = "/v1/events";
	}
	if (port == 0) {
		port = ssl ? 443 : 80;
	}

	Ref<HTTPClient> client = HTTPClient::create();
	client->set_blocking_mode(true);

	Ref<TLSOptions> tls;
	if (ssl) {
		tls = p_verify_tls ? TLSOptions::client() : TLSOptions::client_unsafe(Ref<X509Certificate>());
	}

	err = client->connect_to_host(host, port, tls);
	if (err != OK) {
		result.error = err;
		result.message = "Connect failed.";
		return result;
	}

	const uint64_t start = OS::get_singleton()->get_ticks_msec();
	const uint64_t timeout_ms = (uint64_t)MAX(p_timeout_sec, 1) * 1000;
	while (client->get_status() == HTTPClient::STATUS_RESOLVING || client->get_status() == HTTPClient::STATUS_CONNECTING) {
		if (OS::get_singleton()->get_ticks_msec() - start > timeout_ms) {
			result.error = ERR_TIMEOUT;
			result.message = "Connect timeout.";
			return result;
		}
		client->poll();
		OS::get_singleton()->delay_usec(1000);
	}
	if (client->get_status() != HTTPClient::STATUS_CONNECTED) {
		result.error = ERR_CANT_CONNECT;
		result.message = "Unable to connect.";
		return result;
	}

	Vector<String> headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back("User-Agent: " + p_user_agent);
	headers.push_back("Accept: application/json");
	if (!p_api_key.is_empty()) {
		headers.push_back("X-API-Key: " + p_api_key);
	}

	err = client->request(HTTPClient::METHOD_POST, path, headers, p_body.ptr(), p_body.size());
	if (err != OK) {
		result.error = err;
		result.message = "Request failed to start.";
		return result;
	}

	while (client->get_status() == HTTPClient::STATUS_REQUESTING) {
		if (OS::get_singleton()->get_ticks_msec() - start > timeout_ms) {
			result.error = ERR_TIMEOUT;
			result.message = "Request timeout.";
			return result;
		}
		client->poll();
		OS::get_singleton()->delay_usec(1000);
	}

	result.response_code = client->get_response_code();
	if (result.response_code >= 200 && result.response_code < 300) {
		result.error = OK;
		result.message = "OK";
	} else {
		result.error = FAILED;
		result.message = vformat("HTTP %d", result.response_code);
	}
	return result;
}

#else

String AnalyticsHTTP::normalize_endpoint(const String &p_endpoint) {
	return p_endpoint.strip_edges();
}

AnalyticsHTTPResult AnalyticsHTTP::post_events(const String &, const String &, const String &, const Vector<uint8_t> &, int, bool) {
	AnalyticsHTTPResult result;
	result.error = ERR_UNAVAILABLE;
	result.message = "Analytics HTTP is disabled in this build.";
	return result;
}

#endif
