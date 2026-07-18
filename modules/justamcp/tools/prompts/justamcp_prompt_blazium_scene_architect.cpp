/**************************************************************************/
/*  justamcp_prompt_blazium_scene_architect.cpp                           */
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

#include "justamcp_prompt_blazium_scene_architect.h"

void JustAMCPPromptBlaziumSceneArchitect::_bind_methods() {}

JustAMCPPromptBlaziumSceneArchitect::JustAMCPPromptBlaziumSceneArchitect() {}
JustAMCPPromptBlaziumSceneArchitect::~JustAMCPPromptBlaziumSceneArchitect() {}

String JustAMCPPromptBlaziumSceneArchitect::get_name() const {
	return "blazium_scene_architect";
}

Dictionary JustAMCPPromptBlaziumSceneArchitect::get_prompt() const {
	Dictionary result;
	result["name"] = "blazium_scene_architect";
	result["title"] = "Blazium Scene Architect";
	result["description"] = "Designs a well-structured Godot 4 scene hierarchy using composition, PackedScene encapsulation, and signal-driven communication.";

	Array arguments;
	Dictionary target;
	target["name"] = "target_mechanic";
	target["description"] = "The gameplay feature or system to design (e.g. 'Player Character', 'Inventory System', 'Enemy AI with Patrol').";
	target["required"] = true;
	arguments.push_back(target);

	Dictionary existing;
	existing["name"] = "existing_code";
	existing["description"] = "Optional existing GDScript or scene structure to evaluate and refactor.";
	existing["required"] = false;
	arguments.push_back(existing);

	result["arguments"] = arguments;
	return result;
}

Dictionary JustAMCPPromptBlaziumSceneArchitect::get_messages(const Dictionary &p_args) {
	Dictionary result;
	result["description"] = "Blazium Scene Architecture Design";

	String target = p_args.has("target_mechanic") ? String(p_args["target_mechanic"]) : "[Mechanic]";
	String existing = p_args.has("existing_code") ? String(p_args["existing_code"]) : "";

	Array messages;
	Dictionary msg;
	msg["role"] = "user";

	Dictionary content;
	content["type"] = "text";

	String text = String("You are a senior Blazium game developer. Design a complete, idiomatic Godot 4 scene hierarchy for: ") + target + String("\n\n");

	if (!existing.is_empty()) {
		text += String("Existing code to evaluate and improve:\n") + existing + String("\n\n");
	}

	text += String("ARCHITECTURE RULES - enforce all of the following without exception:\n\n");

	text += String("## Rule 1: Composition over Inheritance\n");
	text += String("- Do NOT suggest class chains deeper than one level.\n");
	text += String("- A Player is NOT an Entity is NOT a MovingObject.\n");
	text += String("- Use small specialized child nodes (Health, Movement, StateMachine) instead of large base classes.\n");
	text += String("- Example: Player scene contains Health (Node), Movement (Node), Inventory (Node) as children.\n\n");

	text += String("## Rule 2: Call Down, Signal Up\n");
	text += String("- Parent nodes MAY call child.method() directly.\n");
	text += String("- Child nodes must NEVER call parent.method() - they emit a signal instead.\n");
	text += String("- Correct: health_component.died.connect(_on_player_died) wired in parent _ready().\n");
	text += String("- Incorrect: health_component calling get_parent().game_over() directly.\n\n");

	text += String("## Rule 3: PackedScene Encapsulation\n");
	text += String("- Any object spawned at runtime (enemies, projectiles, pickups) MUST be its own .tscn file.\n");
	text += String("- Expose the scene's public API via @export properties and declared signals only.\n");
	text += String("- Never access internals of an instanced scene from outside it.\n\n");

	text += String("## Rule 4: Node Responsibility\n");
	text += String("- Each node does exactly one job.\n");
	text += String("- A Health node manages health. A Movement node manages movement.\n");
	text += String("- The root node orchestrates children via signal connections in _ready().\n\n");

	text += String("## Required Output Format\n");
	text += String("Return exactly three sections:\n");
	text += String("1. Scene Tree - node names and GDScript types in a hierarchy diagram.\n");
	text += String("2. Signal Map - which node emits each signal and which node connects to it.\n");
	text += String("3. GDScript Stubs - for each node: @export properties, signal declarations, and public function signatures with types.");

	content["text"] = text;
	msg["content"] = content;

	messages.push_back(msg);
	result["messages"] = messages;
	result["ok"] = true;
	return result;
}

Dictionary JustAMCPPromptBlaziumSceneArchitect::complete(const Dictionary &p_argument) {
	Dictionary completion;
	completion["values"] = Array();
	completion["total"] = 0;
	completion["hasMore"] = false;
	return completion;
}

#endif
