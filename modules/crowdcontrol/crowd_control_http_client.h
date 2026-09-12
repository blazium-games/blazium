/**************************************************************************/
/*  crowd_control_http_client.h                                           */
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

#include "core/io/http_client.h"
#include "core/object/ref_counted.h"

class CrowdControlHTTPClient : public RefCounted {
	GDCLASS(CrowdControlHTTPClient, RefCounted);

public:
	struct RequestData {
		String signal_name;
		HTTPClient::Method method;
		String path;
		String body;
		int retry_count = 0;
	};

private:
	Ref<HTTPClient> http;
	String base_url;
	String auth_token;

	enum State {
		STATE_IDLE,
		STATE_CONNECTING,
		STATE_REQUESTING,
		STATE_READING_BODY
	};
	State state = STATE_IDLE;
	Vector<RequestData> request_queue;
	RequestData current_request;
	PackedByteArray response_body;
	int response_code = 0;

	Callable response_callback;

	void _process_queue();
	void _send_request();
	void _handle_response();
	Error _connect_to_host();

protected:
	static void _bind_methods();

public:
	void set_base_url(const String &p_url);
	void set_auth_token(const String &p_token);

	void set_response_callback(const Callable &p_callback);

	void queue_request(const String &p_signal_name, HTTPClient::Method p_method,
			const String &p_path, const String &p_body = String());

	void poll();
	bool is_busy() const;
	int get_queue_size() const;

	CrowdControlHTTPClient();
	~CrowdControlHTTPClient();
};
