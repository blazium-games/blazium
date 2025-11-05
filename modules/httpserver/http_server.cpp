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
	ClassDB::bind_method(D_METHOD("send_sse_event", "connection_id", "event", "data"), &HTTPServer::send_sse_event);
	ClassDB::bind_method(D_METHOD("send_sse_data", "connection_id", "data"), &HTTPServer::send_sse_data);
	ClassDB::bind_method(D_METHOD("close_sse_connection", "connection_id"), &HTTPServer::close_sse_connection);
	ClassDB::bind_method(D_METHOD("get_active_sse_connections"), &HTTPServer::get_active_sse_connections);

	// Configuration
	ClassDB::bind_method(D_METHOD("set_cors_enabled", "enabled"), &HTTPServer::set_cors_enabled);
	ClassDB::bind_method(D_METHOD("is_cors_enabled"), &HTTPServer::is_cors_enabled);
	ClassDB::bind_method(D_METHOD("set_cors_origin", "origin"), &HTTPServer::set_cors_origin);
	ClassDB::bind_method(D_METHOD("get_cors_origin"), &HTTPServer::get_cors_origin);
	ClassDB::bind_method(D_METHOD("set_max_request_size", "size"), &HTTPServer::set_max_request_size);
	ClassDB::bind_method(D_METHOD("get_max_request_size"), &HTTPServer::get_max_request_size);

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
			clients[client_id] = client;
		}
	}

	// Poll existing clients
	List<int> clients_to_remove;
	for (KeyValue<int, ClientConnection> &E : clients) {
		_poll_client(E.key, E.value);

		// Check timeout (10 seconds for regular requests)
		if (!E.value.is_sse && OS::get_singleton()->get_ticks_usec() - E.value.time > 10000000) {
			clients_to_remove.push_back(E.key);
		}
	}

	// Remove timed out clients
	for (int client_id : clients_to_remove) {
		_clear_client(client_id);
	}
}

void HTTPServer::_poll_client(int p_client_id, ClientConnection &p_client) {
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

	// Check connection status
	if (p_client.tcp->get_status() != StreamPeerTCP::STATUS_CONNECTED) {
		_clear_client(p_client_id);
		return;
	}

	// SSE connections don't need to read requests
	if (p_client.is_sse) {
		return;
	}

	// Read request data
	while (true) {
		if (p_client.req_pos >= max_request_size) {
			_send_error(p_client_id, p_client, 413, "Request Entity Too Large");
			_clear_client(p_client_id);
			return;
		}

		int read = 0;
		Error err = p_client.peer->get_partial_data(&p_client.req_buf[p_client.req_pos], 1, read);

		if (err != OK) {
			_clear_client(p_client_id);
			return;
		}

		if (read != 1) {
			return; // No data available
		}

		p_client.req_pos += read;

		// Check for complete request (ends with \r\n\r\n)
		if (p_client.req_pos > 3) {
			char *r = (char *)p_client.req_buf;
			int l = p_client.req_pos - 1;
			if (r[l] == '\n' && r[l - 1] == '\r' && r[l - 2] == '\n' && r[l - 3] == '\r') {
				_parse_and_dispatch_request(p_client_id, p_client);
				return;
			}
		}
	}
}

void HTTPServer::_parse_and_dispatch_request(int p_client_id, ClientConnection &p_client) {
	Vector<String> lines = String((char *)p_client.req_buf, p_client.req_pos).split("\r\n");

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
	int body_start = 1;
	for (int i = 1; i < lines.size(); i++) {
		if (lines[i].is_empty()) {
			body_start = i + 1;
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
	if (body_start < lines.size()) {
		Vector<String> body_lines;
		for (int i = body_start; i < lines.size(); i++) {
			body_lines.push_back(lines[i]);
		}
		body = String("\r\n").join(body_lines);
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

		// Send SSE headers
		_send_response(p_client_id, p_client, response);

		// Create SSE connection object
		Ref<SSEConnection> sse_conn;
		sse_conn.instantiate();
		sse_conn->set_peer(p_client.peer);
		sse_conn->set_connection_id(sse_id);
		sse_conn->set_path(p_context->get_path());
		sse_connections[sse_id] = sse_conn;

		emit_signal("sse_connection_opened", sse_id, p_context->get_path(), p_context->get_headers());
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
		response_str += "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
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
	for (KeyValue<int, Ref<SSEConnection>> &E : sse_connections) {
		E.value->close_connection();
		emit_signal("sse_connection_closed", E.key);
	}
	sse_connections.clear();
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

Error HTTPServer::send_sse_event(int p_connection_id, const String &p_event, const String &p_data) {
	MutexLock lock(server_lock);

	if (!sse_connections.has(p_connection_id)) {
		return ERR_DOES_NOT_EXIST;
	}

	Ref<SSEConnection> conn = sse_connections[p_connection_id];
	Error err = conn->send_event(p_event, p_data);

	if (err != OK) {
		close_sse_connection(p_connection_id);
	}

	return err;
}

Error HTTPServer::send_sse_data(int p_connection_id, const String &p_data) {
	return send_sse_event(p_connection_id, "", p_data);
}

void HTTPServer::close_sse_connection(int p_connection_id) {
	MutexLock lock(server_lock);

	if (!sse_connections.has(p_connection_id)) {
		return;
	}

	Ref<SSEConnection> conn = sse_connections[p_connection_id];
	conn->close_connection();
	sse_connections.erase(p_connection_id);

	emit_signal("sse_connection_closed", p_connection_id);
}

Array HTTPServer::get_active_sse_connections() const {
	MutexLock lock(server_lock);

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

void HTTPServer::set_max_request_size(int p_size) {
	max_request_size = CLAMP(p_size, 1024, 1024 * 1024); // 1KB to 1MB
}

int HTTPServer::get_max_request_size() const {
	return max_request_size;
}

HTTPServer::HTTPServer() {
	singleton = this;
	server.instantiate();
	_init_mime_types();
}

HTTPServer::~HTTPServer() {
	stop();
	singleton = nullptr;
}
