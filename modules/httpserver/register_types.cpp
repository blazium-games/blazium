/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "http_request_context.h"
#include "http_response.h"
#include "http_server.h"
#include "sse_connection.h"

#include "core/config/engine.h"

static HTTPServer *httpserver_singleton = nullptr;

void initialize_httpserver_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		// Register classes
		GDREGISTER_CLASS(HTTPRequestContext);
		GDREGISTER_CLASS(HTTPResponse);
		GDREGISTER_CLASS(SSEConnection);
		GDREGISTER_CLASS(HTTPServer);

		// Create and register singleton
		httpserver_singleton = memnew(HTTPServer);
		Engine::get_singleton()->add_singleton(Engine::Singleton("HTTPServer", HTTPServer::get_singleton()));
	}
}

void uninitialize_httpserver_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		if (httpserver_singleton) {
			Engine::get_singleton()->remove_singleton("HTTPServer");
			memdelete(httpserver_singleton);
			httpserver_singleton = nullptr;
		}
	}
}
