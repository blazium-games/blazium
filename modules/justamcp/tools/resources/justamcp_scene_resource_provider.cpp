/**************************************************************************/
/*  justamcp_scene_resource_provider.cpp                                  */
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

#include "justamcp_scene_resource_provider.h"

#include "../../justamcp_editor_scene_access.h"
#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "scene/main/node.h"

static Dictionary _scene_json_contents(const String &p_uri, const Dictionary &p_payload) {
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

static Dictionary _scene_json_error(const String &p_uri, const String &p_error) {
	Dictionary result;
	result["ok"] = false;
	result["error"] = p_error;
	result["uri"] = p_uri;
	return result;
}

static Node *_get_edited_root() {
	return JustAMCPEditorSceneAccess::get_edited_root();
}

static Dictionary _serialize_node_brief(Node *p_node, Node *p_root) {
	Dictionary data;
	if (!p_node) {
		return data;
	}
	data["name"] = p_node->get_name();
	data["type"] = p_node->get_class();
	data["path"] = p_node == p_root ? String("/") + p_root->get_name() : String("/") + p_root->get_name() + "/" + String(p_root->get_path_to(p_node));
	data["child_count"] = p_node->get_child_count();
	return data;
}

static void _append_node_tree(Node *p_node, Node *p_root, int p_depth, int p_max_depth, Array &r_nodes) {
	if (!p_node || p_depth > p_max_depth) {
		return;
	}
	r_nodes.push_back(_serialize_node_brief(p_node, p_root));
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_append_node_tree(p_node->get_child(i), p_root, p_depth + 1, p_max_depth, r_nodes);
	}
}

bool JustAMCPSceneResourceProvider::can_read(const String &p_canonical_uri) {
	return p_canonical_uri == "blazium://scene/current" || p_canonical_uri == "blazium://scene/hierarchy";
}

Dictionary JustAMCPSceneResourceProvider::read(const String &p_uri, const String &p_canonical_uri) {
	Node *root = _get_edited_root();
	const bool editor_ready = EditorNode::get_singleton() && EditorInterface::get_singleton();

	if (p_canonical_uri == "blazium://scene/current") {
		Dictionary payload;
		payload["current_scene"] = root && root->get_scene_file_path().is_empty() ? String() : (root ? root->get_scene_file_path() : String());
		payload["root"] = root ? _serialize_node_brief(root, root) : Dictionary();
		payload["project_name"] = ProjectSettings::get_singleton()->get_setting("application/config/name", "");
		payload["is_playing"] = editor_ready ? EditorInterface::get_singleton()->is_playing_scene() : false;
		return _scene_json_contents(p_uri, payload);
	}

	if (p_canonical_uri == "blazium://scene/hierarchy") {
		if (!root) {
			return _scene_json_error(p_uri, "No scene is currently open");
		}
		Array nodes;
		_append_node_tree(root, root, 0, 10, nodes);
		Dictionary payload;
		payload["nodes"] = nodes;
		payload["total_count"] = nodes.size();
		return _scene_json_contents(p_uri, payload);
	}

	return _scene_json_error(p_uri, "Unsupported scene resource URI");
}

#endif
