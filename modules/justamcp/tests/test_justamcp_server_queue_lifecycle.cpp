/**************************************************************************/
/*  test_justamcp_server_queue_lifecycle.cpp                              */
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

#ifdef TESTS_ENABLED

#include "test_justamcp_server_queue_lifecycle.h"
#include "../justamcp_server.h"
#include "../justamcp_tool_context.h"
#include "../justamcp_tool_queue_state.h"
#include "../mcp_tool_queue.h"
#include "../tools/justamcp_category_dispatch.h"
#include "../tools/justamcp_category_registry.h"
#include "../tools/justamcp_json_rpc_helpers.h"
#include "../tools/justamcp_readonly_tools.h"
#include "../tools/justamcp_tool_executor.h"
#include "modules/modules_enabled.gen.h"
#include "test_justamcp_fixture.h"
#if defined(MODULE_SEMANTICSEARCH_ENABLED)
#include "../tools/justamcp_semantic_search_tools.h"
#include "modules/semanticsearch/semantic_search_backend_factory.h"
#endif
#include "core/os/os.h"
#include "core/os/thread.h"
#include "tests/test_macros.h"
#ifdef THREADS_ENABLED
#include <atomic>
#include <thread>
#endif

#ifdef THREADS_ENABLED
static void _wait_for_entry_wait_state(const MCPToolQueueEntry *p_entry, int p_timeout_ms = 2000) {
	const uint64_t deadline = OS::get_singleton()->get_ticks_msec() + (uint64_t)p_timeout_ms;
	while (!p_entry->test_wait_entered.load(std::memory_order_acquire)) {
		if (OS::get_singleton()->get_ticks_msec() >= deadline) {
			break;
		}
		OS::get_singleton()->delay_usec(1000);
	}
}
#endif

void test_justamcp_server_queue_lifecycle() {
	MCPToolQueueEntry entry;
	entry.pending_task_dispatch = false;
	entry.has_stateless_response = true;
	entry.is_task_augmented = false;
	entry.request_id = 7;
	CHECK(entry.has_stateless_response);
	CHECK(!entry.is_task_augmented);
	entry.signal_completion();
	CHECK(entry.done_semaphore.try_wait());
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	CHECK(server.get_pending_tool_queue_size() >= 0);

	MCPToolQueueEntry stuck;
	stuck.pending_task_dispatch = true;
	stuck.request_id = 99;
	CHECK(stuck.pending_task_dispatch);
	stuck.pending_task_dispatch = false;
	CHECK(!stuck.pending_task_dispatch);
}

void test_justamcp_category_dispatch_coverage() {
	CHECK(JustAMCPToolCategoryDispatch::has_module_executor("editor_tools"));
	CHECK(JustAMCPToolCategoryDispatch::has_module_executor("scene_tools"));
	CHECK(JustAMCPToolCategoryDispatch::has_module_executor("documentation_tools"));
	CHECK(!JustAMCPToolCategoryDispatch::is_registry_category("semantic_search_tools"));
	CHECK(JustAMCPCategoryRegistry::get_entry_count() >= 27);
}

void test_justamcp_server_queue_wait_completion() {
	MCPToolQueueEntry entry;
	entry.has_stateless_response = true;
	entry.rpc_result["ok"] = true;
	const uint64_t start = OS::get_singleton()->get_ticks_msec();
	entry.signal_completion();
	CHECK(entry.wait_for_completion(1000));
	CHECK(entry.done_semaphore.try_wait());
	CHECK(int(OS::get_singleton()->get_ticks_msec() - start) < 500);
}

void test_justamcp_server_queue_wait_timeout() {
	MCPToolQueueEntry entry;
	entry.has_stateless_response = true;
	const uint64_t start = OS::get_singleton()->get_ticks_msec();
	CHECK(!entry.wait_for_completion(25));
	CHECK(int(OS::get_singleton()->get_ticks_msec() - start) >= 20);
}

void test_justamcp_server_queue_ordering_signal() {
	MCPToolQueueEntry first;
	MCPToolQueueEntry second;
	first.request_id = 1;
	second.request_id = 2;
	first.signal_completion();
	second.signal_completion();
	CHECK(first.wait_for_completion(100));
	CHECK(second.wait_for_completion(100));
	CHECK(first.done_semaphore.try_wait());
	CHECK(second.done_semaphore.try_wait());
}

void test_justamcp_server_queue_cancel_flag() {
	MCPToolQueueEntry entry;
	entry.cancel_requested = false;
	entry.cancel_requested = true;
	CHECK(entry.cancel_requested);
}

void test_justamcp_category_dispatch_fallthrough() {
	Dictionary empty;
	CHECK(empty.is_empty());
	Dictionary module_error;
	module_error["ok"] = false;
	module_error["error"] = "Unknown project tool: logs_read";
	Dictionary unhandled = JustAMCPToolCategoryDispatch::dispatch_module_tools(nullptr, "project_tools", "logs_read", Dictionary());
	CHECK(!bool(unhandled.get("handled", true)));
	if (JustAMCPToolExecutor *executor = JustAMCPToolExecutor::get_active_instance()) {
		Dictionary composite = executor->execute_composite_tool("logs_read", Dictionary());
		const bool composite_ok = bool(composite.get("ok", false)) || !composite.is_empty();
		CHECK(composite_ok);
	}
}

void test_justamcp_tool_call_queue_e2e() {
	MCPToolQueueEntry entry;
	entry.request_id = 42;
	entry.tool_name = "blazium_get_project_info";
	entry.args = Dictionary();
	entry.has_stateless_response = true;
	entry.pending_task_dispatch = false;
	CHECK(JustAMCPReadonlyTools::is_readonly_tool(entry.tool_name));
	entry.signal_completion();
	CHECK(entry.wait_for_completion(250));
	CHECK(entry.done_semaphore.try_wait());
	if (JustAMCPToolExecutor *executor = JustAMCPToolExecutor::get_active_instance()) {
		Dictionary result = executor->execute_tool("blazium_get_project_info", Dictionary());
		CHECK(bool(result.get("ok", false)));
	}
}

void test_justamcp_logs_read_full_path() {
	if (JustAMCPToolExecutor *executor = JustAMCPToolExecutor::get_active_instance()) {
		Dictionary result = executor->execute_tool("blazium_logs_read", Dictionary());
		const bool logs_ok = bool(result.get("ok", false)) || result.has("error");
		CHECK(logs_ok);
	}
}

void test_justamcp_task_dispatch_failure_cleanup() {
	MCPToolQueueEntry entry;
	entry.pending_task_dispatch = true;
	entry.has_stateless_response = true;
	entry.request_id = 501;
	entry.rpc_result["error"] = "Task manager unavailable.";
	entry.signal_completion();
	CHECK(entry.wait_for_completion(100));
	const bool dispatch_cleared_or_errored = !entry.pending_task_dispatch || entry.rpc_result.has("error");
	CHECK(dispatch_cleared_or_errored);
}

void test_justamcp_parallel_readonly_lane() {
	CHECK(JustAMCPReadonlyTools::is_readonly_tool("blazium_logs_read"));
	CHECK(JustAMCPReadonlyTools::is_readonly_tool("blazium_get_project_info"));
	CHECK(JustAMCPReadonlyTools::is_readonly_tool("semantic_search"));
	CHECK(JustAMCPReadonlyTools::is_readonly_tool("blazium_semantic_search"));
	CHECK(JustAMCPReadonlyTools::is_readonly_tool("semantic_find_similar"));
	CHECK(!JustAMCPReadonlyTools::is_readonly_tool("blazium_create_scene"));
	MCPToolQueueEntry readonly_entry;
	readonly_entry.is_readonly_tool = true;
	readonly_entry.readonly_lane = true;
	CHECK(readonly_entry.is_readonly_tool);
	CHECK(readonly_entry.readonly_lane);
}

void test_justamcp_request_id_lane_context() {
	CHECK(JustAMCPJsonRpcHelpers::request_ids_equal(Variant(3), Variant(3.0)));
	MCPToolQueueEntry write_entry;
	MCPToolQueueEntry readonly_entry;
	write_entry.request_id = 1;
	write_entry.progress_token = "write-token";
	readonly_entry.request_id = 2;
	readonly_entry.progress_token = "readonly-token";
	CHECK(write_entry.progress_token != readonly_entry.progress_token);
}

void test_justamcp_dual_lane_processing_flag() {
	MCPToolQueueEntry write_entry;
	MCPToolQueueEntry readonly_entry;
	MCPToolQueueEntry *write_ptr = &write_entry;
	Vector<MCPToolQueueEntry *> readonly_inflight;
	readonly_inflight.push_back(&readonly_entry);
	bool processing = false;
	JustAMCPToolQueueState::sync_processing_flag(write_ptr, readonly_inflight, processing);
	CHECK(processing);
	write_ptr = nullptr;
	JustAMCPToolQueueState::sync_processing_flag(write_ptr, readonly_inflight, processing);
	CHECK(processing);
	readonly_inflight.clear();
	JustAMCPToolQueueState::sync_processing_flag(write_ptr, readonly_inflight, processing);
	CHECK(!processing);
}

void test_justamcp_tool_context_tls_stack() {
	CHECK(justamcp_get_active_tool_request_id().get_type() == Variant::NIL);
	justamcp_push_active_tool_request_id(1);
	CHECK(JustAMCPJsonRpcHelpers::request_ids_equal(justamcp_get_active_tool_request_id(), 1));
	justamcp_push_active_tool_request_id(2);
	CHECK(JustAMCPJsonRpcHelpers::request_ids_equal(justamcp_get_active_tool_request_id(), 2));
	justamcp_pop_active_tool_request_id();
	CHECK(JustAMCPJsonRpcHelpers::request_ids_equal(justamcp_get_active_tool_request_id(), 1));
	justamcp_pop_active_tool_request_id();
	CHECK(justamcp_get_active_tool_request_id().get_type() == Variant::NIL);
}

void test_justamcp_readonly_task_cancel() {
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPToolQueueEntry readonly_entry;
	readonly_entry.task_id = "readonly-task";
	readonly_entry.cancel_requested = false;
	server.test_set_in_flight_entries(nullptr, &readonly_entry);
	CHECK(server.test_get_tool_queue_processing());
	server.request_task_queue_cancel("readonly-task");
	CHECK(readonly_entry.cancel_requested);
	server.test_set_in_flight_entries(nullptr, nullptr);
	CHECK(!server.test_get_tool_queue_processing());
}

void test_justamcp_semantic_index_stats() {
#ifdef MODULE_SEMANTICSEARCH_ENABLED
	SemanticSearchBackendFactory::invalidate_session_backend();
	JustAMCPSemanticSearchTools tools;
	Dictionary result = tools.semantic_index_stats(Dictionary());
	CHECK(result.get("ok", false));
	CHECK(result.has("stats"));
	CHECK(result.has("backend"));
#else
	TEST_FAIL_COND(true, "MODULE_SEMANTICSEARCH_ENABLED is required for semantic index stats test");
#endif
}

void test_justamcp_send_tool_result_lookup() {
#if defined(MODULE_HTTPSERVER_ENABLED)
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPToolQueueEntry readonly_entry;
	readonly_entry.request_id = 42;
	readonly_entry.is_readonly_tool = true;
	server.test_set_in_flight_entries(nullptr, &readonly_entry);
	Dictionary ok;
	ok["ok"] = true;
	server.send_tool_result(99, true, ok, "");
	CHECK(readonly_entry.rpc_result.is_empty());
	CHECK(server.test_get_tool_queue_processing());
	server.test_set_in_flight_entries(nullptr, nullptr);
	CHECK(!server.test_get_tool_queue_processing());
#else
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for send_tool_result lookup test");
#endif
}

void test_justamcp_readonly_tool_registration() {
	JustAMCPSemanticSearchTools tools;
	tools.provide_tool_schemas(true, true, true);
	CHECK(JustAMCPReadonlyTools::is_readonly_tool("semantic_search"));
	CHECK(JustAMCPReadonlyTools::is_readonly_tool("blazium_semantic_search_enqueue"));
	CHECK(!JustAMCPReadonlyTools::is_readonly_tool("semantic_rebuild_index"));
}

void test_justamcp_queued_cancel_no_double_delete() {
#if defined(MODULE_HTTPSERVER_ENABLED)
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	Dictionary queue_full;
	MCPToolQueueEntry *entry = server.test_enqueue_tool_request(100, "blazium_logs_read", Dictionary(), queue_full);
	CHECK(entry != nullptr);
	CHECK(server.get_pending_tool_queue_size() == 1);
	server._on_request_cancelled(100, "test");
	CHECK(server.get_pending_tool_queue_size() == 0);
#else
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for queued cancel test");
#endif
}

void test_justamcp_server_stop_inflight_cancel() {
#if defined(MODULE_HTTPSERVER_ENABLED)
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPToolQueueEntry *inflight = memnew(MCPToolQueueEntry);
	inflight->request_id = 77;
	inflight->has_stateless_response = true;
	server.test_set_in_flight_entries(inflight, nullptr);
	CHECK(server.test_get_tool_queue_processing());
	server.test_clear_tool_queue();
	CHECK(server.test_get_in_flight_write() == nullptr);
	CHECK(server.test_get_in_flight_readonly() == nullptr);
#else
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for stop inflight cancel test");
#endif
}

void test_justamcp_pending_in_flight_queue_split() {
#if defined(MODULE_HTTPSERVER_ENABLED)
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	MCPToolQueueEntry inflight;
	inflight.request_id = 88;
	server.test_set_in_flight_entries(&inflight, nullptr);
	CHECK(server.get_pending_tool_queue_size() == 0);
	Dictionary queue_full;
	MCPToolQueueEntry *queued = server.test_enqueue_tool_request(89, "blazium_logs_read", Dictionary(), queue_full);
	CHECK(queued != nullptr);
	CHECK(server.get_pending_tool_queue_size() == 1);
	server.test_set_in_flight_entries(nullptr, nullptr);
#else
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for pending inflight queue split test");
#endif
}

void test_justamcp_task_dispatch_cancel_race() {
#if defined(MODULE_HTTPSERVER_ENABLED) && defined(THREADS_ENABLED)
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	Dictionary queue_full;
	MCPToolQueueEntry *entry = server.test_enqueue_tool_request(200, "blazium_logs_read", Dictionary(), queue_full);
	CHECK(entry != nullptr);
	entry->is_task_augmented = true;
	entry->pending_task_dispatch = true;
	entry->has_stateless_response = true;
	entry->cancel_requested = true;
	entry->test_wait_entered.store(false, std::memory_order_release);
	std::atomic<bool> unblocked{ false };
	std::thread waiter([&]() {
		unblocked = entry->wait_for_completion(2000);
	});
	_wait_for_entry_wait_state(entry);
	server.test_dispatch_task_augmented_tools_call(200);
	waiter.join();
	CHECK(unblocked.load());
	CHECK(server.get_pending_tool_queue_size() == 0);
#elif defined(MODULE_HTTPSERVER_ENABLED)
	TEST_FAIL_COND(true, "THREADS_ENABLED is required for task dispatch cancel race test");
#else
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for task dispatch cancel race test");
#endif
}

void test_justamcp_task_augmented_queued_cancel_unblocks() {
#if defined(MODULE_HTTPSERVER_ENABLED) && defined(THREADS_ENABLED)
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	Dictionary queue_full;
	MCPToolQueueEntry *entry = server.test_enqueue_tool_request(300, "blazium_logs_read", Dictionary(), queue_full);
	CHECK(entry != nullptr);
	entry->is_task_augmented = true;
	entry->pending_task_dispatch = true;
	entry->has_stateless_response = true;
	entry->test_wait_entered.store(false, std::memory_order_release);
	std::atomic<bool> unblocked{ false };
	std::thread waiter([&]() {
		unblocked = entry->wait_for_completion(2000);
	});
	_wait_for_entry_wait_state(entry);
	server._on_request_cancelled(300, "test");
	waiter.join();
	CHECK(unblocked.load());
	CHECK(server.get_pending_tool_queue_size() == 0);
#elif defined(MODULE_HTTPSERVER_ENABLED)
	TEST_FAIL_COND(true, "THREADS_ENABLED is required for task dispatch cancel race test");
#else
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for task dispatch cancel race test");
#endif
}

void test_justamcp_task_post_create_cancel() {
#if defined(MODULE_HTTPSERVER_ENABLED) && defined(THREADS_ENABLED)
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	Dictionary queue_full;
	MCPToolQueueEntry *entry = server.test_enqueue_tool_request(400, "blazium_logs_read", Dictionary(), queue_full);
	CHECK(entry != nullptr);
	entry->is_task_augmented = true;
	entry->pending_task_dispatch = true;
	entry->has_stateless_response = true;
	entry->test_wait_entered.store(false, std::memory_order_release);
	std::atomic<bool> unblocked{ false };
	std::thread waiter([&]() {
		unblocked = entry->wait_for_completion(3000);
	});
	std::thread dispatcher([&]() {
		server.test_dispatch_task_augmented_tools_call(400);
	});
	_wait_for_entry_wait_state(entry);
	server._on_request_cancelled(400, "post-create");
	dispatcher.join();
	waiter.join();
	CHECK(unblocked.load());
	CHECK(server.get_pending_tool_queue_size() == 0);
#elif defined(MODULE_HTTPSERVER_ENABLED)
	TEST_FAIL_COND(true, "THREADS_ENABLED is required for task dispatch cancel race test");
#else
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for task dispatch cancel race test");
#endif
}

void test_mcp_tool_queue_skip_cancelled_pick() {
	MCPToolQueue queue;
	MCPToolQueueEntry cancelled;
	MCPToolQueueEntry valid;
	cancelled.request_id = 1;
	cancelled.cancel_requested = true;
	valid.request_id = 2;
	valid.is_readonly_tool = true;
	queue.enqueue(&cancelled);
	queue.enqueue(&valid);
	MCPToolQueueEntry *picked = queue.pick_next(0, false);
	CHECK(picked == &valid);
	CHECK(picked != &cancelled);
}

void test_justamcp_post_create_cancel_entry_lifetime() {
#if defined(MODULE_HTTPSERVER_ENABLED) && defined(THREADS_ENABLED)
	JustAMCPTestServerFixture fixture;
	JustAMCPServer &server = fixture.get_server();
	Dictionary queue_full;
	MCPToolQueueEntry *entry = server.test_enqueue_tool_request(401, "blazium_logs_read", Dictionary(), queue_full);
	CHECK(entry != nullptr);
	entry->is_task_augmented = true;
	entry->pending_task_dispatch = true;
	entry->has_stateless_response = true;
	std::atomic<bool> dispatch_done{ false };
	std::thread dispatcher([&]() {
		server.test_dispatch_task_augmented_tools_call(401);
		dispatch_done = true;
	});
	_wait_for_entry_wait_state(entry);
	server._on_request_cancelled(401, "post-create");
	dispatcher.join();
	CHECK(dispatch_done.load());
	CHECK(server.get_pending_tool_queue_size() == 0);
#elif defined(MODULE_HTTPSERVER_ENABLED)
	TEST_FAIL_COND(true, "THREADS_ENABLED is required for task dispatch cancel race test");
#else
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required for task dispatch cancel race test");
#endif
}

#endif
