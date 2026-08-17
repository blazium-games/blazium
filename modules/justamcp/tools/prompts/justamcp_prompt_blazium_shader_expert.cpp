/**************************************************************************/
/*  justamcp_prompt_blazium_shader_expert.cpp                             */
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

#include "justamcp_prompt_blazium_shader_expert.h"

void JustAMCPPromptBlaziumShaderExpert::_bind_methods() {}

JustAMCPPromptBlaziumShaderExpert::JustAMCPPromptBlaziumShaderExpert() {}
JustAMCPPromptBlaziumShaderExpert::~JustAMCPPromptBlaziumShaderExpert() {}

String JustAMCPPromptBlaziumShaderExpert::get_name() const {
	return "blazium_shader_expert";
}

Dictionary JustAMCPPromptBlaziumShaderExpert::get_prompt() const {
	Dictionary result;
	result["name"] = "blazium_shader_expert";
	result["title"] = "Blazium Shader Expert";
	result["description"] = "Translates a visual effect description into a complete, valid Godot .gdshader file. Strictly uses Godot Shading Language - never raw GLSL.";

	Array arguments;
	Dictionary target;
	target["name"] = "effect_description";
	target["description"] = "Plain-language description of the desired visual effect (e.g. 'dissolve sprite with noise texture', 'fresnel rim glow on 3D mesh').";
	target["required"] = true;
	arguments.push_back(target);

	result["arguments"] = arguments;
	return result;
}

Dictionary JustAMCPPromptBlaziumShaderExpert::get_messages(const Dictionary &p_args) {
	Dictionary result;
	result["description"] = "Blazium Shader Output";

	String target = p_args.has("effect_description") ? String(p_args["effect_description"]) : "[Shader Target]";

	Array messages;
	Dictionary msg;
	msg["role"] = "user";

	Dictionary content;
	content["type"] = "text";

	String text = String("You are a senior Blazium graphics programmer. Write a complete, valid .gdshader file for: ") + target + String("\n\n");
	text += String("STRICT CONSTRAINT: Output ONLY Godot Shading Language. Never output raw GLSL, HLSL, WebGL, or Unity ShaderLab syntax.\n\n");

	text += String("## Rule 1: File Declaration - First non-comment line\n");
	text += String("For 2D sprites, UI elements, particles: shader_type canvas_item;\n");
	text += String("For 3D meshes and surfaces:              shader_type spatial;\n");
	text += String("For ParticleProcessMaterial:             shader_type particles;\n\n");

	text += String("## Rule 2: Fragment Output - Godot built-ins only\n");
	text += String("BAD (raw GLSL): gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n");
	text += String("GOOD (canvas_item): void fragment() { COLOR = vec4(1.0, 0.0, 0.0, 1.0); }\n");
	text += String("GOOD (spatial):     void fragment() { ALBEDO = vec3(1.0, 0.0, 0.0); ALPHA = 1.0; }\n\n");

	text += String("## Rule 3: Texture Uniforms - Always use hints\n");
	text += String("BAD:  uniform sampler2D my_texture;\n");
	text += String("GOOD: uniform sampler2D my_texture : source_color;\n");
	text += String("      uniform sampler2D noise_texture : hint_default_white;\n");
	text += String("      uniform sampler2D normal_map : hint_normal;\n\n");

	text += String("## Rule 4: Time Animation - Use built-in TIME\n");
	text += String("GOOD: void fragment() {\n");
	text += String("          vec2 uv = UV + vec2(sin(TIME * 2.0) * 0.1, 0.0);\n");
	text += String("          COLOR = texture(TEXTURE, uv);\n");
	text += String("      }\n\n");

	text += String("## Rule 5: Vertex Shader\n");
	text += String("GOOD: void vertex() { VERTEX.y += sin(TIME + VERTEX.x) * 0.2; }\n\n");

	text += String("## Rule 6: Common Spatial Built-ins Reference\n");
	text += String("ALBEDO (vec3), ALPHA (float), METALLIC (float), ROUGHNESS (float),\n");
	text += String("NORMAL (vec3), EMISSION (vec3), RIM (float), CLEARCOAT (float),\n");
	text += String("NORMAL_MAP (vec3), SPECULAR (float)\n\n");

	text += String("## Required Output\n");
	text += String("1. The complete .gdshader file ready to paste into Blazium's shader editor.\n");
	text += String("2. All uniform variables listed with their types, hints, and default values.\n");
	text += String("3. Usage instructions: which node to attach the material to and any required texture inputs.");

	content["text"] = text;
	msg["content"] = content;

	messages.push_back(msg);
	result["messages"] = messages;
	result["ok"] = true;
	return result;
}

Dictionary JustAMCPPromptBlaziumShaderExpert::complete(const Dictionary &p_argument) {
	Dictionary completion;
	completion["values"] = Array();
	completion["total"] = 0;
	completion["hasMore"] = false;
	return completion;
}

#endif
