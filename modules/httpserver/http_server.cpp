/**************************************************************************/
/*  http_server.cpp                                                       */
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

#include "http_server.h"

#include "core/crypto/crypto.h"
#include "core/io/file_access.h"
#include "core/os/os.h"

HTTPServer *HTTPServer::singleton = nullptr;

void HTTPServer::_bind_methods() {
	// Server control
	ClassDB::bind_method(D_METHOD("listen", "port", "bind_address", "use_tls", "tls_key", "tls_cert"), &HTTPServer::listen, DEFVAL("*"), DEFVAL(false), DEFVAL(""), DEFVAL(""));
	ClassDB::bind_method(D_METHOD("stop"), &HTTPServer::stop);
	ClassDB::bind_method(D_METHOD("is_listening"), &HTTPServer::is_listening);
	ClassDB::bind_method(D_METHOD("get_port"), &HTTPServer::get_port);

	// Route registration
	ClassDB::bind_method(D_METHOD("register_route", "method", "path", "callback"), &HTTPServer::register_route);
	ClassDB::bind_method(D_METHOD("unregister_route", "method", "path"), &HTTPServer::unregister_route);
	ClassDB::bind_method(D_METHOD("clear_routes"), &HTTPServer::clear_routes);

	// Static file serving
	ClassDB::bind_method(D_METHOD("set_static_directory", "path"), &HTTPServer::set_static_directory);
	ClassDB::bind_method(D_METHOD("get_static_directory"), &HTTPServer::get_static_directory);
	ClassDB::bind_method(D_METHOD("enable_directory_listing", "enable"), &HTTPServer::enable_directory_listing);
	ClassDB::bind_method(D_METHOD("is_directory_listing_enabled"), &HTTPServer::is_directory_listing_enabled);

	// SSE management
	ClassDB::bind_method(D_METHOD("send_sse_event", "connection_id", "event", "data", "event_id"), &HTTPServer::send_sse_event, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("send_sse_data", "connection_id", "data"), &HTTPServer::send_sse_data);
	ClassDB::bind_method(D_METHOD("send_sse_retry", "connection_id", "retry_ms"), &HTTPServer::send_sse_retry);
	ClassDB::bind_method(D_METHOD("send_sse_comment", "connection_id", "comment"), &HTTPServer::send_sse_comment, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("close_sse_connection", "connection_id"), &HTTPServer::close_sse_connection);
	ClassDB::bind_method(D_METHOD("get_active_sse_connections"), &HTTPServer::get_active_sse_connections);

	// Configuration
	ClassDB::bind_method(D_METHOD("set_cors_enabled", "enabled"), &HTTPServer::set_cors_enabled);
	ClassDB::bind_method(D_METHOD("is_cors_enabled"), &HTTPServer::is_cors_enabled);
	ClassDB::bind_method(D_METHOD("set_cors_origin", "origin"), &HTTPServer::set_cors_origin);
	ClassDB::bind_method(D_METHOD("get_cors_origin"), &HTTPServer::get_cors_origin);
	ClassDB::bind_method(D_METHOD("set_max_request_size", "size"), &HTTPServer::set_max_request_size);
	ClassDB::bind_method(D_METHOD("get_max_request_size"), &HTTPServer::get_max_request_size);
	ClassDB::bind_method(D_METHOD("complete_response", "client_id", "response"), &HTTPServer::complete_response);

	// Signals
	ADD_SIGNAL(MethodInfo("sse_connection_opened", PropertyInfo(Variant::INT, "connection_id"), PropertyInfo(Variant::STRING, "path"), PropertyInfo(Variant::DICTIONARY, "headers")));
	ADD_SIGNAL(MethodInfo("sse_connection_closed", PropertyInfo(Variant::INT, "connection_id")));
	ADD_SIGNAL(MethodInfo("server_error", PropertyInfo(Variant::STRING, "error_message")));
}

HTTPServer *HTTPServer::get_singleton() {
	return singleton;
}

void HTTPServer::_init_mime_types() {
	// Common MIME types
	mimes["html"] = "text/html";
	mimes["htm"] = "text/html";
	mimes["css"] = "text/css";
	mimes["js"] = "application/javascript";
	mimes["json"] = "application/json";
	mimes["xml"] = "application/xml";
	mimes["txt"] = "text/plain";

	// Images
	mimes["png"] = "image/png";
	mimes["jpg"] = "image/jpeg";
	mimes["jpeg"] = "image/jpeg";
	mimes["gif"] = "image/gif";
	mimes["svg"] = "image/svg+xml";
	mimes["webp"] = "image/webp";
	mimes["ico"] = "image/x-icon";

	// Fonts
	mimes["woff"] = "font/woff";
	mimes["woff2"] = "font/woff2";
	mimes["ttf"] = "font/ttf";
	mimes["otf"] = "font/otf";

	// Binary
	mimes["wasm"] = "application/wasm";
	mimes["pck"] = "application/octet-stream";
	mimes["zip"] = "application/zip";
	mimes["pdf"] = "application/pdf";
}

void HTTPServer::_server_thread_poll(void *p_data) {
	HTTPServer *http_server = static_cast<HTTPServer *>(p_data);
	while (!http_server->server_quit.is_set()) {
		OS::get_singleton()->delay_usec(6900);
		{
			MutexLock lock(http_server->server_lock);
			http_server->_poll();
		}
	}
}

void HTTPServer::_clear_client(int p_client_id) {
	if (!clients.has(p_client_id)) {
		return;
	}

	ClientConnection &client = clients[p_client_id];

	// If this was an SSE connection, close it
	if (client.is_sse && client.sse_connection_id > 0) {
		close_sse_connection(client.sse_connection_id);
	}

	clients.erase(p_client_id);
}

void HTTPServer::_poll() {
	if (!server->is_listening()) {
		return;
	}

	// Accept new connections
	while (server->is_connection_available()) {
		if (!accept_another_client(clients.size())) {
			Ref<StreamPeerTCP> extra = server->take_connection();
			if (extra.is_valid()) {
				extra->disconnect_from_host();
			}
			break;
		}
		Ref<StreamPeerTCP> tcp = server->take_connection();
		if (tcp.is_valid()) {
			int client_id = next_client_id++;
			ClientConnection client = {};
			client.tcp = tcp;
			client.peer = tcp;
			client.time = OS::get_singleton()->get_ticks_usec();
			client.req_pos = 0;
			client.is_sse = false;
			client.sse_connection_id = 0;
			client.headers_parsed = false;
			client.header_scan = 3;
			client.header_length = 0;
			client.body_length = 0;
			clients[client_id] = client;
		}
	}

	// Poll existing clients
	List<int> clients_to_remove;
	for (KeyValue<int, ClientConnection> &E : clients) {
		_poll_client(E.key, E.value);

		// Check timeout (10 seconds for regular requests). Held async clients and SSE wait longer.
		if (!E.value.is_sse && !E.value.is_held && OS::get_singleton()->get_ticks_usec() - E.value.time > 10000000) {
			clients_to_remove.push_back(E.key);
		}
	}

	// Remove timed out clients
	for (int client_id : clients_to_remove) {
		_clear_client(client_id);
	}

	if (OS::get_singleton()) {
		_pump_sse_keepalives(OS::get_singleton()->get_ticks_usec());
	}
}

void HTTPServer::_pump_sse_keepalives(uint64_t p_now_usec) {
	// Stay under the ~30s idle reset clients see on subscriptions/listen.
	const uint64_t interval_usec = 15000000;
	for (KeyValue<int, ClientConnection> &E : clients) {
		ClientConnection &client = E.value;
		if (!client.is_sse || client.sse_connection_id <= 0) {
			continue;
		}
		if (client.last_sse_keepalive_usec != 0 && p_now_usec - client.last_sse_keepalive_usec < interval_usec) {
			continue;
		}
		send_sse_comment(client.sse_connection_id, "keepalive");
		client.last_sse_keepalive_usec = p_now_usec;
#ifdef TESTS_ENABLED
		test_sse_keepalive_sends++;
#endif
	}
}

void HTTPServer::_poll_client(int p_client_id, ClientConnection &p_client) {
	if (p_client.tcp.is_null()) {
		_clear_client(p_client_id);
		return;
	}

	// Handle TLS handshake if needed
	if (use_tls && p_client.tls.is_null() && p_client.tcp->get_status() == StreamPeerTCP::STATUS_CONNECTED) {
		p_client.tls = Ref<StreamPeerTLS>(StreamPeerTLS::create());
		p_client.peer = p_client.tls;
		if (p_client.tls->accept_stream(p_client.tcp, TLSOptions::server(key, cert)) != OK) {
			_clear_client(p_client_id);
			return;
		}
	}

	if (use_tls && p_client.tls.is_valid()) {
		p_client.tls->poll();
		if (p_client.tls->get_status() == StreamPeerTLS::STATUS_HANDSHAKING) {
			return;
		}
		if (p_client.tls->get_status() != StreamPeerTLS::STATUS_CONNECTED) {
			_clear_client(p_client_id);
			return;
		}
	}

	// SSE sockets are otherwise never read. Poll so a peer FIN is observed
	// instead of leaving the connection until a later write resets it.
	if (p_client.is_sse) {
		p_client.tcp->poll();
	}

	// Check connection status
	if (p_client.tcp->get_status() != StreamPeerTCP::STATUS_CONNECTED) {
		_clear_client(p_client_id);
		return;
	}

	// SSE connections don't need to read requests
	if (p_client.is_sse || p_client.is_held) {
		return;
	}

	// Read request data
	while (true) {
		if (p_client.req_pos >= max_request_size) {
			_send_error(p_client_id, p_client, 413, "Request Entity Too Large");
			_clear_client(p_client_id);
			return;
		}

		// Ensure buffer is large enough
		if (p_client.req_buf.size() <= p_client.req_pos) {
			p_client.req_buf.resize(MAX(p_client.req_buf.size() * 2, 8192));
		}

		int read = 0;
		Error err = p_client.peer->get_partial_data(&p_client.req_buf.ptrw()[p_client.req_pos], p_client.req_buf.size() - p_client.req_pos, read);

		if (err != OK) {
			_clear_client(p_client_id);
			return;
		}

		if (read == 0) {
			return; // No data available
		}

		p_client.req_pos += read;

		if (!p_client.headers_parsed) {
			const int found = find_header_terminator(p_client.req_buf.ptr(), p_client.req_pos, p_client.header_scan, MAX_HEADER_BYTES);
			if (found == -2) {
				_send_error(p_client_id, p_client, 431, "Request Header Fields Too Large");
				_clear_client(p_client_id);
				return;
			}
			if (found >= 0) {
				p_client.headers_parsed = true;
				p_client.header_length = found;

				String header_str = String((const char *)p_client.req_buf.ptr(), p_client.header_length);
				Vector<String> lines = header_str.split("\r\n");
				bool saw_content_length = false;
				for (int j = 1; j < lines.size(); j++) {
					if (lines[j].to_lower().begins_with("content-length:")) {
						if (saw_content_length) {
							_send_error(p_client_id, p_client, 400, "Bad Request");
							_clear_client(p_client_id);
							return;
						}
						saw_content_length = true;
						int parsed_length = 0;
						const int status = accept_content_length(lines[j].substr(15), p_client.header_length, max_request_size, parsed_length);
						if (status != 0) {
							_send_error(p_client_id, p_client, status, status == 413 ? "Request Entity Too Large" : "Bad Request");
							_clear_client(p_client_id);
							return;
						}
						p_client.body_length = parsed_length;
					}
				}
			} else {
				p_client.header_scan = MAX(3, p_client.req_pos - 3);
			}
		}

		if (p_client.headers_parsed) {
			if (request_body_ready(p_client.req_pos, p_client.header_length, p_client.body_length)) {
				_parse_and_dispatch_request(p_client_id, p_client);
				return;
			}
		}
	}
}

void HTTPServer::_parse_and_dispatch_request(int p_client_id, ClientConnection &p_client) {
	String header_str = String((const char *)p_client.req_buf.ptr(), p_client.header_length);
	Vector<String> lines = header_str.split("\r\n");

	if (lines.size() < 2) {
		_send_error(p_client_id, p_client, 400, "Bad Request");
		_clear_client(p_client_id);
		return;
	}

	// Parse request line
	Vector<String> request_line = lines[0].split(" ", false);
	if (request_line.size() < 3) {
		_send_error(p_client_id, p_client, 400, "Bad Request");
		_clear_client(p_client_id);
		return;
	}

	String method = request_line[0];
	String raw_path = request_line[1];
	String protocol = request_line[2];

	// Parse path and query
	String path = raw_path;
	Dictionary query_params;
	int query_index = raw_path.find_char('?');
	if (query_index != -1) {
		path = raw_path.substr(0, query_index);
		String query = raw_path.substr(query_index + 1);
		_parse_query_params(query, query_params);
	}

	// Parse headers
	Dictionary headers;
	for (int i = 1; i < lines.size(); i++) {
		if (lines[i].is_empty()) {
			break;
		}

		int colon = lines[i].find_char(':');
		if (colon == -1) {
			continue;
		}

		String header_key = lines[i].substr(0, colon).strip_edges().to_lower();
		String value = lines[i].substr(colon + 1).strip_edges();
		headers[header_key] = value;
	}

	// Parse body (if any)
	String body;
	if (p_client.body_length > 0) {
		body = String::utf8((const char *)&p_client.req_buf.ptr()[p_client.header_length], p_client.body_length);
	}

	// Create request context
	Ref<HTTPRequestContext> context;
	context.instantiate();
	context->set_method(method);
	context->set_path(path);
	context->set_raw_path(raw_path);
	context->set_query_params(query_params);
	context->set_headers(headers);
	context->set_body(body);
	context->set_client_ip(p_client.tcp->get_connected_host());
	context->set_client_port(p_client.tcp->get_connected_port());
	context->set_client_id(p_client_id);

	_dispatch_request(p_client_id, p_client, context);
}

bool HTTPServer::_match_route(const String &p_pattern, const String &p_path, Dictionary &r_params) const {
	Vector<String> pattern_parts = p_pattern.split("/");
	Vector<String> path_parts = p_path.split("/");

	if (pattern_parts.size() != path_parts.size()) {
		return false;
	}

	for (int i = 0; i < pattern_parts.size(); i++) {
		if (pattern_parts[i].begins_with("{") && pattern_parts[i].ends_with("}")) {
			// Extract parameter name from {param_name}
			String param_name = pattern_parts[i].substr(1, pattern_parts[i].length() - 2);
			r_params[param_name] = path_parts[i];
		} else if (pattern_parts[i] != path_parts[i]) {
			return false;
		}
	}

	return true;
}

void HTTPServer::_dispatch_request(int p_client_id, ClientConnection &p_client, Ref<HTTPRequestContext> p_context) {
	// Debug print for all incoming raw requests
	print_line("HTTPServer Trace: " + p_context->get_method() + " " + p_context->get_raw_path());

	// Find matching route
	Route *matched_route = nullptr;
	Dictionary path_params;

	for (Route &route : routes) {
		if (route.method != p_context->get_method()) {
			continue;
		}

		if (_match_route(route.pattern, p_context->get_path(), path_params)) {
			matched_route = &route;
			break;
		}
	}

	if (matched_route == nullptr) {
		_send_error(p_client_id, p_client, 404, "Not Found");
		_clear_client(p_client_id);
		return;
	}

	// Set path parameters
	p_context->set_path_params(path_params);

	// Create response object
	Ref<HTTPResponse> response;
	response.instantiate();

	// Call route handler
	Callable callback = matched_route->callback;
	Variant context_var = Variant(p_context);
	Variant response_var = Variant(response);
	const Variant *args[2] = { &context_var, &response_var };
	Callable::CallError call_error;
	Variant result;
	callback.callp(args, 2, result, call_error);

	if (call_error.error != Callable::CallError::CALL_OK) {
		ERR_PRINT("Failed to call route handler for " + p_context->get_method() + " " + p_context->get_path());
		_send_error(p_client_id, p_client, 500, "Internal Server Error");
		_clear_client(p_client_id);
		return;
	}

	// Handle SSE response
	if (response->is_sse_response()) {
		int sse_id = next_connection_id++;
		p_client.is_sse = true;
		p_client.sse_connection_id = sse_id;
		p_client.last_sse_keepalive_usec = OS::get_singleton() ? OS::get_singleton()->get_ticks_usec() : 0;

		// Send SSE headers
		_send_response(p_client_id, p_client, response);

		// Create SSE connection object
		Ref<SSEConnection> sse_conn;
		sse_conn.instantiate();
		sse_conn->set_peer(p_client.peer);
		sse_conn->set_connection_id(sse_id);
		sse_conn->set_path(p_context->get_path());

		{
			MutexLock sse_lk(sse_lock);
			sse_connections[sse_id] = sse_conn;
		}

		emit_signal("sse_connection_opened", sse_id, p_context->get_path(), p_context->get_headers());
		return;
	}

	// Hold client for async completion (JSON POST offload).
	if (response->is_held()) {
		response->set_held_client_id(p_client_id);
		p_client.is_held = true;
		return;
	}

	// Send normal response
	_send_response(p_client_id, p_client, response);
	_clear_client(p_client_id);
}

void HTTPServer::_send_response(int p_client_id, ClientConnection &p_client, Ref<HTTPResponse> p_response) {
	if (p_response.is_null()) {
		_send_error(p_client_id, p_client, 500, "Internal Server Error");
		return;
	}

	// Handle file response
	if (p_response->is_file()) {
		_send_file_response(p_client_id, p_client, p_response->get_file_path(), p_response->get_status());
		return;
	}

	// Build response string
	String response_str = "HTTP/1.1 " + itos(p_response->get_status()) + " " + _get_status_text(p_response->get_status()) + "\r\n";

	// Add CORS headers if enabled
	if (cors_enabled) {
		response_str += "Access-Control-Allow-Origin: " + cors_origin + "\r\n";
		response_str += "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, PATCH, OPTIONS, HEAD\r\n";
		response_str += "Access-Control-Allow-Headers: Content-Type, Authorization, MCP-Session-Id, MCP-Protocol-Version, Last-Event-ID, Accept\r\n";
	}

	// Add custom headers
	Dictionary headers = p_response->get_headers();
	Array header_keys = headers.keys();
	for (int i = 0; i < header_keys.size(); i++) {
		String header_key = header_keys[i];
		String value = headers[header_key];
		response_str += header_key + ": " + value + "\r\n";
	}

	// For non-SSE responses, add Content-Length and close connection
	if (!p_response->is_sse_response()) {
		String body = p_response->get_body();
		CharString body_utf8 = body.utf8();
		response_str += "Content-Length: " + itos(body_utf8.length()) + "\r\n";
		response_str += "Connection: close\r\n";
		response_str += "\r\n";
		response_str += body;
	} else {
		response_str += "\r\n";
	}

	// Send response
	CharString cs = response_str.utf8();
	Error err = p_client.peer->put_data((const uint8_t *)cs.get_data(), cs.size() - 1);

	if (err != OK) {
		ERR_PRINT("Failed to send response");
	}

	p_response->mark_sent();
}

void HTTPServer::_send_file_response(int p_client_id, ClientConnection &p_client, const String &p_file_path, int p_status) {
	Ref<FileAccess> file = FileAccess::open(p_file_path, FileAccess::READ);
	if (file.is_null()) {
		_send_error(p_client_id, p_client, 404, "File Not Found");
		return;
	}

	// Get MIME type from extension
	String extension = p_file_path.get_extension();
	String content_type = _get_mime_type(extension);

	// Build response headers
	String response_str = "HTTP/1.1 " + itos(p_status) + " " + _get_status_text(p_status) + "\r\n";
	response_str += "Content-Type: " + content_type + "\r\n";
	response_str += "Content-Length: " + itos(file->get_length()) + "\r\n";
	response_str += "Connection: close\r\n";

	if (cors_enabled) {
		response_str += "Access-Control-Allow-Origin: " + cors_origin + "\r\n";
	}

	response_str += "\r\n";

	// Send headers
	CharString cs = response_str.utf8();
	Error err = p_client.peer->put_data((const uint8_t *)cs.get_data(), cs.size() - 1);
	if (err != OK) {
		ERR_PRINT("Failed to send file response headers");
		return;
	}

	// Send file content
	while (true) {
		uint8_t bytes[4096];
		uint64_t read = file->get_buffer(bytes, 4096);
		if (read == 0) {
			break;
		}
		err = p_client.peer->put_data(bytes, read);
		if (err != OK) {
			ERR_PRINT("Failed to send file content");
			return;
		}
	}
}

void HTTPServer::_send_error(int p_client_id, ClientConnection &p_client, int p_code, const String &p_message) {
	String body = "<html><body><h1>" + itos(p_code) + " " + p_message + "</h1></body></html>";
	String response = "HTTP/1.1 " + itos(p_code) + " " + p_message + "\r\n";
	response += "Content-Type: text/html\r\n";
	response += "Content-Length: " + itos(body.utf8().length()) + "\r\n";
	response += "Connection: close\r\n";
	response += "\r\n";
	response += body;

	CharString cs = response.utf8();
	p_client.peer->put_data((const uint8_t *)cs.get_data(), cs.size() - 1);
}

String HTTPServer::_get_status_text(int p_code) const {
	switch (p_code) {
		case 200:
			return "OK";
		case 201:
			return "Created";
		case 202:
			return "Accepted";
		case 204:
			return "No Content";
		case 400:
			return "Bad Request";
		case 401:
			return "Unauthorized";
		case 403:
			return "Forbidden";
		case 404:
			return "Not Found";
		case 405:
			return "Method Not Allowed";
		case 413:
			return "Request Entity Too Large";
		case 431:
			return "Request Header Fields Too Large";
		case 500:
			return "Internal Server Error";
		case 501:
			return "Not Implemented";
		case 503:
			return "Service Unavailable";
		default:
			return "Unknown";
	}
}

String HTTPServer::_get_mime_type(const String &p_extension) const {
	if (mimes.has(p_extension)) {
		return mimes[p_extension];
	}
	return "application/octet-stream";
}

void HTTPServer::_parse_query_params(const String &p_query, Dictionary &r_params) const {
	Vector<String> pairs = p_query.split("&");
	for (int i = 0; i < pairs.size(); i++) {
		Vector<String> kv = pairs[i].split("=");
		if (kv.size() == 2) {
			String param_key = kv[0].uri_decode();
			String value = kv[1].uri_decode();
			r_params[param_key] = value;
		} else if (kv.size() == 1) {
			String param_key = kv[0].uri_decode();
			r_params[param_key] = "";
		}
	}
}

// Public API

Error HTTPServer::listen(int p_port, const String &p_bind_address, bool p_use_tls, const String &p_tls_key, const String &p_tls_cert) {
	MutexLock lock(server_lock);

	if (server->is_listening()) {
		return ERR_ALREADY_IN_USE;
	}

	use_tls = p_use_tls;

	if (use_tls) {
		Ref<Crypto> crypto = Crypto::create();
		if (crypto.is_null()) {
			return ERR_UNAVAILABLE;
		}

		if (!p_tls_key.is_empty() && !p_tls_cert.is_empty()) {
			key = Ref<CryptoKey>(CryptoKey::create());
			Error err = key->load(p_tls_key);
			ERR_FAIL_COND_V(err != OK, err);

			cert = Ref<X509Certificate>(X509Certificate::create());
			err = cert->load(p_tls_cert);
			ERR_FAIL_COND_V(err != OK, err);
		} else {
			ERR_PRINT("TLS enabled but no key/cert provided");
			return ERR_INVALID_PARAMETER;
		}
	}

	IPAddress bind_ip;
	if (p_bind_address == "*") {
		bind_ip = IPAddress("0.0.0.0");
	} else {
		bind_ip = IPAddress(p_bind_address);
	}

	Error err = server->listen(p_port, bind_ip);
	if (err == OK) {
		server_quit.clear();
		server_thread.start(_server_thread_poll, this);
	}

	return err;
}

void HTTPServer::stop() {
	server_quit.set();
	if (server_thread.is_started()) {
		server_thread.wait_to_finish();
	}

	MutexLock lock(server_lock);
	if (server.is_valid()) {
		server->stop();
	}

	// Close all client connections
	clients.clear();

	// Close all SSE connections
	{
		MutexLock sse_lk(sse_lock);
		for (KeyValue<int, Ref<SSEConnection>> &E : sse_connections) {
			E.value->close_connection();
			emit_signal("sse_connection_closed", E.key);
		}
		sse_connections.clear();
	}
}

bool HTTPServer::is_listening() const {
	MutexLock lock(server_lock);
	return server->is_listening();
}

int HTTPServer::get_port() const {
	MutexLock lock(server_lock);
	return server->get_local_port();
}

void HTTPServer::register_route(const String &p_method, const String &p_path, const Callable &p_callback) {
	MutexLock lock(server_lock);

	Route route;
	route.method = p_method.to_upper();
	route.pattern = p_path;
	route.callback = p_callback;
	routes.push_back(route);
}

void HTTPServer::unregister_route(const String &p_method, const String &p_path) {
	MutexLock lock(server_lock);

	String method_upper = p_method.to_upper();
	for (List<Route>::Element *E = routes.front(); E; E = E->next()) {
		if (E->get().method == method_upper && E->get().pattern == p_path) {
			routes.erase(E);
			return;
		}
	}
}

void HTTPServer::clear_routes() {
	MutexLock lock(server_lock);
	routes.clear();
}

void HTTPServer::set_static_directory(const String &p_path) {
	static_directory = p_path;
}

String HTTPServer::get_static_directory() const {
	return static_directory;
}

void HTTPServer::enable_directory_listing(bool p_enable) {
	directory_listing_enabled = p_enable;
}

bool HTTPServer::is_directory_listing_enabled() const {
	return directory_listing_enabled;
}

Error HTTPServer::send_sse_event(int p_connection_id, const String &p_event, const String &p_data, const String &p_event_id) {
	MutexLock lock(sse_lock);

	if (!sse_connections.has(p_connection_id)) {
		return ERR_DOES_NOT_EXIST;
	}

	Ref<SSEConnection> conn = sse_connections[p_connection_id];
	Error err = conn->send_event(p_event, p_data, p_event_id);

	if (err != OK) {
		close_sse_connection(p_connection_id);
	}

	return err;
}

Error HTTPServer::send_sse_data(int p_connection_id, const String &p_data) {
	return send_sse_event(p_connection_id, "", p_data);
}

Error HTTPServer::send_sse_retry(int p_connection_id, int p_retry_ms) {
	MutexLock lock(sse_lock);

	if (!sse_connections.has(p_connection_id)) {
		return ERR_DOES_NOT_EXIST;
	}

	Ref<SSEConnection> conn = sse_connections[p_connection_id];
	String message = "retry: " + itos(MAX(0, p_retry_ms)) + "\r\n\r\n";
	CharString cs = message.utf8();
	Error err = conn->get_peer()->put_data((const uint8_t *)cs.get_data(), cs.size() - 1);
	if (err != OK) {
		close_sse_connection(p_connection_id);
	}
	return err;
}

Error HTTPServer::send_sse_comment(int p_connection_id, const String &p_comment) {
	MutexLock lock(sse_lock);

	if (!sse_connections.has(p_connection_id)) {
		return ERR_DOES_NOT_EXIST;
	}

	Ref<SSEConnection> conn = sse_connections[p_connection_id];
	String message = ":";
	if (!p_comment.is_empty()) {
		message += " " + p_comment;
	}
	message += "\r\n\r\n";
	CharString cs = message.utf8();
	Error err = conn->get_peer()->put_data((const uint8_t *)cs.get_data(), cs.size() - 1);
	if (err != OK) {
		close_sse_connection(p_connection_id);
	}
	return err;
}

void HTTPServer::close_sse_connection(int p_connection_id) {
	MutexLock lock(sse_lock);

	if (!sse_connections.has(p_connection_id)) {
		return;
	}

	Ref<SSEConnection> conn = sse_connections[p_connection_id];
	conn->close_connection();
	sse_connections.erase(p_connection_id);

	emit_signal("sse_connection_closed", p_connection_id);
}

Array HTTPServer::get_active_sse_connections() const {
	MutexLock lock(sse_lock);

	Array result;
	for (const KeyValue<int, Ref<SSEConnection>> &E : sse_connections) {
		result.push_back(E.key);
	}
	return result;
}

void HTTPServer::set_cors_enabled(bool p_enabled) {
	cors_enabled = p_enabled;
}

bool HTTPServer::is_cors_enabled() const {
	return cors_enabled;
}

void HTTPServer::set_cors_origin(const String &p_origin) {
	cors_origin = p_origin;
}

String HTTPServer::get_cors_origin() const {
	return cors_origin;
}

bool HTTPServer::accept_another_client(int p_current_clients) {
	return p_current_clients >= 0 && p_current_clients < MAX_CONCURRENT_CLIENTS;
}

int HTTPServer::accept_content_length(const String &p_value, int p_header_length, int p_max_request_size, int &r_length) {
	r_length = 0;
	const String value = p_value.strip_edges();
	if (value.is_empty() || p_header_length < 0 || p_max_request_size <= 0) {
		return 400;
	}

	// RFC 9110: Content-Length is 1*DIGIT. Signs and trailing junk are not a length.
	int64_t parsed = 0;
	for (int i = 0; i < value.length(); i++) {
		const char32_t c = value[i];
		if (c < '0' || c > '9') {
			return 400;
		}
		const int digit = int(c - '0');
		if (parsed > (INT64_MAX - digit) / 10) {
			return 413;
		}
		parsed = parsed * 10 + digit;
	}

	const int64_t room = (int64_t)p_max_request_size - (int64_t)p_header_length;
	if (parsed > room || parsed > INT32_MAX) {
		return 413;
	}
	r_length = (int)parsed;
	return 0;
}

bool HTTPServer::request_body_ready(int p_received, int p_header_length, int p_body_length) {
	if (p_body_length < 0 || p_header_length < 0 || p_received < p_header_length) {
		return false;
	}
	return p_received - p_header_length >= p_body_length;
}

int HTTPServer::find_header_terminator(const uint8_t *p_data, int p_len, int p_scan_from, int p_max_header_bytes) {
	if (p_data == nullptr || p_len <= 0 || p_max_header_bytes < 4) {
		return p_len >= p_max_header_bytes ? -2 : -1;
	}
	const int limit = MIN(p_len, p_max_header_bytes);
	int start = p_scan_from < 3 ? 3 : p_scan_from;
	if (start > limit) {
		start = limit;
	}
	for (int i = start; i < limit; i++) {
		if (p_data[i] == '\n' && p_data[i - 1] == '\r' && p_data[i - 2] == '\n' && p_data[i - 3] == '\r') {
			return i + 1;
		}
	}
	if (p_len >= p_max_header_bytes) {
		return -2;
	}
	return -1;
}

void HTTPServer::set_max_request_size(int p_size) {
	max_request_size = CLAMP(p_size, 1024, 1024 * 1024); // 1KB to 1MB
}

int HTTPServer::get_max_request_size() const {
	return max_request_size;
}

Error HTTPServer::complete_response(int p_client_id, const Ref<HTTPResponse> &p_response) {
	MutexLock lock(server_lock);
	if (!clients.has(p_client_id)) {
		return ERR_DOES_NOT_EXIST;
	}
	ClientConnection &client = clients[p_client_id];
	if (!client.is_held) {
		return ERR_INVALID_PARAMETER;
	}
	Ref<HTTPResponse> response = p_response;
	if (response.is_null()) {
		_send_error(p_client_id, client, 500, "Internal Server Error");
		_clear_client(p_client_id);
		return ERR_INVALID_PARAMETER;
	}
	client.is_held = false;
	_send_response(p_client_id, client, response);
	_clear_client(p_client_id);
	return OK;
}

#ifdef TESTS_ENABLED
int HTTPServer::test_pump_sse_keepalive_once(int p_connection_id, uint64_t p_last_keepalive_usec, uint64_t p_now_usec) {
	MutexLock lock(server_lock);
	ClientConnection client;
	client.is_sse = true;
	client.sse_connection_id = p_connection_id;
	client.last_sse_keepalive_usec = p_last_keepalive_usec;
	const int client_id = next_client_id++;
	clients[client_id] = client;
	const int before = test_sse_keepalive_sends;
	_pump_sse_keepalives(p_now_usec);
	const int sent = test_sse_keepalive_sends - before;
	clients.erase(client_id);
	return sent;
}
#endif

HTTPServer::HTTPServer() {
	singleton = this;
	server.instantiate();
	_init_mime_types();
}

HTTPServer::~HTTPServer() {
	stop();
	singleton = nullptr;
}
