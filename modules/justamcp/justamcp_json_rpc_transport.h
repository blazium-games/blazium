/**************************************************************************/
/*  justamcp_json_rpc_transport.h                                         */
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

#include "modules/modules_enabled.gen.h"

#if defined(MODULE_HTTPSERVER_ENABLED)

#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

class JustAMCPServer;
class HTTPResponse;

class JustAMCPJsonRpcTransport {
public:
	static Dictionary handle_json_rpc(JustAMCPServer *p_server, const String &p_body, Ref<HTTPResponse> p_response, const String &p_caller_session_id = String());
	static Dictionary handle_json_rpc_parsed(JustAMCPServer *p_server, const Dictionary &p_payload, Ref<HTTPResponse> p_response, const String &p_caller_session_id = String());

private:
	static bool _is_http_transport(Ref<HTTPResponse> p_response);
	static Dictionary _handle_json_rpc_payload(JustAMCPServer *p_server, const Dictionary &p_payload, Ref<HTTPResponse> p_response, const String &p_caller_session_id);
};

#endif
