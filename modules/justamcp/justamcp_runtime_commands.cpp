/**************************************************************************/
/*  justamcp_runtime_commands.cpp                                         */
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

#include "justamcp_read_limits.h"
#include "justamcp_runtime.h"
#include "tools/justamcp_tool_executor.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/input/input.h"
#include "core/input/input_event.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/json.h"
#include "core/math/expression.h"
#include "core/object/callable_mp.h"
#include "core/object/message_queue.h"
#include "core/object/script_language.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "main/performance.h"
#include "scene/gui/base_button.h"
#include "scene/gui/control.h"
#include "scene/main/multiplayer_api.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "servers/audio/audio_server.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_interface.h"
#include "editor/settings/editor_settings.h"
#endif

Dictionary JustAMCPRuntime::execute_command(const String &p_command, const Dictionary &p_params) {
	if (this != get_singleton()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["type"] = "error";
		ret["message"] = "JustAMCPRuntime command refused on non-singleton instance.";
		return ret;
	}
	if (p_command == "ping") {
		Dictionary ret;
		ret["type"] = "pong";
		ret["timestamp"] = Time::get_singleton()->get_unix_time_from_system();
		return ret;
	} else if (p_command == "get_tree") {
		return _cmd_get_tree(p_params);
	} else if (p_command == "get_node") {
		return _cmd_get_node(p_params);
	} else if (p_command == "set_property") {
		return _cmd_set_property(p_params);
	} else if (p_command == "call_method") {
		return _cmd_call_method(p_params);
	} else if (p_command == "get_metrics") {
		return _cmd_get_metrics(p_params);
	} else if (p_command == "capture_screenshot" || p_command == "capture_viewport") {
		return _cmd_capture_screenshot(p_params);
	} else if (p_command == "inject_action") {
		return _cmd_inject_action(p_params);
	} else if (p_command == "inject_key") {
		return _cmd_inject_key(p_params);
	} else if (p_command == "inject_mouse_click") {
		return _cmd_inject_mouse_click(p_params);
	} else if (p_command == "inject_mouse_motion") {
		return _cmd_inject_mouse_motion(p_params);
	} else if (p_command == "watch_signal") {
		return _cmd_watch_signal(p_params);
	} else if (p_command == "unwatch_signal") {
		return _cmd_unwatch_signal(p_params);
	}

	else if (p_command == "inject_drag") {
		return _cmd_inject_drag(p_params);
	} else if (p_command == "inject_scroll") {
		return _cmd_inject_scroll(p_params);
	} else if (p_command == "inject_gesture") {
		return _cmd_inject_gesture(p_params);
	} else if (p_command == "find_nodes") {
		return _cmd_find_nodes(p_params);
	} else if (p_command == "get_node_property") {
		return _cmd_get_node_property(p_params);
	} else if (p_command == "call_node_method") {
		return _cmd_call_node_method(p_params);
	} else if (p_command == "press_button") {
		return _cmd_press_button(p_params);
	} else if (p_command == "wait_for_property") {
		return _cmd_wait_for_property(p_params);
	} else if (p_command == "runtime_info") {
		return _cmd_runtime_info(p_params);
	} else if (p_command == "runtime_get_errors") {
		return _cmd_runtime_get_errors(p_params);
	} else if (p_command == "runtime_capabilities") {
		return _cmd_runtime_capabilities(p_params);
	} else if (p_command == "eval_expression") {
		return _cmd_eval_expression(p_params);
	} else if (p_command == "quit" || p_command == "runtime_quit") {
		return _cmd_runtime_quit(p_params);
	} else if (p_command == "network_state" || p_command == "get_network_info") {
		return _cmd_get_network_info(p_params);
	} else if (p_command == "audio_state" || p_command == "get_audio_info") {
		return _cmd_get_audio_info(p_params);
	} else if (p_command == "gamepad" || p_command == "inject_gamepad") {
		return _cmd_inject_gamepad(p_params);
	} else if (p_command == "run_custom_command") {
		return _cmd_run_custom_command(p_params);
	} else if (p_command == "get_autoload") {
		return _cmd_get_autoload(p_params);
	} else if (p_command == "find_nodes_by_script") {
		return _cmd_find_nodes_by_script(p_params);
	} else if (p_command == "batch_get_properties") {
		return _cmd_batch_get_properties(p_params);
	} else if (p_command == "find_ui_elements") {
		return _cmd_find_ui_elements(p_params);
	} else if (p_command == "click_button_by_text") {
		return _cmd_click_button_by_text(p_params);
	} else if (p_command == "navigate_to" || p_command == "move_to" || p_command == "move_node") {
		return _cmd_move_node(p_params);
	} else if (p_command == "monitor_properties") {
		return _cmd_monitor_properties(p_params);
	} else if (p_command == "auth_info") {
		return _cmd_auth_info(p_params);
	} else if (p_command == "performance" || p_command == "blazium_performance" || p_command == "get_performance_metrics") {
		return _cmd_get_metrics(p_params);
	} else if (p_command == "tool_call") {
		return _cmd_tool_call(p_params);
	} else if (p_command == "list_tools") {
		return _cmd_list_tools(p_params);
	} else if (p_command == "diagnose" || p_command == "runtime_diagnose") {
		return _cmd_diagnose(p_params);
	} else if (p_command == "get_log_tail" || p_command == "runtime_get_log_tail") {
		return _cmd_get_log_tail(p_params);
	} else if (p_command == "tags_get_on_asset") {
		return _cmd_tags_get_on_asset(p_params);
	} else if (p_command == "tags_list") {
		return _cmd_tags_list(p_params);
	} else if (p_command == "tags_find_assets") {
		return _cmd_tags_find_assets(p_params);
	}

	Dictionary ret;
	ret["type"] = "error";
	ret["message"] = "Unknown command: " + p_command;
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_runtime_quit(const Dictionary &p_params) {
	if (this != get_singleton()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["status"] = "refused";
		ret["message"] = "Quit refused on non-singleton JustAMCPRuntime instance.";
		return ret;
	}
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton() && Engine::get_singleton()->is_editor_hint()) {
		if (EditorInterface::get_singleton() && EditorInterface::get_singleton()->is_playing_scene()) {
			EditorInterface::get_singleton()->stop_playing_scene();
			Dictionary ret;
			ret["ok"] = true;
			ret["status"] = "stopped_play";
			ret["message"] = "Stopped editor play mode (refused to kill editor process).";
			return ret;
		}
		Dictionary ret;
		ret["ok"] = true;
		ret["status"] = "noop";
		ret["message"] = "No play session to stop; editor process left running.";
		return ret;
	}
#endif
	if (MessageQueue::get_singleton()) {
		MessageQueue::get_singleton()->push_callable(callable_mp(this, &JustAMCPRuntime::_quit_engine));
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["status"] = "quitting";
	return ret;
}

void JustAMCPRuntime::_quit_engine() {
	OS::get_singleton()->kill(OS::get_singleton()->get_process_id());
}

Dictionary JustAMCPRuntime::_cmd_auth_info(const Dictionary &p_params) {
	Dictionary ret;
	ret["proto"] = "blazium/1";
	ret["tier"] = 3;
	ret["danger_enabled"] = true;
	ret["engine"] = "Blazium";
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_get_network_info(const Dictionary &p_params) {
	Dictionary ret;
	ret["type"] = "network_info";

	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!tree) {
		ret["error"] = "No SceneTree";
		return ret;
	}

	Ref<MultiplayerAPI> mp = tree->get_multiplayer();
	if (mp.is_valid()) {
		ret["is_active"] = true;
		ret["unique_id"] = mp->get_unique_id();
		ret["is_server"] = mp->is_server();

		Array peers;
		Vector<int> peer_ids = mp->get_peer_ids();
		for (int i = 0; i < peer_ids.size(); i++) {
			peers.push_back(peer_ids[i]);
		}
		ret["peers"] = peers;
	} else {
		ret["is_active"] = false;
	}

	return ret;
}

Dictionary JustAMCPRuntime::_cmd_get_audio_info(const Dictionary &p_params) {
	Dictionary ret;
	ret["type"] = "audio_info";

	AudioServer *as = AudioServer::get_singleton();
	if (!as) {
		ret["type"] = "error";
		ret["message"] = "AudioServer not available";
		return ret;
	}
	Array buses;
	for (int i = 0; i < as->get_bus_count(); i++) {
		Dictionary bus;
		bus["name"] = as->get_bus_name(i);
		bus["volume_db"] = as->get_bus_volume_db(i);
		bus["mute"] = as->is_bus_mute(i);
		bus["bypass_effects"] = as->is_bus_bypassing_effects(i);
		bus["channels"] = as->get_bus_channels(i);
		buses.push_back(bus);
	}
	ret["buses"] = buses;
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_run_custom_command(const Dictionary &p_params) {
	String name = p_params.get("name", "");
	Array args = p_params.get("args", Array());

	if (name.is_empty()) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Missing custom command 'name'";
		return ret;
	}

	if (!_custom_commands.has(name)) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Custom command not found: " + name;
		return ret;
	}

	Callable c = _custom_commands[name];
	if (!c.is_valid()) {
		_custom_commands.erase(name);
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Custom command callable is no longer valid: " + name;
		return ret;
	}

	Variant result = c.callv(args);

	Dictionary ret;
	ret["type"] = "custom_command_result";
	ret["name"] = name;
	ret["result"] = _serialize_value(result);
	return ret;
}

void JustAMCPRuntime::register_custom_command(const String &p_name, const Callable &p_callable) {
	_custom_commands[p_name] = p_callable;
}

void JustAMCPRuntime::unregister_custom_command(const String &p_name) {
	_custom_commands.erase(p_name);
}
Dictionary JustAMCPRuntime::_cmd_tool_call(const Dictionary &p_params) {
	if (!executor) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Tool executor not initialized";
		return ret;
	}

	String tool_name = p_params.get("name", "");
	Dictionary tool_args = p_params.get("args", Dictionary());

	if (tool_name.is_empty()) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Missing tool 'name'";
		return ret;
	}

	return executor->execute_tool(tool_name, tool_args);
}

Dictionary JustAMCPRuntime::_cmd_list_tools(const Dictionary &p_params) {
	Dictionary ret;
	ret["type"] = "tool_list";
	ret["tools"] = JustAMCPToolExecutor::get_tool_schemas(false, true);
	return ret;
}
Dictionary JustAMCPRuntime::_cmd_get_log_tail(const Dictionary &p_params) {
	int count = p_params.get("count", 20);
	if (count <= 0) {
		count = 20;
	}
	if (count > 500) {
		count = 500;
	}

	MutexLock lock(_error_log_mutex);
	Array tail;
	int start = MAX(0, (int)_error_log.size() - count);
	for (int i = start; i < _error_log.size(); i++) {
		tail.push_back(_error_log[i]);
	}

	Dictionary ret;
	ret["type"] = "log_tail";
	ret["logs"] = tail;
	ret["total_available"] = _error_log.size();
	return ret;
}

static Dictionary _runtime_read_asset_index() {
	Dictionary index;
	const String path = "res://.blazium/asset_tags/asset_index.json";
	if (!FileAccess::exists(path)) {
		return index;
	}
	String text;
	int64_t size = 0;
	Dictionary limit_err;
	if (!justamcp_read_utf8_within_limit(path, JUSTAMCP_MAX_SYNC_READ_BYTES, text, size, limit_err)) {
		return index;
	}
	Variant parsed = JSON::parse_string(text);
	if (parsed.get_type() == Variant::DICTIONARY) {
		index = parsed;
	}
	return index;
}

static Dictionary _runtime_read_tag_dictionary() {
	Dictionary tags;
	const String path = "res://.blazium/asset_tags/tags.json";
	if (!FileAccess::exists(path)) {
		return tags;
	}
	String text;
	int64_t size = 0;
	Dictionary limit_err;
	if (!justamcp_read_utf8_within_limit(path, JUSTAMCP_MAX_SYNC_READ_BYTES, text, size, limit_err)) {
		return tags;
	}
	Variant parsed = JSON::parse_string(text);
	if (parsed.get_type() == Variant::DICTIONARY) {
		Dictionary root = parsed;
		if (root.has("tags")) {
			tags = root["tags"];
		}
	}
	return tags;
}

Dictionary JustAMCPRuntime::_cmd_tags_get_on_asset(const Dictionary &p_params) {
	String path = p_params.get("path", "");
	if (!path.begins_with("res://")) {
		path = "res://" + path;
	}
	Dictionary index = _runtime_read_asset_index();
	Dictionary ret;
	ret["type"] = "tags_get_on_asset";
	ret["path"] = path;
	ret["tags"] = index.get(path, Array());
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_tags_list(const Dictionary &p_params) {
	const String parent = p_params.get("parent_tag", "");
	Dictionary tags_dict = _runtime_read_tag_dictionary();
	Array names = tags_dict.keys();
	Array listed;
	for (int i = 0; i < names.size(); i++) {
		const String tag = names[i];
		if (parent.is_empty()) {
			if (!tag.contains(".")) {
				listed.push_back(tag);
			}
		} else if (tag.begins_with(parent + ".")) {
			const String remainder = tag.substr(parent.length() + 1);
			if (!remainder.contains(".")) {
				listed.push_back(tag);
			}
		}
	}
	Dictionary ret;
	ret["type"] = "tags_list";
	ret["tags"] = listed;
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_tags_find_assets(const Dictionary &p_params) {
	const String tag_name = p_params.get("tag_name", "");
	Dictionary index = _runtime_read_asset_index();
	Array paths;
	Array keys = index.keys();
	for (int i = 0; i < keys.size(); i++) {
		const String path = keys[i];
		Array tags = index[path];
		for (int j = 0; j < tags.size(); j++) {
			const String tag = tags[j];
			if (tag == tag_name || tag.begins_with(tag_name + ".")) {
				paths.push_back(path);
				break;
			}
		}
	}
	Dictionary ret;
	ret["type"] = "tags_find_assets";
	ret["tag_name"] = tag_name;
	ret["paths"] = paths;
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_diagnose(const Dictionary &p_params) {
	Dictionary report;
	report["type"] = "diagnostic_report";
	report["timestamp"] = Time::get_singleton()->get_unix_time_from_system();
	report["engine"] = "Blazium Engine (JustAMCP Module)";

	report["metrics"] = _cmd_get_metrics(Dictionary());

	Dictionary log_params;
	log_params["count"] = 10;
	report["recent_logs"] = _cmd_get_log_tail(log_params).get("logs", Array());

	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (tree) {
		Node *root = tree->get_root();
		Dictionary scene;
		scene["node_count"] = tree->get_node_count();
		if (tree->get_current_scene()) {
			scene["current_scene"] = tree->get_current_scene()->get_scene_file_path();
			scene["root_name"] = tree->get_current_scene()->get_name();
		} else {
			scene["current_scene"] = "None";
			scene["root_name"] = root ? String(root->get_name()) : "null";
		}
		report["scene"] = scene;
	}

	return report;
}
