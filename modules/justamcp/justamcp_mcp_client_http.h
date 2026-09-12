/**************************************************************************/
/*  justamcp_mcp_client_http.h                                            */
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

#ifdef TOOLS_ENABLED

#include "core/io/http_client.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"

class JustAMCPMCPClientHTTP {
public:
	static String normalize_scheme(const String &p_scheme);
	static bool host_allowed(const String &p_host);
	static bool url_allowed(const String &p_url, String &r_error);
	static bool parse_url(const String &p_url, String &r_scheme, String &r_host, int &r_port, String &r_path);

	static Vector<String> streamable_headers(const String &p_method, const String &p_protocol, const String &p_auth_token, const Dictionary &p_extra_headers, bool p_modern, const String &p_mcp_name = String());
	static Dictionary parse_json_or_sse(const String &p_body, const String &p_content_type);

	static Dictionary request(const String &p_url, HTTPClient::Method p_method, const Vector<String> &p_headers, const String &p_body, int p_timeout_ms);
	static Dictionary header_value(const Dictionary &p_headers, const String &p_name);
};

#endif
