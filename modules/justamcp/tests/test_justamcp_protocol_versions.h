/**************************************************************************/
/*  test_justamcp_protocol_versions.h                                     */
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

void test_justamcp_negotiate_protocol_versions();
void test_justamcp_protocol_version_setting_and_cli();
void test_justamcp_initialize_result_shape();
void test_justamcp_validate_protocol_header_rejects_unknown();
void test_justamcp_http_initialize_all_protocol_versions();
void test_justamcp_http_protocol_header_falls_back_to_session();
void test_justamcp_http_protocol_header_optional_for_older_versions();
void test_justamcp_batch_rejected_for_newer_protocols();
void test_justamcp_http_list_toolsets_smoke_per_strict_protocol();
void test_justamcp_json_rpc_rejects_null_id();

TEST_CASE("[Modules][JustAMCP] negotiate protocol versions") {
	test_justamcp_negotiate_protocol_versions();
}

TEST_CASE("[Modules][JustAMCP] protocol version setting and CLI override") {
	test_justamcp_protocol_version_setting_and_cli();
}

TEST_CASE("[Modules][JustAMCP] initialize result shape") {
	test_justamcp_initialize_result_shape();
}

TEST_CASE("[Modules][JustAMCP] validate protocol header rejects unknown") {
	test_justamcp_validate_protocol_header_rejects_unknown();
}

TEST_CASE("[Modules][JustAMCP] http initialize all protocol versions") {
	test_justamcp_http_initialize_all_protocol_versions();
}

TEST_CASE("[Modules][JustAMCP] http protocol header falls back to session") {
	test_justamcp_http_protocol_header_falls_back_to_session();
}

TEST_CASE("[Modules][JustAMCP] http protocol header optional for older versions") {
	test_justamcp_http_protocol_header_optional_for_older_versions();
}

TEST_CASE("[Modules][JustAMCP] batch rejected for newer protocols") {
	test_justamcp_batch_rejected_for_newer_protocols();
}

TEST_CASE("[Modules][JustAMCP] http list toolsets smoke per strict protocol") {
	test_justamcp_http_list_toolsets_smoke_per_strict_protocol();
}

TEST_CASE("[Modules][JustAMCP] json-rpc rejects null id") {
	test_justamcp_json_rpc_rejects_null_id();
}
