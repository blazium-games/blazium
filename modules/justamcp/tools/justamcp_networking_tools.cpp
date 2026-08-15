/**************************************************************************/
/*  justamcp_networking_tools.cpp                                         */
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

#include "core/object/class_db.h"
#include "justamcp_networking_tools.h"
#include "../justamcp_editor_plugin.h"
#include "../justamcp_editor_scene_access.h"
#include "editor/editor_interface.h"
#include "editor/editor_undo_redo_manager.h"
#include "scene/main/http_request.h"
#include "scene/main/multiplayer_api.h"
#include "scene/main/multiplayer_peer.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"

void JustAMCPNetworkingTools::_bind_methods() {}

void JustAMCPNetworkingTools::set_editor_plugin(JustAMCPEditorPlugin *p_plugin) {
	editor_plugin = p_plugin;
}

Node *JustAMCPNetworkingTools::_get_scene_root() {
	if (editor_plugin && editor_plugin->get_editor_interface()) {
		return JustAMCPEditorSceneAccess::get_edited_root();
	}
	return nullptr;
}

Node *JustAMCPNetworkingTools::_get_node(const String &p_path) {
	if (p_path.is_empty()) {
		return _get_scene_root();
	}
	Node *scene_root = _get_scene_root();
	if (!scene_root) {
		return nullptr;
	}
	if (p_path.begins_with("/root/")) {
		String rel = p_path.substr(6);
		return scene_root->get_tree()->get_root()->get_node_or_null(NodePath(rel));
	} else if (p_path == "/root") {
		return scene_root;
	} else {
		return scene_root->get_node_or_null(NodePath(p_path));
	}
}

Dictionary JustAMCPNetworkingTools::networking_create_http_request(const Dictionary &p_args) {
	Dictionary result;
	String parent_path = p_args.get("parent_path", "");
	String node_name = p_args.get("name", "HTTPRequest");
	int timeout = p_args.get("timeout", 0);

	Node *parent = _get_node(parent_path);
	if (!parent) {
		result["ok"] = false;
		result["error"] = "Parent node not found: " + parent_path;
		return result;
	}

	HTTPRequest *node = memnew(HTTPRequest);
	node->set_name(node_name);
	if (timeout > 0) {
		node->set_timeout(timeout);
	}

	Node *scene_root = _get_scene_root();
	if (EditorUndoRedoManager::get_singleton()) {
		EditorUndoRedoManager::get_singleton()->create_action("Add HTTPRequest '" + node_name + "'");
		EditorUndoRedoManager::get_singleton()->add_do_method(parent, "add_child", node, true);
		EditorUndoRedoManager::get_singleton()->add_do_method(node, "set_owner", scene_root);
		EditorUndoRedoManager::get_singleton()->add_do_reference(node);
		EditorUndoRedoManager::get_singleton()->add_undo_method(parent, "remove_child", node);
		EditorUndoRedoManager::get_singleton()->commit_action();
	} else {
		parent->add_child(node, true);
		node->set_owner(scene_root);
	}

	result["ok"] = true;
	result["node_path"] = String(node->get_path());
	result["name"] = node->get_name();
	result["message"] = "HTTPRequest node created. Connect to 'request_completed' signal for response handling.";
	return result;
}

Dictionary JustAMCPNetworkingTools::networking_setup_websocket(const Dictionary &p_args) {
	Dictionary result;
	String parent_path = p_args.get("parent_path", "");
	String mode = p_args.get("mode", "client");
	String node_name = p_args.get("name", "WebSocket");

	Node *parent = _get_node(parent_path);
	if (!parent) {
		result["ok"] = false;
		result["error"] = "Parent node not found: " + parent_path;
		return result;
	}

	Node *node = memnew(Node);
	node->set_name(node_name);

	Node *scene_root = _get_scene_root();
	if (EditorUndoRedoManager::get_singleton()) {
		EditorUndoRedoManager::get_singleton()->create_action("Add WebSocket '" + node_name + "'");
		EditorUndoRedoManager::get_singleton()->add_do_method(parent, "add_child", node, true);
		EditorUndoRedoManager::get_singleton()->add_do_method(node, "set_owner", scene_root);
		EditorUndoRedoManager::get_singleton()->add_do_reference(node);
		EditorUndoRedoManager::get_singleton()->add_undo_method(parent, "remove_child", node);
		EditorUndoRedoManager::get_singleton()->commit_action();
	} else {
		parent->add_child(node, true);
		node->set_owner(scene_root);
	}

	result["ok"] = true;
	result["node_path"] = String(node->get_path());
	result["name"] = node->get_name();
	result["mode"] = mode;
	result["message"] = "Node created for WebSocket. Attach a script using WebSocketPeer for " + mode + " functionality.";
	return result;
}

Dictionary JustAMCPNetworkingTools::networking_setup_multiplayer(const Dictionary &p_args) {
	Dictionary result;
	String parent_path = p_args.get("parent_path", "");
	String transport = p_args.get("transport", "enet");
	String mode = p_args.get("mode", "server");
	String address = p_args.get("address", "127.0.0.1");
	int port = p_args.get("port", 7000);
	int max_clients = p_args.get("max_clients", 32);

	Node *parent = _get_node(parent_path);
	if (!parent) {
		result["ok"] = false;
		result["error"] = "Parent node not found: " + parent_path;
		return result;
	}

	Node *node = memnew(Node);
	node->set_name("MultiplayerManager");

	Node *scene_root = _get_scene_root();
	if (EditorUndoRedoManager::get_singleton()) {
		EditorUndoRedoManager::get_singleton()->create_action("Add MultiplayerManager");
		EditorUndoRedoManager::get_singleton()->add_do_method(parent, "add_child", node, true);
		EditorUndoRedoManager::get_singleton()->add_do_method(node, "set_owner", scene_root);
		EditorUndoRedoManager::get_singleton()->add_do_reference(node);
		EditorUndoRedoManager::get_singleton()->add_undo_method(parent, "remove_child", node);
		EditorUndoRedoManager::get_singleton()->commit_action();
	} else {
		parent->add_child(node, true);
		node->set_owner(scene_root);
	}

	result["ok"] = true;
	result["node_path"] = String(node->get_path());
	result["transport"] = transport;
	result["mode"] = mode;
	result["address"] = (mode == "client") ? address : "0.0.0.0";
	result["port"] = port;
	result["max_clients"] = (mode == "server") ? max_clients : 0;
	result["message"] = "MultiplayerManager created. Attach a script to set up " + transport.to_upper() + " " + mode + " peer on port " + itos(port) + ".";
	return result;
}

Dictionary JustAMCPNetworkingTools::networking_setup_rpc(const Dictionary &p_args) {
	Dictionary result;
	String node_path = p_args.get("node_path", "");
	String method_name = p_args.get("method_name", "");
	String rpc_mode = p_args.get("rpc_mode", "authority");
	String transfer_mode = p_args.get("transfer_mode", "reliable");
	bool call_local = p_args.get("call_local", false);
	int channel = p_args.get("channel", 0);

	if (method_name.is_empty()) {
		result["ok"] = false;
		result["error"] = "Missing 'method_name' parameter for RPC configuration.";
		return result;
	}

	Dictionary rpc_config;
	rpc_config["rpc_mode"] = rpc_mode;
	rpc_config["transfer_mode"] = transfer_mode;
	rpc_config["call_local"] = call_local;
	rpc_config["channel"] = channel;

	String call_mode = call_local ? "call_local" : "call_remote";
	String annotation = "@rpc(\"" + rpc_mode + "\", \"" + transfer_mode + "\", \"" + call_mode + "\", " + itos(channel) + ")";

	result["ok"] = true;
	result["node_path"] = node_path;
	result["method_name"] = method_name;
	result["rpc_config"] = rpc_config;
	result["annotation"] = annotation;
	result["message"] = "Add this annotation above your method: " + annotation;
	return result;
}

Dictionary JustAMCPNetworkingTools::networking_setup_sync(const Dictionary &p_args) {
	Dictionary result;
	String parent_path = p_args.get("parent_path", "");
	String node_name = p_args.get("name", "MultiplayerSynchronizer");
	Array properties = p_args.get("properties", Array());

	Node *parent = _get_node(parent_path);
	if (!parent) {
		result["ok"] = false;
		result["error"] = "Parent node not found: " + parent_path;
		return result;
	}

	if (properties.is_empty()) {
		result["ok"] = false;
		result["error"] = "Explicit properties array required specifying attributes to synchronize.";
		return result;
	}

	Object *sync_obj = ClassDB::instantiate("MultiplayerSynchronizer");
	Node *node = Object::cast_to<Node>(sync_obj);
	if (!node) {
		if (sync_obj) {
			memdelete(sync_obj);
		}
		result["ok"] = false;
		result["error"] = "Engine does not have Multiplayer module loaded.";
		return result;
	}

	Object *config_obj = ClassDB::instantiate("SceneReplicationConfig");
	Ref<Resource> config(Object::cast_to<Resource>(config_obj));
	if (config.is_valid()) {
		for (int i = 0; i < properties.size(); i++) {
			String prop_path = properties[i];
			config->call("add_property", NodePath(prop_path));
		}
		node->set("replication_config", config);
	}

	node->set_name(node_name);

	Node *scene_root = _get_scene_root();
	if (EditorUndoRedoManager::get_singleton()) {
		EditorUndoRedoManager::get_singleton()->create_action("Add MultiplayerSynchronizer '" + node_name + "'");
		EditorUndoRedoManager::get_singleton()->add_do_method(parent, "add_child", node, true);
		EditorUndoRedoManager::get_singleton()->add_do_method(node, "set_owner", scene_root);
		EditorUndoRedoManager::get_singleton()->add_do_reference(node);
		EditorUndoRedoManager::get_singleton()->add_undo_method(parent, "remove_child", node);
		EditorUndoRedoManager::get_singleton()->commit_action();
	} else {
		parent->add_child(node, true);
		node->set_owner(scene_root);
	}

	result["ok"] = true;
	result["node_path"] = String(node->get_path());
	result["name"] = node->get_name();
	result["properties_mapped"] = properties.size();
	return result;
}

Dictionary JustAMCPNetworkingTools::networking_get_info(const Dictionary &p_args) {
	Dictionary result;
	Node *root = _get_scene_root();
	if (!root || !root->get_tree()) {
		result["ok"] = false;
		result["error"] = "No scene root available.";
		return result;
	}
	Ref<MultiplayerAPI> multiplayer = root->get_tree()->get_multiplayer();
	if (multiplayer.is_null()) {
		result["ok"] = true;
		result["has_multiplayer_peer"] = false;
		result["unique_id"] = 0;
		result["is_server"] = false;
		return result;
	}

	Ref<MultiplayerPeer> peer = multiplayer->get_multiplayer_peer();
	result["ok"] = true;
	result["has_multiplayer_peer"] = peer.is_valid();
	result["unique_id"] = peer.is_valid() ? multiplayer->get_unique_id() : 0;
	result["is_server"] = peer.is_valid() ? multiplayer->is_server() : false;
	if (peer.is_valid()) {
		result["connection_status"] = int(peer->get_connection_status());
	}
	return result;
}

Dictionary JustAMCPNetworkingTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "networking_create_http_request") {
		return networking_create_http_request(p_args);
	}
	if (p_tool_name == "networking_setup_websocket") {
		return networking_setup_websocket(p_args);
	}
	if (p_tool_name == "networking_setup_multiplayer") {
		return networking_setup_multiplayer(p_args);
	}
	if (p_tool_name == "networking_setup_rpc") {
		return networking_setup_rpc(p_args);
	}
	if (p_tool_name == "networking_setup_sync") {
		return networking_setup_sync(p_args);
	}
	if (p_tool_name == "networking_get_info") {
		return networking_get_info(p_args);
	}
	return Dictionary();
}

#endif
