/**************************************************************************/
/*  dddbrowser_preview_server.cpp                                         */
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

#include "core/object/class_db.h"
#include "core/object/callable_mp.h"
#include "dddbrowser_preview_server.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "modules/httpserver/http_request_context.h"
#include "modules/httpserver/http_response.h"
#include "modules/httpserver/http_server.h"

void DDDBrowserPreviewServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start", "root_dir", "port"), &DDDBrowserPreviewServer::start, DEFVAL(8081));
	ClassDB::bind_method(D_METHOD("stop"), &DDDBrowserPreviewServer::stop);
	ClassDB::bind_method(D_METHOD("is_running"), &DDDBrowserPreviewServer::is_running);
	ClassDB::bind_method(D_METHOD("get_port"), &DDDBrowserPreviewServer::get_port);
	ClassDB::bind_method(D_METHOD("get_root_dir"), &DDDBrowserPreviewServer::get_root_dir);
	ClassDB::bind_method(D_METHOD("get_index_url"), &DDDBrowserPreviewServer::get_index_url);
}

void DDDBrowserPreviewServer::_serve_file(const Ref<HTTPRequestContext> &p_req, const Ref<HTTPResponse> &p_res) {
	String path = p_req->get_path();
	if (path.is_empty() || path == "/") {
		path = "/index.html";
	}
	while (path.begins_with("/")) {
		path = path.substr(1);
	}
	path = path.replace("..", "");
	String full = root_dir.path_join(path);
	if (!FileAccess::exists(full)) {
		p_res->set_status(404);
		p_res->set_body("Not Found");
		return;
	}
	p_res->set_file(full);
}

Error DDDBrowserPreviewServer::start(const String &p_root_dir, int p_port) {
	HTTPServer *server = HTTPServer::get_singleton();
	ERR_FAIL_NULL_V(server, ERR_UNAVAILABLE);
	if (server->is_listening()) {
		server->stop();
		server->clear_routes();
	}
	root_dir = p_root_dir.replace("\\", "/");
	port = p_port;
	server->set_cors_enabled(true);
	server->set_static_directory(root_dir);

	Callable cb = callable_mp(this, &DDDBrowserPreviewServer::_serve_file);
	server->register_route("GET", "/", cb);
	server->register_route("GET", "/index.html", cb);
	server->register_route("GET", "/scene.json", cb);
	server->register_route("GET", "/meshes/{name}", cb);
	server->register_route("GET", "/scripts/{name}", cb);
	server->register_route("GET", "/audio/{name}", cb);
	server->register_route("GET", "/fonts/{name}", cb);
	server->register_route("GET", "/textures/{name}", cb);
	server->register_route("GET", "/{name}", cb);

	Error err = server->listen(port, "127.0.0.1", false, "", "");
	listening = err == OK;
	return err;
}

void DDDBrowserPreviewServer::stop() {
	HTTPServer *server = HTTPServer::get_singleton();
	if (server && server->is_listening()) {
		server->stop();
		server->clear_routes();
	}
	listening = false;
}

bool DDDBrowserPreviewServer::is_running() const {
	HTTPServer *server = HTTPServer::get_singleton();
	return listening && server && server->is_listening();
}

int DDDBrowserPreviewServer::get_port() const {
	return port;
}
String DDDBrowserPreviewServer::get_root_dir() const {
	return root_dir;
}

String DDDBrowserPreviewServer::get_index_url() const {
	return vformat("http://127.0.0.1:%d/index.html", port);
}
