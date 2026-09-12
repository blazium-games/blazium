/**************************************************************************/
/*  justamcp_selection_resource_provider.cpp                              */
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

#include "justamcp_selection_resource_provider.h"

#include "core/io/json.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "scene/main/node.h"

static Dictionary _selection_json_contents(const String &p_uri, const Dictionary &p_payload) {
	Dictionary result;
	result["ok"] = true;
	Array contents;
	Dictionary content;
	content["uri"] = p_uri;
	content["mimeType"] = "application/json";
	content["text"] = JSON::stringify(p_payload, "\t");
	contents.push_back(content);
	result["contents"] = contents;
	return result;
}

bool JustAMCPSelectionResourceProvider::can_read(const String &p_canonical_uri) {
	return p_canonical_uri == "blazium://selection/current";
}

Dictionary JustAMCPSelectionResourceProvider::read(const String &p_uri, const String &p_canonical_uri) {
	(void)p_canonical_uri;
	Array selected_paths;
	Array selected_nodes;
	const bool editor_ready = EditorNode::get_singleton() && EditorInterface::get_singleton();
	if (editor_ready && EditorInterface::get_singleton()->get_selection()) {
		Array selected = EditorInterface::get_singleton()->get_selection()->get_selected_nodes();
		for (int i = 0; i < selected.size(); i++) {
			Node *node = Object::cast_to<Node>(selected[i]);
			if (node) {
				selected_paths.push_back(String(node->get_path()));
				Dictionary entry;
				entry["name"] = node->get_name();
				entry["type"] = node->get_class();
				entry["path"] = String(node->get_path());
				selected_nodes.push_back(entry);
			}
		}
	}
	Dictionary payload;
	payload["selected_paths"] = selected_paths;
	payload["nodes"] = selected_nodes;
	payload["count"] = selected_paths.size();
	return _selection_json_contents(p_uri, payload);
}

#endif
