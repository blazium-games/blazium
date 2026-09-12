/**************************************************************************/
/*  justamcp_prompt_blazium_ui_scaffolder.cpp                             */
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

#include "justamcp_prompt_blazium_ui_scaffolder.h"

void JustAMCPPromptBlaziumUIScaffolder::_bind_methods() {}

JustAMCPPromptBlaziumUIScaffolder::JustAMCPPromptBlaziumUIScaffolder() {}
JustAMCPPromptBlaziumUIScaffolder::~JustAMCPPromptBlaziumUIScaffolder() {}

String JustAMCPPromptBlaziumUIScaffolder::get_name() const {
	return "blazium_ui_scaffolder";
}

Dictionary JustAMCPPromptBlaziumUIScaffolder::get_prompt() const {
	Dictionary result;
	result["name"] = "blazium_ui_scaffolder";
	result["title"] = "Blazium UI Scaffolder";
	result["description"] = "Generates responsive Godot 4 UI layouts using Container nodes, anchors, and theme overrides. Never outputs hardcoded position or size values.";

	Array arguments;
	Dictionary target;
	target["name"] = "ui_concept";
	target["description"] = "Plain-language description of the interface to generate (e.g. 'Game HUD with health bar and minimap', 'Main menu with 3 buttons').";
	target["required"] = true;
	arguments.push_back(target);

	result["arguments"] = arguments;
	return result;
}

Dictionary JustAMCPPromptBlaziumUIScaffolder::get_messages(const Dictionary &p_args) {
	Dictionary result;
	result["description"] = "Blazium UI Scaffold Output";

	String target = p_args.has("ui_concept") ? String(p_args["ui_concept"]) : "[UI Concept]";

	Array messages;
	Dictionary msg;
	msg["role"] = "user";

	Dictionary content;
	content["type"] = "text";

	String text = String("You are a senior Blazium UI developer. Generate a complete, responsive Godot 4 UI scene for: ") + target + String("\n\n");
	text += String("STRICT CONSTRAINT: You must NEVER output hardcoded position or size values. Every layout measurement flows through Godot's Container system.\n\n");

	text += String("## Rule 1: Container Hierarchy\n");
	text += String("- Top-level node must be Control or CanvasLayer.\n");
	text += String("- All content must use VBoxContainer, HBoxContainer, GridContainer, or MarginContainer.\n");
	text += String("- Correct example:\n");
	text += String("    CanvasLayer\n");
	text += String("    └─ MarginContainer        (outer padding)\n");
	text += String("       └─ VBoxContainer       (vertical stack)\n");
	text += String("          ├─ HBoxContainer    (health/stamina row)\n");
	text += String("          └─ Label            (score)\n\n");

	text += String("## Rule 2: Sizing\n");
	text += String("BAD:  button.position = Vector2(100, 50); button.size = Vector2(200, 40)\n");
	text += String("GOOD: button.size_flags_horizontal = Control.SIZE_EXPAND_FILL\n");
	text += String("- All children use size_flags_horizontal and size_flags_vertical.\n");
	text += String("- layout_mode = 2 on all Container children.\n\n");

	text += String("## Rule 3: Padding\n");
	text += String("BAD:  control.position.x = 10 (pixel offset hack)\n");
	text += String("GOOD: margin_container.add_theme_constant_override(\"margin_left\", 16)\n");
	text += String("      margin_container.add_theme_constant_override(\"margin_top\", 16)\n\n");

	text += String("## Rule 4: Screen-Relative Anchoring for HUD Elements\n");
	text += String("GOOD: control.set_anchors_preset(Control.PRESET_TOP_RIGHT)   # top-right HUD element\n");
	text += String("      control.set_anchors_preset(Control.PRESET_BOTTOM_LEFT) # bottom-left ammo counter\n");
	text += String("      control.set_anchors_preset(Control.PRESET_CENTER)      # centered dialog\n\n");

	text += String("## Rule 5: Theme Overrides for Fonts and Colors\n");
	text += String("- Use add_theme_font_override(), add_theme_color_override(), add_theme_constant_override().\n");
	text += String("- Never hardcode Color() values directly on nodes - use theme variables.\n\n");

	text += String("## Required Output\n");
	text += String("1. Scene tree with all node names and GDScript types.\n");
	text += String("2. GDScript for wiring dynamic content (e.g. binding a ProgressBar to a health signal).\n");
	text += String("3. Theme override recommendations for fonts, colors, and spacing.");

	content["text"] = text;
	msg["content"] = content;

	messages.push_back(msg);
	result["messages"] = messages;
	result["ok"] = true;
	return result;
}

Dictionary JustAMCPPromptBlaziumUIScaffolder::complete(const Dictionary &p_argument) {
	Dictionary completion;
	completion["values"] = Array();
	completion["total"] = 0;
	completion["hasMore"] = false;
	return completion;
}

#endif
