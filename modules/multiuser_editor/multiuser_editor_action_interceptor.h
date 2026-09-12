/**************************************************************************/
/*  multiuser_editor_action_interceptor.h                                 */
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

#ifdef TOOLS_ENABLED

#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/variant/variant.h"
#include "editor/editor_data.h"
#include "multiuser_editor_lock_manager.h"
#include "multiuser_editor_security_sink.h"

class EditorUndoRedoManager;

class MultiuserEditorActionInterceptor {
	Node *scene_root = nullptr;
	EditorSelection *selection = nullptr;
	EditorUndoRedoManager *undo_redo = nullptr;
	MultiuserEditorLockManager *lock_manager = nullptr;
	HashMap<String, Variant> transform_cache;
	HashMap<String, Dictionary> property_cache;
	HashMap<String, String> script_cache;
	HashSet<String> remote_changed_paths;
	mutable bool locked_delete_blocked = false;
	mutable bool locked_add_blocked = false;
	multiuser_editor::SecuritySink _security;

	void _collect_scene_nodes(Node *p_node, Vector<Node *> &r_nodes) const;
	Vector<Node *> _get_all_scene_nodes() const;
	Array _get_sync_properties(Node *p_node) const;
	Node *_resolve_node(const String &p_path) const;
	bool _is_safe_node_type(const String &p_type) const;
	bool _is_edited_scene_node(Node *p_node) const;
	Dictionary _make_action(const String &p_type, const Dictionary &p_data) const;

public:
	static bool is_safe_node_path(const String &p_path);
	static bool is_safe_file_path(const String &p_path);
	static bool canonicalize_res_path(const String &p_in, String &r_out);
	static bool is_safe_property_name(const String &p_name);
	static bool is_safe_node_name(const String &p_name);
	static bool is_safe_branch_name(const String &p_name);
	static bool is_safe_remote_name(const String &p_name);
	static bool is_safe_commit_message(const String &p_message);
	static bool is_safe_remote_value(const Variant &p_value, int64_t p_byte_cap);

	String _get_script_path(Node *p_node) const;
	String _get_script_content(Node *p_node) const;

public:
	void set_context(Node *p_scene_root, EditorSelection *p_selection, EditorUndoRedoManager *p_undo_redo, MultiuserEditorLockManager *p_lock_manager);
	Dictionary capture_selection() const;
	Dictionary capture_node_add(Node *p_node) const;
	Dictionary capture_node_delete(Node *p_node) const;
	bool consume_locked_add_blocked();
	bool consume_locked_delete_blocked();
	void undo_last_scene_action();
	void apply_inspector_property_lock(Object *p_undo_redo, Object *p_modified_object, const String &p_property, const Variant &p_new_value);
	void build_initial_state_actions(Vector<Dictionary> &r_actions) const;
	uint64_t generate_scene_hash(Node *p_root) const;
	void apply_remote_action(const Dictionary &p_action);
	void poll_scene_changes(Vector<Dictionary> &r_actions);
	void clear_caches();
	void set_security_sink(const multiuser_editor::SecuritySink &p_sink) { _security = p_sink; }
};

#endif
