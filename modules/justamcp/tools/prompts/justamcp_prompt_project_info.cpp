/**************************************************************************/
/*  justamcp_prompt_project_info.cpp                                      */
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

#include "justamcp_prompt_project_info.h"
#include "core/config/project_settings.h"

void JustAMCPPromptProjectInfo::_bind_methods() {}

JustAMCPPromptProjectInfo::JustAMCPPromptProjectInfo() {}
JustAMCPPromptProjectInfo::~JustAMCPPromptProjectInfo() {}

String JustAMCPPromptProjectInfo::get_name() const {
	return "project_info";
}

Dictionary JustAMCPPromptProjectInfo::get_prompt() const {
	Dictionary result;
	result["name"] = "project_info";
	result["title"] = "Godot Project Info";
	result["description"] = "Retrieves the core Godot Project metadata natively.";
	result["arguments"] = Array();
	return result;
}

Dictionary JustAMCPPromptProjectInfo::get_messages(const Dictionary &p_args) {
	Dictionary result;
	result["description"] = "Project Context";

	Array messages;
	Dictionary msg;
	msg["role"] = "user";

	Dictionary content;
	content["type"] = "text";
	String pname = ProjectSettings::get_singleton() ? ProjectSettings::get_singleton()->get("application/config/name") : "Unknown";
	content["text"] = "Project Name: " + pname;
	msg["content"] = content;

	messages.push_back(msg);
	result["messages"] = messages;
	result["ok"] = true;
	return result;
}

Dictionary JustAMCPPromptProjectInfo::complete(const Dictionary &p_argument) {
	Dictionary completion;
	completion["values"] = Array();
	completion["total"] = 0;
	completion["hasMore"] = false;
	return completion;
}

#endif
