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

#include "core/string/ustring.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"

#ifdef TOOLS_ENABLED
#include "justamcp_test_scene_root.h"
#include "tools/justamcp_agent_policy.h"

#include "core/io/file_access.h"
#include "core/os/thread.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
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

inline bool in_edited_scene(Node *p_node) {
	Node *root = get_edited_root();
	return root && p_node && (p_node == root || root->is_ancestor_of(p_node));
}

inline void add_child_with_undo(Node *p_node, Node *p_parent, Node *p_root, const String &p_action_name) {
	if (!p_node || !p_parent) {
		return;
	}
	Node *owner = p_root ? p_root : p_parent->get_owner();
	const bool edited = in_edited_scene(p_parent);
#ifdef TOOLS_ENABLED
	EditorUndoRedoManager *undo = EditorUndoRedoManager::get_singleton();
	if (edited && undo && Thread::is_main_thread()) {
		undo->create_action(p_action_name + " [" + JustAMCPAgentPolicy::current_session_id() + "]", UndoRedo::MERGE_DISABLE);
		undo->add_do_method(p_parent, "add_child", p_node, true);
		if (owner) {
			undo->add_do_method(p_node, "set_owner", owner);
		}
		undo->add_do_reference(p_node);
		undo->add_undo_method(p_parent, "remove_child", p_node);
		undo->commit_action();
		return;
	}
#endif
	p_parent->add_child(p_node, true);
	if (owner) {
		p_node->set_owner(owner);
	}
#ifdef TOOLS_ENABLED
	if (edited) {
		Array added;
		added.push_back(int64_t(p_node->get_instance_id()));
		JustAMCPAgentPolicy::note_batch_undo(Array(), added);
	}
#endif
}

inline void set_property_with_undo(Object *p_object, const StringName &p_property, const Variant &p_value, const String &p_action_name) {
	if (!p_object) {
		return;
	}
	Node *node = Object::cast_to<Node>(p_object);
	const bool edited = node && in_edited_scene(node);
	bool valid = false;
	const Variant previous = p_object->get(p_property, &valid);
#ifdef TOOLS_ENABLED
	EditorUndoRedoManager *undo = EditorUndoRedoManager::get_singleton();
	if (edited && valid && undo && Thread::is_main_thread()) {
		undo->create_action(p_action_name + " [" + JustAMCPAgentPolicy::current_session_id() + "]", UndoRedo::MERGE_DISABLE);
		undo->add_do_property(p_object, p_property, p_value);
		undo->add_undo_property(p_object, p_property, previous);
		undo->commit_action();
		return;
	}
#endif
	p_object->set(p_property, p_value);
#ifdef TOOLS_ENABLED
	if (edited && valid) {
		Dictionary row;
		row["id"] = int64_t(node->get_instance_id());
		row["property"] = p_property;
		row["value"] = previous;
		Array properties;
		properties.push_back(row);
		JustAMCPAgentPolicy::note_batch_undo(properties, Array());
	}
#endif
}

inline void set_unique_name_with_undo(Node *p_node, bool p_unique) {
	if (!p_node) {
		return;
	}
#ifdef TOOLS_ENABLED
	const bool previous = p_node->is_unique_name_in_owner();
	EditorUndoRedoManager *undo = EditorUndoRedoManager::get_singleton();
	if (in_edited_scene(p_node) && undo && Thread::is_main_thread()) {
		undo->create_action(String("Set Unique Name [") + JustAMCPAgentPolicy::current_session_id() + "]", UndoRedo::MERGE_DISABLE);
		undo->add_do_method(p_node, "set_unique_name_in_owner", p_unique);
		undo->add_undo_method(p_node, "set_unique_name_in_owner", previous);
		undo->commit_action();
		return;
	}
#endif
	p_node->set_unique_name_in_owner(p_unique);
}

// The editor's external-change scan compares this time with the file. A tool
// save has to stamp it, or the open scene is reloaded as an outside edit.
inline void acknowledge_saved_scene(const String &p_scene_path) {
#ifdef TOOLS_ENABLED
	if (!EditorNode::get_singleton() || p_scene_path.is_empty()) {
		return;
	}
	EditorData &data = EditorNode::get_editor_data();
	const int idx = data.get_edited_scene_from_path(p_scene_path);
	if (idx < 0) {
		return;
	}
	data.set_scene_modified_time(idx, FileAccess::get_modified_time(p_scene_path));
#else
	(void)p_scene_path;
#endif
}

} //namespace JustAMCPEditorSceneAccess
