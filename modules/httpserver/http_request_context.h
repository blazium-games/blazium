/**************************************************************************/
/*  http_request_context.h                                                */
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

#pragma once

#include "core/io/ip_address.h"
#include "core/object/ref_counted.h"
#include "core/variant/dictionary.h"

class HTTPRequestContext : public RefCounted {
	GDCLASS(HTTPRequestContext, RefCounted);

private:
	String method;
	String path;
	String raw_path;
	Dictionary query_params;
	Dictionary path_params;
	Dictionary headers;
	String body;
	IPAddress client_ip;
	int client_port = 0;

protected:
	static void _bind_methods();

public:
	// Setters (internal use by HTTPServer)
	void set_method(const String &p_method);
	void set_path(const String &p_path);
	void set_raw_path(const String &p_raw_path);
	void set_query_params(const Dictionary &p_params);
	void set_path_params(const Dictionary &p_params);
	void set_headers(const Dictionary &p_headers);
	void set_body(const String &p_body);
	void set_client_ip(const IPAddress &p_ip);
	void set_client_port(int p_port);

	// Getters
	String get_method() const;
	String get_path() const;
	String get_raw_path() const;
	Dictionary get_query_params() const;
	Dictionary get_path_params() const;
	Dictionary get_headers() const;
	String get_body() const;
	IPAddress get_client_ip() const;
	int get_client_port() const;

	// Utility methods
	String get_header(const String &p_name, const String &p_default = "") const;
	bool has_header(const String &p_name) const;

	String get_query_param(const String &p_name, const String &p_default = "") const;
	bool has_query_param(const String &p_name) const;

	String get_path_param(const String &p_name, const String &p_default = "") const;
	bool has_path_param(const String &p_name) const;

	// Body parsing
	Dictionary parse_json_body() const;
	Dictionary parse_form_data() const;

	HTTPRequestContext();
	~HTTPRequestContext();
};
