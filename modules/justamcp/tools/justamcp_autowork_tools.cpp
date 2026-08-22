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

#include "../justamcp_read_limits.h"
#include "../justamcp_tool_context.h"
#include "justamcp_agent_helpers.h"
#include "justamcp_autowork_tools.h"
#include "modules/autowork/autowork_collector.h"
#include "modules/autowork/autowork_main.h"
#include "modules/justamcp/justamcp_editor_plugin.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/object/message_queue.h"
#include "core/os/main_loop.h"
#include "core/os/os.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/display_server.h"

void JustAMCPAutoworkTools::_bind_methods() {
}

JustAMCPAutoworkTools::JustAMCPAutoworkTools() {
}

JustAMCPAutoworkTools::~JustAMCPAutoworkTools() {
}

static void _justamcp_free_autowork(Autowork *p_autowork) {
	if (!p_autowork) {
		return;
	}
	if (p_autowork->get_parent()) {
		p_autowork->get_parent()->remove_child(p_autowork);
	}
	if (p_autowork->is_inside_tree()) {
		p_autowork->queue_free();
		return;
	}
	memdelete(p_autowork);
}

static void _justamcp_pump_main_loop() {
	if (MessageQueue::get_singleton()) {
		MessageQueue::get_singleton()->flush();
	}
	MainLoop *loop = OS::get_singleton() ? OS::get_singleton()->get_main_loop() : nullptr;
	if (loop) {
		loop->process(0.016);
	}
	if (DisplayServer::get_singleton()) {
		DisplayServer::get_singleton()->process_events();
	}
}

static Dictionary _execute_autowork(Autowork *p_autowork, int p_timeout_sec) {
	Dictionary result;
	if (justamcp_is_cancel_requested()) {
		result["ok"] = false;
		result["error"] = "cancelled";
		p_autowork->abort();
		_justamcp_free_autowork(p_autowork);
		return result;
	}
	justamcp_report_progress(0, 1, "Running Autowork tests");

	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (tree && tree->get_root()) {
		p_autowork->set_name("MCP_AutoworkInstance");
		tree->get_root()->add_child(p_autowork);
	}

	p_autowork->set_json_output_path("user://autowork_results.json");
	const uint64_t started_usec = OS::get_singleton()->get_ticks_usec();
	const uint64_t max_usec = uint64_t(CLAMP(p_timeout_sec, 1, 120)) * 1000ULL * 1000ULL;
	p_autowork->run_tests();

	bool timed_out = false;
	while (!p_autowork->is_finished()) {
		if (justamcp_is_cancel_requested()) {
			p_autowork->abort();
			for (int i = 0; i < 8; i++) {
				_justamcp_pump_main_loop();
			}
			result["ok"] = false;
			result["error"] = "cancelled";
			result["timed_out"] = false;
			_justamcp_free_autowork(p_autowork);
			return result;
		}
		if ((OS::get_singleton()->get_ticks_usec() - started_usec) > max_usec) {
			timed_out = true;
			p_autowork->abort();
			for (int i = 0; i < 8; i++) {
				_justamcp_pump_main_loop();
			}
			break;
		}
		_justamcp_pump_main_loop();
		OS::get_singleton()->delay_usec(8000);
	}

	result["pass_count"] = p_autowork->get_pass_count();
	result["fail_count"] = p_autowork->get_fail_count();
	result["assert_count"] = p_autowork->get_assert_count();
	result["test_count"] = p_autowork->get_test_count();
	result["pending_count"] = p_autowork->get_pending_count();
	result["timed_out"] = timed_out;

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
	result["failures"] = failures;
	result["ok"] = !timed_out && int(result["fail_count"]) == 0;
	if (timed_out) {
		result["error"] = "Autowork run timed out before tests finished.";
		result["incomplete"] = true;
		Dictionary incomplete;
		incomplete["ok"] = false;
		incomplete["timed_out"] = true;
		incomplete["incomplete"] = true;
		incomplete["message"] = "Autowork run timed out before tests finished.";
		incomplete["pass_count"] = result["pass_count"];
		incomplete["fail_count"] = result["fail_count"];
		incomplete["test_count"] = result["test_count"];
		Ref<FileAccess> out = FileAccess::open("user://autowork_results.json", FileAccess::WRITE);
		if (out.is_valid()) {
			out->store_string(JSON::stringify(incomplete));
			out->close();
		}
	}

	_justamcp_free_autowork(p_autowork);

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
		Autowork *aw = Object::cast_to<Autowork>(child);
		if (aw && !aw->is_finished()) {
			return true;
		}
	}
	return false;
}

static Dictionary _mcp_tool_error(int p_code, const String &p_message) {
	Dictionary err;
	err["ok"] = false;
	Dictionary payload;
	payload["code"] = p_code;
	payload["message"] = p_message;
	err["error"] = payload;
	return err;
}

static Dictionary _nested_autowork_error() {
	return _mcp_tool_error(-32000, "Autowork is already running. Nested Autowork execution is blocked to avoid recursively launching the active test suite.");
}

static void _apply_autowork_collector_args(Ref<AutoworkCollector> p_collector, const Dictionary &p_args) {
	if (p_collector.is_null()) {
		return;
	}
	if (p_args.has("include_subdirs")) {
		p_collector->set_include_subdirectories(bool(p_args.get("include_subdirs", false)));
	}
	if (p_args.has("prefix")) {
		p_collector->set_script_prefix(String(p_args.get("prefix", "")));
	}
	if (p_args.has("suffix")) {
		p_collector->set_script_suffix(String(p_args.get("suffix", "")));
	}
	if (p_args.has("select")) {
		p_collector->script_pattern = String(p_args.get("select", "")).strip_edges();
	}
}

static Dictionary _autowork_is_running_result() {
	Dictionary result;
	result["ok"] = true;
	result["running"] = _is_autowork_already_running();
	return result;
}

static Dictionary _autowork_list_tests(const Dictionary &p_args) {
	String path = String(p_args.get("path", p_args.get("directory_path", p_args.get("test_script", p_args.get("script_path", ""))))).strip_edges();
	if (path.is_empty()) {
		path = DirAccess::exists("res://test") ? String("res://test") : String("res://");
	} else {
		String sandbox_error;
		if (!justamcp_canonical_sandbox_path(path, path, sandbox_error)) {
			return _mcp_tool_error(-32602, sandbox_error);
		}
	}

	Ref<AutoworkCollector> collector;
	collector.instantiate();
	_apply_autowork_collector_args(collector, p_args);
	if (DirAccess::exists(path) || path.ends_with("/")) {
		collector->process_directory(path);
	} else {
		collector->add_script(path);
	}

	Array scripts_out;
	Array collected = collector->get_scripts();
	for (int i = 0; i < collected.size(); i++) {
		Dictionary script_info = collected[i];
		Dictionary item;
		item["path"] = script_info.get("path", "");
		item["tests"] = script_info.get("tests", Array());
		scripts_out.push_back(item);
	}

	Dictionary result;
	result["ok"] = true;
	result["path"] = path;
	result["scripts"] = scripts_out;
	result["count"] = scripts_out.size();
	return result;
}

static int _autowork_timeout_sec(const Dictionary &p_args) {
	int timeout = 30;
	if (p_args.has("timeout")) {
		timeout = int(p_args.get("timeout", 30));
	}
	return CLAMP(timeout, 1, 120);
}

static Dictionary _autowork_read_results() {
	Dictionary result;
	const String json_path = "user://autowork_results.json";
	const String xml_path = "user://autowork_results.xml";
	if (FileAccess::exists(json_path)) {
		String text;
		int64_t size = 0;
		Dictionary read_err;
		if (!justamcp_read_utf8_within_limit(json_path, JUSTAMCP_MAX_SYNC_READ_BYTES, text, size, read_err)) {
			return read_err;
		}
		Ref<JSON> json;
		json.instantiate();
		if (json->parse(text) == OK && json->get_data().get_type() == Variant::DICTIONARY) {
			result = Dictionary(json->get_data());
		} else {
			result["raw"] = text;
		}
		const bool incomplete = bool(result.get("incomplete", false)) || bool(result.get("timed_out", false));
		result["ok"] = !incomplete && bool(result.get("ok", true));
		result["path"] = json_path;
		if (incomplete && !result.has("message")) {
			result["message"] = "Last Autowork run is incomplete.";
		}
		return result;
	}
	if (FileAccess::exists(xml_path)) {
		String text;
		int64_t size = 0;
		Dictionary read_err;
		if (!justamcp_read_utf8_within_limit(xml_path, JUSTAMCP_MAX_SYNC_READ_BYTES, text, size, read_err)) {
			return read_err;
		}
		result["ok"] = true;
		result["path"] = xml_path;
		result["raw"] = text;
		result["results"] = Array();
		return result;
	}
	result["ok"] = true;
	result["results"] = Array();
	result["message"] = "No Autowork results yet.";
	return result;
}

static Dictionary _run_autowork_from_args(const Dictionary &p_args, const String &p_mode) {
	if (_is_autowork_already_running()) {
		return _nested_autowork_error();
	}
	Autowork *aw = memnew(Autowork);
	_apply_autowork_collector_args(aw->get_test_collector(), p_args);
	String path = String(p_args.get("path", p_args.get("test_script", p_args.get("directory_path", p_args.get("script_path", ""))))).strip_edges();
	String filter = String(p_args.get("filter", p_args.get("test_name", ""))).strip_edges();
	if (!path.is_empty()) {
		String sandbox_error;
		if (!justamcp_canonical_sandbox_path(path, path, sandbox_error)) {
			_justamcp_free_autowork(aw);
			return _mcp_tool_error(-32602, sandbox_error);
		}
		if (DirAccess::exists(path) || path.ends_with("/")) {
			aw->add_directory(path);
		} else {
			aw->add_script(path);
		}
	} else if (p_mode == "by_name") {
		aw->add_directory("res://");
	} else if (DirAccess::exists("res://test")) {
		aw->add_directory("res://test");
	} else {
		aw->add_directory("res://");
	}
	if (!filter.is_empty()) {
		aw->set_test(filter);
	}
	String junit_xml = String(p_args.get("junit_xml", "")).strip_edges();
	if (!junit_xml.is_empty()) {
		String sandbox_error;
		if (!justamcp_canonical_sandbox_path(junit_xml, junit_xml, sandbox_error)) {
			_justamcp_free_autowork(aw);
			return _mcp_tool_error(-32602, sandbox_error);
		}
		if (!junit_xml.begins_with("user://")) {
			_justamcp_free_autowork(aw);
			return _mcp_tool_error(-32602, "junit_xml must stay under user://.");
		}
		aw->set_xml_output_path(junit_xml);
	}
	return _execute_autowork(aw, _autowork_timeout_sec(p_args));
}

Dictionary JustAMCPAutoworkTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "blazium_autowork_run_all_tests" || p_tool_name == "autowork_run_all_tests" ||
			p_tool_name == "runtime_run_autowork_tests" || p_tool_name == "blazium_runtime_run_autowork_tests") {
		return _run_autowork_from_args(p_args, "all");
	}
	if (p_tool_name == "blazium_autowork_run_tests_in_directory" || p_tool_name == "autowork_run_tests_in_directory") {
		if (!p_args.has("directory_path") && !p_args.has("path")) {
			return _mcp_tool_error(-32602, "Missing required parameter for tool " + p_tool_name + ": directory_path");
		}
		return _run_autowork_from_args(p_args, "directory");
	}
	if (p_tool_name == "blazium_autowork_run_test_script" || p_tool_name == "autowork_run_test_script") {
		if (!p_args.has("script_path") && !p_args.has("path") && !p_args.has("test_script")) {
			return _mcp_tool_error(-32602, "Missing required parameter for tool " + p_tool_name + ": script_path");
		}
		return _run_autowork_from_args(p_args, "script");
	}
	if (p_tool_name == "blazium_autowork_run_test_by_name" || p_tool_name == "autowork_run_test_by_name") {
		if (!p_args.has("test_name") && !p_args.has("filter")) {
			return _mcp_tool_error(-32602, "Missing required parameter for tool " + p_tool_name + ": test_name");
		}
		return _run_autowork_from_args(p_args, "by_name");
	}
	if (p_tool_name == "runtime_get_test_results" || p_tool_name == "blazium_runtime_get_test_results") {
		return _autowork_read_results();
	}
	if (p_tool_name == "autowork_list_tests" || p_tool_name == "blazium_autowork_list_tests") {
		return _autowork_list_tests(p_args);
	}
	if (p_tool_name == "autowork_is_running" || p_tool_name == "blazium_autowork_is_running") {
		return _autowork_is_running_result();
	}

	return Dictionary();
}

#endif
#endif
