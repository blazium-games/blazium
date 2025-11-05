/**************************************************************************/
/*  twitch_request_base.cpp                                               */
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

#include "twitch_request_base.h"

void TwitchRequestBase::_bind_methods() {
	// Base class has no methods to bind
}

void TwitchRequestBase::set_http_client(const Ref<TwitchHTTPClient> &p_client) {
	http_client = p_client;
}

void TwitchRequestBase::_request_get(const String &p_signal, const String &p_path, const Dictionary &p_params) {
	ERR_FAIL_COND(http_client.is_null());
	http_client->queue_request(p_signal, HTTPClient::METHOD_GET, p_path, p_params);
}

void TwitchRequestBase::_request_post(const String &p_signal, const String &p_path, const Dictionary &p_params, const String &p_body) {
	ERR_FAIL_COND(http_client.is_null());
	http_client->queue_request(p_signal, HTTPClient::METHOD_POST, p_path, p_params, p_body);
}

void TwitchRequestBase::_request_patch(const String &p_signal, const String &p_path, const Dictionary &p_params, const String &p_body) {
	ERR_FAIL_COND(http_client.is_null());
	http_client->queue_request(p_signal, HTTPClient::METHOD_PATCH, p_path, p_params, p_body);
}

void TwitchRequestBase::_request_put(const String &p_signal, const String &p_path, const Dictionary &p_params, const String &p_body) {
	ERR_FAIL_COND(http_client.is_null());
	http_client->queue_request(p_signal, HTTPClient::METHOD_PUT, p_path, p_params, p_body);
}

void TwitchRequestBase::_request_delete(const String &p_signal, const String &p_path, const Dictionary &p_params) {
	ERR_FAIL_COND(http_client.is_null());
	http_client->queue_request(p_signal, HTTPClient::METHOD_DELETE, p_path, p_params);
}
