/**************************************************************************/
/*  test_justamcp_task_manager.cpp                                        */
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

#include "test_justamcp_task_manager.h"

#ifdef TOOLS_ENABLED

#include "core/config/project_settings.h"
#include "core/os/os.h"

#include "modules/justamcp/tools/justamcp_task_manager.h"

void test_justamcp_task_manager_create_complete_cancel() {
	JustAMCPTaskManager manager;
	const String task_id = manager.create_task(60000, 1000, "progress-1");
	CHECK(!task_id.is_empty());

	Dictionary task = manager.get_task(task_id);
	CHECK(task.get("ok", false));
	CHECK(String(task.get("status", "")) == "working");

	manager.complete_task(task_id, Dictionary());
	task = manager.get_task(task_id);
	CHECK(String(task.get("status", "")) == "completed");

	Dictionary cancel_result = manager.cancel_task(task_id);
	CHECK(!cancel_result.get("ok", true));
}

void test_justamcp_task_manager_ttl_purge() {
	JustAMCPTaskManager manager;
	const String task_id = manager.create_task(1, 1000, String());
	CHECK(!task_id.is_empty());
	manager.test_backdate_task_created_usec(task_id, 5000);
	Dictionary task = manager.get_task(task_id);
	CHECK(!task.get("ok", true));
}

void test_justamcp_task_manager_max_concurrent() {
	JustAMCPTaskManager manager;
	Vector<String> ids;
	for (int i = 0; i < 20; i++) {
		const String task_id = manager.create_task(60000, 1000, String());
		if (!task_id.is_empty()) {
			ids.push_back(task_id);
		}
	}
	CHECK(ids.size() >= 1);
	CHECK(ids.size() <= 16);
}

void test_justamcp_task_manager_result_timeout() {
	JustAMCPTaskManager manager;
	const int prev_timeout = int(GLOBAL_GET("blazium/justamcp/task_result_max_wait_ms"));
	ProjectSettings::get_singleton()->set_setting("blazium/justamcp/task_result_max_wait_ms", 50);

	const String task_id = manager.create_task(600000, 1000, String());
	CHECK(!task_id.is_empty());

	Dictionary result = manager.get_task_result(task_id, true);
	CHECK(!result.get("ok", true));
	CHECK(int(result.get("error_code", 0)) == -32003);
	CHECK(String(result.get("error", "")).contains("timed out"));

	ProjectSettings::get_singleton()->set_setting("blazium/justamcp/task_result_max_wait_ms", prev_timeout);
}

void test_justamcp_task_manager_result_no_wait() {
	JustAMCPTaskManager manager;
	const String task_id = manager.create_task(600000, 1000, String());
	CHECK(!task_id.is_empty());

	const uint64_t start_msec = OS::get_singleton()->get_ticks_msec();
	Dictionary result = manager.get_task_result(task_id, false);
	const uint64_t elapsed_msec = OS::get_singleton()->get_ticks_msec() - start_msec;
	CHECK(result.get("ok", false));
	CHECK(String(result.get("status", "")) == "working");
	CHECK(elapsed_msec < 20);
}

#endif
