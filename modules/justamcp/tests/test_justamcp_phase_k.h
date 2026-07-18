/**************************************************************************/
/*  test_justamcp_phase_k.h                                               */
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

#ifdef TESTS_ENABLED

#include "tests/test_macros.h"

void test_justamcp_worker_safe_registry_locked();
void test_justamcp_main_only_not_scheduled_on_pool();
void test_justamcp_parallel_readonly_overlap();
void test_justamcp_cancel_scoped_multi_inflight();
void test_justamcp_legacy_message_rpc_deferred();
void test_justamcp_json_post_hold_client_completes();
void test_justamcp_tools_list_does_not_block_accept();

TEST_CASE("[Modules][JustAMCP] worker safe registry locked") {
	test_justamcp_worker_safe_registry_locked();
}

TEST_CASE("[Modules][JustAMCP] main only not scheduled on pool") {
	test_justamcp_main_only_not_scheduled_on_pool();
}

TEST_CASE("[Modules][JustAMCP] parallel readonly overlap") {
	test_justamcp_parallel_readonly_overlap();
}

TEST_CASE("[Modules][JustAMCP] cancel scoped multi inflight") {
	test_justamcp_cancel_scoped_multi_inflight();
}

TEST_CASE("[Modules][JustAMCP] legacy message rpc deferred") {
	test_justamcp_legacy_message_rpc_deferred();
}

TEST_CASE("[Modules][JustAMCP] json post hold client completes") {
	test_justamcp_json_post_hold_client_completes();
}

TEST_CASE("[Modules][JustAMCP] tools list does not block accept") {
	test_justamcp_tools_list_does_not_block_accept();
}

#endif
