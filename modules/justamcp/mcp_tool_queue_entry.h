/**************************************************************************/
/*  mcp_tool_queue_entry.h                                                */
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

#include "modules/modules_enabled.gen.h"

#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

#include <atomic>

#if defined(MODULE_HTTPSERVER_ENABLED)
#include "core/os/semaphore.h"
#include "modules/httpserver/http_response.h"
#endif

#ifdef THREADS_ENABLED
#ifdef MINGW_ENABLED
#define MINGW_STDTHREAD_REDUNDANCY_WARNING
#include "thirdparty/mingw-std-threads/mingw.condition_variable.h"
#include "thirdparty/mingw-std-threads/mingw.mutex.h"
#ifndef THREADING_NAMESPACE
#define THREADING_NAMESPACE mingw_stdthread
#endif
#else
#include <condition_variable>
#include <mutex>
#ifndef THREADING_NAMESPACE
#define THREADING_NAMESPACE std
#endif
#endif
#include <chrono>
#include <thread>
#endif

struct MCPToolQueueEntry {
	Variant request_id;
	String tool_name;
	Dictionary args;
#if defined(MODULE_HTTPSERVER_ENABLED)
	Ref<HTTPResponse> stateless_response;
	Semaphore done_semaphore;
#endif
	Dictionary rpc_result;
#ifdef THREADS_ENABLED
	mutable THREADING_NAMESPACE::mutex completion_mutex;
	mutable THREADING_NAMESPACE::condition_variable completion_cv;
	bool completion_ready = false;
#endif
	bool has_stateless_response = false;
	String task_id;
	String progress_token;
	bool is_task_augmented = false;
	bool pending_task_dispatch = false;
	int pending_task_ttl_ms = 0;
	int pending_task_poll_interval_ms = 0;
	std::atomic<bool> cancel_requested{ false };
	uint64_t cancel_requested_usec = 0;
	bool is_readonly_tool = false;
	bool readonly_lane = false;
	String session_id;
	int sse_connection_id = -1;

	uint64_t completion_generation = 0;
	bool result_completed = false;
#ifdef THREADS_ENABLED
	mutable std::atomic<int> waiter_count{ 0 };
#endif

#ifdef TESTS_ENABLED
	mutable std::atomic<bool> test_wait_entered{ false };
#endif

	void signal_completion() {
#if defined(MODULE_HTTPSERVER_ENABLED)
		done_semaphore.post();
#endif
#ifdef THREADS_ENABLED
		THREADING_NAMESPACE::lock_guard<THREADING_NAMESPACE::mutex> lock(completion_mutex);
		completion_ready = true;
		completion_cv.notify_all();
#endif
	}

	bool has_completion_waiters() const {
#ifdef THREADS_ENABLED
		return waiter_count.load(std::memory_order_acquire) != 0;
#else
		return false;
#endif
	}

	void signal_and_join_waiters() {
		signal_completion();
#ifdef THREADS_ENABLED
		// Give in-flight waiters time to observe completion_ready and drop
		// completion_mutex. Callers must not memdelete while waiters remain.
		for (int i = 0; i < 10000; i++) {
			if (!has_completion_waiters()) {
				return;
			}
			std::this_thread::sleep_for(std::chrono::microseconds(200));
		}
#endif
	}

	bool wait_for_completion(int p_timeout_ms) const {
#ifdef THREADS_ENABLED
		waiter_count.fetch_add(1, std::memory_order_acq_rel);
		bool ready = false;
		{
			THREADING_NAMESPACE::unique_lock<THREADING_NAMESPACE::mutex> lock(completion_mutex);
#ifdef TESTS_ENABLED
			// Publish "entered wait" only after the mutex is held so cancel/dispatch
			// cannot destroy this entry during the increment-to-lock window.
			test_wait_entered.store(true, std::memory_order_release);
#endif
			ready = completion_cv.wait_for(lock, std::chrono::milliseconds(p_timeout_ms), [this]() {
				return completion_ready;
			});
		}
		waiter_count.fetch_sub(1, std::memory_order_acq_rel);
		return ready;
#else
		(void)p_timeout_ms;
		return true;
#endif
	}
};
