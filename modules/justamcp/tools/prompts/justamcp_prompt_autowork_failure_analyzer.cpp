/**************************************************************************/
/*  justamcp_prompt_autowork_failure_analyzer.cpp                         */
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

#include "justamcp_prompt_autowork_failure_analyzer.h"

void JustAMCPPromptAutoworkFailureAnalyzer::_bind_methods() {}

JustAMCPPromptAutoworkFailureAnalyzer::JustAMCPPromptAutoworkFailureAnalyzer() {}
JustAMCPPromptAutoworkFailureAnalyzer::~JustAMCPPromptAutoworkFailureAnalyzer() {}

String JustAMCPPromptAutoworkFailureAnalyzer::get_name() const {
	return "analyze_autowork_test_failures";
}

Dictionary JustAMCPPromptAutoworkFailureAnalyzer::get_prompt() const {
	Dictionary result;
	result["name"] = "analyze_autowork_test_failures";
	result["title"] = "Blazium Autowork Failure Analyzer";
	result["description"] = "Diagnoses failing Autowork test output or Godot stack traces and returns structured root-cause analysis with corrected GDScript.";

	Array arguments;
	Dictionary output_log;
	output_log["name"] = "test_output";
	output_log["description"] = "The raw Autowork failure log text or Godot output stack trace.";
	output_log["required"] = true;
	arguments.push_back(output_log);

	Dictionary script_content;
	script_content["name"] = "test_script_content";
	script_content["description"] = "The full content of the failing .gd test script for deeper analysis.";
	script_content["required"] = false;
	arguments.push_back(script_content);

	result["arguments"] = arguments;
	return result;
}

Dictionary JustAMCPPromptAutoworkFailureAnalyzer::get_messages(const Dictionary &p_args) {
	Dictionary result;
	result["description"] = "Autowork Test Failure Analysis";

	String log = p_args.has("test_output") ? String(p_args["test_output"]) : "[Missing Error Trace]";
	String script_content = p_args.has("test_script_content") ? String(p_args["test_script_content"]) : "";

	Array messages;
	Dictionary msg;
	msg["role"] = "user";

	Dictionary content;
	content["type"] = "text";

	String text = String("You are a senior Blazium QA engineer. Diagnose this Autowork test failure and return structured fixes.\n\n");
	text += String("Error Trace:\n") + log + String("\n\n");

	if (!script_content.is_empty()) {
		text += String("Failing Test Script:\n") + script_content + String("\n\n");
	}

	text += String("Check for ALL of these Godot-specific failure patterns:\n\n");

	text += String("## Failure Pattern 1: Null Instance\n");
	text += String("Symptom: 'Invalid get index on base null instance'\n");
	text += String("Cause: The subject was not added to the scene tree, so _ready() never fired.\n");
	text += String("Fix:\n");
	text += String("    BAD:  var subject = TargetClass.new()\n");
	text += String("    GOOD: func _before_each():\n");
	text += String("              subject = TargetClass.new()\n");
	text += String("              add_child(subject)  # This triggers _ready()\n\n");

	text += String("## Failure Pattern 2: assert_eq Type Mismatch\n");
	text += String("Symptom: assert_eq fails even though values look equal\n");
	text += String("Cause: Comparing incompatible Godot types (String vs StringName, int vs float).\n");
	text += String("Fix:\n");
	text += String("    BAD:  assert_eq(node.name, \"Player\")\n");
	text += String("    GOOD: assert_eq(String(node.name), \"Player\")\n");
	text += String("    BAD:  assert_eq(health, 100)\n");
	text += String("    GOOD: assert_almost_eq(float(health), 100.0, 0.001)\n\n");

	text += String("## Failure Pattern 3: Signal Await Hang\n");
	text += String("Symptom: Test runner stalls indefinitely or times out\n");
	text += String("Cause: 'await signal' blocks forever if the signal is never emitted.\n");
	text += String("Fix:\n");
	text += String("    BAD:  await subject.some_signal\n");
	text += String("    GOOD: var signal_received: bool = false\n");
	text += String("          subject.some_signal.connect(func(): signal_received = true)\n");
	text += String("          subject.trigger()\n");
	text += String("          assert_true(signal_received, \"signal should have been emitted\")\n\n");

	text += String("## Failure Pattern 4: Orphaned Nodes Warning\n");
	text += String("Symptom: 'ObjectDB instances leaked at exit' or node count warnings\n");
	text += String("Cause: _after_each() does not clean up the instantiated subject.\n");
	text += String("Fix:\n");
	text += String("    GOOD: func _after_each():\n");
	text += String("              subject.queue_free()\n");
	text += String("              subject = null\n\n");

	text += String("## Failure Pattern 5: Float Precision Mismatch\n");
	text += String("Symptom: assert_eq fails on calculated float values\n");
	text += String("Cause: Floating-point arithmetic produces microsecond differences.\n");
	text += String("Fix:\n");
	text += String("    BAD:  assert_eq(result_velocity.x, 200.0)\n");
	text += String("    GOOD: assert_almost_eq(result_velocity.x, 200.0, 0.001)\n\n");

	text += String("## Required Output\n");
	text += String("1. Failure summary - which tests failed and the error category for each.\n");
	text += String("2. Root-cause explanation - what Godot behavior caused each failure.\n");
	text += String("3. Corrected GDScript - drop-in fix for each failing test case.");

	content["text"] = text;
	msg["content"] = content;

	messages.push_back(msg);
	result["messages"] = messages;
	result["ok"] = true;
	return result;
}

Dictionary JustAMCPPromptAutoworkFailureAnalyzer::complete(const Dictionary &p_argument) {
	Dictionary completion;
	completion["values"] = Array();
	completion["total"] = 0;
	completion["hasMore"] = false;
	return completion;
}

#endif
