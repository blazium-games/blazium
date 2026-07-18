/**************************************************************************/
/*  justamcp_node_resource_provider.cpp                                   */
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

#include "justamcp_node_resource_provider.h"
#include "../../justamcp_editor_scene_access.h"

#include "justamcp_resource_json.h"

#include "core/io/resource.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "scene/main/node.h"

static Node *_node_provider_edited_root() {
	if (EditorNode::get_singleton() && EditorInterface::get_singleton()) {
		return JustAMCPEditorSceneAccess::get_edited_root();
	}
	return nullptr;
}

static Node *_node_provider_find_node(const String &p_path) {
	Node *root = _node_provider_edited_root();
	if (!root) {
		return nullptr;
	}
	String path = p_path;
	if (path.is_empty() || path == "." || path == "/" || path == String("/") + root->get_name()) {
		return root;
	}
	if (path.begins_with("/")) {
		const String root_prefix = String("/") + root->get_name() + "/";
		if (path.begins_with(root_prefix)) {
			path = path.substr(root_prefix.length());
		} else {
			path = path.substr(1);
		}
	}
	return root->get_node_or_null(NodePath(path));
}

static Variant _node_provider_serialize_value(const Variant &p_value) {
	if (p_value.get_type() == Variant::OBJECT) {
		Object *obj = p_value;
		if (!obj) {
			return Variant();
		}
		Dictionary object_info;
		object_info["type"] = obj->get_class();
		Node *node = Object::cast_to<Node>(obj);
		if (node) {
			object_info["path"] = String(node->get_path());
		}
		Resource *resource = Object::cast_to<Resource>(obj);
		if (resource) {
			object_info["resource_path"] = resource->get_path();
		}
		return object_info;
	}
	if (p_value.get_type() == Variant::NODE_PATH) {
		return String(p_value);
	}
	return p_value;
}

static Dictionary _node_provider_serialize_brief(Node *p_node, Node *p_root) {
	Dictionary node;
	node["name"] = p_node->get_name();
	node["type"] = p_node->get_class();
	node["path"] = p_node == p_root ? String("/") + p_root->get_name() : String("/") + p_root->get_name() + "/" + String(p_root->get_path_to(p_node));
	node["child_count"] = p_node->get_child_count();
	return node;
}

bool JustAMCPNodeResourceProvider::can_read(const String &p_canonical_uri) {
	return (p_canonical_uri.begins_with("blazium://node/") || p_canonical_uri.begins_with("godot://node/")) &&
			(p_canonical_uri.ends_with("/properties") || p_canonical_uri.ends_with("/children") || p_canonical_uri.ends_with("/groups"));
}

Dictionary JustAMCPNodeResourceProvider::read(const String &p_uri, const String &p_canonical_uri) {
	String prefix = p_canonical_uri.begins_with("blazium://node/") ? "blazium://node/" : "godot://node/";
	String suffix;
	if (p_canonical_uri.ends_with("/properties")) {
		suffix = "/properties";
	} else if (p_canonical_uri.ends_with("/children")) {
		suffix = "/children";
	} else if (p_canonical_uri.ends_with("/groups")) {
		suffix = "/groups";
	} else {
		return JustAMCPResourceJson::make_json_error(p_uri, "Unsupported node resource URI");
	}

	String path = p_canonical_uri.substr(prefix.length());
	path = path.substr(0, path.length() - suffix.length());
	Node *root = _node_provider_edited_root();
	if (!root) {
		return JustAMCPResourceJson::make_json_error(p_uri, "No scene is currently open");
	}
	Node *node = _node_provider_find_node(path);
	if (!node) {
		return JustAMCPResourceJson::make_json_error(p_uri, "Node not found: /" + path);
	}

	Dictionary payload;
	payload["path"] = String("/") + root->get_name() + (node == root ? String() : String("/") + String(root->get_path_to(node)));

	if (suffix == "/properties") {
		Dictionary props;
		List<PropertyInfo> prop_list;
		node->get_property_list(&prop_list);
		for (const PropertyInfo &prop : prop_list) {
			if ((prop.usage & PROPERTY_USAGE_EDITOR) && !String(prop.name).begins_with("_")) {
				props[prop.name] = _node_provider_serialize_value(node->get(prop.name));
			}
		}
		payload["type"] = node->get_class();
		payload["properties"] = props;
	} else if (suffix == "/children") {
		Array children;
		for (int i = 0; i < node->get_child_count(); i++) {
			children.push_back(_node_provider_serialize_brief(node->get_child(i), root));
		}
		payload["children"] = children;
		payload["count"] = children.size();
	} else if (suffix == "/groups") {
		Array groups;
		List<Node::GroupInfo> group_info;
		node->get_groups(&group_info);
		for (const Node::GroupInfo &group : group_info) {
			const String group_name = group.name;
			if (!group_name.begins_with("_")) {
				groups.push_back(group_name);
			}
		}
		payload["groups"] = groups;
		payload["count"] = groups.size();
	}

	return JustAMCPResourceJson::make_json_contents(p_uri, payload);
}

#endif
