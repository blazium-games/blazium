/**************************************************************************/
/*  justamcp_runtime_query_eval.cpp                                       */
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

#include "justamcp_runtime.h"

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
#include "editor/settings/editor_settings.h"
#endif

Dictionary JustAMCPRuntime::_cmd_get_metrics(const Dictionary &p_params) {
	Dictionary ret;
	ret["type"] = "metrics";
	Dictionary metrics_data;

	Performance *perf = Performance::get_singleton();
	metrics_data["fps"] = Engine::get_singleton()->get_frames_per_second();
	metrics_data["frame_time"] = perf->get_monitor(Performance::TIME_PROCESS);
	metrics_data["physics_time"] = perf->get_monitor(Performance::TIME_PHYSICS_PROCESS);
	metrics_data["memory_static"] = perf->get_monitor(Performance::MEMORY_STATIC);
	metrics_data["memory_static_max"] = perf->get_monitor(Performance::MEMORY_STATIC_MAX);
	metrics_data["object_count"] = perf->get_monitor(Performance::OBJECT_COUNT);
	metrics_data["object_resource_count"] = perf->get_monitor(Performance::OBJECT_RESOURCE_COUNT);
	metrics_data["object_node_count"] = perf->get_monitor(Performance::OBJECT_NODE_COUNT);
	metrics_data["object_orphan_node_count"] = perf->get_monitor(Performance::OBJECT_ORPHAN_NODE_COUNT);
	metrics_data["render_total_objects"] = perf->get_monitor(Performance::RENDER_TOTAL_OBJECTS_IN_FRAME);
	metrics_data["render_total_primitives"] = perf->get_monitor(Performance::RENDER_TOTAL_PRIMITIVES_IN_FRAME);
	metrics_data["render_total_draw_calls"] = perf->get_monitor(Performance::RENDER_TOTAL_DRAW_CALLS_IN_FRAME);
	metrics_data["render_video_mem_used"] = perf->get_monitor(Performance::RENDER_VIDEO_MEM_USED);

	ret["data"] = metrics_data;
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_get_autoload(const Dictionary &p_params) {
	String name = p_params.get("name", "");
	if (name.is_empty()) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Missing 'name'";
		return ret;
	}

	String setting_key = "autoload/" + name;
	Dictionary ret;
	ret["type"] = "autoload";
	ret["name"] = name;
	ret["configured"] = ProjectSettings::get_singleton()->has_setting(setting_key);
	ret["resource_path"] = ProjectSettings::get_singleton()->get_setting(setting_key, "");

	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	Node *node = tree && tree->get_root() ? tree->get_root()->get_node_or_null(NodePath(name)) : nullptr;
	ret["loaded"] = node != nullptr;
	if (node) {
		ret["node"] = _serialize_node(node, false);
	}
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_runtime_info(const Dictionary &p_params) {
	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());

	String current_scene_path;
	String current_scene_name;
	int node_count = 0;

	if (tree) {
		Node *scene = tree->get_current_scene();
		if (scene) {
			current_scene_path = scene->get_scene_file_path();
			current_scene_name = String(scene->get_name());
		}
		node_count = (int)Performance::get_singleton()->get_monitor(Performance::OBJECT_NODE_COUNT);
	}

	Dictionary ret;
	ret["type"] = "runtime_info";
	ret["engine_version"] = Engine::get_singleton()->get_version_info().get("string", "unknown");
	ret["fps"] = Engine::get_singleton()->get_frames_per_second();
	ret["process_frames"] = (int64_t)Engine::get_singleton()->get_process_frames();
	ret["time_scale"] = Engine::get_singleton()->get_time_scale();
	ret["current_scene"] = current_scene_path;
	ret["current_scene_name"] = current_scene_name;
	ret["node_count"] = node_count;
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_runtime_get_errors(const Dictionary &p_params) {
	int since_index = p_params.get("since_index", 0);

	MutexLock lock(_error_log_mutex);

	Array entries;
	for (int i = since_index; i < _error_log.size(); i++) {
		entries.push_back(_error_log[i]);
	}

	int error_count = 0;
	int warning_count = 0;
	for (int i = 0; i < _error_log.size(); i++) {
		String t = _error_log[i].get("type", "");
		if (t == "error") {
			error_count++;
		} else {
			warning_count++;
		}
	}

	Dictionary ret;
	ret["type"] = "runtime_errors";
	ret["errors"] = entries;
	ret["next_index"] = _error_log.size();
	ret["error_count"] = error_count;
	ret["warning_count"] = warning_count;
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_runtime_capabilities(const Dictionary &p_params) {
	Array commands;

	const char *all_cmds[] = {
		"ping", "get_tree", "get_node", "set_property", "call_method",
		"get_metrics", "capture_screenshot", "capture_viewport",
		"inject_action", "inject_key", "inject_mouse_click", "inject_mouse_motion",
		"watch_signal", "unwatch_signal",
		"inject_drag", "inject_scroll", "inject_gesture", "inject_gamepad",
		"find_nodes", "get_node_property", "call_node_method",
		"press_button", "wait_for_property",
		"runtime_info", "runtime_get_errors", "runtime_capabilities",
		"eval_expression", "runtime_quit", "get_network_info", "get_audio_info",
		"run_custom_command", "get_autoload", "find_nodes_by_script",
		"batch_get_properties", "find_ui_elements", "click_button_by_text",
		"navigate_to", "move_to", "monitor_properties",
		"auth_info", "performance",
		nullptr
	};
	for (int i = 0; all_cmds[i] != nullptr; i++) {
		commands.push_back(String(all_cmds[i]));
	}

	Dictionary ret;
	ret["type"] = "capabilities";
	ret["commands"] = commands;
	ret["count"] = commands.size();
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_eval_expression(const Dictionary &p_params) {
	String expr_str = p_params.get("expr", "");
	if (expr_str.is_empty()) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Missing 'expr'";
		return ret;
	}

	String expr_lower = expr_str.to_lower();
	for (int i = 0; _EVAL_BLOCKED_PATTERNS[i] != nullptr; i++) {
		if (expr_lower.contains(String(_EVAL_BLOCKED_PATTERNS[i]).to_lower())) {
			Dictionary ret;
			ret["type"] = "error";
			ret["message"] = vformat("Expression contains disallowed pattern: %s", _EVAL_BLOCKED_PATTERNS[i]);
			return ret;
		}
	}

	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	Node *context = tree ? tree->get_root() : nullptr;

	Expression expr;
	Error err = expr.parse(expr_str);
	if (err != OK) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Parse error: " + expr.get_error_text();
		return ret;
	}

	Variant result_v = expr.execute(Array(), context);
	if (expr.has_execute_failed()) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Execution error: " + expr.get_error_text();
		return ret;
	}

	Dictionary ret;
	ret["type"] = "eval_result";
	ret["result"] = String(result_v);
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_watch_signal(const Dictionary &p_params) {
	String node_path = p_params.get("node", "");
	String signal_name = p_params.get("signal", "");

	if (node_path.is_empty() || signal_name.is_empty()) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Requires 'node' and 'signal'";
		return ret;
	}

	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!tree) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "No SceneTree available";
		return ret;
	}

	Node *node = tree->get_root()->get_node_or_null(node_path);
	if (!node) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Node not found: " + node_path;
		return ret;
	}

	if (!node->has_signal(signal_name)) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Signal not found on node: " + signal_name;
		return ret;
	}

	String key = node_path + ":" + signal_name;
	if (watched_signals.has(key)) {
		Dictionary ret;
		ret["type"] = "signal_watching";
		ret["node"] = node_path;
		ret["signal"] = signal_name;
		ret["status"] = "already_watching";
		return ret;
	}

	Callable c = callable_mp(this, &JustAMCPRuntime::_broadcast_signal_event).bind(node_path, signal_name);
	node->connect(signal_name, c);
	watched_signals[key] = c;

	Dictionary ret;
	ret["type"] = "signal_watching";
	ret["node"] = node_path;
	ret["signal"] = signal_name;
	ret["status"] = "started";
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_unwatch_signal(const Dictionary &p_params) {
	String node_path = p_params.get("node", "");
	String signal_name = p_params.get("signal", "");

	String key = node_path + ":" + signal_name;
	if (!watched_signals.has(key)) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Not watching signal: " + key;
		return ret;
	}

	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (tree) {
		Node *node = tree->get_root()->get_node_or_null(node_path);
		if (node) {
			node->disconnect(signal_name, watched_signals[key]);
		}
	}

	watched_signals.erase(key);

	Dictionary ret;
	ret["type"] = "signal_unwatched";
	ret["node"] = node_path;
	ret["signal"] = signal_name;
	return ret;
}

void JustAMCPRuntime::_broadcast_signal_event(const String &p_node_path, const String &p_signal_name, const Array &p_args) {
	Dictionary event;
	event["type"] = "signal_event";
	event["node"] = p_node_path;
	event["signal"] = p_signal_name;

	Array serialized_args;
	for (int i = 0; i < p_args.size(); i++) {
		serialized_args.push_back(_serialize_value(p_args[i]));
	}
	event["args"] = serialized_args;

	MutexLock lock(clients_mutex);
	for (int i = 0; i < clients.size(); i++) {
		_send_response(clients[i], event);
	}
}
