/**************************************************************************/
/*  justamcp_tool_dispatch.cpp                                            */
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

#include "justamcp_tool_dispatch.h"

#include "justamcp_server.h"
#include "justamcp_tool_context.h"
#include "tools/justamcp_readonly_tools.h"
#include "tools/justamcp_tool_executor.h"

#include "core/object/worker_thread_pool.h"
#include "core/os/thread.h"

static bool _justamcp_tool_error_is_cancelled(const Dictionary &p_result) {
	const Variant err = p_result.get("error", Variant());
	if (err.get_type() == Variant::STRING) {
		return String(err) == "cancelled";
	}
	if (err.get_type() == Variant::DICTIONARY) {
		return String(Dictionary(err).get("message", "")) == "cancelled";
	}
	return false;
}

static String _justamcp_tool_error_message(const Dictionary &p_result) {
	const Variant err = p_result.get("error", Variant());
	if (err.get_type() == Variant::DICTIONARY) {
		const Dictionary err_dict = err;
		if (err_dict.has("message")) {
			return String(err_dict["message"]);
		}
	}
	if (err.get_type() == Variant::STRING) {
		return err;
	}
	return "Unknown error";
}

void JustAMCPToolDispatch::execute_and_send(JustAMCPServer *p_server, JustAMCPToolExecutor *p_executor, const Variant &p_request_id, const String &p_tool_name, const Dictionary &p_args) {
	if (!p_server) {
		return;
	}

	if (!p_executor) {
		Dictionary payload;
		payload["code"] = -32000;
		payload["message"] = "JustAMCP tool executor is unavailable.";
		p_server->send_tool_result(p_request_id, false, payload, "JustAMCP tool executor is unavailable.");
		return;
	}

	JustAMCPToolContextScope context_scope(p_request_id);

	if (p_server->is_tool_cancel_requested(p_request_id)) {
		Dictionary cancelled;
		cancelled["ok"] = false;
		cancelled["error"] = "cancelled";
		p_server->send_tool_result(p_request_id, false, cancelled, "cancelled");
		return;
	}

	Dictionary result = p_executor->execute_tool(p_tool_name, p_args);

	if (result.get("_justamcp_async_pending", false)) {
		return;
	}

	if (result.get("elicitation_required", false)) {
		Dictionary schema = result.get("elicitation_schema", Dictionary());
		const String mode = result.get("elicitation_mode", "form");
		p_server->hold_tool_for_elicitation(p_request_id, p_tool_name, p_args, schema, mode);
		return;
	}

	if (p_server->is_tool_cancel_requested(p_request_id) && !_justamcp_tool_error_is_cancelled(result)) {
		result["ok"] = false;
		result["error"] = "cancelled";
	}

	const bool success = result.get("ok", false);
	if (success) {
		Dictionary payload = result.duplicate();
		payload.erase("ok");
		p_server->send_tool_result(p_request_id, true, payload, "");
	} else {
		const String error_msg = _justamcp_tool_error_message(result);
		const Variant payload = result.get("error", Variant());
		p_server->send_tool_result(p_request_id, false, payload, error_msg);
	}
}

struct JustAMCPWorkerToolJob {
	JustAMCPServer *server = nullptr;
	JustAMCPToolExecutor *executor = nullptr;
	Variant request_id;
	String tool_name;
	Dictionary args;
};

static void _justamcp_worker_tool_execute(void *p_userdata) {
	JustAMCPWorkerToolJob *job = static_cast<JustAMCPWorkerToolJob *>(p_userdata);
	if (!job) {
		return;
	}
	JustAMCPServer *server = job->server;
	JustAMCPToolExecutor *executor = job->executor;
	const Variant request_id = job->request_id;
	const String tool_name = job->tool_name;
	const Dictionary args = job->args;
	memdelete(job);

	if (!server || !executor) {
		return;
	}

	JustAMCPToolContextScope context_scope(request_id);
	Dictionary result = executor->execute_tool(tool_name, args);
	if (result.get("_justamcp_async_pending", false)) {
		return;
	}
	server->call_deferred(SNAME("_deferred_complete_tool_dict"), request_id, result);
}

bool JustAMCPToolDispatch::try_schedule_worker_execute(JustAMCPServer *p_server, const Variant &p_request_id, const String &p_tool_name, const Dictionary &p_args) {
	if (!p_server || !JustAMCPReadonlyTools::is_worker_safe_tool(p_tool_name)) {
		return false;
	}
	if (!Thread::is_main_thread()) {
		return false;
	}
	WorkerThreadPool *pool = WorkerThreadPool::get_singleton();
	JustAMCPToolExecutor *executor = JustAMCPToolExecutor::get_active_instance();
	if (!pool || !executor) {
		return false;
	}
	JustAMCPWorkerToolJob *job = memnew(JustAMCPWorkerToolJob);
	job->server = p_server;
	job->executor = executor;
	job->request_id = p_request_id;
	job->tool_name = p_tool_name;
	job->args = p_args;
	const WorkerThreadPool::TaskID task_id = pool->add_native_task(&_justamcp_worker_tool_execute, job, true, "JustAMCPWorkerSafeTool");
	if (task_id == WorkerThreadPool::INVALID_TASK_ID) {
		memdelete(job);
		return false;
	}
	executor->track_worker_task(task_id);
	return true;
}
