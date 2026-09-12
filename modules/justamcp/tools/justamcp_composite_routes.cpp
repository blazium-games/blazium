/**************************************************************************/
/*  justamcp_composite_routes.cpp                                         */
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

#include "justamcp_scene_tree_dump.h"
#include "justamcp_tool_executor.h"

#include "../justamcp_editor_scene_access.h"
#include "../justamcp_mcp_tool_macros.h"
#include "../justamcp_runtime.h"
#include "../justamcp_server.h"
#include "../justamcp_tool_context.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/templates/pair.h"
#include "justamcp_agent_helpers.h"
#include "justamcp_profiling_tools.h"
#include "justamcp_scene_file_io.h"
#include "justamcp_script_tools.h"
#include "servers/display_server.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_file_system.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "scene/main/node.h"
#include "scene/main/window.h"
#endif

#ifdef TOOLS_ENABLED
static bool _justamcp_node_matches_filters(Node *p_node, Node * /*p_root*/, const Dictionary &p_args) {
	const String type_filter = p_args.get("type_filter", "");
	if (!type_filter.is_empty()) {
		if (bool(p_args.get("type_filter_inherit", false))) {
			if (!p_node->is_class(type_filter)) {
				return false;
			}
		} else if (p_node->get_class() != type_filter) {
			return false;
		}
	}
	const String name_filter = p_args.get("name_filter", "");
	if (!name_filter.is_empty() && !justamcp_glob_match(p_node->get_name(), name_filter)) {
		return false;
	}
	const String script_filter = p_args.get("script_filter", "");
	if (!script_filter.is_empty()) {
		Ref<Script> script = p_node->get_script();
		if (script.is_null()) {
			return false;
		}
		if (script_filter != "*" && !justamcp_glob_match(script->get_path(), script_filter)) {
			return false;
		}
	}
	const String group_filter = p_args.get("group_filter", "");
	if (!group_filter.is_empty()) {
		Vector<String> patterns = group_filter.split(",", false);
		const String mode = String(p_args.get("group_filter_mode", "any")).to_lower();
		int matched = 0;
		for (int i = 0; i < patterns.size(); i++) {
			const String pattern = patterns[i].strip_edges();
			List<Node::GroupInfo> groups;
			p_node->get_groups(&groups);
			bool in_group = false;
			for (const Node::GroupInfo &g : groups) {
				if (justamcp_glob_match(String(g.name), pattern)) {
					in_group = true;
					break;
				}
			}
			if (in_group) {
				matched++;
			}
		}
		if (mode == "all") {
			if (matched != patterns.size()) {
				return false;
			}
		} else if (matched == 0) {
			return false;
		}
	}
	return true;
}

static Dictionary _justamcp_describe_node(Node *p_node, Node *p_root) {
	Dictionary item;
	item["name"] = p_node->get_name();
	item["type"] = p_node->get_class();
	item["path"] = p_node == p_root ? String(".") : String(p_root->get_path_to(p_node));
	Ref<Script> node_script = p_node->get_script();
	item["script"] = node_script.is_valid() ? node_script->get_path() : String();
	item["child_count"] = p_node->get_child_count();
	item["visible"] = p_node->has_method("is_visible") ? bool(p_node->call("is_visible")) : true;
	if (!p_node->get_scene_file_path().is_empty() && p_node != p_root) {
		item["instanced_from"] = p_node->get_scene_file_path();
	}
	List<Node::GroupInfo> groups;
	p_node->get_groups(&groups);
	Array group_names;
	for (const Node::GroupInfo &g : groups) {
		group_names.push_back(String(g.name));
	}
	if (!group_names.is_empty()) {
		item["groups"] = group_names;
	}
	return item;
}

static String _justamcp_scene_tree_xml(const String &p_scene_path, const String &p_mode, const Array &p_nodes, bool p_truncated, int p_next_offset) {
	String xml = "<scene_tree scene=\"" + p_scene_path.xml_escape() + "\"";
	if (p_mode == "flat") {
		xml += " mode=\"flat\" total_matches=\"" + itos(p_nodes.size()) + "\"";
	}
	xml += ">\n";
	for (int i = 0; i < p_nodes.size(); i++) {
		Dictionary item = p_nodes[i];
		xml += "  <node path=\"" + String(item.get("path", "")).xml_escape() + "\" name=\"" + String(item.get("name", "")).xml_escape() + "\" type=\"" + String(item.get("type", "")).xml_escape() + "\" children_count=\"" + itos(int(item.get("child_count", 0))) + "\"";
		if (item.has("script") && !String(item["script"]).is_empty()) {
			xml += " script=\"" + String(item["script"]).xml_escape() + "\"";
		}
		if (item.has("instanced_from")) {
			xml += " instanced_from=\"" + String(item["instanced_from"]).xml_escape() + "\"";
		}
		xml += "/>\n";
	}
	if (p_truncated) {
		xml += "  <truncated next_offset=\"" + itos(p_next_offset) + "\"/>\n";
	}
	xml += "</scene_tree>";
	return xml;
}

Dictionary justamcp_scene_tree_dump(const Dictionary &p_args) {
	Dictionary result;
	Node *owned = nullptr;
	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	const String file_path = p_args.get("file_path", p_args.get("scene_path", ""));
	if (!file_path.is_empty()) {
		Dictionary load_err = justamcp_load_scene_root(file_path, &owned);
		if (!load_err.is_empty()) {
			return load_err.has("ok") ? load_err : Dictionary();
		}
		root = owned;
	}
	if (!root) {
		result["ok"] = false;
		result["error"] = "No scene is currently open.";
		return result;
	}

	const String root_node_path = p_args.get("root_node_path", "");
	Node *walk_root = root;
	if (!root_node_path.is_empty()) {
		walk_root = justamcp_find_node_in_root(root, root_node_path);
		if (!walk_root) {
			if (owned) {
				memdelete(owned);
			}
			result["ok"] = false;
			result["error"] = "root_node_path not found: " + root_node_path;
			return result;
		}
	}

	const bool has_filter = p_args.has("type_filter") || p_args.has("name_filter") || p_args.has("group_filter") || p_args.has("script_filter");
	String mode = String(p_args.get("mode", has_filter ? "flat" : "tree")).to_lower();
	const int max_depth = p_args.has("max_depth") ? int(p_args.get("max_depth", -1)) : -1;
	if (max_depth < -1) {
		if (owned) {
			memdelete(owned);
		}
		result["ok"] = false;
		result["error"] = "max_depth must be greater than or equal to 0.";
		return result;
	}
	const int offset = MAX(0, int(p_args.get("offset", 0)));
	const int limit = CLAMP(int(p_args.get("limit", p_args.get("max_nodes", 100))), 1, 10000);
	const int max_nodes = CLAMP(int(p_args.get("max_nodes", 2000)), 1, 10000);

	Array nodes;
	List<Pair<Node *, int>> stack;
	stack.push_back(Pair<Node *, int>(walk_root, 0));
	bool truncated = false;
	int skipped = 0;
	while (!stack.is_empty()) {
		if (nodes.size() >= max_nodes) {
			truncated = true;
			break;
		}
		Pair<Node *, int> cur = stack.front()->get();
		stack.pop_front();
		Node *node = cur.first;
		const int depth = cur.second;
		if (max_depth >= 0 && depth > max_depth) {
			continue;
		}
		if (!_justamcp_node_matches_filters(node, root, p_args)) {
			if (!has_filter) {
				// Keep walking children even when the current node is omitted by depth-only rules.
			} else {
				for (int i = node->get_child_count() - 1; i >= 0; i--) {
					stack.push_front(Pair<Node *, int>(node->get_child(i), depth + 1));
				}
				continue;
			}
		}
		if (skipped < offset) {
			skipped++;
		} else if (nodes.size() < limit) {
			nodes.push_back(_justamcp_describe_node(node, root));
		} else {
			truncated = true;
			break;
		}
		if (max_depth < 0 || depth < max_depth) {
			const int child_limit = MIN(node->get_child_count(), 100);
			for (int i = child_limit - 1; i >= 0; i--) {
				stack.push_front(Pair<Node *, int>(node->get_child(i), depth + 1));
			}
		}
	}

	result["ok"] = true;
	result["root"] = walk_root->get_name();
	result["scene_path"] = file_path.is_empty() ? root->get_scene_file_path() : justamcp_resolve_project_path(file_path);
	result["nodes"] = nodes;
	result["count"] = nodes.size();
	result["truncated"] = truncated;
	result["mode"] = mode;
	if (String(p_args.get("format", "json")) == "xml") {
		result["xml"] = _justamcp_scene_tree_xml(result["scene_path"], mode, nodes, truncated, offset + nodes.size());
	}
	if (owned) {
		memdelete(owned);
	}
	return result;
}
#endif

Dictionary JustAMCPToolExecutor::execute_composite_tool(const String &p_internal_name, const Dictionary &p_args) {
	(void)p_internal_name;
	(void)p_args;
	return Dictionary();
}
