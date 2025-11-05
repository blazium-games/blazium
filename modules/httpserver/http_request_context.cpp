/**************************************************************************/
/*  http_request_context.cpp                                              */
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

#include "http_request_context.h"

#include "core/io/json.h"

void HTTPRequestContext::_bind_methods() {
	// Setters
	ClassDB::bind_method(D_METHOD("set_method", "method"), &HTTPRequestContext::set_method);
	ClassDB::bind_method(D_METHOD("set_path", "path"), &HTTPRequestContext::set_path);
	ClassDB::bind_method(D_METHOD("set_raw_path", "raw_path"), &HTTPRequestContext::set_raw_path);
	ClassDB::bind_method(D_METHOD("set_query_params", "params"), &HTTPRequestContext::set_query_params);
	ClassDB::bind_method(D_METHOD("set_path_params", "params"), &HTTPRequestContext::set_path_params);
	ClassDB::bind_method(D_METHOD("set_headers", "headers"), &HTTPRequestContext::set_headers);
	ClassDB::bind_method(D_METHOD("set_body", "body"), &HTTPRequestContext::set_body);
	ClassDB::bind_method(D_METHOD("set_client_ip", "ip"), &HTTPRequestContext::set_client_ip);
	ClassDB::bind_method(D_METHOD("set_client_port", "port"), &HTTPRequestContext::set_client_port);

	// Getters
	ClassDB::bind_method(D_METHOD("get_method"), &HTTPRequestContext::get_method);
	ClassDB::bind_method(D_METHOD("get_path"), &HTTPRequestContext::get_path);
	ClassDB::bind_method(D_METHOD("get_raw_path"), &HTTPRequestContext::get_raw_path);
	ClassDB::bind_method(D_METHOD("get_query_params"), &HTTPRequestContext::get_query_params);
	ClassDB::bind_method(D_METHOD("get_path_params"), &HTTPRequestContext::get_path_params);
	ClassDB::bind_method(D_METHOD("get_headers"), &HTTPRequestContext::get_headers);
	ClassDB::bind_method(D_METHOD("get_body"), &HTTPRequestContext::get_body);
	ClassDB::bind_method(D_METHOD("get_client_ip"), &HTTPRequestContext::get_client_ip);
	ClassDB::bind_method(D_METHOD("get_client_port"), &HTTPRequestContext::get_client_port);

	// Utility methods
	ClassDB::bind_method(D_METHOD("get_header", "name", "default_value"), &HTTPRequestContext::get_header, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("has_header", "name"), &HTTPRequestContext::has_header);
	ClassDB::bind_method(D_METHOD("get_query_param", "name", "default_value"), &HTTPRequestContext::get_query_param, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("has_query_param", "name"), &HTTPRequestContext::has_query_param);
	ClassDB::bind_method(D_METHOD("get_path_param", "name", "default_value"), &HTTPRequestContext::get_path_param, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("has_path_param", "name"), &HTTPRequestContext::has_path_param);

	// Body parsing
	ClassDB::bind_method(D_METHOD("parse_json_body"), &HTTPRequestContext::parse_json_body);
	ClassDB::bind_method(D_METHOD("parse_form_data"), &HTTPRequestContext::parse_form_data);

	// Properties
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "method"), "set_method", "get_method");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "path"), "set_path", "get_path");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "raw_path"), "set_raw_path", "get_raw_path");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "query_params"), "set_query_params", "get_query_params");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "path_params"), "set_path_params", "get_path_params");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "headers"), "set_headers", "get_headers");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "body"), "set_body", "get_body");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "client_ip"), "set_client_ip", "get_client_ip");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "client_port"), "set_client_port", "get_client_port");
}

// Setters
void HTTPRequestContext::set_method(const String &p_method) {
	method = p_method;
}

void HTTPRequestContext::set_path(const String &p_path) {
	path = p_path;
}

void HTTPRequestContext::set_raw_path(const String &p_raw_path) {
	raw_path = p_raw_path;
}

void HTTPRequestContext::set_query_params(const Dictionary &p_params) {
	query_params = p_params;
}

void HTTPRequestContext::set_path_params(const Dictionary &p_params) {
	path_params = p_params;
}

void HTTPRequestContext::set_headers(const Dictionary &p_headers) {
	headers = p_headers;
}

void HTTPRequestContext::set_body(const String &p_body) {
	body = p_body;
}

void HTTPRequestContext::set_client_ip(const IPAddress &p_ip) {
	client_ip = p_ip;
}

void HTTPRequestContext::set_client_port(int p_port) {
	client_port = p_port;
}

// Getters
String HTTPRequestContext::get_method() const {
	return method;
}

String HTTPRequestContext::get_path() const {
	return path;
}

String HTTPRequestContext::get_raw_path() const {
	return raw_path;
}

Dictionary HTTPRequestContext::get_query_params() const {
	return query_params;
}

Dictionary HTTPRequestContext::get_path_params() const {
	return path_params;
}

Dictionary HTTPRequestContext::get_headers() const {
	return headers;
}

String HTTPRequestContext::get_body() const {
	return body;
}

IPAddress HTTPRequestContext::get_client_ip() const {
	return client_ip;
}

int HTTPRequestContext::get_client_port() const {
	return client_port;
}

// Utility methods
String HTTPRequestContext::get_header(const String &p_name, const String &p_default) const {
	String lower_name = p_name.to_lower();
	if (headers.has(lower_name)) {
		return headers[lower_name];
	}
	return p_default;
}

bool HTTPRequestContext::has_header(const String &p_name) const {
	String lower_name = p_name.to_lower();
	return headers.has(lower_name);
}

String HTTPRequestContext::get_query_param(const String &p_name, const String &p_default) const {
	if (query_params.has(p_name)) {
		return query_params[p_name];
	}
	return p_default;
}

bool HTTPRequestContext::has_query_param(const String &p_name) const {
	return query_params.has(p_name);
}

String HTTPRequestContext::get_path_param(const String &p_name, const String &p_default) const {
	if (path_params.has(p_name)) {
		return path_params[p_name];
	}
	return p_default;
}

bool HTTPRequestContext::has_path_param(const String &p_name) const {
	return path_params.has(p_name);
}

// Body parsing
Dictionary HTTPRequestContext::parse_json_body() const {
	if (body.is_empty()) {
		return Dictionary();
	}

	JSON json;
	Error err = json.parse(body);
	if (err != OK) {
		ERR_PRINT("Failed to parse JSON body: " + json.get_error_message());
		return Dictionary();
	}

	Variant result = json.get_data();
	if (result.get_type() == Variant::DICTIONARY) {
		return result;
	}

	return Dictionary();
}

Dictionary HTTPRequestContext::parse_form_data() const {
	Dictionary result;
	if (body.is_empty()) {
		return result;
	}

	// Parse application/x-www-form-urlencoded
	Vector<String> pairs = body.split("&");
	for (int i = 0; i < pairs.size(); i++) {
		Vector<String> kv = pairs[i].split("=");
		if (kv.size() == 2) {
			String key = kv[0].uri_decode();
			String value = kv[1].uri_decode();
			result[key] = value;
		} else if (kv.size() == 1) {
			String key = kv[0].uri_decode();
			result[key] = "";
		}
	}

	return result;
}

HTTPRequestContext::HTTPRequestContext() {
}

HTTPRequestContext::~HTTPRequestContext() {
}
