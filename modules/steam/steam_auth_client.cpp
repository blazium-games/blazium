/**************************************************************************/
/*  steam_auth_client.cpp                                                 */
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

#include "steam_auth_client.h"

#include "core/io/http_client.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "core/os/time.h"

Ref<SteamAuthResult> SteamAuthClient::authenticate_sync(const String &p_url, const String &p_ticket, int p_app_id, bool p_debug_logging) {
	Ref<SteamAuthResult> result;
	result.instantiate();

	if (p_url.is_empty()) {
		result->set_error_message("Auth server URL is empty");
		return result;
	}
	if (p_ticket.is_empty()) {
		result->set_error_message("Ticket is empty");
		return result;
	}

	Dictionary body_dict;
	body_dict["ticket"] = p_ticket;
	body_dict["app_id"] = p_app_id;
	String body = JSON::stringify(body_dict);
	PackedByteArray body_bytes = body.to_utf8_buffer();

	Vector<String> headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back("Accept: application/json");

	Ref<HTTPClient> http = HTTPClient::create();
	http->set_blocking_mode(true);

	String host;
	int port = 80;
	String scheme = "http";

	int scheme_end = p_url.find("://");
	if (scheme_end == -1) {
		result->set_error_message("Invalid auth server URL");
		return result;
	}

	scheme = p_url.substr(0, scheme_end).to_lower();
	String remainder = p_url.substr(scheme_end + 3);
	int slash = remainder.find("/");
	String authority = slash == -1 ? remainder : remainder.substr(0, slash);
	int colon = authority.find(":");
	if (colon != -1) {
		host = authority.substr(0, colon);
		port = authority.substr(colon + 1).to_int();
	} else {
		host = authority;
		port = scheme == "https" ? 443 : 80;
	}

	Error err = http->connect_to_host(host, port, scheme == "https" ? TLSOptions::client() : Ref<TLSOptions>());
	if (err != OK) {
		result->set_error_message(vformat("Failed to connect to auth server: %s", error_names[err]));
		return result;
	}

	const double connect_timeout_sec = 15.0;
	const double connect_start_usec = Time::get_singleton()->get_ticks_usec();
	while (http->get_status() != HTTPClient::STATUS_CONNECTED) {
		http->poll();
		HTTPClient::Status status = http->get_status();
		if (status == HTTPClient::STATUS_CANT_CONNECT ||
				status == HTTPClient::STATUS_CANT_RESOLVE ||
				status == HTTPClient::STATUS_CONNECTION_ERROR ||
				status == HTTPClient::STATUS_TLS_HANDSHAKE_ERROR) {
			result->set_error_message(vformat("Auth server connection error (status=%d)", (int)status));
			http->close();
			return result;
		}
		if ((Time::get_singleton()->get_ticks_usec() - connect_start_usec) / 1000000.0 > connect_timeout_sec) {
			result->set_error_message("Timed out connecting to auth server");
			http->close();
			return result;
		}
		::OS::get_singleton()->delay_usec(1000);
	}

	err = http->request(HTTPClient::METHOD_POST, p_url, headers, body_bytes.ptr(), body_bytes.size());
	if (err != OK) {
		result->set_error_message(vformat("HTTP request failed: %s", error_names[err]));
		return result;
	}

	PackedByteArray response_bytes;
	while (true) {
		http->poll();
		HTTPClient::Status status = http->get_status();
		if (status == HTTPClient::STATUS_BODY) {
			PackedByteArray chunk = http->read_response_body_chunk();
			if (!chunk.is_empty()) {
				response_bytes.append_array(chunk);
			}
		} else if (status == HTTPClient::STATUS_CONNECTED || status == HTTPClient::STATUS_DISCONNECTED) {
			break;
		} else if (status == HTTPClient::STATUS_CANT_CONNECT ||
				status == HTTPClient::STATUS_CANT_RESOLVE ||
				status == HTTPClient::STATUS_CONNECTION_ERROR ||
				status == HTTPClient::STATUS_TLS_HANDSHAKE_ERROR) {
			result->set_error_message(vformat("Auth server connection error (status=%d)", (int)status));
			http->close();
			return result;
		}
	}

	int response_code = http->get_response_code();
	result->set_http_status(response_code);
	String response_text = String::utf8((const char *)response_bytes.ptr(), response_bytes.size());

	if (p_debug_logging) {
		print_line(vformat("[SteamAuthClient] POST %s -> HTTP %d", p_url, response_code));
		print_line(vformat("[SteamAuthClient] Response: %s", response_text));
	}

	Variant parsed = JSON::parse_string(response_text);
	if (parsed.get_type() != Variant::DICTIONARY) {
		result->set_error_message("Invalid JSON response from auth server");
		http->close();
		return result;
	}

	Dictionary response_dict = parsed;

	if (response_code != 200) {
		String message = response_dict.has("error") ? String(response_dict["error"]) : response_text;
		result->set_error_message(message);
		http->close();
		return result;
	}

	if (!response_dict.has("jwt")) {
		result->set_error_message("Auth response missing jwt");
		http->close();
		return result;
	}

	result->set_success(true);
	result->set_jwt(response_dict["jwt"]);
	if (response_dict.has("steam_id")) {
		result->set_steam_id(String(response_dict["steam_id"]));
	}
	if (response_dict.has("persona")) {
		result->set_persona(String(response_dict["persona"]));
	}
	if (response_dict.has("app_id")) {
		result->set_app_id((int)response_dict["app_id"]);
	} else {
		result->set_app_id(p_app_id);
	}

	http->close();
	return result;
}
