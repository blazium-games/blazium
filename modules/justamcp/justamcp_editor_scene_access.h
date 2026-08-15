/**************************************************************************/
/*  justamcp_editor_scene_access.h                                        */
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

#pragma once

#include "scene/main/node.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "justamcp_test_scene_root.h"
#endif

namespace JustAMCPEditorSceneAccess {

inline Node *get_edited_root() {
#ifdef TOOLS_ENABLED
	if (Node *test_root = JustAMCPTestSceneRoot::get()) {
		return test_root;
	}
	if (EditorNode::get_singleton() && EditorInterface::get_singleton()) {
		return EditorInterface::get_singleton()->get_edited_scene_root();
	}
#endif
	return nullptr;
}

inline Node *find_node(Node *p_root, const String &p_path) {
	if (!p_root) {
		return nullptr;
	}
	if (p_path.is_empty() || p_path == "." || p_path == p_root->get_name()) {
		return p_root;
	}
	if (p_root->has_node(p_path)) {
		return p_root->get_node(p_path);
	}
	if (p_path.begins_with(String(p_root->get_name()) + "/")) {
		String rel = p_path.substr(String(p_root->get_name()).length() + 1);
		if (p_root->has_node(rel)) {
			return p_root->get_node(rel);
		}
	}
	return nullptr;
}

inline Node *find_node_in_edited_scene(const String &p_path) {
	return find_node(get_edited_root(), p_path);
}

inline String safe_path_to(Node *p_root, Node *p_node) {
	if (!p_root || !p_node) {
		return String();
	}
	if (p_node == p_root || p_root->is_ancestor_of(p_node)) {
		return String(p_root->get_path_to(p_node));
	}
	return String(p_node->get_name());
}

} //namespace JustAMCPEditorSceneAccess
