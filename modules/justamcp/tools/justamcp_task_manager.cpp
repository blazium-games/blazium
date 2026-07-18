/**************************************************************************/
/*  justamcp_task_manager.cpp                                             */
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

#ifdef TOOLS_ENABLED

#include "justamcp_task_manager.h"
#include "../justamcp_pagination.h"
#include "../justamcp_server.h"
#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/os/time.h"

static int _justamcp_task_result_max_wait_ms() {
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/justamcp/task_result_max_wait_ms")) {
		return int(GLOBAL_GET("blazium/justamcp/task_result_max_wait_ms"));
	}
	return 120000;
}

void JustAMCPTaskManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("list_tasks", "cursor"), &JustAMCPTaskManager::list_tasks, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("get_task", "task_id"), &JustAMCPTaskManager::get_task);
	ClassDB::bind_method(D_METHOD("get_task_result", "task_id", "wait"), &JustAMCPTaskManager::get_task_result, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("cancel_task", "task_id"), &JustAMCPTaskManager::cancel_task);
}

JustAMCPTaskManager::JustAMCPTaskManager() {}
JustAMCPTaskManager::~JustAMCPTaskManager() {
	MutexLock lock(tasks_mutex);
	for (const KeyValue<String, JustAMCPTaskRecord *> &kv : tasks) {
		memdelete(kv.value);
	}
	tasks.clear();
}

void JustAMCPTaskManager::set_server(JustAMCPServer *p_server) {
	server = p_server;
}

String JustAMCPTaskManager::_iso_timestamp_now() const {
	return Time::get_singleton()->get_datetime_string_from_system(true);
}

String JustAMCPTaskManager::_generate_task_id() const {
	uint8_t bytes[16];
	CryptoCore::RandomGenerator rng;
	if (rng.init() != OK || rng.get_random_bytes(bytes, 16) != OK) {
		return vformat("task-%d", Time::get_singleton()->get_ticks_usec());
	}
	String hex;
	for (int i = 0; i < 16; i++) {
		hex += vformat("%02x", bytes[i]);
	}
	return hex;
}

bool JustAMCPTaskManager::_is_terminal_status(const String &p_status) const {
	return p_status == "completed" || p_status == "failed" || p_status == "cancelled";
}

Dictionary JustAMCPTaskManager::_task_to_dict(const JustAMCPTaskRecord &p_task) const {
	Dictionary d;
	d["taskId"] = p_task.task_id;
	d["status"] = p_task.status;
	if (!p_task.status_message.is_empty()) {
		d["statusMessage"] = p_task.status_message;
	}
	d["createdAt"] = p_task.created_at;
	d["lastUpdatedAt"] = p_task.last_updated_at;
	d["ttl"] = p_task.ttl_ms;
	d["pollInterval"] = p_task.poll_interval_ms;
	return d;
}

void JustAMCPTaskManager::_notify_status(const String &p_task_id) {
	if (server) {
		server->broadcast_task_status(p_task_id);
	}
}

void JustAMCPTaskManager::_purge_expired_tasks() {
	const uint64_t now_usec = Time::get_singleton()->get_ticks_usec();
	Vector<String> to_remove;
	for (const KeyValue<String, JustAMCPTaskRecord *> &kv : tasks) {
		const JustAMCPTaskRecord *task = kv.value;
		if (!task || task->ttl_ms <= 0 || task->created_at_usec == 0) {
			continue;
		}
		const uint64_t age_ms = (now_usec - task->created_at_usec) / 1000;
		if (age_ms > (uint64_t)task->ttl_ms) {
			to_remove.push_back(kv.key);
		}
	}
	for (int i = 0; i < to_remove.size(); i++) {
		if (tasks.has(to_remove[i])) {
			memdelete(tasks[to_remove[i]]);
			tasks.erase(to_remove[i]);
		}
	}
}

String JustAMCPTaskManager::create_task(int p_ttl_ms, int p_poll_interval_ms, const String &p_progress_token) {
	MutexLock lock(tasks_mutex);
	_purge_expired_tasks();

	int max_tasks = 16;
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/justamcp/task_max_concurrent")) {
		max_tasks = int(GLOBAL_GET("blazium/justamcp/task_max_concurrent"));
	}
	if (tasks.size() >= (uint32_t)max_tasks) {
		return String();
	}

	JustAMCPTaskRecord *task = memnew(JustAMCPTaskRecord);
	task->task_id = _generate_task_id();
	task->status = "working";
	task->status_message = "The operation is now in progress.";
	task->created_at = _iso_timestamp_now();
	task->last_updated_at = task->created_at;
	task->created_at_usec = Time::get_singleton()->get_ticks_usec();
	task->ttl_ms = p_ttl_ms;
	task->poll_interval_ms = p_poll_interval_ms;
	task->progress_token = p_progress_token;
	tasks.insert(task->task_id, task);
	return task->task_id;
}

Dictionary JustAMCPTaskManager::list_tasks(const String &p_cursor) {
	MutexLock lock(tasks_mutex);
	_purge_expired_tasks();
	Array arr;
	for (const KeyValue<String, JustAMCPTaskRecord *> &kv : tasks) {
		if (kv.value) {
			arr.push_back(_task_to_dict(*kv.value));
		}
	}
	return justamcp_pagination_slice_array(arr, p_cursor, "tasks");
}

Dictionary JustAMCPTaskManager::get_task(const String &p_task_id) {
	MutexLock lock(tasks_mutex);
	_purge_expired_tasks();
	if (!tasks.has(p_task_id) || !tasks[p_task_id]) {
		Dictionary err;
		err["ok"] = false;
		err["error_code"] = -32602;
		err["error"] = "Failed to retrieve task: Task not found";
		return err;
	}
	Dictionary result;
	result["ok"] = true;
	Dictionary task_dict = _task_to_dict(*tasks[p_task_id]);
	for (int i = 0; i < task_dict.size(); i++) {
		result[task_dict.keys()[i]] = task_dict.values()[i];
	}
	return result;
}

Dictionary JustAMCPTaskManager::get_task_result(const String &p_task_id, bool p_wait) {
	bool wait_for_terminal = false;
	{
		MutexLock lock(tasks_mutex);
		_purge_expired_tasks();
		if (!tasks.has(p_task_id) || !tasks[p_task_id]) {
			Dictionary err;
			err["ok"] = false;
			err["error_code"] = -32602;
			err["error"] = "Failed to retrieve task: Task not found";
			return err;
		}
		if (!_is_terminal_status(tasks[p_task_id]->status)) {
			if (!p_wait) {
				lock.temp_unlock();
				return get_task(p_task_id);
			}
			wait_for_terminal = true;
		}
	}

	if (wait_for_terminal) {
		int timeout_ms = _justamcp_task_result_max_wait_ms();
		{
			MutexLock lock(tasks_mutex);
			if (tasks.has(p_task_id) && tasks[p_task_id] && tasks[p_task_id]->ttl_ms > 0) {
				const uint64_t now_usec = Time::get_singleton()->get_ticks_usec();
				const uint64_t age_ms = (now_usec - tasks[p_task_id]->created_at_usec) / 1000;
				const int remaining_ms = tasks[p_task_id]->ttl_ms - int(age_ms);
				if (remaining_ms > 0 && remaining_ms < timeout_ms) {
					timeout_ms = remaining_ms;
				}
			}
		}

		bool timed_out = false;
#ifdef THREADS_ENABLED
		JustAMCPTaskRecord *task_ptr = nullptr;
		{
			MutexLock lock(tasks_mutex);
			if (tasks.has(p_task_id)) {
				task_ptr = tasks[p_task_id];
			}
		}
		if (task_ptr) {
			THREADING_NAMESPACE::unique_lock<THREADING_NAMESPACE::mutex> wait_lock(task_ptr->wait_mutex);
			timed_out = !task_ptr->wait_cv.wait_for(wait_lock, std::chrono::milliseconds(timeout_ms), [&]() {
				MutexLock lock(tasks_mutex);
				if (!tasks.has(p_task_id) || !tasks[p_task_id]) {
					return true;
				}
				return _is_terminal_status(tasks[p_task_id]->status) || tasks[p_task_id]->terminal_notified;
			});
		}
#else
		timed_out = false;
#endif
		if (timed_out) {
			Dictionary err;
			err["ok"] = false;
			err["error_code"] = -32003;
			err["error"] = "Task result wait timed out.";
			return err;
		}
	}

	MutexLock lock(tasks_mutex);
	_purge_expired_tasks();
	if (!tasks.has(p_task_id) || !tasks[p_task_id]) {
		Dictionary err;
		err["ok"] = false;
		err["error_code"] = -32602;
		err["error"] = "Failed to retrieve task: Task not found";
		return err;
	}

	const JustAMCPTaskRecord *task = tasks[p_task_id];
	if (task->has_stored_error) {
		Dictionary err;
		err["ok"] = false;
		err["error_code"] = task->stored_error.get("code", -32603);
		err["error"] = task->stored_error.get("message", "Task failed.");
		return err;
	}

	Dictionary result = task->stored_result.duplicate();
	result["ok"] = true;
	Dictionary meta;
	Dictionary related;
	related["taskId"] = p_task_id;
	meta["io.modelcontextprotocol/related-task"] = related;
	result["_meta"] = meta;
	return result;
}

Dictionary JustAMCPTaskManager::cancel_task(const String &p_task_id) {
	{
		MutexLock lock(tasks_mutex);
		_purge_expired_tasks();
		if (!tasks.has(p_task_id) || !tasks[p_task_id]) {
			Dictionary err;
			err["ok"] = false;
			err["error_code"] = -32602;
			err["error"] = "Failed to retrieve task: Task not found";
			return err;
		}
		if (_is_terminal_status(tasks[p_task_id]->status)) {
			Dictionary err;
			err["ok"] = false;
			err["error_code"] = -32602;
			err["message"] = vformat("Cannot cancel task: already in terminal status '%s'", tasks[p_task_id]->status);
			err["error"] = err["message"];
			return err;
		}
	}
	cancel_task_execution(p_task_id, "The task was cancelled by request.");
	return get_task(p_task_id);
}

void JustAMCPTaskManager::complete_task(const String &p_task_id, const Dictionary &p_result, bool p_is_error) {
	MutexLock lock(tasks_mutex);
	if (!tasks.has(p_task_id) || !tasks[p_task_id]) {
		return;
	}
	JustAMCPTaskRecord *task = tasks[p_task_id];
	if (_is_terminal_status(task->status)) {
		return;
	}
	task->stored_result = p_result.duplicate();
	task->has_stored_error = false;
	if (p_is_error) {
		task->status = "failed";
		task->status_message = "Tool execution returned an error result.";
	} else {
		task->status = "completed";
		task->status_message = "Task completed successfully.";
	}
	task->last_updated_at = _iso_timestamp_now();
	task->is_terminal = true;
	task->result_ready.post();
#ifdef THREADS_ENABLED
	task->terminal_notified = true;
	{
		THREADING_NAMESPACE::lock_guard<THREADING_NAMESPACE::mutex> wait_lock(task->wait_mutex);
		task->wait_cv.notify_all();
	}
#endif
	lock.temp_unlock();
	_notify_status(p_task_id);
}

void JustAMCPTaskManager::fail_task(const String &p_task_id, const String &p_error) {
	MutexLock lock(tasks_mutex);
	if (!tasks.has(p_task_id) || !tasks[p_task_id]) {
		return;
	}
	JustAMCPTaskRecord *task = tasks[p_task_id];
	if (_is_terminal_status(task->status)) {
		return;
	}
	task->status = "failed";
	task->status_message = p_error;
	task->last_updated_at = _iso_timestamp_now();
	task->is_terminal = true;
	task->has_stored_error = true;
	Dictionary err;
	err["code"] = -32603;
	err["message"] = p_error;
	task->stored_error = err;
	task->result_ready.post();
#ifdef THREADS_ENABLED
	task->terminal_notified = true;
	{
		THREADING_NAMESPACE::lock_guard<THREADING_NAMESPACE::mutex> wait_lock(task->wait_mutex);
		task->wait_cv.notify_all();
	}
#endif
	lock.temp_unlock();
	_notify_status(p_task_id);
}

void JustAMCPTaskManager::cancel_task_execution(const String &p_task_id, const String &p_message) {
	MutexLock lock(tasks_mutex);
	if (!tasks.has(p_task_id) || !tasks[p_task_id]) {
		return;
	}
	JustAMCPTaskRecord *task = tasks[p_task_id];
	if (_is_terminal_status(task->status)) {
		return;
	}
	task->cancel_requested = true;
	task->status = "cancelled";
	task->status_message = p_message;
	task->last_updated_at = _iso_timestamp_now();
	task->is_terminal = true;
	task->has_stored_error = false;
	Array content;
	Dictionary content_item;
	content_item["type"] = "text";
	content_item["text"] = p_message;
	content.push_back(content_item);
	Dictionary stored;
	stored["content"] = content;
	stored["isError"] = true;
	task->stored_result = stored;
	task->result_ready.post();
#ifdef THREADS_ENABLED
	task->terminal_notified = true;
	{
		THREADING_NAMESPACE::lock_guard<THREADING_NAMESPACE::mutex> wait_lock(task->wait_mutex);
		task->wait_cv.notify_all();
	}
#endif
	lock.temp_unlock();
	_notify_status(p_task_id);
}

bool JustAMCPTaskManager::is_cancel_requested(const String &p_task_id) const {
	MutexLock lock(tasks_mutex);
	if (!tasks.has(p_task_id) || !tasks[p_task_id]) {
		return false;
	}
	return tasks[p_task_id]->cancel_requested;
}

String JustAMCPTaskManager::get_progress_token(const String &p_task_id) const {
	MutexLock lock(tasks_mutex);
	if (!tasks.has(p_task_id) || !tasks[p_task_id]) {
		return String();
	}
	return tasks[p_task_id]->progress_token;
}

#ifdef TESTS_ENABLED
void JustAMCPTaskManager::test_backdate_task_created_usec(const String &p_task_id, uint64_t p_age_usec) {
	MutexLock lock(tasks_mutex);
	if (!tasks.has(p_task_id) || !tasks[p_task_id]) {
		return;
	}
	const uint64_t now_usec = Time::get_singleton()->get_ticks_usec();
	tasks[p_task_id]->created_at_usec = now_usec > p_age_usec ? now_usec - p_age_usec : 0;
}
#endif

#endif
