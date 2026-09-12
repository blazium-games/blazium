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

#include "justamcp_tool_context.h"

#include "core/templates/vector.h"

static thread_local Vector<Variant> g_justamcp_active_tool_request_stack;

Variant justamcp_get_active_tool_request_id() {
	if (g_justamcp_active_tool_request_stack.is_empty()) {
		return Variant();
	}
	return g_justamcp_active_tool_request_stack[g_justamcp_active_tool_request_stack.size() - 1];
}

void justamcp_push_active_tool_request_id(const Variant &p_request_id) {
	g_justamcp_active_tool_request_stack.push_back(p_request_id);
}

void justamcp_pop_active_tool_request_id() {
	if (!g_justamcp_active_tool_request_stack.is_empty()) {
		g_justamcp_active_tool_request_stack.resize(g_justamcp_active_tool_request_stack.size() - 1);
	}
}

#ifdef TOOLS_ENABLED

#include "justamcp_server.h"

JustAMCPToolContextScope::JustAMCPToolContextScope(const Variant &p_request_id) {
	request_id = p_request_id;
	justamcp_push_active_tool_request_id(p_request_id);
	active = true;
}

JustAMCPToolContextScope::~JustAMCPToolContextScope() {
	if (active) {
		justamcp_pop_active_tool_request_id();
	}
}

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

#endif
