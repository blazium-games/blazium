/**************************************************************************/
/*  twitch_http_client.cpp                                                */
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

#include "twitch_http_client.h"

#include "core/io/json.h"

void TwitchHTTPClient::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_base_url", "url"), &TwitchHTTPClient::set_base_url);
	ClassDB::bind_method(D_METHOD("set_credentials", "client_id", "access_token"), &TwitchHTTPClient::set_credentials);
	ClassDB::bind_method(D_METHOD("set_access_token", "token"), &TwitchHTTPClient::set_access_token);
	ClassDB::bind_method(D_METHOD("set_client_id", "client_id"), &TwitchHTTPClient::set_client_id);
	ClassDB::bind_method(D_METHOD("set_response_callback", "callback"), &TwitchHTTPClient::set_response_callback);
	ClassDB::bind_method(D_METHOD("queue_request", "signal_name", "method", "path", "query_params", "body"),
			&TwitchHTTPClient::queue_request, DEFVAL(Dictionary()), DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("poll"), &TwitchHTTPClient::poll);
	ClassDB::bind_method(D_METHOD("is_busy"), &TwitchHTTPClient::is_busy);
	ClassDB::bind_method(D_METHOD("get_rate_limit_remaining"), &TwitchHTTPClient::get_rate_limit_remaining);
	ClassDB::bind_method(D_METHOD("get_rate_limit_reset"), &TwitchHTTPClient::get_rate_limit_reset);
	ClassDB::bind_method(D_METHOD("get_queue_size"), &TwitchHTTPClient::get_queue_size);
	ClassDB::bind_static_method("TwitchHTTPClient", D_METHOD("query_string_from_dict", "params"),
			&TwitchHTTPClient::query_string_from_dict);
}

TwitchHTTPClient::TwitchHTTPClient() {
	http = HTTPClient::create();
	base_url = "https://api.twitch.tv/helix";
}

TwitchHTTPClient::~TwitchHTTPClient() {
	if (http.is_valid()) {
		http->close();
	}
}

void TwitchHTTPClient::set_base_url(const String &p_url) {
	base_url = p_url;
}

void TwitchHTTPClient::set_credentials(const String &p_client_id, const String &p_access_token) {
	client_id = p_client_id;
	access_token = p_access_token;
}

void TwitchHTTPClient::set_access_token(const String &p_token) {
	access_token = p_token;
}

void TwitchHTTPClient::set_client_id(const String &p_client_id) {
	client_id = p_client_id;
}

void TwitchHTTPClient::set_response_callback(const Callable &p_callback) {
	response_callback = p_callback;
}

void TwitchHTTPClient::queue_request(const String &p_signal_name, HTTPClient::Method p_method,
		const String &p_path, const Dictionary &p_query_params, const String &p_body) {
	RequestData req;
	req.signal_name = p_signal_name;
	req.method = p_method;
	req.path = p_path;
	req.query_params = p_query_params;
	req.body = p_body;
	req.retry_count = 0;

	request_queue.push_back(req);
}

void TwitchHTTPClient::poll() {
	if (http.is_null()) {
		return;
	}

	Error err = http->poll();
	if (err != OK) {
		ERR_PRINT(vformat("HTTP poll error: %d", err));
		state = STATE_IDLE;
		return;
	}

	_process_queue();
}

void TwitchHTTPClient::_process_queue() {
	HTTPClient::Status status = http->get_status();

	switch (state) {
		case STATE_IDLE: {
			if (request_queue.size() > 0) {
				current_request = request_queue[0];
				request_queue.remove_at(0);
				response_body.clear();
				response_code = 0;

				Error err = _connect_to_host();
				if (err != OK) {
					ERR_PRINT("Failed to initiate connection to Twitch API");
					if (response_callback.is_valid()) {
						Dictionary error_data;
						error_data["error"] = "connection_failed";
						error_data["message"] = "Failed to connect to Twitch API";
						response_callback.call(current_request.signal_name, 0, error_data);
					}
					state = STATE_IDLE;
				} else {
					state = STATE_CONNECTING;
				}
			}
		} break;

		case STATE_CONNECTING: {
			if (status == HTTPClient::STATUS_CONNECTED) {
				_send_request();
			} else if (status == HTTPClient::STATUS_CANT_CONNECT ||
					status == HTTPClient::STATUS_CANT_RESOLVE ||
					status == HTTPClient::STATUS_CONNECTION_ERROR ||
					status == HTTPClient::STATUS_TLS_HANDSHAKE_ERROR) {
				ERR_PRINT(vformat("Connection failed with status: %d", status));
				if (response_callback.is_valid()) {
					Dictionary error_data;
					error_data["error"] = "connection_failed";
					error_data["message"] = vformat("Connection failed with status: %d", status);
					response_callback.call(current_request.signal_name, 0, error_data);
				}
				http->close();
				state = STATE_IDLE;
			}
		} break;

		case STATE_REQUESTING: {
			if (status == HTTPClient::STATUS_BODY) {
				response_code = http->get_response_code();
				List<String> headers;
				http->get_response_headers(&headers);
				_parse_rate_limit_headers(headers);
				state = STATE_READING_BODY;
			} else if (status == HTTPClient::STATUS_CONNECTED) {
				// Request completed with no body
				response_code = http->get_response_code();
				_handle_response();
			} else if (status == HTTPClient::STATUS_CONNECTION_ERROR) {
				ERR_PRINT("Connection error during request");
				if (response_callback.is_valid()) {
					Dictionary error_data;
					error_data["error"] = "connection_error";
					error_data["message"] = "Connection error during request";
					response_callback.call(current_request.signal_name, 0, error_data);
				}
				http->close();
				state = STATE_IDLE;
			}
		} break;

		case STATE_READING_BODY: {
			if (status == HTTPClient::STATUS_BODY) {
				PackedByteArray chunk = http->read_response_body_chunk();
				if (chunk.size() > 0) {
					response_body.append_array(chunk);
				}
			} else if (status == HTTPClient::STATUS_CONNECTED) {
				// Body fully received
				_handle_response();
			} else if (status == HTTPClient::STATUS_CONNECTION_ERROR) {
				ERR_PRINT("Connection error while reading body");
				if (response_callback.is_valid()) {
					Dictionary error_data;
					error_data["error"] = "connection_error";
					error_data["message"] = "Connection error while reading body";
					response_callback.call(current_request.signal_name, 0, error_data);
				}
				http->close();
				state = STATE_IDLE;
			}
		} break;
	}
}

Error TwitchHTTPClient::_connect_to_host() {
	if (http.is_null()) {
		return ERR_UNCONFIGURED;
	}

	// Check if already connected
	HTTPClient::Status status = http->get_status();
	if (status == HTTPClient::STATUS_CONNECTED) {
		return OK;
	}

	// Parse base URL
	String host = base_url;
	int port = 443;
	bool use_tls = true;

	if (host.begins_with("https://")) {
		host = host.substr(8);
		port = 443;
		use_tls = true;
	} else if (host.begins_with("http://")) {
		host = host.substr(7);
		port = 80;
		use_tls = false;
	}

	// Remove trailing slash
	if (host.ends_with("/")) {
		host = host.substr(0, host.length() - 1);
	}

	// Connect
	Ref<TLSOptions> tls_opts;
	if (use_tls) {
		tls_opts = TLSOptions::client();
	}

	Error err = http->connect_to_host(host, port, tls_opts);
	return err;
}

void TwitchHTTPClient::_send_request() {
	if (client_id.is_empty() || access_token.is_empty()) {
		ERR_PRINT("TwitchAPI not configured. Call configure() with client_id and access_token first.");
		if (response_callback.is_valid()) {
			Dictionary error_data;
			error_data["error"] = "not_configured";
			error_data["message"] = "TwitchAPI not configured";
			response_callback.call(current_request.signal_name, 401, error_data);
		}
		state = STATE_IDLE;
		return;
	}

	// Build full path with query string
	String full_path = current_request.path;
	if (!current_request.query_params.is_empty()) {
		String query = _build_query_string(current_request.query_params);
		if (!query.is_empty()) {
			full_path += "?" + query;
		}
	}

	// Build headers
	Vector<String> headers;
	headers.push_back("Client-Id: " + client_id);
	headers.push_back("Authorization: Bearer " + access_token);

	if (!current_request.body.is_empty()) {
		headers.push_back("Content-Type: application/json");
	}

	// Send request
	Error err;
	if (!current_request.body.is_empty()) {
		CharString body_utf8 = current_request.body.utf8();
		err = http->request(current_request.method, full_path, headers,
				(const uint8_t *)body_utf8.get_data(), body_utf8.length());
	} else {
		err = http->request(current_request.method, full_path, headers, nullptr, 0);
	}

	if (err != OK) {
		ERR_PRINT(vformat("Failed to send request: %d", err));
		if (response_callback.is_valid()) {
			Dictionary error_data;
			error_data["error"] = "request_failed";
			error_data["message"] = vformat("Failed to send request: %d", err);
			response_callback.call(current_request.signal_name, 0, error_data);
		}
		state = STATE_IDLE;
	} else {
		state = STATE_REQUESTING;
	}
}

void TwitchHTTPClient::_handle_response() {
	// Handle different response codes
	if (response_code == 503 && current_request.retry_count == 0) {
		// Service Unavailable - retry once per Twitch API guidelines
		print_line("Twitch API returned 503, retrying once...");
		current_request.retry_count = 1;
		request_queue.insert(0, current_request); // Re-queue at front
		http->close();
		state = STATE_IDLE;
		return;
	}

	if (response_code == 429) {
		// Rate limited
		ERR_PRINT(vformat("Rate limited. Reset at: %d", rate_limit_reset));
		if (response_callback.is_valid()) {
			Dictionary error_data;
			error_data["error"] = "rate_limited";
			error_data["message"] = "Too many requests";
			error_data["reset_time"] = rate_limit_reset;
			response_callback.call(current_request.signal_name, response_code, error_data);
		}
		state = STATE_IDLE;
		return;
	}

	// Parse response body as JSON
	Dictionary response_data;
	if (response_body.size() > 0) {
		String body_text;
		body_text.parse_utf8((const char *)response_body.ptr(), response_body.size());

		Ref<JSON> json;
		json.instantiate();
		Error err = json->parse(body_text);
		if (err == OK) {
			Variant data = json->get_data();
			if (data.get_type() == Variant::DICTIONARY) {
				response_data = data;
			}
		} else {
			WARN_PRINT("Failed to parse JSON response");
			response_data["_raw"] = body_text;
		}
	}

	// Handle error responses
	if (response_code >= 400) {
		String error_msg = "Request failed";
		if (response_data.has("message")) {
			error_msg = response_data["message"];
		} else if (response_data.has("error")) {
			error_msg = response_data["error"];
		}

		ERR_PRINT(vformat("HTTP %d: %s", response_code, error_msg));
	}

	// Emit rate limit warning
	if (rate_limit_remaining >= 0 && rate_limit_remaining < 100) {
		WARN_PRINT(vformat("Approaching rate limit: %d requests remaining", rate_limit_remaining));
	}

	// Call response callback
	if (response_callback.is_valid()) {
		response_callback.call(current_request.signal_name, response_code, response_data);
	}

	// Connection keep-alive - stay connected for next request
	state = STATE_IDLE;
}

void TwitchHTTPClient::_parse_rate_limit_headers(const List<String> &p_headers) {
	for (const String &header : p_headers) {
		if (header.findn("Ratelimit-Remaining:") == 0) {
			String value = header.substr(header.find(":") + 1).strip_edges();
			rate_limit_remaining = value.to_int();
		} else if (header.findn("Ratelimit-Reset:") == 0) {
			String value = header.substr(header.find(":") + 1).strip_edges();
			rate_limit_reset = value.to_int();
		}
	}
}

String TwitchHTTPClient::_build_query_string(const Dictionary &p_params) const {
	return query_string_from_dict(p_params);
}

String TwitchHTTPClient::query_string_from_dict(const Dictionary &p_params) {
	if (p_params.is_empty()) {
		return String();
	}

	String query;
	Array keys = p_params.keys();
	for (int i = 0; i < keys.size(); i++) {
		String key = keys[i];
		Variant value = p_params[key];

		// Handle arrays (for multiple values with same key)
		if (value.get_type() == Variant::ARRAY) {
			Array arr = value;
			for (int j = 0; j < arr.size(); j++) {
				if (!query.is_empty()) {
					query += "&";
				}
				query += key.uri_encode() + "=" + String(arr[j]).uri_encode();
			}
		} else {
			if (!query.is_empty()) {
				query += "&";
			}
			query += key.uri_encode() + "=" + String(value).uri_encode();
		}
	}

	return query;
}

bool TwitchHTTPClient::is_busy() const {
	return state != STATE_IDLE || request_queue.size() > 0;
}

int TwitchHTTPClient::get_rate_limit_remaining() const {
	return rate_limit_remaining;
}

int TwitchHTTPClient::get_rate_limit_reset() const {
	return rate_limit_reset;
}

int TwitchHTTPClient::get_queue_size() const {
	return request_queue.size();
}
