/**************************************************************************/
/*  test_justamcp_non_object_json_400.cpp                                 */
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

#include "test_justamcp_non_object_json_400.h"

#include "test_justamcp_fixture.h"

#ifdef TESTS_ENABLED

#if defined(MODULE_HTTPSERVER_ENABLED)

#include "../justamcp_json_rpc_transport.h"
#include "../justamcp_server.h"

#include "tests/test_macros.h"

#include "modules/httpserver/http_response.h"

void test_justamcp_non_object_json_400() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();

	Ref<HTTPResponse> response;
	response.instantiate();
	const Dictionary result = JustAMCPJsonRpcTransport::handle_json_rpc(&server, "[1,2,3]", response);
	CHECK(result.has("error"));
	CHECK(int(result["error"].operator Dictionary().get("code", 0)) == -32600);
	CHECK(response->get_status() == 400);

	Ref<HTTPResponse> response2;
	response2.instantiate();
	const Dictionary result2 = JustAMCPJsonRpcTransport::handle_json_rpc(&server, "\"not-an-object\"", response2);
	CHECK(result2.has("error"));
	CHECK(int(result2["error"].operator Dictionary().get("code", 0)) == -32600);
	CHECK(response2->get_status() == 400);
}

#else
void test_justamcp_non_object_json_400() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}
#endif

#endif
