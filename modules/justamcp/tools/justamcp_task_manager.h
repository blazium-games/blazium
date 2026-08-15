/**************************************************************************/
/*  justamcp_task_manager.h                                               */
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

#ifdef TOOLS_ENABLED

#include "core/object/object.h"
#include "core/os/mutex.h"
#include "core/os/semaphore.h"
#include "core/templates/hash_map.h"

#ifdef THREADS_ENABLED
#ifdef MINGW_ENABLED
#define MINGW_STDTHREAD_REDUNDANCY_WARNING
#include "thirdparty/mingw-std-threads/mingw.condition_variable.h"
#include "thirdparty/mingw-std-threads/mingw.mutex.h"
#define THREADING_NAMESPACE mingw_stdthread
#else
#include <condition_variable>
#include <mutex>
#define THREADING_NAMESPACE std
#endif
#include <chrono>
#endif

class JustAMCPServer;

struct JustAMCPTaskRecord {
	String task_id;
	String status = "working";
	String status_message;
	String created_at;
	String last_updated_at;
	uint64_t created_at_usec = 0;
	int ttl_ms = 60000;
	int poll_interval_ms = 1000;
	String progress_token;
	bool cancel_requested = false;
	bool is_terminal = false;
	Dictionary stored_result;
	Dictionary stored_error;
	bool has_stored_error = false;
	Semaphore result_ready;
#ifdef THREADS_ENABLED
	mutable THREADING_NAMESPACE::mutex wait_mutex;
	mutable THREADING_NAMESPACE::condition_variable wait_cv;
	bool terminal_notified = false;
#endif
};

class JustAMCPTaskManager : public Object {
	GDCLASS(JustAMCPTaskManager, Object);

	HashMap<String, JustAMCPTaskRecord *> tasks;
	Mutex tasks_mutex;
	JustAMCPServer *server = nullptr;

	String _iso_timestamp_now() const;
	String _generate_task_id() const;
	bool _is_terminal_status(const String &p_status) const;
	Dictionary _task_to_dict(const JustAMCPTaskRecord &p_task) const;
	void _purge_expired_tasks();
	void _notify_status(const String &p_task_id);

protected:
	static void _bind_methods();

public:
	void set_server(JustAMCPServer *p_server);

	String create_task(int p_ttl_ms, int p_poll_interval_ms, const String &p_progress_token = String());
	Dictionary list_tasks(const String &p_cursor = "");
	Dictionary get_task(const String &p_task_id);
	Dictionary get_task_result(const String &p_task_id, bool p_wait = false);
	Dictionary cancel_task(const String &p_task_id);

	void complete_task(const String &p_task_id, const Dictionary &p_result, bool p_is_error = false);
	void fail_task(const String &p_task_id, const String &p_error);
	void cancel_task_execution(const String &p_task_id, const String &p_message = "The task was cancelled by request.");

	bool is_cancel_requested(const String &p_task_id) const;
	String get_progress_token(const String &p_task_id) const;

#ifdef TESTS_ENABLED
	void test_backdate_task_created_usec(const String &p_task_id, uint64_t p_age_usec);
#endif

	JustAMCPTaskManager();
	~JustAMCPTaskManager();
};

#endif
