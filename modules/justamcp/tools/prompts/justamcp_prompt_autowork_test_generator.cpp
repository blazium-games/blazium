/**************************************************************************/
/*  justamcp_prompt_autowork_test_generator.cpp                           */
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

#include "justamcp_prompt_autowork_test_generator.h"

void JustAMCPPromptAutoworkTestGenerator::_bind_methods() {}

JustAMCPPromptAutoworkTestGenerator::JustAMCPPromptAutoworkTestGenerator() {}
JustAMCPPromptAutoworkTestGenerator::~JustAMCPPromptAutoworkTestGenerator() {}

String JustAMCPPromptAutoworkTestGenerator::get_name() const {
	return "generate_autowork_test";
}

Dictionary JustAMCPPromptAutoworkTestGenerator::get_prompt() const {
	Dictionary result;
	result["name"] = "generate_autowork_test";
	result["title"] = "Blazium Autowork Test Generator";
	result["description"] = "Generates a complete Autowork test_*.gd suite for a given GDScript target, mapping public methods to TestContext assertion cases.";

	Array arguments;
	Dictionary target_script;
	target_script["name"] = "target_script";
	target_script["description"] = "The res:// path to the GDScript file to generate tests for.";
	target_script["required"] = true;
	arguments.push_back(target_script);

	Dictionary functions;
	functions["name"] = "functions";
	functions["description"] = "Comma-separated list of specific function names to test. Tests all public functions if omitted.";
	functions["required"] = false;
	arguments.push_back(functions);

	result["arguments"] = arguments;
	return result;
}

Dictionary JustAMCPPromptAutoworkTestGenerator::get_messages(const Dictionary &p_args) {
	Dictionary result;
	result["description"] = "Blazium Autowork Test Generation";

	String target = p_args.has("target_script") ? String(p_args["target_script"]) : "[Target Script]";
	String functions = p_args.has("functions") ? String(p_args["functions"]) : "";

	Array messages;
	Dictionary msg;
	msg["role"] = "user";

	Dictionary content;
	content["type"] = "text";

	String text = String("You are a senior Blazium QA engineer. Generate a complete Autowork test suite for: ") + target + String("\n\n");

	if (!functions.is_empty()) {
		text += String("Focus ONLY on these functions: ") + functions + String("\n\n");
	}

	text += String("## Required File Structure\n");
	text += String("- File name must use the prefix 'test_': test_inventory.gd, test_player.gd\n");
	text += String("- Place in a 'tests/' directory adjacent to the source file, or in res://tests/\n");
	text += String("- Root class must 'extends TestContext' (not Node or RefCounted)\n\n");

	text += String("## Required Test Structure Template\n");
	text += String("extends TestContext\n\n");
	text += String("var subject: TargetClass\n\n");
	text += String("func _before_each() -> void:\n");
	text += String("    subject = TargetClass.new()\n");
	text += String("    add_child(subject)  # Required for _ready() to fire\n\n");
	text += String("func _after_each() -> void:\n");
	text += String("    subject.queue_free()\n");
	text += String("    subject = null\n\n");
	text += String("func test_method_returns_expected_value() -> void:\n");
	text += String("    # Arrange\n");
	text += String("    var input: int = 5\n");
	text += String("    var expected: int = 10\n");
	text += String("    # Act\n");
	text += String("    var result: int = subject.method(input)\n");
	text += String("    # Assert\n");
	text += String("    assert_eq(result, expected, \"method should double the input\")\n\n");

	text += String("## Available Assertions\n");
	text += String("assert_eq(a, b, msg)                   - asserts a == b\n");
	text += String("assert_ne(a, b, msg)                   - asserts a != b\n");
	text += String("assert_true(condition, msg)            - asserts condition is true\n");
	text += String("assert_false(condition, msg)           - asserts condition is false\n");
	text += String("assert_null(value, msg)                - asserts value is null\n");
	text += String("assert_not_null(value, msg)            - asserts value is not null\n");
	text += String("assert_almost_eq(a, b, tolerance, msg) - for float comparisons\n\n");

	text += String("## Critical Rules\n");
	text += String("1. ALWAYS call add_child(subject) in _before_each() so _ready() fires.\n");
	text += String("2. ALWAYS call subject.queue_free() in _after_each() to prevent orphaned nodes.\n");
	text += String("3. Use assert_almost_eq() for all float comparisons, NOT assert_eq().\n");
	text += String("4. For signal testing: connect a lambda and check a bool flag, do not use bare 'await signal'.\n");
	text += String("5. Use static types on ALL local test variables.\n");
	text += String("6. Name test functions descriptively: test_add_item_increases_count(), not test1().\n\n");

	text += String("## Naming Convention\n");
	text += String("Run with Autowork CLI: blazium --aw-dir=res://tests/ --headless\n");
	text += String("Or in the Autowork editor panel within Blazium.");

	content["text"] = text;
	msg["content"] = content;

	messages.push_back(msg);
	result["messages"] = messages;
	result["ok"] = true;
	return result;
}

Dictionary JustAMCPPromptAutoworkTestGenerator::complete(const Dictionary &p_argument) {
	Dictionary completion;
	completion["values"] = Array();
	completion["total"] = 0;
	completion["hasMore"] = false;
	return completion;
}

#endif
