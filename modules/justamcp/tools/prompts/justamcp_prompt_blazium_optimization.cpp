/**************************************************************************/
/*  justamcp_prompt_blazium_optimization.cpp                              */
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

#include "justamcp_prompt_blazium_optimization.h"

void JustAMCPPromptBlaziumOptimization::_bind_methods() {}

JustAMCPPromptBlaziumOptimization::JustAMCPPromptBlaziumOptimization() {}
JustAMCPPromptBlaziumOptimization::~JustAMCPPromptBlaziumOptimization() {}

String JustAMCPPromptBlaziumOptimization::get_name() const {
	return "blazium_project_optimization";
}

Dictionary JustAMCPPromptBlaziumOptimization::get_prompt() const {
	Dictionary result;
	result["name"] = "blazium_project_optimization";
	result["title"] = "Blazium Project Optimizer";
	result["description"] = "Scans a GDScript file or scene for Godot performance anti-patterns and returns targeted drop-in fixes with explanations.";

	Array arguments;
	Dictionary target;
	target["name"] = "target_script";
	target["description"] = "res:// path to a specific GDScript file to optimize. Omit to analyze the currently active script.";
	target["required"] = false;
	arguments.push_back(target);

	result["arguments"] = arguments;
	return result;
}

Dictionary JustAMCPPromptBlaziumOptimization::get_messages(const Dictionary &p_args) {
	Dictionary result;
	result["description"] = "Blazium Project Optimization Pass";

	String target = p_args.has("target_script") ? String(p_args["target_script"]) : "[Current Script]";

	Array messages;
	Dictionary msg;
	msg["role"] = "user";

	Dictionary content;
	content["type"] = "text";

	String text = String("You are a senior Blazium performance engineer. Analyze the following target for Godot anti-patterns and return targeted fixes: ") + target + String("\n\n");
	text += String("Detect and fix ALL of the following patterns:\n\n");

	text += String("## Anti-Pattern 1: Node Lookups Inside _process()\n");
	text += String("BAD:  func _process(delta): $AnimationPlayer.play('run')\n");
	text += String("GOOD: @onready var anim: AnimationPlayer = $AnimationPlayer\n");
	text += String("      func _process(delta): anim.play('run')\n");
	text += String("Rule: ALL node references must be cached in @onready properties, never fetched at runtime.\n\n");

	text += String("## Anti-Pattern 2: State Polling Instead of Signals\n");
	text += String("BAD:  func _process(delta): if player.health <= 0: game_over()\n");
	text += String("GOOD: func _ready(): player.died.connect(_on_player_died)\n");
	text += String("      func _on_player_died(): game_over()\n");
	text += String("Rule: Use signals for event-driven state changes. Never poll state in _process().\n\n");

	text += String("## Anti-Pattern 3: Non-Physics Logic in _physics_process()\n");
	text += String("BAD:  func _physics_process(delta): update_hud()\n");
	text += String("GOOD: Move non-physics logic to _process() or signal handlers.\n");
	text += String("Rule: _physics_process() is exclusively for CharacterBody movement and physics queries.\n\n");

	text += String("## Anti-Pattern 4: Memory Leaks - Missing queue_free()\n");
	text += String("BAD:  func _on_enemy_died(): enemy.hide()\n");
	text += String("GOOD: func _on_enemy_died(): enemy.queue_free()\n");
	text += String("Rule: Every dynamically instantiated node must have a corresponding queue_free() call.\n\n");

	text += String("## Anti-Pattern 5: Deep Inheritance Chains\n");
	text += String("BAD:  class_name Boss extends Enemy  # Enemy extends Character extends Entity\n");
	text += String("GOOD: class_name Boss extends CharacterBody2D\n");
	text += String("      # Boss scene has Health, Movement, BossAI as child nodes\n");
	text += String("Rule: Maximum one level of class inheritance. Use composition for shared behavior.\n\n");

	text += String("## Anti-Pattern 6: _ready() Fragile Node Paths\n");
	text += String("BAD:  var hud = get_node(\"/root/Game/UI/HUD\")\n");
	text += String("GOOD: Use signals or @export var hud: HUD to inject the reference.\n\n");

	text += String("## Required Output\n");
	text += String("1. A numbered list of every detected issue with its file location and severity (Warning / Error / Info).\n");
	text += String("2. Drop-in replacement code for each detected anti-pattern.\n");
	text += String("3. A one-sentence explanation of why each change improves performance or reliability.");

	content["text"] = text;
	msg["content"] = content;

	messages.push_back(msg);
	result["messages"] = messages;
	result["ok"] = true;
	return result;
}

Dictionary JustAMCPPromptBlaziumOptimization::complete(const Dictionary &p_argument) {
	Dictionary completion;
	completion["values"] = Array();
	completion["total"] = 0;
	completion["hasMore"] = false;
	return completion;
}

#endif
