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
#include "core/os/time.h"

void JustAMCPTaskManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create_task", "task_id", "ttl"), &JustAMCPTaskManager::create_task, DEFVAL(60000));
	ClassDB::bind_method(D_METHOD("list_tasks", "cursor"), &JustAMCPTaskManager::list_tasks, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("get_task", "task_id"), &JustAMCPTaskManager::get_task);
	ClassDB::bind_method(D_METHOD("get_task_result", "task_id"), &JustAMCPTaskManager::get_task_result);
	ClassDB::bind_method(D_METHOD("cancel_task", "task_id"), &JustAMCPTaskManager::cancel_task);
	ClassDB::bind_method(D_METHOD("complete_task", "task_id", "result"), &JustAMCPTaskManager::complete_task);
	ClassDB::bind_method(D_METHOD("fail_task", "task_id", "error"), &JustAMCPTaskManager::fail_task);
}

JustAMCPTaskManager::JustAMCPTaskManager() {}
JustAMCPTaskManager::~JustAMCPTaskManager() {}

Dictionary JustAMCPTaskManager::create_task(const String &p_task_id, int p_ttl) {
	Dictionary result;
	if (active_tasks.has(p_task_id)) {
		result["ok"] = false;
		result["error"] = "Task already exists";
		return result;
	}

	Dictionary task_info;
	task_info["taskId"] = p_task_id;
	task_info["status"] = "working";
	task_info["createdAt"] = Time::get_singleton()->get_datetime_string_from_system();
	task_info["ttl"] = p_ttl;
	task_info["pollInterval"] = 1000;

	active_tasks[p_task_id] = task_info;

	result["task"] = task_info;
	result["ok"] = true;
	return result;
}

Dictionary JustAMCPTaskManager::list_tasks(const String &cursor) {
	Array tasks_array;
	Array keys = active_tasks.keys();
	for (int i = 0; i < keys.size(); i++) {
		Dictionary task_info = active_tasks[keys[i]];
		tasks_array.push_back(task_info);
	}
	return justamcp_pagination_slice_array(tasks_array, cursor, "tasks");
}

Dictionary JustAMCPTaskManager::get_task(const String &p_task_id) {
	Dictionary result;
	if (!active_tasks.has(p_task_id)) {
		result["ok"] = false;
		result["error_code"] = -32602;
		result["error"] = "Task ID unknown";
		return result;
	}
	Dictionary task_info = active_tasks[p_task_id];
	result["ok"] = true;
	for (int i = 0; i < task_info.size(); i++) {
		result[task_info.keys()[i]] = task_info.values()[i];
	}
	return result;
}

Dictionary JustAMCPTaskManager::get_task_result(const String &p_task_id) {
	Dictionary result;
	if (!active_tasks.has(p_task_id)) {
		result["ok"] = false;
		result["error_code"] = -32602;
		result["error"] = "Task ID unknown";
		return result;
	}

	Dictionary task_info = active_tasks[p_task_id];
	if (task_info.get("status", "") != "completed") {
		result["ok"] = false;
		result["error_code"] = -32602;
		result["error"] = "Task not completed yet or failed.";
		return result;
	}

	result["ok"] = true;
	result["content"] = task_info.get("stored_result_content", Array());
	result["isError"] = false;

	Dictionary meta;
	Dictionary related_task;
	related_task["taskId"] = p_task_id;
	meta["io.modelcontextprotocol/related-task"] = related_task;
	result["_meta"] = meta;
	return result;
}

Dictionary JustAMCPTaskManager::cancel_task(const String &p_task_id) {
	Dictionary result;
	if (!active_tasks.has(p_task_id)) {
		result["ok"] = false;
		result["error_code"] = -32602;
		result["error"] = "Task ID unknown";
		return result;
	}

	Dictionary task_info = active_tasks[p_task_id];
	if (task_info.get("status", "") != "completed" && task_info.get("status", "") != "failed") {
		task_info["status"] = "cancelled";
		task_info["statusMessage"] = "Cancelled normally";
		active_tasks[p_task_id] = task_info;
	}

	result["ok"] = true;
	for (int i = 0; i < task_info.size(); i++) {
		result[task_info.keys()[i]] = task_info.values()[i];
	}
	return result;
}

void JustAMCPTaskManager::complete_task(const String &p_task_id, const Dictionary &p_result) {
	if (active_tasks.has(p_task_id)) {
		Dictionary task_info = active_tasks[p_task_id];
		task_info["status"] = "completed";
		task_info["stored_result_content"] = p_result.get("content", Array());
		active_tasks[p_task_id] = task_info;
	}
}

void JustAMCPTaskManager::fail_task(const String &p_task_id, const String &p_error) {
	if (active_tasks.has(p_task_id)) {
		Dictionary task_info = active_tasks[p_task_id];
		task_info["status"] = "failed";
		task_info["statusMessage"] = p_error;
		active_tasks[p_task_id] = task_info;
	}
}

#endif // TOOLS_ENABLED
