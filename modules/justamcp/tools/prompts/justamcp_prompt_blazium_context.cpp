/**************************************************************************/
/*  justamcp_prompt_blazium_context.cpp                                   */
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

#include "justamcp_prompt_blazium_context.h"

#ifdef MODULE_ASSETTAGS_ENABLED
#include "modules/assettags/asset_tag_manager.h"
#endif

void JustAMCPPromptBlaziumContext::_bind_methods() {}

JustAMCPPromptBlaziumContext::JustAMCPPromptBlaziumContext() {}
JustAMCPPromptBlaziumContext::~JustAMCPPromptBlaziumContext() {}

String JustAMCPPromptBlaziumContext::get_name() const {
	return "blazium_context";
}

Dictionary JustAMCPPromptBlaziumContext::get_prompt() const {
	Dictionary result;
	result["name"] = "blazium_context";
	result["title"] = "Blazium Context Info";
	result["description"] = "Retrieves Blazium Engine JustAMCP context and points users to the richer blazium_project_intake workflow.";
	Array arguments;
	Dictionary mode;
	mode["name"] = "mode";
	mode["description"] = "Optional context tone: strict, casual, or debug.";
	mode["required"] = false;
	arguments.push_back(mode);
	result["arguments"] = arguments;
	return result;
}

Dictionary JustAMCPPromptBlaziumContext::get_messages(const Dictionary &p_args) {
	Dictionary result;
	result["description"] = "Blazium Environment Context";

	Array messages;
	String mode = p_args.has("mode") ? String(p_args["mode"]) : "strict";
	String text = "The Blazium Engine MCP server is active and exposes tools, resources, prompts, and tasks over JSON-RPC.\n";
	text += "Mode: " + mode + "\n\n";
	text += "For substantial work, prefer the `blazium_project_intake` prompt first. It embeds project, scene, selection, and JustAMCP guide resources so the client starts with reliable context.\n";
	text += "Use `blazium://guide/tool-index` and `blazium://guide/troubleshooting` for orientation, and prefer deterministic `blazium_*` tools for editor/runtime state, validation, tests, and resource reads.\n";
#ifdef MODULE_ASSETTAGS_ENABLED
	if (AssetTagManager *tag_manager = AssetTagManager::get_singleton()) {
		PackedStringArray tags = tag_manager->list_tags();
		text += "\nAsset tags: " + itos(tags.size()) + " dictionary entries. Use `blazium://tags/dictionary`, `blazium_tags_*` tools, or the `blazium_asset_tagging_workflow` prompt for tagged asset search.\n";
	} else {
		text += "\nAsset tag tools are available via `blazium_tags_*` when the assettags module is loaded.\n";
	}
#endif

	messages.push_back(_make_text_message(text));
	messages.push_back(_make_resource_message("blazium://project/info"));
	messages.push_back(_make_resource_message("blazium://guide/tool-index"));
#ifdef MODULE_ASSETTAGS_ENABLED
	messages.push_back(_make_resource_message("blazium://tags/dictionary"));
#endif
	result["messages"] = messages;
	result["ok"] = true;
	return result;
}

Dictionary JustAMCPPromptBlaziumContext::complete(const Dictionary &p_argument) {
	Dictionary completion;
	Array values;

	String arg_name = p_argument.has("name") ? String(Variant(p_argument["name"])) : "";
	String arg_value = p_argument.has("value") ? String(Variant(p_argument["value"])) : "";

	if (arg_name == "mode") {
		Array modes;
		modes.push_back("strict");
		modes.push_back("casual");
		modes.push_back("debug");
		for (int i = 0; i < modes.size(); i++) {
			if (String(modes[i]).begins_with(arg_value)) {
				values.push_back(modes[i]);
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
