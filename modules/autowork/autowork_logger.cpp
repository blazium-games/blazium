/**************************************************************************/
/*  autowork_logger.cpp                                                   */
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

#include "core/object/class_db.h"
#include "autowork_logger.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/os/os.h"

void AutoworkLogger::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_pass", "message"), &AutoworkLogger::add_pass);
	ClassDB::bind_method(D_METHOD("add_fail", "message"), &AutoworkLogger::add_fail);
	ClassDB::bind_method(D_METHOD("add_warning", "message"), &AutoworkLogger::add_warning);
	ClassDB::bind_method(D_METHOD("inc_orphans", "count"), &AutoworkLogger::inc_orphans);
	ClassDB::bind_method(D_METHOD("inc_test_count"), &AutoworkLogger::inc_test_count);
	ClassDB::bind_method(D_METHOD("print_summary"), &AutoworkLogger::print_summary);
	ClassDB::bind_method(D_METHOD("export_json", "file_path"), &AutoworkLogger::export_json);
	ClassDB::bind_method(D_METHOD("export_xml", "file_path"), &AutoworkLogger::export_xml);
}

AutoworkLogger::AutoworkLogger() {
}

AutoworkLogger::~AutoworkLogger() {
}

void AutoworkLogger::begin_test(const StringName &p_script, const StringName &p_method) {
	if (current_test.script_name != p_script) {
		print_log(vformat("\n[b]=== Script: %s ===[/b]", p_script));
	}

	current_test.script_name = p_script;
	current_test.method_name = p_method;
	current_test.passes = 0;
	current_test.fails = 0;
	current_test.orphans = 0;
	current_test.fail_messages.clear();

	print_log(vformat("  [b]%s[/b]", p_method));
}

void AutoworkLogger::end_test() {
	test_results.push_back(current_test);
	current_test.script_name = StringName();
	current_test.method_name = StringName();
	current_test.passes = 0;
	current_test.fails = 0;
	current_test.orphans = 0;
	current_test.fail_messages.clear();
}

void AutoworkLogger::set_output_ui(Object *p_ui) {
	output_ui = p_ui;
}

void AutoworkLogger::print_log(const String &p_msg, bool p_error) {
	String bbmsg = p_msg;
	String console_msg = p_msg;

	// UI bbcode formatting
	if (p_error || p_msg.contains("[FAIL]")) {
		bbmsg = "[color=red]" + p_msg + "[/color]";
		console_msg = "\033[31;1m" + p_msg + "\033[0m";
	} else if (p_msg.contains("[PASS]")) {
		bbmsg = "[color=green]" + p_msg + "[/color]";
		console_msg = "\033[32;1m" + p_msg + "\033[0m";
	} else if (p_msg.contains("[WARN]")) {
		bbmsg = "[color=yellow]" + p_msg + "[/color]";
		console_msg = "\033[33;1m" + p_msg + "\033[0m";
	}

	if (p_error) {
		print_error(p_msg); // Error stream gets plain text natively
	} else {
		// Drop the literal [b] from script titles on console
		console_msg = console_msg.replace("[b]", "\033[1m").replace("[/b]", "\033[0m");
		print_line(console_msg);
	}

	if (output_ui && output_ui->has_method("append_text")) {
		output_ui->call("append_text", bbmsg + "\n");
	}
}

void AutoworkLogger::add_pass(const String &p_message) {
	passes++;
	current_test.passes++;
	print_log(vformat("\t\t[PASS] %s", p_message));
}

void AutoworkLogger::add_fail(const String &p_message) {
	fails++;
	current_test.fails++;
	current_test.fail_messages.push_back(p_message);
	print_log(vformat("\t\t[FAIL] %s", p_message), true);
}

void AutoworkLogger::add_warning(const String &p_message) {
	warnings++;
	print_log(vformat("\t\t[WARN] %s", p_message));
}

void AutoworkLogger::inc_orphans(int p_count) {
	orphans += p_count;
	current_test.orphans += p_count;
}

void AutoworkLogger::inc_test_count() {
	test_count++;
}

void AutoworkLogger::print_summary() {
	print_log("\n=================================");
	print_log("AUTOWORK TEST SUMMARY");
	print_log(vformat("Tests Run: %d", test_count));
	print_log(vformat("Passing Asserts: %d", passes));
	if (fails > 0) {
		print_log(vformat("Failing Asserts: %d", fails));
	} else {
		print_log("Failing Asserts: None!");
	}
	if (warnings > 0) {
		print_log(vformat("Warnings: %d", warnings));
	}
	if (!hide_orphans && orphans > 0) {
		print_log(vformat("Orphaned Objects: %d", orphans));
	}
	print_log("=================================\n");
}

bool AutoworkLogger::export_json(const String &p_file_path) {
	Dictionary root;
	root["test_count"] = get_test_count();
	root["pass_count"] = get_passes();
	root["fail_count"] = get_fails();
	root["warning_count"] = get_warnings();
	root["orphan_count"] = get_orphans();

	const Vector<AutoworkTestMethodResult> &results = get_test_results();

	// Group by script
	HashMap<StringName, Array> script_map;
	for (int i = 0; i < results.size(); i++) {
		const AutoworkTestMethodResult &res = results[i];
		Dictionary t_dict;
		t_dict["name"] = res.method_name;
		t_dict["passes"] = res.passes;
		t_dict["fails"] = res.fails;
		t_dict["orphans"] = res.orphans;

		Array errors_arr;
		for (int k = 0; k < res.fail_messages.size(); k++) {
			errors_arr.push_back(res.fail_messages[k]);
		}
		t_dict["errors"] = errors_arr;
		if (!script_map.has(res.script_name)) {
			script_map.insert(res.script_name, Array());
		}
		script_map[res.script_name].push_back(t_dict);
	}

	Array scripts_arr;
	for (const KeyValue<StringName, Array> &E : script_map) {
		Dictionary s_dict;
		s_dict["name"] = E.key;
		s_dict["tests"] = E.value;
		scripts_arr.push_back(s_dict);
	}
	root["scripts"] = scripts_arr;

	Ref<FileAccess> file = FileAccess::open(p_file_path, FileAccess::WRITE);
	if (file.is_null()) {
		ERR_PRINT("AutoworkLogger: Cannot open path for writing: " + p_file_path);
		return false;
	}

	file->store_string(JSON::stringify(root, "  "));

	return true;
}

bool AutoworkLogger::export_xml(const String &p_file_path) {
	if (p_file_path.is_empty()) {
		return false;
	}

	Ref<FileAccess> file = FileAccess::open(p_file_path, FileAccess::WRITE);
	if (file.is_null()) {
		ERR_PRINT("AutoworkLogger: Cannot open path for writing: " + p_file_path);
		return false;
	}

	int total_tests = get_test_count();
	int total_failures = get_fails();

	file->store_string("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	file->store_string(vformat("<testsuites name=\"AutoworkTests\" failures=\"%d\" tests=\"%d\">\n", total_failures, total_tests));

	// We export a single suite since the Logger doesn't deeply segregate script boundaries internally for simplistic summary reporting yet
	file->store_string(_xml_indent(1) + vformat("<testsuite name=\"summary\" tests=\"%d\" failures=\"%d\" skipped=\"%d\" time=\"0.0\">\n", total_tests, total_failures, get_warnings()));

	// Instead of deep iteration for now, we just log a generic testcase based on the summary so CI doesn't crash on parse
	// A complete mapping would require logger tracking individual assert vectors
	if (total_failures > 0) {
		file->store_string(_xml_indent(2) + vformat("<testcase name=\"failing_tests\" assertions=\"%d\" status=\"fail\" classname=\"summary\" time=\"0.0\">\n", total_failures));
		file->store_string(_xml_indent(3) + "<failure message=\"failed\"><![CDATA[There were failing tests in the suite.]]></failure>\n");
		file->store_string(_xml_indent(2) + "</testcase>\n");
	}

	if (get_passes() > 0) {
		file->store_string(_xml_indent(2) + vformat("<testcase name=\"passing_tests\" assertions=\"%d\" status=\"pass\" classname=\"summary\" time=\"0.0\">\n", get_passes()));
		file->store_string(_xml_indent(2) + "</testcase>\n");
	}

	file->store_string(_xml_indent(1) + "</testsuite>\n");
	file->store_string("</testsuites>\n");

	return true;
}

String AutoworkLogger::_xml_indent(int p_level) {
	return String("  ").repeat(p_level);
}
