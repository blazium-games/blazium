/**************************************************************************/
/*  http_response.cpp                                                     */
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

#include "http_response.h"

#include "core/io/json.h"

void HTTPResponse::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_status", "code"), &HTTPResponse::set_status);
	ClassDB::bind_method(D_METHOD("get_status"), &HTTPResponse::get_status);
	ClassDB::bind_method(D_METHOD("set_body", "body"), &HTTPResponse::set_body);
	ClassDB::bind_method(D_METHOD("get_body"), &HTTPResponse::get_body);
	ClassDB::bind_method(D_METHOD("set_json", "data"), &HTTPResponse::set_json);
	ClassDB::bind_method(D_METHOD("set_file", "path"), &HTTPResponse::set_file);
	ClassDB::bind_method(D_METHOD("add_header", "name", "value"), &HTTPResponse::add_header);
	ClassDB::bind_method(D_METHOD("set_content_type", "mime_type"), &HTTPResponse::set_content_type);
	ClassDB::bind_method(D_METHOD("get_headers"), &HTTPResponse::get_headers);
	ClassDB::bind_method(D_METHOD("start_sse"), &HTTPResponse::start_sse);
	ClassDB::bind_method(D_METHOD("is_sse_response"), &HTTPResponse::is_sse_response);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "status_code"), "set_status", "get_status");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "body"), "set_body", "get_body");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "headers"), "", "get_headers");
}

void HTTPResponse::set_status(int p_code) {
	status_code = p_code;
}

int HTTPResponse::get_status() const {
	return status_code;
}

void HTTPResponse::set_body(const String &p_body) {
	body_content = p_body;
	is_file_response = false;
}

String HTTPResponse::get_body() const {
	return body_content;
}

void HTTPResponse::set_json(const Dictionary &p_data) {
	JSON json;
	body_content = json.stringify(p_data);
	set_content_type("application/json");
	is_file_response = false;
}

void HTTPResponse::set_file(const String &p_path) {
	file_path = p_path;
	is_file_response = true;
}

void HTTPResponse::add_header(const String &p_name, const String &p_value) {
	headers[p_name] = p_value;
}

void HTTPResponse::set_content_type(const String &p_mime_type) {
	add_header("Content-Type", p_mime_type);
}

Dictionary HTTPResponse::get_headers() const {
	return headers;
}

void HTTPResponse::start_sse() {
	is_sse = true;
	set_content_type("text/event-stream");
	add_header("Cache-Control", "no-cache");
	add_header("Connection", "keep-alive");
	add_header("Access-Control-Allow-Origin", "*");
}

bool HTTPResponse::is_sse_response() const {
	return is_sse;
}

bool HTTPResponse::is_file() const {
	return is_file_response;
}

String HTTPResponse::get_file_path() const {
	return file_path;
}

bool HTTPResponse::is_sent() const {
	return sent;
}

void HTTPResponse::mark_sent() {
	sent = true;
}

HTTPResponse::HTTPResponse() {
	// Set default headers
	add_header("Server", "Blazium-HTTPServer/1.0");
}

HTTPResponse::~HTTPResponse() {
}
