/**************************************************************************/
/*  test_justamcp_json_rpc_transport.h                                    */
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

#include "tests/test_macros.h"

void test_justamcp_json_rpc_transport_invalid_json();
void test_justamcp_json_rpc_request_ids_equal();
void test_justamcp_tool_queue_full();
void test_justamcp_in_flight_cancel_flag_only();
void test_justamcp_json_rpc_tools_call_e2e();

TEST_CASE("[Modules][JustAMCP] json rpc tools call e2e") {
	test_justamcp_json_rpc_tools_call_e2e();
}

TEST_CASE("[Modules][JustAMCP] json rpc transport invalid json") {
	test_justamcp_json_rpc_transport_invalid_json();
}

TEST_CASE("[Modules][JustAMCP] json rpc request ids equal") {
	test_justamcp_json_rpc_request_ids_equal();
}

TEST_CASE("[Modules][JustAMCP] tool queue full") {
	test_justamcp_tool_queue_full();
}

TEST_CASE("[Modules][JustAMCP] in flight cancel flag only") {
	test_justamcp_in_flight_cancel_flag_only();
}
