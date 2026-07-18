/**************************************************************************/
/*  justamcp_input_tools.cpp                                              */
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

#include "justamcp_input_tools.h"

#include "core/io/file_access.h"
#include "core/io/json.h"

#include "../justamcp_mcp_tool_macros.h"
#include "../justamcp_runtime.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_interface.h"
#endif

void JustAMCPInputTools::_bind_methods() {}

JustAMCPInputTools::JustAMCPInputTools() {}
JustAMCPInputTools::~JustAMCPInputTools() {}

static bool _justamcp_game_input_bridge_active() {
#ifdef TOOLS_ENABLED

	if (EditorInterface::get_singleton() && EditorInterface::get_singleton()->is_playing_scene()) {
		return true;
	}
	return false;
#else
	return JustAMCPRuntime::get_singleton() != nullptr;
#endif
}

static Dictionary _justamcp_input_bridge_inactive_error() {
	return MCP_ERROR(-32000, "Game input bridge inactive. Start play mode with MCP enabled before simulating input.");
}

void JustAMCPInputTools::_write_commands(const Array &p_events) {
	String json = JSON::stringify(p_events);
	Ref<FileAccess> file = FileAccess::open(COMMANDS_PATH, FileAccess::WRITE);
	if (file.is_valid()) {
		file->store_string(json);
		file->close();
	} else {
		ERR_PRINT("[MCP Input] Failed to write commands");
	}
}

Dictionary JustAMCPInputTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "simulate_key") {
		return _simulate_key(p_args);
	}
	if (p_tool_name == "simulate_mouse_click") {
		return _simulate_mouse_click(p_args);
	}
	if (p_tool_name == "simulate_mouse_move") {
		return _simulate_mouse_move(p_args);
	}
	if (p_tool_name == "simulate_action") {
		return _simulate_action(p_args);
	}
	if (p_tool_name == "simulate_touch") {
		return _simulate_touch(p_args);
	}
	if (p_tool_name == "simulate_gamepad") {
		return _simulate_gamepad(p_args);
	}
	if (p_tool_name == "simulate_sequence") {
		return _simulate_sequence(p_args);
	}
	if (p_tool_name == "input_record") {
		return _input_record(p_args);
	}
	if (p_tool_name == "input_replay") {
		return _input_replay(p_args);
	}

	Dictionary err;
	err["code"] = -32601;
	err["message"] = "Method not found: " + p_tool_name;
	Dictionary res;
	res["error"] = err;
	return Dictionary();
}

Dictionary JustAMCPInputTools::_simulate_key(const Dictionary &p_params) {
	if (!_justamcp_game_input_bridge_active()) {
		return _justamcp_input_bridge_inactive_error();
	}
	if (!p_params.has("keycode")) {
		return MCP_INVALID_PARAMS("Missing param: keycode");
	}
	String keycode = p_params["keycode"];

	bool pressed = p_params.has("pressed") ? bool(p_params["pressed"]) : true;
	bool shift = p_params.has("shift") ? bool(p_params["shift"]) : false;
	bool ctrl = p_params.has("ctrl") ? bool(p_params["ctrl"]) : false;
	bool alt = p_params.has("alt") ? bool(p_params["alt"]) : false;

	Dictionary event;
	event["type"] = "key";
	event["keycode"] = keycode;
	event["pressed"] = pressed;
	event["shift"] = shift;
	event["ctrl"] = ctrl;
	event["alt"] = alt;

	Array events;
	events.push_back(event);
	_write_commands(events);

	Dictionary res;
	res["sent"] = true;
	res["event"] = event;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPInputTools::_simulate_mouse_click(const Dictionary &p_params) {
	if (!_justamcp_game_input_bridge_active()) {
		return _justamcp_input_bridge_inactive_error();
	}
	int button = p_params.has("button") ? int(p_params["button"]) : 1;
	bool pressed = p_params.has("pressed") ? bool(p_params["pressed"]) : true;
	bool double_click = p_params.has("double_click") ? bool(p_params["double_click"]) : false;
	bool auto_release = p_params.has("auto_release") ? bool(p_params["auto_release"]) : true;
	float x = p_params.has("x") ? float(p_params["x"]) : 0.0;
	float y = p_params.has("y") ? float(p_params["y"]) : 0.0;

	Dictionary press_event;
	press_event["type"] = "mouse_button";
	press_event["button"] = button;
	press_event["pressed"] = pressed;
	press_event["double_click"] = double_click;
	Dictionary pos;
	pos["x"] = x;
	pos["y"] = y;
	press_event["position"] = pos;

	if (pressed && auto_release) {
		Dictionary release_event = press_event.duplicate();
		release_event["pressed"] = false;

		Dictionary sequence_data;
		Array seq_events;
		seq_events.push_back(press_event);
		seq_events.push_back(release_event);
		sequence_data["sequence_events"] = seq_events;
		sequence_data["frame_delay"] = 1;

		String json = JSON::stringify(sequence_data);
		Ref<FileAccess> file = FileAccess::open(COMMANDS_PATH, FileAccess::WRITE);
		if (file.is_null()) {
			return MCP_INTERNAL("Failed to write commands");
		}
		file->store_string(json);
		file->close();

		Dictionary res;
		res["sent"] = true;
		res["event"] = press_event;
		res["auto_release"] = true;
		return MCP_SUCCESS(res);
	}

	Array events;
	events.push_back(press_event);
	_write_commands(events);

	Dictionary res;
	res["sent"] = true;
	res["event"] = press_event;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPInputTools::_simulate_mouse_move(const Dictionary &p_params) {
	if (!_justamcp_game_input_bridge_active()) {
		return _justamcp_input_bridge_inactive_error();
	}
	float x = p_params.has("x") ? float(p_params["x"]) : 0.0;
	float y = p_params.has("y") ? float(p_params["y"]) : 0.0;
	float rel_x = p_params.has("relative_x") ? float(p_params["relative_x"]) : 0.0;
	float rel_y = p_params.has("relative_y") ? float(p_params["relative_y"]) : 0.0;
	int button_mask = p_params.has("button_mask") ? int(p_params["button_mask"]) : 0;
	bool unhandled = p_params.has("unhandled") ? bool(p_params["unhandled"]) : false;

	Dictionary event;
	event["type"] = "mouse_motion";
	Dictionary pos;
	pos["x"] = x;
	pos["y"] = y;
	event["position"] = pos;
	Dictionary rel;
	rel["x"] = rel_x;
	rel["y"] = rel_y;
	event["relative"] = rel;
	event["button_mask"] = button_mask;

	if (unhandled || button_mask > 0) {
		event["unhandled"] = true;
	}

	Array events;
	events.push_back(event);
	_write_commands(events);

	Dictionary res;
	res["sent"] = true;
	res["event"] = event;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPInputTools::_simulate_action(const Dictionary &p_params) {
	if (!_justamcp_game_input_bridge_active()) {
		return _justamcp_input_bridge_inactive_error();
	}
	if (!p_params.has("action")) {
		return MCP_INVALID_PARAMS("Missing param: action");
	}
	String action_name = p_params["action"];

	bool pressed = p_params.has("pressed") ? bool(p_params["pressed"]) : true;
	float strength = p_params.has("strength") ? float(p_params["strength"]) : 1.0;

	Dictionary event;
	event["type"] = "action";
	event["action"] = action_name;
	event["pressed"] = pressed;
	event["strength"] = strength;

	Array events;
	events.push_back(event);
	_write_commands(events);

	Dictionary res;
	res["sent"] = true;
	res["event"] = event;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPInputTools::_simulate_touch(const Dictionary &p_params) {
	if (!_justamcp_game_input_bridge_active()) {
		return _justamcp_input_bridge_inactive_error();
	}
	int index = p_params.has("index") ? int(p_params["index"]) : 0;
	Dictionary position = p_params.get("position", Dictionary());
	float x = p_params.has("x") ? float(p_params["x"]) : float(position.get("x", 0.0));
	float y = p_params.has("y") ? float(p_params["y"]) : float(position.get("y", 0.0));
	String action = p_params.get("action", "");
	bool pressed = p_params.has("pressed") ? bool(p_params["pressed"]) : (action != "release");
	bool double_tap = p_params.has("double_tap") ? bool(p_params["double_tap"]) : false;

	Dictionary event;
	event["type"] = action == "move" ? "screen_drag" : "screen_touch";
	event["index"] = index;
	event["pressed"] = pressed;
	event["double_tap"] = double_tap;
	Dictionary pos;
	pos["x"] = x;
	pos["y"] = y;
	event["position"] = pos;

	Array events;
	events.push_back(event);
	_write_commands(events);

	Dictionary res;
	res["sent"] = true;
	res["event"] = event;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPInputTools::_simulate_gamepad(const Dictionary &p_params) {
	if (!_justamcp_game_input_bridge_active()) {
		return _justamcp_input_bridge_inactive_error();
	}
	int device = p_params.has("device") ? int(p_params["device"]) : 0;

	Dictionary event;
	if (p_params.has("button_index") || p_params.has("button")) {
		event["type"] = "joypad_button";
		event["device"] = device;
		event["button_index"] = p_params.has("button_index") ? int(p_params["button_index"]) : int(p_params["button"]);
		event["pressed"] = p_params.has("pressed") ? bool(p_params["pressed"]) : true;
		event["pressure"] = p_params.has("pressure") ? float(p_params["pressure"]) : 1.0;
	} else if (p_params.has("axis")) {
		event["type"] = "joypad_motion";
		event["device"] = device;
		event["axis"] = int(p_params["axis"]);
		event["axis_value"] = p_params.has("axis_value") ? float(p_params["axis_value"]) : 0.0;
	} else {
		return MCP_INVALID_PARAMS("Missing joypad parameters (button_index or axis).");
	}

	Array events;
	events.push_back(event);
	_write_commands(events);

	Dictionary res;
	res["sent"] = true;
	res["event"] = event;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPInputTools::_simulate_sequence(const Dictionary &p_params) {
	if (!_justamcp_game_input_bridge_active()) {
		return _justamcp_input_bridge_inactive_error();
	}
	if (!p_params.has("events") || p_params["events"].get_type() != Variant::ARRAY) {
		return MCP_INVALID_PARAMS("Missing required parameter: events (Array)");
	}
	Array events = p_params["events"];
	if (events.is_empty()) {
		return MCP_INVALID_PARAMS("Events array is empty");
	}

	int frame_delay = p_params.has("frame_delay") ? int(p_params["frame_delay"]) : 1;

	for (int i = 0; i < events.size(); i++) {
		if (events[i].get_type() != Variant::DICTIONARY) {
			return MCP_INVALID_PARAMS("Invalid event in sequence");
		}
		Dictionary event_data = events[i];
		if (!event_data.has("type") || String(event_data["type"]).is_empty()) {
			return MCP_INVALID_PARAMS("Invalid event in sequence");
		}
	}

	if (frame_delay <= 0) {
		_write_commands(events);
	} else {
		Dictionary sequence_data;
		sequence_data["sequence_events"] = events;
		sequence_data["frame_delay"] = frame_delay;

		String json = JSON::stringify(sequence_data);
		Ref<FileAccess> file = FileAccess::open(COMMANDS_PATH, FileAccess::WRITE);
		if (file.is_null()) {
			return MCP_INTERNAL("Failed to write commands");
		}
		file->store_string(json);
		file->close();
	}

	Dictionary res;
	res["sent"] = true;
	res["event_count"] = events.size();
	res["frame_delay"] = frame_delay;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPInputTools::_input_record(const Dictionary &p_args) {
	Dictionary result;
	bool state = p_args.get("state", true);

	Dictionary event;
	event["type"] = "meta";
	event["action"] = state ? "record_start" : "record_stop";

	Array events;
	events.push_back(event);
	_write_commands(events);

	result["sent"] = true;
	result["recording"] = state;
	result["message"] = state ? "Started buffering OS Input events." : "Stopped tracking inputs.";
	return MCP_SUCCESS(result);
}

Dictionary JustAMCPInputTools::_input_replay(const Dictionary &p_args) {
	Dictionary result;
	String sequence_buffer = p_args.get("sequence_buffer_id", "");

	Dictionary event;
	event["type"] = "meta";
	event["action"] = "replay_buffer";
	event["sequence_buffer_id"] = sequence_buffer;

	Array events;
	events.push_back(event);
	_write_commands(events);

	result["sent"] = true;
	result["replaying"] = true;
	result["sequence_buffer_id"] = sequence_buffer;
	return MCP_SUCCESS(result);
}
