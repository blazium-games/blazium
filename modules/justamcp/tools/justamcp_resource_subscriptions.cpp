/**************************************************************************/
/*  justamcp_resource_subscriptions.cpp                                   */
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

#include "justamcp_resource_subscriptions.h"

#include "../justamcp_server.h"

void JustAMCPResourceSubscriptions::subscribe(const String &p_uri) {
	if (p_uri.is_empty()) {
		return;
	}
	if (JustAMCPServer *server = JustAMCPServer::get_singleton()) {
		server->subscribe_resource(p_uri);
	}
}

void JustAMCPResourceSubscriptions::unsubscribe(const String &p_uri) {
	if (JustAMCPServer *server = JustAMCPServer::get_singleton()) {
		server->unsubscribe_resource(p_uri);
	}
}

bool JustAMCPResourceSubscriptions::is_subscribed(const String &p_uri) {
	if (JustAMCPServer *server = JustAMCPServer::get_singleton()) {
		return server->is_resource_subscribed(p_uri);
	}
	return false;
}

void JustAMCPResourceSubscriptions::notify_uri_changed(const String &p_uri) {
	if (!is_subscribed(p_uri)) {
		return;
	}
	if (JustAMCPServer *server = JustAMCPServer::get_singleton()) {
		server->broadcast_resource_updated(p_uri);
	}
}

#endif
