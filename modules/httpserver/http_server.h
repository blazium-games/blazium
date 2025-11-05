/**************************************************************************/
/*  http_server.h                                                         */
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

#include "http_request_context.h"
#include "http_response.h"
#include "sse_connection.h"

#include "core/io/stream_peer_tcp.h"
#include "core/io/stream_peer_tls.h"
#include "core/io/tcp_server.h"
#include "core/object/object.h"
#include "core/os/mutex.h"
#include "core/os/thread.h"
#include "core/templates/hash_map.h"
#include "core/templates/safe_refcount.h"

class HTTPServer : public Object {
	GDCLASS(HTTPServer, Object);

	static HTTPServer *singleton;

	struct Route {
		String method;
		String pattern;
		Callable callback;
	};

	struct ClientConnection {
		Ref<StreamPeerTCP> tcp;
		Ref<StreamPeerTLS> tls;
		Ref<StreamPeer> peer;
		uint64_t time = 0;
		uint8_t req_buf[8192];
		int req_pos = 0;
		bool is_sse = false;
		int sse_connection_id = 0;
	};

private:
	Ref<TCPServer> server;
	Ref<CryptoKey> key;
	Ref<X509Certificate> cert;
	bool use_tls = false;

	List<Route> routes;
	HashMap<int, ClientConnection> clients;
	HashMap<int, Ref<SSEConnection>> sse_connections;
	int next_connection_id = 1;
	int next_client_id = 1;

	SafeFlag server_quit;
	Mutex server_lock;
	Thread server_thread;

	// Configuration
	String cors_origin = "*";
	bool cors_enabled = true;
	int max_request_size = 8192;
	String static_directory;
	bool directory_listing_enabled = false;

	// MIME types
	HashMap<String, String> mimes;

	void _init_mime_types();
	void _clear_client(int p_client_id);
	void _poll();
	void _poll_client(int p_client_id, ClientConnection &p_client);
	void _parse_and_dispatch_request(int p_client_id, ClientConnection &p_client);
	void _dispatch_request(int p_client_id, ClientConnection &p_client, Ref<HTTPRequestContext> p_context);
	bool _match_route(const String &p_pattern, const String &p_path, Dictionary &r_params) const;
	void _send_response(int p_client_id, ClientConnection &p_client, Ref<HTTPResponse> p_response);
	void _send_file_response(int p_client_id, ClientConnection &p_client, const String &p_file_path, int p_status);
	void _send_error(int p_client_id, ClientConnection &p_client, int p_code, const String &p_message);
	String _get_status_text(int p_code) const;
	String _get_mime_type(const String &p_extension) const;
	void _parse_query_params(const String &p_query, Dictionary &r_params) const;

	static void _server_thread_poll(void *p_data);

protected:
	static void _bind_methods();

public:
	static HTTPServer *get_singleton();

	// Server control
	Error listen(int p_port, const String &p_bind_address = "*", bool p_use_tls = false, const String &p_tls_key = "", const String &p_tls_cert = "");
	void stop();
	bool is_listening() const;
	int get_port() const;

	// Route registration
	void register_route(const String &p_method, const String &p_path, const Callable &p_callback);
	void unregister_route(const String &p_method, const String &p_path);
	void clear_routes();

	// Static file serving
	void set_static_directory(const String &p_path);
	String get_static_directory() const;
	void enable_directory_listing(bool p_enable);
	bool is_directory_listing_enabled() const;

	// SSE management
	Error send_sse_event(int p_connection_id, const String &p_event, const String &p_data);
	Error send_sse_data(int p_connection_id, const String &p_data);
	void close_sse_connection(int p_connection_id);
	Array get_active_sse_connections() const;

	// Configuration
	void set_cors_enabled(bool p_enabled);
	bool is_cors_enabled() const;
	void set_cors_origin(const String &p_origin);
	String get_cors_origin() const;
	void set_max_request_size(int p_size);
	int get_max_request_size() const;

	HTTPServer();
	~HTTPServer();
};
