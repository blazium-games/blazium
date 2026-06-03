/**************************************************************************/
/*  justamcp_autowork_tools.cpp                                           */
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

#include "modules/modules_enabled.gen.h"

#ifdef TOOLS_ENABLED
#ifdef MODULE_AUTOWORK_ENABLED

#include "../justamcp_tool_context.h"
#include "justamcp_autowork_tools.h"
#include "modules/autowork/autowork_main.h"
#include "modules/justamcp/justamcp_editor_plugin.h"

#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

void JustAMCPAutoworkTools::_bind_methods() {
}

JustAMCPAutoworkTools::JustAMCPAutoworkTools() {
}

JustAMCPAutoworkTools::~JustAMCPAutoworkTools() {
}

static Dictionary _execute_autowork(Autowork *p_autowork) {
	Dictionary result;
	if (justamcp_is_cancel_requested()) {
		result["ok"] = false;
		result["error"] = "cancelled";
		return result;
	}
	justamcp_report_progress(0, 1, "Running autowork tests");

	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (tree) {
		p_autowork->set_name("MCP_AutoworkInstance");
		tree->get_root()->add_child(p_autowork);
	}

	p_autowork->run_tests();

	result["ok"] = true;

	Dictionary payload;
	payload["pass_count"] = p_autowork->get_pass_count();
	payload["fail_count"] = p_autowork->get_fail_count();
	payload["assert_count"] = p_autowork->get_assert_count();
	payload["test_count"] = p_autowork->get_test_count();
	payload["pending_count"] = p_autowork->get_pending_count();

	Array failures;
	if (p_autowork->get_logger().is_valid()) {
		const Vector<AutoworkTestMethodResult> &test_results = p_autowork->get_logger()->get_test_results();
		for (int i = 0; i < test_results.size(); i++) {
			const AutoworkTestMethodResult &tr = test_results[i];
			if (tr.fails > 0 || tr.fail_messages.size() > 0) {
				Dictionary f;
				f["script"] = tr.script_name;
				f["method"] = tr.method_name;
				Array messages;
				for (int j = 0; j < tr.fail_messages.size(); j++) {
					messages.push_back(tr.fail_messages[j]);
				}
				f["messages"] = messages;
				failures.push_back(f);
			}
		}
	}
	payload["failures"] = failures;
	result["result"] = payload;

	if (tree) {
		tree->get_root()->remove_child(p_autowork);
	}
	p_autowork->queue_free();

	justamcp_report_progress(1, 1, "Autowork tests finished");
	return result;
}

static bool _is_autowork_already_running() {
	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!tree || !tree->get_root()) {
		return false;
	}

	for (int i = 0; i < tree->get_root()->get_child_count(); i++) {
		Node *child = tree->get_root()->get_child(i);
		if (child && child->is_class("Autowork")) {
			return true;
		}
	}
	return false;
}

static Dictionary _nested_autowork_error() {
	Dictionary err;
	err["ok"] = false;
	err["error"] = "Autowork is already running. Nested Autowork execution is blocked to avoid recursively launching the active test suite.";
	return err;
}

Dictionary JustAMCPAutoworkTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "blazium_autowork_run_all_tests" || p_tool_name == "autowork_run_all_tests") {
		if (_is_autowork_already_running()) {
			return _nested_autowork_error();
		}
		Autowork *aw = memnew(Autowork);
		aw->add_directory("res://");
		return _execute_autowork(aw);
	}

	if (p_tool_name == "blazium_autowork_run_tests_in_directory" || p_tool_name == "autowork_run_tests_in_directory") {
		if (!p_args.has("directory_path")) {
			Dictionary err;
			err["ok"] = false;
			err["error"] = "Missing directory_path argument.";
			return err;
		}

		if (_is_autowork_already_running()) {
			return _nested_autowork_error();
		}
		Autowork *aw = memnew(Autowork);
		aw->add_directory(p_args["directory_path"]);
		return _execute_autowork(aw);
	}

	if (p_tool_name == "blazium_autowork_run_test_script" || p_tool_name == "autowork_run_test_script") {
		if (!p_args.has("script_path")) {
			Dictionary err;
			err["ok"] = false;
			err["error"] = "Missing script_path argument.";
			return err;
		}

		if (_is_autowork_already_running()) {
			return _nested_autowork_error();
		}
		Autowork *aw = memnew(Autowork);
		aw->add_script(p_args["script_path"]);
		return _execute_autowork(aw);
	}

	if (p_tool_name == "blazium_autowork_run_test_by_name" || p_tool_name == "autowork_run_test_by_name") {
		if (!p_args.has("test_name")) {
			Dictionary err;
			err["ok"] = false;
			err["error"] = "Missing test_name argument.";
			return err;
		}

		if (_is_autowork_already_running()) {
			return _nested_autowork_error();
		}
		Autowork *aw = memnew(Autowork);
		aw->add_directory("res://");
		aw->set_test(p_args["test_name"]);
		return _execute_autowork(aw);
	}

	Dictionary err;
	err["ok"] = false;
	err["error"] = "Unknown Tool: " + p_tool_name;
	return err;
}

#endif // MODULE_AUTOWORK_ENABLED
#endif // TOOLS_ENABLED
