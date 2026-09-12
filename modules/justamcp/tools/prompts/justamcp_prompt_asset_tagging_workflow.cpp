/**************************************************************************/
/*  justamcp_prompt_asset_tagging_workflow.cpp                            */
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

#include "justamcp_prompt_asset_tagging_workflow.h"

void JustAMCPPromptAssetTaggingWorkflow::_bind_methods() {}

JustAMCPPromptAssetTaggingWorkflow::JustAMCPPromptAssetTaggingWorkflow() {}
JustAMCPPromptAssetTaggingWorkflow::~JustAMCPPromptAssetTaggingWorkflow() {}

String JustAMCPPromptAssetTaggingWorkflow::get_name() const {
	return "blazium_asset_tagging_workflow";
}

Dictionary JustAMCPPromptAssetTaggingWorkflow::get_prompt() const {
	Dictionary result;
	result["name"] = "blazium_asset_tagging_workflow";
	result["title"] = "Asset Tagging Workflow";
	result["description"] = "Guides AI through listing tags, assigning tags to assets, and verifying tag-based asset lookup.";
	result["arguments"] = Array();
	return result;
}

Dictionary JustAMCPPromptAssetTaggingWorkflow::get_messages(const Dictionary &p_args) {
	(void)p_args;
	Dictionary result;
	result["description"] = "Asset Tagging Workflow";

	Array messages;
	String text = "Use the asset tag tools to organize project assets for search and AI discovery.\n\n";
	text += "Recommended flow:\n";
	text += "1. Read `blazium://tags/dictionary` or call `blazium_tags_list` to inspect existing tags.\n";
	text += "2. Add missing dictionary tags with `blazium_tags_add` only after explicit user approval.\n";
	text += "3. Assign tags to assets with `blazium_tags_set_on_asset` using res:// paths for models, textures, scenes, and scripts.\n";
	text += "4. Verify assignments with `blazium_tags_get_on_asset` and discover related assets via `blazium_tags_find_assets`.\n";
	text += "5. Use `blazium_tags_search_assets` for multi-tag queries and optional type/path filters.\n";
	text += "6. Use `semantic_search` or read `blazium://semantic/search/{query}` to discover assets by caption and tag tokens.\n";
	text += "7. Rebuild the semantic index with `semantic_rebuild_index` after large tagging sessions.\n\n";
	text += "In the editor FileSystem dock, users can filter with `tag:Namespace.Tag` syntax.";

	messages.push_back(_make_text_message(text));
	messages.push_back(_make_resource_message("blazium://tags/dictionary"));
	messages.push_back(_make_resource_message("blazium://semantic/index/stats"));
	messages.push_back(_make_resource_message("blazium://project/info"));
	result["messages"] = messages;
	result["ok"] = true;
	return result;
}

#endif
