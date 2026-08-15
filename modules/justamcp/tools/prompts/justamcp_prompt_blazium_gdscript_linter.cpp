/**************************************************************************/
/*  justamcp_prompt_blazium_gdscript_linter.cpp                           */
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

#include "justamcp_prompt_blazium_gdscript_linter.h"

void JustAMCPPromptBlaziumGDScriptLinter::_bind_methods() {}

JustAMCPPromptBlaziumGDScriptLinter::JustAMCPPromptBlaziumGDScriptLinter() {}
JustAMCPPromptBlaziumGDScriptLinter::~JustAMCPPromptBlaziumGDScriptLinter() {}

String JustAMCPPromptBlaziumGDScriptLinter::get_name() const {
	return "blazium_gdscript_linter";
}

Dictionary JustAMCPPromptBlaziumGDScriptLinter::get_prompt() const {
	Dictionary result;
	result["name"] = "blazium_gdscript_linter";
	result["title"] = "Blazium GDScript Linter";
	result["description"] = "Audits and rewrites a GDScript file enforcing Godot 4.x / Blazium full static typing, typed arrays, @export, @onready, and typed signal conventions.";

	Array arguments;
	Dictionary target;
	target["name"] = "script_path";
	target["description"] = "The res:// path to the GDScript file to audit and rewrite.";
	target["required"] = true;
	arguments.push_back(target);

	result["arguments"] = arguments;
	return result;
}

Dictionary JustAMCPPromptBlaziumGDScriptLinter::get_messages(const Dictionary &p_args) {
	Dictionary result;
	result["description"] = "Blazium GDScript Linting Pass";

	String target = p_args.has("script_path") ? String(p_args["script_path"]) : "[Target Script]";

	Array messages;
	Dictionary msg;
	msg["role"] = "user";

	Dictionary content;
	content["type"] = "text";

	String text = String("You are a senior Blazium developer performing a strict GDScript code quality audit on: ") + target + String("\n\n");
	text += String("Treat ANY dynamically typed declaration as an error. Apply ALL of the following rules without exception:\n\n");

	text += String("## Variables - All must have explicit types\n");
	text += String("BAD:  var speed = 200\n");
	text += String("GOOD: var speed: float = 200.0\n\n");

	text += String("## Arrays - All must be typed\n");
	text += String("BAD:  var enemies = []\n");
	text += String("GOOD: var enemies: Array[Node2D] = []\n\n");

	text += String("## Functions - All parameters and return types must be declared\n");
	text += String("BAD:  func take_damage(amount):\n");
	text += String("GOOD: func take_damage(amount: int) -> void:\n\n");

	text += String("## Node References - Always @onready, never in-body $\n");
	text += String("BAD:  var sprite = $Sprite2D\n");
	text += String("GOOD: @onready var sprite: Sprite2D = $Sprite2D\n\n");

	text += String("## Exported Properties - Always @export with type\n");
	text += String("BAD:  var max_health = 100\n");
	text += String("GOOD: @export var max_health: int = 100\n\n");

	text += String("## Signal Declarations - Always typed parameters\n");
	text += String("BAD:  signal health_changed\n");
	text += String("GOOD: signal health_changed(new_health: int)\n\n");

	text += String("## Constants - Use const with type annotation\n");
	text += String("BAD:  var GRAVITY = 980\n");
	text += String("GOOD: const GRAVITY: float = 980.0\n\n");

	text += String("## Required Output\n");
	text += String("1. The fully rewritten GDScript file with all types applied.\n");
	text += String("2. A change summary listing every variable and function that was updated.\n");
	text += String("3. Any logical issues spotted during the audit (signal never connected, variable shadowing, etc).");

	content["text"] = text;
	msg["content"] = content;

	messages.push_back(msg);
	result["messages"] = messages;
	result["ok"] = true;
	return result;
}

Dictionary JustAMCPPromptBlaziumGDScriptLinter::complete(const Dictionary &p_argument) {
	Dictionary completion;
	completion["values"] = Array();
	completion["total"] = 0;
	completion["hasMore"] = false;
	return completion;
}

#endif
