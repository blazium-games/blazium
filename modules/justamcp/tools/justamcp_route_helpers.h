/**************************************************************************/
/*  justamcp_route_helpers.h                                              */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

#include "../justamcp_editor_scene_access.h"
#include "../justamcp_mcp_tool_macros.h"
#include "../justamcp_runtime.h"
#include "../justamcp_server.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "servers/display_server.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "scene/main/node.h"
#include "scene/main/window.h"
#endif

inline Dictionary justamcp_normalize_runtime_result(const Dictionary &p_raw) {
	if (p_raw.has("ok")) {
		return p_raw;
	}
	const String type = p_raw.get("type", "");
	if (type == "error") {
		return MCP_ERROR(-32000, String(p_raw.get("message", "Runtime error")));
	}
	Dictionary out = p_raw.duplicate();
	out["ok"] = true;
	return out;
}

inline Dictionary justamcp_runtime_bridge_execute(const String &p_internal_name, const Dictionary &p_args) {
	if (!JustAMCPRuntime::get_singleton()) {
		return MCP_ERROR(-32000, "JustAMCPRuntime not initialized or game bridge inactive. Start play mode with MCP enabled, or ensure '--enable-mcp' / feature flags are active.");
	}
	String cmd = p_internal_name;
	if (cmd == "take_game_screenshot") {
		cmd = "capture_screenshot";
	} else if (cmd == "runtime_quit") {
		cmd = "quit";
	} else if (cmd == "get_network_info") {
		cmd = "network_state";
	} else if (cmd == "get_audio_info") {
		cmd = "audio_state";
	} else if (cmd == "inject_gamepad") {
		cmd = "gamepad";
	} else if (cmd == "runtime_get_tree") {
		cmd = "get_tree";
	} else if (cmd == "runtime_inspect_node" || cmd == "query_runtime_node") {
		cmd = "get_node";
	} else if (cmd == "runtime_get_autoload") {
		cmd = "get_autoload";
	} else if (cmd == "runtime_find_nodes_by_script") {
		cmd = "find_nodes_by_script";
	} else if (cmd == "runtime_batch_get_properties") {
		cmd = "batch_get_properties";
	} else if (cmd == "runtime_find_ui_elements") {
		cmd = "find_ui_elements";
	} else if (cmd == "runtime_click_button_by_text") {
		cmd = "click_button_by_text";
	} else if (cmd == "runtime_move_node") {
		cmd = "move_node";
	} else if (cmd == "runtime_monitor_properties") {
		cmd = "monitor_properties";
	}
	Dictionary runtime_args = p_args;
	if (p_internal_name == "runtime_get_tree" && runtime_args.has("max_depth") && !runtime_args.has("depth")) {
		runtime_args["depth"] = runtime_args["max_depth"];
	}
	if (p_internal_name == "runtime_inspect_node" && runtime_args.has("node") && !runtime_args.has("path")) {
		runtime_args["path"] = runtime_args["node"];
	}
	if (p_internal_name == "query_runtime_node" && runtime_args.has("node_path") && !runtime_args.has("path")) {
		runtime_args["path"] = runtime_args["node_path"];
	}
	if (p_internal_name == "runtime_batch_get_properties" && runtime_args.has("node_paths") && !runtime_args.has("nodes")) {
		runtime_args["nodes"] = runtime_args["node_paths"];
	}
	return justamcp_normalize_runtime_result(JustAMCPRuntime::get_singleton()->execute_command(cmd, runtime_args));
}

inline bool justamcp_is_runtime_bridge_tool(const String &p_name) {
	return p_name == "take_game_screenshot" || p_name == "runtime_info" ||
			p_name == "runtime_get_errors" || p_name == "runtime_capabilities" ||
			p_name == "eval_expression" || p_name == "find_nodes" ||
			p_name == "runtime_get_tree" || p_name == "runtime_inspect_node" ||
			p_name == "query_runtime_node" || p_name == "get_node_property" ||
			p_name == "call_node_method" || p_name == "wait_for_property" ||
			p_name == "press_button" || p_name == "runtime_get_autoload" ||
			p_name == "runtime_find_nodes_by_script" || p_name == "runtime_batch_get_properties" ||
			p_name == "runtime_find_ui_elements" || p_name == "runtime_click_button_by_text" ||
			p_name == "runtime_move_node" || p_name == "runtime_monitor_properties" ||
			p_name == "inject_drag" || p_name == "inject_scroll" ||
			p_name == "inject_gesture" || p_name == "runtime_quit" ||
			p_name == "get_network_info" || p_name == "get_audio_info" ||
			p_name == "inject_gamepad" || p_name == "run_custom_command";
}

#ifdef TOOLS_ENABLED
inline Node *justamcp_find_edited_node(const String &p_path) {
	Node *node = JustAMCPEditorSceneAccess::find_node_in_edited_scene(p_path);
	if (node) {
		return node;
	}
	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (root && p_path.begins_with("/root/") && root->get_tree()) {
		return root->get_tree()->get_root()->get_node_or_null(NodePath(p_path.substr(6)));
	}
	return nullptr;
}

inline Variant justamcp_serialize_basic_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::OBJECT) {
		Object *obj = p_value;
		Dictionary info;
		info["class"] = obj ? obj->get_class() : String();
		Resource *res = Object::cast_to<Resource>(obj);
		if (res) {
			info["path"] = res->get_path();
		}
		Node *node = Object::cast_to<Node>(obj);
		if (node) {
			info["path"] = String(node->get_path());
		}
		return info;
	}
	if (p_value.get_type() == Variant::NODE_PATH) {
		return String(p_value);
	}
	return p_value;
}

inline Dictionary justamcp_set_node_resource_property(const Dictionary &p_args, const String &p_default_property, const String &p_resource_arg) {
	String node_path = p_args.get("node_path", p_args.get("path", ""));
	String property = p_args.get("property", p_default_property);
	String resource_path = p_args.get(p_resource_arg, p_args.get("resource_path", ""));
	Dictionary result;
	if (node_path.is_empty() || property.is_empty() || resource_path.is_empty()) {
		result["ok"] = false;
		result["error"] = "node_path, property, and resource path are required.";
		return result;
	}
	Node *node = justamcp_find_edited_node(node_path);
	if (!node) {
		result["ok"] = false;
		result["error"] = "Node not found: " + node_path;
		return result;
	}
	Ref<Resource> resource = ResourceLoader::load(resource_path);
	if (resource.is_null()) {
		result["ok"] = false;
		result["error"] = "Resource not found or failed to load: " + resource_path;
		return result;
	}
	node->set(property, resource);
	result["ok"] = true;
	result["node_path"] = node_path;
	result["property"] = property;
	result["resource_path"] = resource_path;
	return result;
}

inline Dictionary justamcp_set_resource_property_or_value(const Dictionary &p_args) {
	if (p_args.has("resource_path")) {
		return justamcp_set_node_resource_property(p_args, String(p_args.get("property", "")), "resource_path");
	}
	Dictionary ret;
	String node_path = p_args.get("node_path", p_args.get("path", ""));
	String property = p_args.get("property", "");
	Node *node = justamcp_find_edited_node(node_path);
	if (node && !property.is_empty() && p_args.has("value")) {
		node->set(property, p_args["value"]);
		ret["ok"] = true;
		ret["node_path"] = node_path;
		ret["property"] = property;
		ret["value"] = justamcp_serialize_basic_variant(p_args["value"]);
		return ret;
	}
	ret["ok"] = false;
	ret["error"] = "Requires node_path, property, and either resource_path or value.";
	return ret;
}

inline Dictionary justamcp_save_resource_to_file(const Dictionary &p_args) {
	Dictionary ret;
	String node_path = p_args.get("node_path", p_args.get("path", ""));
	String property = p_args.get("property", "");
	String save_path = p_args.get("save_path", p_args.get("resource_path", ""));
	Node *node = justamcp_find_edited_node(node_path);
	bool valid = false;
	Variant value = node && !property.is_empty() ? node->get(property, &valid) : Variant();
	Object *object_value = valid && value.get_type() == Variant::OBJECT ? Object::cast_to<Object>(value) : nullptr;
	Ref<Resource> resource = object_value ? Ref<Resource>(Object::cast_to<Resource>(object_value)) : Ref<Resource>();
	if (resource.is_valid() && !save_path.is_empty()) {
		Error err = ResourceSaver::save(resource, save_path);
		ret["ok"] = err == OK;
		ret["path"] = save_path;
		if (err != OK) {
			ret["error"] = "Failed to save resource.";
			ret["error_code"] = err;
		}
		return ret;
	}
	ret["ok"] = false;
	ret["error"] = "Resource property not found or save_path missing.";
	return ret;
}
#endif

inline Dictionary justamcp_wait(const Dictionary &p_args) {
	int ms = p_args.get("ms", 0);
	if (ms <= 0 && p_args.has("seconds")) {
		ms = int(double(p_args.get("seconds", 0.0)) * 1000.0);
	}
	ms = CLAMP(ms, 0, 5000);
	if (Thread::is_main_thread()) {
		const uint64_t end_ms = OS::get_singleton()->get_ticks_msec() + uint64_t(ms);
		while (OS::get_singleton()->get_ticks_msec() < end_ms) {
			const uint64_t remaining = end_ms - OS::get_singleton()->get_ticks_msec();
			const uint64_t slice_ms = MIN(remaining, (uint64_t)8);
			OS::get_singleton()->delay_usec(slice_ms * 1000ULL);
			if (DisplayServer::get_singleton()) {
				DisplayServer::get_singleton()->process_events();
			}
		}
	} else {
		OS::get_singleton()->delay_usec(uint64_t(ms) * 1000ULL);
	}
	Dictionary ret;
	ret["ok"] = true;
	ret["waited_ms"] = ms;
	return ret;
}

inline Dictionary justamcp_get_runtime_status() {
	Dictionary ret;
	ret["ok"] = true;
	ret["runtime_available"] = JustAMCPRuntime::get_singleton() != nullptr;
#ifdef TOOLS_ENABLED
	bool editor_ready = EditorNode::get_singleton() && EditorInterface::get_singleton();
	ret["editor_playing"] = editor_ready ? EditorInterface::get_singleton()->is_playing_scene() : false;
	ret["playing_scene"] = editor_ready ? EditorInterface::get_singleton()->get_playing_scene() : String();
#else
	ret["editor_playing"] = false;
	ret["playing_scene"] = String();
#endif
	return ret;
}

inline Dictionary justamcp_get_runtime_log(const Dictionary &p_args) {
	Dictionary ret;
	Array logs;
	int limit = p_args.get("limit", 200);
	if (JustAMCPServer::get_singleton()) {
		Vector<String> engine_logs = JustAMCPServer::get_singleton()->get_engine_logs();
		for (int i = MAX(0, engine_logs.size() - limit); i < engine_logs.size(); i++) {
			logs.push_back(engine_logs[i]);
		}
	}
	ret["ok"] = true;
	ret["logs"] = logs;
	ret["count"] = logs.size();
	return ret;
}

inline Dictionary justamcp_classdb_query(const Dictionary &p_args) {
	StringName class_name = p_args.get("class_name", "");
	String query = p_args.get("query", "");
	const bool include_virtual = bool(p_args.get("include_virtual", true));
	Dictionary ret;
	if (String(class_name).is_empty() || !ClassDB::class_exists(class_name)) {
		ret["ok"] = false;
		ret["error"] = "Class not found: " + String(class_name);
		return ret;
	}
	List<PropertyInfo> properties;
	ClassDB::get_property_list(class_name, &properties, false);
	Array property_list;
	for (const PropertyInfo &property : properties) {
		Dictionary entry = property.operator Dictionary();
		if (query.is_empty() || String(entry.get("name", "")).containsn(query)) {
			property_list.push_back(entry);
		}
	}
	List<MethodInfo> methods;
	ClassDB::get_method_list(class_name, &methods, false);
	Array method_list;
	for (const MethodInfo &method : methods) {
		if (!include_virtual && (method.flags & METHOD_FLAG_VIRTUAL)) {
			continue;
		}
#ifdef DEBUG_METHODS_ENABLED
		Dictionary entry = method.operator Dictionary();
#else
		Dictionary entry;
		entry["name"] = method.name;
#endif
		if (query.is_empty() || String(entry.get("name", "")).containsn(query)) {
			method_list.push_back(entry);
		}
	}
	List<MethodInfo> signals;
	ClassDB::get_signal_list(class_name, &signals, false);
	Array signal_list;
	for (const MethodInfo &signal : signals) {
#ifdef DEBUG_METHODS_ENABLED
		Dictionary entry = signal.operator Dictionary();
#else
		Dictionary entry;
		entry["name"] = signal.name;
#endif
		if (query.is_empty() || String(entry.get("name", "")).containsn(query)) {
			signal_list.push_back(entry);
		}
	}
	List<String> constants;
	ClassDB::get_integer_constant_list(class_name, &constants, false);
	Array constant_list;
	for (const String &constant : constants) {
		if (query.is_empty() || constant.containsn(query)) {
			constant_list.push_back(constant);
		}
	}
	ret["ok"] = true;
	ret["class_name"] = String(class_name);
	ret["parent_class"] = ClassDB::get_parent_class(class_name);
	ret["can_instantiate"] = ClassDB::can_instantiate(class_name);
	ret["properties"] = property_list;
	ret["methods"] = method_list;
	ret["signals"] = signal_list;
	ret["constants"] = constant_list;
	return ret;
}
