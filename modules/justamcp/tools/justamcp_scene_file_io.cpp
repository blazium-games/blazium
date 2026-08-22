/**************************************************************************/
/*  justamcp_scene_file_io.cpp                                            */
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

#include "justamcp_scene_file_io.h"
#include "../justamcp_editor_filesystem.h"
#include "justamcp_agent_helpers.h"

#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/io/resource_uid.h"
#include "scene/main/node.h"
#include "scene/resources/packed_scene.h"

#ifdef TOOLS_ENABLED
#include "../justamcp_editor_scene_access.h"
#include "editor/editor_interface.h"
#endif

String justamcp_resolve_project_path(const String &p_path) {
	String path;
	String sandbox_error;
	if (!justamcp_canonical_sandbox_path(p_path, path, sandbox_error)) {
		return String();
	}
	return path;
}

Node *justamcp_find_node_in_root(Node *p_root, const String &p_path) {
	if (!p_root) {
		return nullptr;
	}
	if (p_path.is_empty() || p_path == "." || p_path == p_root->get_name()) {
		return p_root;
	}
	Node *found = p_root->get_node_or_null(NodePath(p_path));
	if (found) {
		return found;
	}
	if (p_path.begins_with(String(p_root->get_name()) + "/")) {
		const String rel = p_path.substr(String(p_root->get_name()).length() + 1);
		return p_root->get_node_or_null(NodePath(rel));
	}
	if (p_path.begins_with("/root/")) {
		return p_root->get_node_or_null(NodePath(p_path.substr(6)));
	}
	return nullptr;
}

Dictionary justamcp_load_scene_root(const String &p_path, Node **r_root) {
	Dictionary err;
	*r_root = nullptr;
	const String path = justamcp_resolve_project_path(p_path);
	if (path.is_empty() || !FileAccess::exists(path)) {
		err["ok"] = false;
		err["error"] = "Scene not found: " + path;
		return err;
	}
	Ref<PackedScene> packed = ResourceLoader::load(path);
	if (packed.is_null()) {
		err["ok"] = false;
		err["error"] = "Failed to load: " + path;
		return err;
	}
	Node *root = packed->instantiate();
	if (!root) {
		err["ok"] = false;
		err["error"] = "Failed to instantiate: " + path;
		return err;
	}
	*r_root = root;
	return Dictionary();
}

Dictionary justamcp_save_scene_root(Node *p_root, const String &p_path, bool p_free_root) {
	Dictionary err;
	if (!p_root) {
		err["ok"] = false;
		err["error"] = "No scene root to save.";
		return err;
	}
	const String path = justamcp_resolve_project_path(p_path);
	Ref<PackedScene> packed;
	packed.instantiate();
	if (packed->pack(p_root) != OK) {
		if (p_free_root) {
			memdelete(p_root);
		}
		err["ok"] = false;
		err["error"] = "Failed to pack scene: " + path;
		return err;
	}
	if (ResourceSaver::save(packed, path) != OK) {
		if (p_free_root) {
			memdelete(p_root);
		}
		err["ok"] = false;
		err["error"] = "Failed to save scene: " + path;
		return err;
	}
	if (p_free_root) {
		memdelete(p_root);
	}
	justamcp_refresh_project_path(path);
	return Dictionary();
}

void justamcp_refresh_project_path(const String &p_path) {
	JustAMCPEditorFilesystem::refresh_path(p_path);
#ifdef TOOLS_ENABLED
	if (EditorInterface::get_singleton()) {
		Node *edited = JustAMCPEditorSceneAccess::get_edited_root();
		if (edited && edited->get_scene_file_path() == p_path) {
			EditorInterface::get_singleton()->reload_scene_from_path(p_path);
		}
	}
#endif
}
