/**************************************************************************/
/*  justamcp_prompt_editor_state.cpp                                      */
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

#include "justamcp_prompt_editor_state.h"

#include "../../justamcp_editor_scene_access.h"

#include "editor/editor_interface.h"
#include "editor/editor_node.h"

void JustAMCPPromptEditorState::_bind_methods() {}

JustAMCPPromptEditorState::JustAMCPPromptEditorState() {}
JustAMCPPromptEditorState::~JustAMCPPromptEditorState() {}

String JustAMCPPromptEditorState::get_name() const {
	return "editor_state";
}

Dictionary JustAMCPPromptEditorState::get_prompt() const {
	Dictionary result;
	result["name"] = "editor_state";
	result["title"] = "Editor UI State";
	result["description"] = "Provides real-time insight into the actively opened Godot Editor environment.";

	Array arguments;
	Dictionary target_arg;
	target_arg["name"] = "target";
	target_arg["description"] = "What environment area to analyze (selection, active_scene, mode).";
	target_arg["required"] = false;
	arguments.push_back(target_arg);

	result["arguments"] = arguments;
	return result;
}

Dictionary JustAMCPPromptEditorState::get_messages(const Dictionary &p_args) {
	Dictionary result;
	result["description"] = "Godot Editor Environment State";

	Array messages;
	Dictionary msg;
	msg["role"] = "user";

	Dictionary content;
	content["type"] = "text";

	String target = p_args.has("target") ? String(Variant(p_args["target"])) : "all";
	String summary = "Editor State: ";

#ifdef TOOLS_ENABLED
	if (EditorNode::get_singleton() && EditorInterface::get_singleton()) {
		if (target == "all" || target == "selection") {
			Array selected = EditorInterface::get_singleton()->get_selection()->get_selected_nodes();
			summary += "Selection size: " + itos(selected.size()) + ". ";
		}
		if (target == "all" || target == "active_scene") {
			Node *edited = JustAMCPEditorSceneAccess::get_edited_root();
			summary += edited ? ("Active Scene: " + edited->get_name() + ". ") : "No Active Scene. ";
		}
	} else {
		summary = "Editor interfaces unavailable natively here! (Are we running headless?)";
	}
#endif

	content["text"] = summary;
	msg["content"] = content;
	messages.push_back(msg);

	result["messages"] = messages;
	result["ok"] = true;
	return result;
}

Dictionary JustAMCPPromptEditorState::complete(const Dictionary &p_argument) {
	Dictionary completion;
	Array values;

	String arg_name = p_argument.has("name") ? String(Variant(p_argument["name"])) : "";
	String arg_value = p_argument.has("value") ? String(Variant(p_argument["value"])) : "";

	if (arg_name == "target") {
		Array targets;
		targets.push_back("all");
		targets.push_back("selection");
		targets.push_back("active_scene");
		targets.push_back("mode");
		for (int i = 0; i < targets.size(); i++) {
			if (String(targets[i]).begins_with(arg_value)) {
				values.push_back(targets[i]);
			}
		}
		completion["values"] = values;
		completion["total"] = values.size();
		completion["hasMore"] = false;
		return completion;
	}

	completion["values"] = Array();
	completion["total"] = 0;
	completion["hasMore"] = false;
	return completion;
}

#endif
