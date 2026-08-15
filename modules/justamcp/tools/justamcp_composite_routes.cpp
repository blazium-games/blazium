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

#include "core/object/class_db.h"
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
#include "justamcp_profiling_tools.h"
#include "justamcp_script_tools.h"
#include "servers/display/display_server.h"

#ifdef TOOLS_ENABLED
#include "editor/file_system/editor_file_system.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "scene/main/node.h"
#include "scene/main/window.h"
#endif

#ifdef TOOLS_ENABLED
static Node *_justamcp_find_node(const String &p_path) {
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

static Dictionary _justamcp_normalize_runtime_result(const Dictionary &p_raw) {
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

static Variant _justamcp_serialize_basic_variant(const Variant &p_value) {
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

static Dictionary _justamcp_set_node_resource_property(const Dictionary &p_args, const String &p_default_property, const String &p_resource_arg) {
	String node_path = p_args.get("node_path", p_args.get("path", ""));
	String property = p_args.get("property", p_default_property);
	String resource_path = p_args.get(p_resource_arg, p_args.get("resource_path", ""));
	Dictionary result;
	if (node_path.is_empty() || property.is_empty() || resource_path.is_empty()) {
		result["ok"] = false;
		result["error"] = "node_path, property, and resource path are required.";
		return result;
	}
	Node *node = _justamcp_find_node(node_path);
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

static Dictionary _justamcp_scene_tree_dump(const Dictionary &p_args = Dictionary()) {
	Dictionary result;
	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		result["ok"] = false;
		result["error"] = "No scene is currently open.";
		return result;
	}
	const int max_nodes = CLAMP(int(p_args.get("max_nodes", 2000)), 1, 10000);
	Array nodes;
	List<Node *> stack;
	stack.push_back(root);
	bool truncated = false;
	while (!stack.is_empty()) {
		if (nodes.size() >= max_nodes) {
			truncated = true;
			break;
		}
		Node *node = stack.front()->get();
		stack.pop_front();
		Dictionary item;
		item["name"] = node->get_name();
		item["type"] = node->get_class();
		item["path"] = node == root ? "." : String(root->get_path_to(node));
		Ref<Script> node_script = node->get_script();
		item["script"] = node_script.is_valid() ? node_script->get_path() : String();
		item["child_count"] = node->get_child_count();
		nodes.push_back(item);
		if (nodes.size() >= max_nodes) {
			truncated = true;
			break;
		}
		for (int i = node->get_child_count() - 1; i >= 0; i--) {
			stack.push_front(node->get_child(i));
		}
	}
	result["ok"] = true;
	result["root"] = root->get_name();
	result["scene_path"] = root->get_scene_file_path();
	result["nodes"] = nodes;
	result["count"] = nodes.size();
	result["truncated"] = truncated;
	return result;
}
#endif

Dictionary JustAMCPToolExecutor::execute_composite_tool(const String &p_internal_name, const Dictionary &p_args) {
	if (p_internal_name == "logs_read") {
		const String source = String(p_args.get("source", "editor"));
		const String cursor = String(p_args.get("cursor", ""));
		if (source == "mcp" && JustAMCPServer::get_singleton()) {
			Dictionary page = JustAMCPServer::get_singleton()->get_mcp_notification_log_page(cursor);
			if (page.has("ok") && !bool(page.get("ok", true))) {
				Dictionary ret;
				ret["ok"] = false;
				ret["error"] = page.get("error", "Invalid pagination cursor.");
				ret["error_code"] = page.get("error_code", -32602);
				return ret;
			}
			Dictionary ret;
			ret["ok"] = true;
			ret["source"] = "mcp";
			ret["notifications"] = page.get("notifications", Array());
			ret["count"] = Array(ret["notifications"]).size();
			if (page.has("nextCursor")) {
				ret["nextCursor"] = page["nextCursor"];
			}
			return ret;
		}

		Dictionary ret;
		Dictionary log_args;
		log_args["limit"] = p_args.get("limit", 200);
		log_args["cursor"] = cursor;
		Dictionary logs = editor_tools->editor_get_output_log(log_args);
		if (!bool(logs.get("ok", true))) {
			return logs;
		}
		Array lines = logs.get("logs", Array());
		int since_index = p_args.get("since_index", 0);
		Array sliced;
		for (int i = MAX(0, since_index); i < lines.size(); i++) {
			sliced.push_back(lines[i]);
		}
		ret["ok"] = true;
		ret["source"] = source;
		ret["logs"] = sliced;
		ret["count"] = sliced.size();
		ret["next_index"] = lines.size();
		if (logs.has("nextCursor")) {
			ret["nextCursor"] = logs["nextCursor"];
		}
		return ret;
	}
	if (p_internal_name == "open_in_godot") {
		String path = p_args.get("path", "");
		Dictionary open_args;
		open_args["path"] = path;
		open_args["line"] = p_args.get("line", -1);
		if (path.ends_with(".tscn") || path.ends_with(".scn")) {
			return editor_tools->editor_open_scene(open_args);
		}
		if (path.ends_with(".gd") || path.ends_with(".cs") || path.ends_with(".gdshader")) {
			return script_tools->execute_tool("open_script_in_editor", open_args);
		}
		Dictionary open_result;
#ifdef TOOLS_ENABLED
		Ref<Resource> resource = ResourceLoader::load(path);
		if (resource.is_valid() && EditorInterface::get_singleton()) {
			EditorInterface::get_singleton()->edit_resource(resource);
			open_result["ok"] = true;
			open_result["path"] = path;
			open_result["message"] = "Resource opened in the editor inspector.";
			return open_result;
		}
#endif
		open_result["ok"] = false;
		open_result["error"] = "Unsupported or missing resource path: " + path;
		return open_result;
	}
	if (p_internal_name == "rescan_filesystem") {
		Dictionary ret;
#ifdef TOOLS_ENABLED
		if (EditorFileSystem::get_singleton()) {
			EditorFileSystem::get_singleton()->call_deferred(SNAME("scan"));
			ret["ok"] = true;
			ret["message"] = "Editor filesystem rescan requested.";
			return ret;
		}
#endif
		ret["ok"] = false;
		ret["error"] = "Editor filesystem unavailable.";
		return ret;
	}
	if (p_internal_name == "scene_tree_dump") {
#ifdef TOOLS_ENABLED
		return _justamcp_scene_tree_dump(p_args);
#else
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Scene tree dump is only available in tools/editor builds.";
		return ret;
#endif
	}

	if (p_internal_name == "project_run") {
		if (p_args.get("autosave", true)) {
			editor_tools->editor_save_all_scenes(Dictionary());
		}
		if (p_args.get("main", false)) {
			return editor_tools->editor_play_main(Dictionary());
		}
		Dictionary play_args;
		play_args["scene_path"] = p_args.get("scene_path", "");
		return editor_tools->editor_play_scene(play_args);
	}

	if (p_internal_name == "set_mesh") {
		return _justamcp_set_node_resource_property(p_args, "mesh", "mesh_path");
	}
	if (p_internal_name == "set_material") {
		return _justamcp_set_node_resource_property(p_args, "material_override", "material_path");
	}
	if (p_internal_name == "set_sprite_texture") {
		return _justamcp_set_node_resource_property(p_args, "texture", "texture_path");
	}
	if (p_internal_name == "set_collision_shape") {
		return _justamcp_set_node_resource_property(p_args, "shape", "shape_path");
	}
	if (p_internal_name == "set_resource_property") {
		if (p_args.has("resource_path")) {
			return _justamcp_set_node_resource_property(p_args, String(p_args.get("property", "")), "resource_path");
		}
		Dictionary ret;
#ifdef TOOLS_ENABLED
		String node_path = p_args.get("node_path", p_args.get("path", ""));
		String property = p_args.get("property", "");
		Node *node = _justamcp_find_node(node_path);
		if (node && !property.is_empty() && p_args.has("value")) {
			node->set(property, p_args["value"]);
			ret["ok"] = true;
			ret["node_path"] = node_path;
			ret["property"] = property;
			ret["value"] = _justamcp_serialize_basic_variant(p_args["value"]);
			return ret;
		}
#endif
		ret["ok"] = false;
		ret["error"] = "Requires node_path, property, and either resource_path or value.";
		return ret;
	}
	if (p_internal_name == "save_resource_to_file") {
		Dictionary ret;
#ifdef TOOLS_ENABLED
		String node_path = p_args.get("node_path", p_args.get("path", ""));
		String property = p_args.get("property", "");
		String save_path = p_args.get("save_path", p_args.get("resource_path", ""));
		Node *node = _justamcp_find_node(node_path);
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
#endif
		ret["ok"] = false;
		ret["error"] = "Resource property not found or save_path missing.";
		return ret;
	}

	if (p_internal_name == "batch_execute") {
		Array steps = p_args.get("steps", Array());
		bool stop_on_error = p_args.get("stop_on_error", true);
		bool undo_on_error = p_args.get("undo_on_error", false);
		Array results;
		int completed = 0;
		const int total_steps = steps.size();
		justamcp_report_progress(0, total_steps > 0 ? total_steps : 1, "Starting batch_execute");
		for (int i = 0; i < steps.size(); i++) {
			if (justamcp_is_cancel_requested()) {
				Dictionary err;
				err["ok"] = false;
				err["error"] = "cancelled";
				results.push_back(err);
				break;
			}
			justamcp_report_progress(i, total_steps > 0 ? total_steps : 1, vformat("batch step %d", i + 1));
			if (steps[i].get_type() != Variant::DICTIONARY) {
				Dictionary step_error;
				step_error["ok"] = false;
				step_error["error"] = "Step is not an object.";
				results.push_back(step_error);
				if (stop_on_error) {
					break;
				}
				continue;
			}
			Dictionary step = steps[i];
			String tool_name = step.get("tool_name", step.get("tool", ""));
			if (tool_name == "batch_execute" || tool_name == "blazium_batch_execute") {
				Dictionary step_error;
				step_error["ok"] = false;
				step_error["error"] = "Nested batch_execute is not allowed.";
				results.push_back(step_error);
				if (stop_on_error) {
					break;
				}
				continue;
			}
			if (tool_name == "execute_tool" || tool_name == "blazium_execute_tool") {
				Dictionary step_error;
				step_error["ok"] = false;
				step_error["error"] = "execute_tool is not allowed in batch_execute.";
				results.push_back(step_error);
				if (stop_on_error) {
					break;
				}
				continue;
			}
			Dictionary args = step.get("arguments", step.get("args", Dictionary()));
			Dictionary step_result = execute_tool(tool_name, args);
			results.push_back(step_result);
			bool ok = step_result.get("ok", !step_result.has("error"));
			if (!ok) {
				if (undo_on_error) {
					for (int undo_idx = 0; undo_idx < completed; undo_idx++) {
						editor_tools->editor_undo(Dictionary());
					}
				}
				if (stop_on_error) {
					break;
				}
			} else {
				completed++;
			}
		}
		justamcp_report_progress(total_steps > 0 ? total_steps : 1, total_steps > 0 ? total_steps : 1, "batch_execute finished");
		Dictionary ret;
		ret["ok"] = true;
		ret["results"] = results;
		ret["completed"] = completed;
		ret["count"] = results.size();
		return ret;
	}

	if (p_internal_name == "export_release" || p_internal_name == "export_debug" || p_internal_name == "export_custom") {
		Dictionary export_args = p_args.duplicate();
		if (p_internal_name == "export_release") {
			export_args["debug"] = false;
		} else if (p_internal_name == "export_debug") {
			export_args["debug"] = true;
		}
		justamcp_report_progress(0, 1, vformat("Starting %s", p_internal_name));
		if (justamcp_is_cancel_requested()) {
			Dictionary err;
			err["ok"] = false;
			err["error"] = "cancelled";
			return err;
		}
		Dictionary export_result = export_tools->execute_tool("export_project", export_args);
		justamcp_report_progress(1, 1, vformat("Finished %s", p_internal_name));
		return export_result;
	}

	if (p_internal_name == "wait") {
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
	if (p_internal_name == "get_runtime_status") {
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
	if (p_internal_name == "get_runtime_log") {
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

	if (p_internal_name == "take_game_screenshot" || p_internal_name == "runtime_info" ||
			p_internal_name == "runtime_get_errors" || p_internal_name == "runtime_capabilities" ||
			p_internal_name == "eval_expression" || p_internal_name == "find_nodes" ||
			p_internal_name == "runtime_get_tree" || p_internal_name == "runtime_inspect_node" ||
			p_internal_name == "query_runtime_node" ||
			p_internal_name == "get_node_property" || p_internal_name == "call_node_method" ||
			p_internal_name == "wait_for_property" || p_internal_name == "press_button" ||
			p_internal_name == "runtime_get_autoload" || p_internal_name == "runtime_find_nodes_by_script" ||
			p_internal_name == "runtime_batch_get_properties" || p_internal_name == "runtime_find_ui_elements" ||
			p_internal_name == "runtime_click_button_by_text" || p_internal_name == "runtime_move_node" ||
			p_internal_name == "runtime_monitor_properties" ||
			p_internal_name == "inject_drag" || p_internal_name == "inject_scroll" ||
			p_internal_name == "inject_gesture" || p_internal_name == "runtime_quit" ||
			p_internal_name == "get_network_info" || p_internal_name == "get_audio_info" ||
			p_internal_name == "inject_gamepad" || p_internal_name == "run_custom_command") {
		if (JustAMCPRuntime::get_singleton()) {
			String cmd = p_internal_name;
			if (cmd == "take_game_screenshot") {
				cmd = "capture_screenshot";
			}
			if (cmd == "runtime_quit") {
				cmd = "quit";
			}
			if (cmd == "get_network_info") {
				cmd = "network_state";
			}
			if (cmd == "get_audio_info") {
				cmd = "audio_state";
			}
			if (cmd == "inject_gamepad") {
				cmd = "gamepad";
			}
			if (cmd == "runtime_get_tree") {
				cmd = "get_tree";
			}
			if (cmd == "runtime_inspect_node") {
				cmd = "get_node";
			}
			if (cmd == "query_runtime_node") {
				cmd = "get_node";
			}
			if (cmd == "runtime_get_autoload") {
				cmd = "get_autoload";
			}
			if (cmd == "runtime_find_nodes_by_script") {
				cmd = "find_nodes_by_script";
			}
			if (cmd == "runtime_batch_get_properties") {
				cmd = "batch_get_properties";
			}
			if (cmd == "runtime_find_ui_elements") {
				cmd = "find_ui_elements";
			}
			if (cmd == "runtime_click_button_by_text") {
				cmd = "click_button_by_text";
			}
			if (cmd == "runtime_move_node") {
				cmd = "move_node";
			}
			if (cmd == "runtime_monitor_properties") {
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
			return _justamcp_normalize_runtime_result(JustAMCPRuntime::get_singleton()->execute_command(cmd, runtime_args));
		} else {
			return MCP_ERROR(-32000, "JustAMCPRuntime not initialized or game bridge inactive. Start play mode with MCP enabled, or ensure '--enable-mcp' / feature flags are active.");
		}
	}

	if (p_internal_name == "runtime_run_gut_tests") {
		Dictionary ret;
		ret["ok"] = true;
		ret["message"] = "Run GUT tests from the editor panel or with Godot command line using addons/gut/gut_cmdln.gd.";
		ret["test_script"] = p_args.get("test_script", "");
		return ret;
	}
	if (p_internal_name == "runtime_get_test_results") {
		Dictionary ret;
		ret["ok"] = true;
		ret["results"] = Array();
		ret["message"] = "No native GUT result bridge is active. Check editor or command-line GUT output.";
		return ret;
	}

	if (p_internal_name == "autowork_generate_test") {
		return script_tools->execute_tool(p_internal_name, p_args);
	}

	if (p_internal_name == "classdb_query") {
		StringName class_name = p_args.get("class_name", "");
		String query = p_args.get("query", "");
		bool no_inheritance = false;
		Dictionary ret;
		if (String(class_name).is_empty() || !ClassDB::class_exists(class_name)) {
			ret["ok"] = false;
			ret["error"] = "Class not found: " + String(class_name);
			return ret;
		}
		List<PropertyInfo> properties;
		ClassDB::get_property_list(class_name, &properties, no_inheritance);
		Array property_list;
		for (const PropertyInfo &property : properties) {
			Dictionary entry = property.operator Dictionary();
			if (query.is_empty() || String(entry.get("name", "")).containsn(query)) {
				property_list.push_back(entry);
			}
		}
		List<MethodInfo> methods;
		ClassDB::get_method_list(class_name, &methods, no_inheritance);
		Array method_list;
		for (const MethodInfo &method : methods) {
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
		ClassDB::get_signal_list(class_name, &signals, no_inheritance);
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
		ClassDB::get_integer_constant_list(class_name, &constants, no_inheritance);
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

	if (p_internal_name == "project_state") {
		Dictionary ret;
		ret["ok"] = true;
		ret["statistics"] = analysis_tools->execute_tool("get_project_statistics", p_args);
		Dictionary settings_args;
		settings_args["max_results"] = 200;
		ret["settings"] = project_tools->execute_tool("list_settings", settings_args);
		ret["current_scene"] = scene_tools->get_current_scene(Dictionary());
		return ret;
	}
	if (p_internal_name == "project_advise") {
		Dictionary ret;
		Array advice;
		Dictionary current_scene = scene_tools->get_current_scene(Dictionary());
		if (!current_scene.get("ok", false)) {
			advice.push_back("Open or create a scene before using scene/node editing tools.");
		}
		Dictionary errors_args;
		errors_args["limit"] = 50;
		Dictionary errors = editor_tools->editor_get_errors(errors_args);
		if (int(errors.get("count", 0)) > 0) {
			advice.push_back("Review recent editor errors before making structural changes.");
		}
		Dictionary map_args;
		map_args["lod"] = 0;
		Dictionary project_map = project_tools->execute_tool("map_project", map_args);
		if (int(project_map.get("total_scripts", 0)) == 0) {
			advice.push_back("No scripts were found under res://; create scripts before requesting script intelligence.");
		}
		ret["ok"] = true;
		ret["advice"] = advice;
		ret["current_scene"] = current_scene;
		return ret;
	}
	if (p_internal_name == "runtime_diagnose") {
		Dictionary ret;
		Dictionary errors_args;
		errors_args["limit"] = p_args.get("limit", 100);
		ret["ok"] = true;
		ret["status"] = execute_tool("blazium_get_runtime_status", Dictionary());
		ret["errors"] = editor_tools->editor_get_errors(errors_args);
		ret["performance"] = profiling_tools->execute_tool("get_performance_monitors", Dictionary());
		return ret;
	}
	if (p_internal_name == "scene_validate") {
		Dictionary ret;
		Array issues;
#ifdef TOOLS_ENABLED
		Node *root = JustAMCPEditorSceneAccess::get_edited_root();
		if (!root) {
			issues.push_back("No edited scene is open.");
		} else {
			if (root->get_scene_file_path().is_empty()) {
				issues.push_back("The current scene has not been saved to a scene file.");
			}
			List<Node *> stack;
			stack.push_back(root);
			while (!stack.is_empty()) {
				Node *node = stack.front()->get();
				stack.pop_front();
				if (node != root && !node->get_owner()) {
					issues.push_back("Node has no owner and may not be saved: " + String(root->get_path_to(node)));
				}
				Ref<Script> node_script = node->get_script();
				if (node_script.is_valid() && !node_script->get_path().is_empty() && !ResourceLoader::exists(node_script->get_path())) {
					issues.push_back("Missing script resource on node: " + String(root->get_path_to(node)));
				}
				for (int i = 0; i < node->get_child_count(); i++) {
					stack.push_back(node->get_child(i));
				}
			}
		}
#endif
		ret["ok"] = true;
		ret["valid"] = issues.is_empty();
		ret["issues"] = issues;
		return ret;
	}
	if (p_internal_name == "scene_analyze") {
		Dictionary ret;
		ret["ok"] = true;
		ret["tree"] = execute_tool("blazium_scene_tree_dump", Dictionary());
		ret["complexity"] = analysis_tools->execute_tool("analyze_scene_complexity", p_args);
		return ret;
	}
	if (p_internal_name == "script_analyze") {
		Dictionary script_args = p_args;
		if (script_args.has("query") && !script_args.has("pattern")) {
			script_args["pattern"] = script_args["query"];
		}
		return script_tools->execute_tool("search_in_scripts", script_args);
	}
	if (p_internal_name == "project_symbol_search") {
		Dictionary script_args;
		script_args["query"] = p_args.get("query", "");
		script_args["path"] = p_args.get("path", "res://");
		script_args["include_addons"] = p_args.get("include_addons", false);
		return analysis_tools->execute_tool("find_script_references", script_args);
	}
	if (p_internal_name == "project_index") {
		Dictionary ret;
		ret["ok"] = true;
		ret["scripts"] = project_tools->execute_tool("map_project", p_args);
		ret["scenes"] = project_tools->execute_tool("map_scenes", p_args);
		ret["statistics"] = analysis_tools->execute_tool("get_project_statistics", p_args);
		return ret;
	}
	if (p_internal_name == "scene_dependency_graph") {
		Dictionary deps_args;
		deps_args["path"] = p_args.get("scene_path", p_args.get("path", ""));
		return batch_tools->execute_tool("get_scene_dependencies", deps_args);
	}

	return Dictionary();
}
