/**************************************************************************/
/*  test_justamcp_server_queue_lifecycle.h                                */
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

void test_justamcp_server_queue_lifecycle();
void test_justamcp_category_dispatch_coverage();
void test_justamcp_server_queue_wait_completion();
void test_justamcp_server_queue_wait_timeout();
void test_justamcp_server_queue_ordering_signal();
void test_justamcp_server_queue_cancel_flag();
void test_justamcp_category_dispatch_fallthrough();
void test_justamcp_tool_call_queue_e2e();
void test_justamcp_logs_read_full_path();
void test_justamcp_task_dispatch_failure_cleanup();
void test_justamcp_parallel_readonly_lane();
void test_justamcp_request_id_lane_context();
void test_justamcp_dual_lane_processing_flag();
void test_justamcp_tool_context_tls_stack();
void test_justamcp_readonly_task_cancel();
void test_justamcp_semantic_index_stats();
void test_justamcp_send_tool_result_lookup();
void test_justamcp_readonly_tool_registration();
void test_justamcp_queued_cancel_no_double_delete();
void test_justamcp_server_stop_inflight_cancel();
void test_justamcp_pending_in_flight_queue_split();
void test_justamcp_task_dispatch_cancel_race();
void test_justamcp_task_augmented_queued_cancel_unblocks();
void test_justamcp_task_post_create_cancel();
void test_mcp_tool_queue_skip_cancelled_pick();
void test_justamcp_post_create_cancel_entry_lifetime();

TEST_CASE("[Modules][JustAMCP] mcp tool queue skip cancelled pick") {
	test_mcp_tool_queue_skip_cancelled_pick();
}

TEST_CASE("[Modules][JustAMCP] post-create cancel entry lifetime") {
	test_justamcp_post_create_cancel_entry_lifetime();
}

TEST_CASE("[Modules][JustAMCP] server queue lifecycle") {
	test_justamcp_server_queue_lifecycle();
}

TEST_CASE("[Modules][JustAMCP] category dispatch coverage") {
	test_justamcp_category_dispatch_coverage();
}

TEST_CASE("[Modules][JustAMCP] server queue wait completion") {
	test_justamcp_server_queue_wait_completion();
}

TEST_CASE("[Modules][JustAMCP] server queue wait timeout") {
	test_justamcp_server_queue_wait_timeout();
}

TEST_CASE("[Modules][JustAMCP] server queue ordering signal") {
	test_justamcp_server_queue_ordering_signal();
}

TEST_CASE("[Modules][JustAMCP] server queue cancel flag") {
	test_justamcp_server_queue_cancel_flag();
}

TEST_CASE("[Modules][JustAMCP] category dispatch fallthrough") {
	test_justamcp_category_dispatch_fallthrough();
}

TEST_CASE("[Modules][JustAMCP] tool call queue e2e") {
	test_justamcp_tool_call_queue_e2e();
}

TEST_CASE("[Modules][JustAMCP] logs read full path") {
	test_justamcp_logs_read_full_path();
}

TEST_CASE("[Modules][JustAMCP] task dispatch failure cleanup") {
	test_justamcp_task_dispatch_failure_cleanup();
}

TEST_CASE("[Modules][JustAMCP] parallel readonly lane") {
	test_justamcp_parallel_readonly_lane();
}

TEST_CASE("[Modules][JustAMCP] request id lane context") {
	test_justamcp_request_id_lane_context();
}

TEST_CASE("[Modules][JustAMCP] dual lane processing flag") {
	test_justamcp_dual_lane_processing_flag();
}

TEST_CASE("[Modules][JustAMCP] tool context tls stack") {
	test_justamcp_tool_context_tls_stack();
}

TEST_CASE("[Modules][JustAMCP] readonly task cancel") {
	test_justamcp_readonly_task_cancel();
}

TEST_CASE("[Modules][JustAMCP] semantic index stats") {
	test_justamcp_semantic_index_stats();
}

TEST_CASE("[Modules][JustAMCP] send tool result lookup") {
	test_justamcp_send_tool_result_lookup();
}

TEST_CASE("[Modules][JustAMCP] readonly tool registration") {
	test_justamcp_readonly_tool_registration();
}

TEST_CASE("[Modules][JustAMCP] queued cancel no double delete") {
	test_justamcp_queued_cancel_no_double_delete();
}

TEST_CASE("[Modules][JustAMCP] server stop inflight cancel") {
	test_justamcp_server_stop_inflight_cancel();
}

TEST_CASE("[Modules][JustAMCP] pending in flight queue split") {
	test_justamcp_pending_in_flight_queue_split();
}

TEST_CASE("[Modules][JustAMCP] task dispatch cancel race") {
	test_justamcp_task_dispatch_cancel_race();
}

TEST_CASE("[Modules][JustAMCP] task augmented queued cancel unblocks") {
	test_justamcp_task_augmented_queued_cancel_unblocks();
}

TEST_CASE("[Modules][JustAMCP] task post-create cancel") {
	test_justamcp_task_post_create_cancel();
}
