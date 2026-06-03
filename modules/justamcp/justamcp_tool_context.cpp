/**************************************************************************/
/*  justamcp_tool_context.cpp                                             */
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

#ifdef TOOLS_ENABLED

#include "justamcp_tool_context.h"
#include "justamcp_server.h"

bool justamcp_is_cancel_requested() {
	JustAMCPServer *server = JustAMCPServer::get_singleton();
	return server && server->is_current_tool_cancel_requested();
}

void justamcp_report_progress(double p_progress, double p_total, const String &p_message) {
	JustAMCPServer *server = JustAMCPServer::get_singleton();
	if (!server) {
		return;
	}
	const String token = server->get_current_progress_token();
	if (token.is_empty()) {
		return;
	}
	server->report_tool_progress(token, p_progress, p_total, p_message);
}

String justamcp_get_active_progress_token() {
	JustAMCPServer *server = JustAMCPServer::get_singleton();
	return server ? server->get_current_progress_token() : String();
}

#endif // TOOLS_ENABLED
