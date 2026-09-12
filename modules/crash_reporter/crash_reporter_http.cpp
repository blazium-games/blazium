/**************************************************************************/
/*  crash_reporter_http.cpp                                               */
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

#include "crash_reporter_http.h"

#include "core/crypto/crypto.h"
#include "core/io/http_client.h"
#include "core/os/os.h"

#if defined(CRASH_REPORTER_ENABLED) && !defined(TOOLS_ENABLED)

static CrashReporterHTTPResult _upload_once(const String &p_endpoint, const String &p_app_id, const String &p_build_id, const String &p_user_agent, const Vector<uint8_t> &p_body, const String &p_content_type, int p_timeout_sec, bool p_verify_tls, bool *r_cancel) {
	CrashReporterHTTPResult result;
	String scheme;
	String host;
	String path;
	String fragment;
	int port = 0;
	Error err = p_endpoint.parse_url(scheme, host, port, path, fragment);
	if (err != OK || host.is_empty()) {
		result.error = ERR_INVALID_PARAMETER;
		result.message = "Invalid endpoint URL.";
		return result;
	}
	const bool ssl = scheme == "https://";
	if (path.is_empty()) {
		path = "/";
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
		if (r_cancel && *r_cancel) {
			result.error = ERR_SKIP;
			result.message = "Cancelled.";
			return result;
		}
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
	headers.push_back("Content-Type: " + p_content_type);
	headers.push_back("User-Agent: " + p_user_agent);
	headers.push_back("Accept: application/json");
	if (!p_app_id.is_empty()) {
		headers.push_back("X-App-Id: " + p_app_id);
	}
	if (!p_build_id.is_empty()) {
		headers.push_back("X-Build-Id: " + p_build_id);
	}

	err = client->request(HTTPClient::METHOD_POST, path, headers, p_body.ptr(), p_body.size());
	if (err != OK) {
		result.error = err;
		result.message = "Request failed to start.";
		return result;
	}

	while (client->get_status() == HTTPClient::STATUS_REQUESTING) {
		if (r_cancel && *r_cancel) {
			result.error = ERR_SKIP;
			result.message = "Cancelled.";
			return result;
		}
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

CrashReporterHTTPResult CrashReporterHTTP::upload_report(const String &p_endpoint, const String &p_app_id, const String &p_build_id, const String &p_user_agent, const Vector<uint8_t> &p_body, const String &p_content_type, int p_timeout_sec, bool p_verify_tls, int p_retry_count, int p_retry_backoff_sec, bool *r_cancel) {
	CrashReporterHTTPResult last;
	const int attempts = MAX(p_retry_count, 0) + 1;
	for (int i = 0; i < attempts; i++) {
		if (r_cancel && *r_cancel) {
			last.error = ERR_SKIP;
			last.message = "Cancelled.";
			return last;
		}
		last = _upload_once(p_endpoint, p_app_id, p_build_id, p_user_agent, p_body, p_content_type, p_timeout_sec, p_verify_tls, r_cancel);
		if (last.error == OK || last.error == ERR_SKIP || last.error == ERR_INVALID_PARAMETER) {
			return last;
		}
		if (i + 1 < attempts) {
			OS::get_singleton()->delay_usec((uint64_t)MAX(p_retry_backoff_sec, 1) * 1000000);
		}
	}
	return last;
}

#else

CrashReporterHTTPResult CrashReporterHTTP::upload_report(const String &, const String &, const String &, const String &, const Vector<uint8_t> &, const String &, int, bool, int, int, bool *) {
	CrashReporterHTTPResult result;
	result.error = ERR_UNAVAILABLE;
	result.message = "In-engine HTTP upload is only available in crash_reporter templates.";
	return result;
}

#endif
