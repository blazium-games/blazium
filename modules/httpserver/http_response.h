/**************************************************************************/
/*  http_response.h                                                       */
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

#include "core/object/ref_counted.h"
#include "core/variant/dictionary.h"

class HTTPResponse : public RefCounted {
	GDCLASS(HTTPResponse, RefCounted);

private:
	int status_code = 200;
	Dictionary headers;
	String body_content;
	String file_path;
	bool is_sse = false;
	bool is_file_response = false;
	bool sent = false;

protected:
	static void _bind_methods();

public:
	// Response building
	void set_status(int p_code);
	int get_status() const;

	void set_body(const String &p_body);
	String get_body() const;

	void set_json(const Dictionary &p_data);
	void set_file(const String &p_path);

	void add_header(const String &p_name, const String &p_value);
	void set_content_type(const String &p_mime_type);
	Dictionary get_headers() const;

	// SSE
	void start_sse();
	bool is_sse_response() const;

	// Internal
	bool is_file() const;
	String get_file_path() const;
	bool is_sent() const;
	void mark_sent();

	HTTPResponse();
	~HTTPResponse();
};
