/**************************************************************************/
/*  justamcp_editor_tools.cpp                                             */
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

#include "justamcp_editor_tools.h"
#include "../justamcp_editor_plugin.h"
#include "../justamcp_editor_scene_access.h"
#include "../justamcp_mcp_tool_macros.h"
#include "../justamcp_pagination.h"
#include "../justamcp_runtime.h"
#include "../justamcp_server.h"
#include "../justamcp_tool_context.h"
#include "justamcp_agent_helpers.h"
#include "justamcp_scene_tree_dump.h"
#include "core/input/input_event.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/json.h"
#include "core/math/math_funcs.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "scene/resources/texture.h"
#include "core/io/resource_loader.h"
#include "editor/editor_data.h"
#include "editor/editor_file_system.h"
#include "editor/editor_interface.h"
#include "editor/editor_log.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_undo_redo_manager.h"
#include "scene/2d/node_2d.h"
#include "scene/gui/control.h"
#include "scene/main/canvas_item.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "servers/display_server.h"

static Dictionary g_qa_evidence;

static Node *_justamcp_find_control_by_text(Node *p_root, const String &p_text) {
	if (!p_root || p_text.is_empty()) {
		return nullptr;
	}
	if (String(p_root->get_name()) == p_text) {
		return p_root;
	}
	if (Control *control = Object::cast_to<Control>(p_root)) {
		if (control->has_method("get_text") && String(control->call("get_text")) == p_text) {
			return control;
		}
	}
	for (int i = 0; i < p_root->get_child_count(); i++) {
		Node *found = _justamcp_find_control_by_text(p_root->get_child(i), p_text);
		if (found) {
			return found;
		}
	}
	return nullptr;
}

void JustAMCPEditorTools::_bind_methods() {}

void JustAMCPEditorTools::set_editor_plugin(JustAMCPEditorPlugin *p_plugin) {
	editor_plugin = p_plugin;
}

bool JustAMCPEditorTools::_wait_ms(int p_ms) {
	p_ms = CLAMP(p_ms, 0, 60000);
	const uint64_t end_ms = OS::get_singleton()->get_ticks_msec() + uint64_t(p_ms);
	while (OS::get_singleton()->get_ticks_msec() < end_ms) {
		if (justamcp_is_cancel_requested()) {
			return false;
		}
		const uint64_t remaining = end_ms - OS::get_singleton()->get_ticks_msec();
		const uint64_t slice_ms = MIN(remaining, (uint64_t)16);
		OS::get_singleton()->delay_usec(slice_ms * 1000ULL);
		if (DisplayServer::get_singleton() && Thread::is_main_thread()) {
			DisplayServer::get_singleton()->process_events();
		}
	}
	return true;
}

Dictionary JustAMCPEditorTools::_deliver_playtest_input(const Dictionary &p_input, int p_edge) {
	Dictionary delivered;
	delivered["ok"] = false;
	const String kind = String(p_input.get("kind", p_input.get("type", "action"))).to_lower();
	delivered["kind"] = kind;
	delivered["edge"] = p_edge == 1 ? "release" : "press";
	if (!JustAMCPRuntime::get_singleton()) {
		delivered["error"] = "JustAMCPRuntime is not live.";
		return delivered;
	}
	const int hold_ms = int(p_input.get("hold_ms", 0));
	Dictionary cmd;
	String command;
	if (kind == "action" || kind == "sustain" || kind == "release") {
		command = "inject_action";
		cmd["action"] = p_input.get("action", p_input.get("name", ""));
		if (kind == "sustain") {
			cmd["pressed"] = true;
		} else if (kind == "release") {
			cmd["pressed"] = false;
		}
	} else if (kind == "key") {
		command = "inject_key";
		if (p_input.has("keycode") && p_input["keycode"].get_type() == Variant::INT) {
			cmd["keycode"] = p_input["keycode"];
		} else {
			const String key_name = String(p_input.get("key", p_input.get("keycode", "")));
			cmd["keycode"] = int(find_keycode(key_name));
			if (int(cmd["keycode"]) == 0 && !key_name.is_empty()) {
				cmd["action"] = key_name;
			}
		}
	} else if (kind == "mouse" || kind == "target") {
		command = "inject_mouse_click";
		cmd["x"] = p_input.get("x", 0);
		cmd["y"] = p_input.get("y", 0);
		cmd["button"] = p_input.get("button", 1);
		if (kind == "target") {
			const String target = p_input.get("target", p_input.get("node", p_input.get("node_path", "")));
			delivered["target"] = target;
			SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
			Node *node = tree && tree->get_root() ? tree->get_root()->get_node_or_null(NodePath(target)) : nullptr;
			if (!node && tree && tree->get_root()) {
				node = _justamcp_find_control_by_text(tree->get_root(), target);
			}
			if (!node) {
				delivered["error"] = "Target node not found: " + target;
				return delivered;
			}
			Vector2 pos;
			if (Control *control = Object::cast_to<Control>(node)) {
				pos = control->get_global_position() + control->get_size() * 0.5;
			} else if (CanvasItem *item = Object::cast_to<CanvasItem>(node)) {
				pos = item->get_global_transform_with_canvas().get_origin();
			} else if (node->has_method("get_global_position")) {
				pos = node->call("get_global_position");
			}
			cmd["x"] = pos.x;
			cmd["y"] = pos.y;
		}
	} else if (kind == "motion" || kind == "mouse_motion") {
		command = "inject_mouse_motion";
		cmd["x"] = p_input.get("x", 0);
		cmd["y"] = p_input.get("y", 0);
		cmd["relative_x"] = p_input.get("relative_x", p_input.get("dx", 0));
		cmd["relative_y"] = p_input.get("relative_y", p_input.get("dy", 0));
	} else if (kind == "drag") {
		command = "inject_drag";
		Array from_arr;
		Array to_arr;
		if (p_input.get("from", Variant()).get_type() == Variant::ARRAY) {
			from_arr = p_input["from"];
		} else {
			from_arr.push_back(p_input.get("x", p_input.get("from_x", 0)));
			from_arr.push_back(p_input.get("y", p_input.get("from_y", 0)));
		}
		if (p_input.get("to", Variant()).get_type() == Variant::ARRAY) {
			to_arr = p_input["to"];
		} else {
			const float from_x = from_arr.size() > 0 ? float(from_arr[0]) : 0.0f;
			const float from_y = from_arr.size() > 1 ? float(from_arr[1]) : 0.0f;
			to_arr.push_back(p_input.get("to_x", from_x + float(p_input.get("dx", p_input.get("relative_x", 0)))));
			to_arr.push_back(p_input.get("to_y", from_y + float(p_input.get("dy", p_input.get("relative_y", 0)))));
		}
		cmd["from"] = from_arr;
		cmd["to"] = to_arr;
	} else {
		delivered["error"] = "Unknown input kind: " + kind;
		return delivered;
	}
	if (kind != "motion" && kind != "mouse_motion" && kind != "drag" && kind != "sustain" && kind != "release") {
		if (p_edge == 1) {
			cmd["pressed"] = false;
		} else if (hold_ms > 0) {
			cmd["pressed"] = true;
		}
	}
	Dictionary runtime_res = JustAMCPRuntime::get_singleton()->execute_command(command, cmd);
	delivered["ok"] = String(runtime_res.get("type", "")) != "error";
	delivered["runtime"] = runtime_res;
	if (!bool(delivered["ok"])) {
		delivered["error"] = runtime_res.get("message", "Input was not delivered.");
	}
	return delivered;
}

Dictionary JustAMCPEditorTools::_capture_scaled_screenshot(const Dictionary &p_args, const String &p_output_path) {
	Dictionary result;
	const String view = String(p_args.get("view", "")).strip_edges();
	if (!view.is_empty() && editor_plugin && editor_plugin->get_editor_interface()) {
		String screen = view;
		if (screen == "2D" || screen == "3D" || screen == "Script" || screen == "AssetLib") {
			editor_plugin->get_editor_interface()->set_main_screen_editor(screen);
		}
	}

	Ref<Image> screenshot;
	if (editor_plugin && editor_plugin->get_editor_interface()) {
		SubViewport *vp = nullptr;
		if (view == "2D") {
			vp = editor_plugin->get_editor_interface()->get_editor_viewport_2d();
		} else if (view == "3D") {
			vp = editor_plugin->get_editor_interface()->get_editor_viewport_3d(0);
		}
		if (vp && vp->get_texture().is_valid()) {
			screenshot = vp->get_texture()->get_image();
		}
	}
	if (screenshot.is_null() && DisplayServer::get_singleton()) {
		screenshot = DisplayServer::get_singleton()->screen_get_image(DisplayServer::get_singleton()->get_primary_screen());
	}
	if (screenshot.is_null()) {
		result["ok"] = false;
		result["error"] = "Could not capture editor image.";
		return result;
	}

	const double scale = double(p_args.get("scale", 1.0));
	if (scale > 0.0 && !Math::is_equal_approx(scale, 1.0)) {
		const int w = MAX(1, int(screenshot->get_width() * scale));
		const int h = MAX(1, int(screenshot->get_height() * scale));
		screenshot->resize(w, h, Image::INTERPOLATE_BILINEAR);
	}

	String output_path = p_output_path.is_empty() ? String("res://.screenshot.png") : p_output_path;
	String sandbox_error;
	if (!justamcp_canonical_sandbox_path(output_path, output_path, sandbox_error)) {
		return MCP_INVALID_PARAMS(sandbox_error);
	}
	if (screenshot->save_png(output_path) != OK) {
		result["ok"] = false;
		result["error"] = "Could not stream screenshot pixel data into OS buffers.";
		return result;
	}
	result["ok"] = true;
	result["path"] = output_path;
	result["view"] = view;
	result["scale"] = scale;
	if (p_args.has("prompt")) {
		result["prompt"] = p_args["prompt"];
	}
	result["message"] = "Editor viewport captured to Output Path.";
	return result;
}

Dictionary JustAMCPEditorTools::editor_play_scene(const Dictionary &p_args) {
	Dictionary result;
	if (justamcp_is_cancel_requested()) {
		result["ok"] = false;
		result["error"] = "cancelled";
		return result;
	}
	justamcp_report_progress(0, 1, "Starting editor play");
	String scene_path = p_args.get("scene_path", "");
	if (!scene_path.is_empty()) {
		String sandbox_error;
		if (!justamcp_canonical_sandbox_path(scene_path, scene_path, sandbox_error)) {
			result["ok"] = false;
			result["error"] = sandbox_error;
			return result;
		}
	}
	const bool has_duration = p_args.has("duration_ms");
	int duration_ms = int(p_args.get("duration_ms", 0));
	Array inputs = p_args.get("inputs", Array());
	if (inputs.size() > 64) {
		result["ok"] = false;
		result["error"] = "inputs is capped at 64 entries.";
		return result;
	}
	for (int i = 0; i < inputs.size(); i++) {
		if (inputs[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary input = inputs[i];
		if (int(input.get("at_ms", 0)) < 0) {
			result["ok"] = false;
			result["error"] = "at_ms must be >= 0.";
			return result;
		}
		input["hold_ms"] = CLAMP(int(input.get("hold_ms", 0)), 0, 60000);
		inputs[i] = input;
	}

	if (has_duration) {
		duration_ms = CLAMP(duration_ms, 0, 60000);
		if (duration_ms <= 0) {
			result["ok"] = false;
			result["error"] = "duration_ms is required whenever a timed run is requested. Without it the run captures a boot frame after zero frames of gameplay.";
			return result;
		}
		const int needed = justamcp_playtest_required_duration_ms(inputs);
		if (duration_ms < needed) {
			result["ok"] = false;
			result["error"] = vformat("duration_ms is %dms but the last input ends at %dms. Raise duration_ms to at least %d.", duration_ms, needed, needed);
			return result;
		}
	} else if (!inputs.is_empty()) {
		return MCP_INVALID_PARAMS("duration_ms is required when inputs are provided.");
	}

	if (!editor_plugin || !editor_plugin->get_editor_interface()) {
		result["ok"] = false;
		result["error"] = "Failed to evaluate play request.";
		return result;
	}

	if (scene_path.is_empty()) {
		editor_plugin->get_editor_interface()->play_current_scene();
	} else if (FileAccess::exists(scene_path)) {
		editor_plugin->get_editor_interface()->play_custom_scene(scene_path);
	} else {
		result["ok"] = false;
		result["error"] = "Target scene file not found.";
		return result;
	}

	if (!has_duration) {
		result["ok"] = true;
		result["message"] = scene_path.is_empty() ? String("Playing current scene.") : ("Playing scene: " + scene_path);
		return result;
	}

	Array delivered;
	int frames = 0;
	const uint64_t start_ms = OS::get_singleton()->get_ticks_msec();
	const uint64_t end_ms = start_ms + uint64_t(duration_ms);
	Vector<int> phase;
	phase.resize(inputs.size());
	for (int i = 0; i < phase.size(); i++) {
		phase.write[i] = 0;
	}

	while (OS::get_singleton()->get_ticks_msec() < end_ms) {
		if (justamcp_is_cancel_requested()) {
			if (editor_plugin->get_editor_interface()->is_playing_scene()) {
				editor_plugin->get_editor_interface()->stop_playing_scene();
			}
			result["ok"] = false;
			result["error"] = "cancelled";
			return result;
		}
		const int elapsed = int(OS::get_singleton()->get_ticks_msec() - start_ms);
		for (int i = 0; i < inputs.size(); i++) {
			if (inputs[i].get_type() != Variant::DICTIONARY) {
				continue;
			}
			Dictionary input = inputs[i];
			const int at_ms = int(input.get("at_ms", 0));
			const int hold_ms = int(input.get("hold_ms", 0));
			if (phase[i] == 0 && elapsed >= at_ms) {
				delivered.push_back(_deliver_playtest_input(input, 0));
				phase.write[i] = hold_ms > 0 ? 1 : 2;
			} else if (phase[i] == 1 && elapsed >= at_ms + hold_ms) {
				delivered.push_back(_deliver_playtest_input(input, 1));
				phase.write[i] = 2;
			}
		}
		frames++;
		justamcp_report_progress(elapsed, duration_ms, "Running scene");
		if (!_wait_ms(16)) {
			if (editor_plugin->get_editor_interface()->is_playing_scene()) {
				editor_plugin->get_editor_interface()->stop_playing_scene();
			}
			result["ok"] = false;
			result["error"] = "cancelled";
			return result;
		}
	}

	Dictionary shot = _capture_scaled_screenshot(p_args, "res://.screenshot_game.png");
	if (editor_plugin->get_editor_interface()->is_playing_scene()) {
		editor_plugin->get_editor_interface()->stop_playing_scene();
	}

	int inputs_ok = 0;
	int inputs_failed = 0;
	for (int i = 0; i < delivered.size(); i++) {
		if (bool(Dictionary(delivered[i]).get("ok", false))) {
			inputs_ok++;
		} else {
			inputs_failed++;
		}
	}

	Dictionary report;
	report["frames"] = MAX(frames, 1);
	report["duration_ms"] = duration_ms;
	report["inputs_ok"] = inputs_ok;
	report["inputs_failed"] = inputs_failed;
	report["inputs_delivered"] = delivered;
	const bool shot_ok = bool(shot.get("ok", false));
	result["ok"] = shot_ok;
	if (!inputs.is_empty() && inputs_failed > 0) {
		result["ok"] = false;
		result["error"] = "One or more playtest inputs failed to land.";
	} else if (!inputs.is_empty() && inputs_ok == 0) {
		result["ok"] = false;
		result["error"] = "No inputs were delivered. JustAMCPRuntime is not live or every input failed.";
	}
	result["path"] = shot.get("path", "");
	result["report"] = report;
	result["logs"] = editor_get_errors(Dictionary()).get("errors", Array());
	if (p_args.has("prompt")) {
		result["prompt"] = p_args["prompt"];
	}
	if (!bool(result["ok"])) {
		if (!result.has("error")) {
			result["error"] = shot.get("error", "Timed run finished but screenshot failed.");
		}
	} else {
		result["message"] = "Timed scene run completed.";
	}
	return result;
}

Dictionary JustAMCPEditorTools::editor_play_main(const Dictionary &p_args) {
	Dictionary result;
	if (editor_plugin && editor_plugin->get_editor_interface()) {
		editor_plugin->get_editor_interface()->play_main_scene();
		result["ok"] = true;
		result["message"] = "Playing main project scene.";
		return result;
	}
	result["ok"] = false;
	result["error"] = "Editor interface unavailable.";
	return result;
}

Dictionary JustAMCPEditorTools::editor_stop_play(const Dictionary &p_args) {
	Dictionary result;
	if (editor_plugin && editor_plugin->get_editor_interface()) {
		if (editor_plugin->get_editor_interface()->is_playing_scene()) {
			editor_plugin->get_editor_interface()->stop_playing_scene();
			result["ok"] = true;
			result["message"] = "Stopped currently running scene.";
			return result;
		}
		result["ok"] = false;
		result["error"] = "No scene is currently actively running.";
		return result;
	}
	result["ok"] = false;
	result["error"] = "Editor interface unavailable.";
	return result;
}

Dictionary JustAMCPEditorTools::editor_is_playing(const Dictionary &p_args) {
	Dictionary result;
	if (editor_plugin && editor_plugin->get_editor_interface()) {
		result["ok"] = true;
		result["is_playing"] = editor_plugin->get_editor_interface()->is_playing_scene();
		result["playing_scene"] = editor_plugin->get_editor_interface()->get_playing_scene();
		return result;
	}
	result["ok"] = false;
	result["error"] = "Editor interface unavailable.";
	return result;
}

Dictionary JustAMCPEditorTools::editor_select_node(const Dictionary &p_args) {
	Dictionary result;
	if (!editor_plugin || !editor_plugin->get_editor_interface()) {
		result["ok"] = false;
		result["error"] = "Editor interface unavailable.";
		return result;
	}

	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		result["ok"] = false;
		result["error"] = "No scene is currently actively edited in the hierarchy.";
		return result;
	}

	Array node_paths = p_args.get("node_paths", Array());
	EditorSelection *selection = editor_plugin->get_editor_interface()->get_selection();

	if (selection) {
		selection->clear();
		Array successful_selections;

		for (int i = 0; i < node_paths.size(); ++i) {
			String path = node_paths[i];
			Node *target = root->get_node_or_null(path);
			if (target) {
				selection->add_node(target);
				successful_selections.push_back(path);
			}
		}

		result["ok"] = true;
		result["selected"] = successful_selections;
		result["count"] = successful_selections.size();
		return result;
	}

	result["ok"] = false;
	result["error"] = "Selection controller unavailable.";
	return result;
}

Dictionary JustAMCPEditorTools::editor_get_selected(const Dictionary &p_args) {
	Dictionary result;
	if (editor_plugin && editor_plugin->get_editor_interface()) {
		EditorSelection *selection = editor_plugin->get_editor_interface()->get_selection();
		if (selection) {
			Array nodes_arr;
			for (const KeyValue<Node *, Object *> &E : selection->get_selection()) {
				Node *node = E.key;
				if (node) {
					Dictionary properties;
					properties["name"] = node->get_name();
					properties["path"] = String(node->get_path());
					properties["class"] = node->get_class();
					nodes_arr.push_back(properties);
				}
			}
			result["ok"] = true;
			result["nodes"] = nodes_arr;
			result["count"] = nodes_arr.size();
			return result;
		}
	}
	result["ok"] = false;
	result["error"] = "Editor GUI unreachable.";
	return result;
}

Dictionary JustAMCPEditorTools::editor_undo(const Dictionary &p_args) {
	Dictionary result;
	if (editor_plugin && editor_plugin->get_editor_interface() && EditorUndoRedoManager::get_singleton()) {
		if (EditorUndoRedoManager::get_singleton()->has_undo()) {
			EditorUndoRedoManager::get_singleton()->undo();
			result["ok"] = true;
			result["message"] = "Undo step executed natively.";
			return result;
		}
		result["ok"] = false;
		result["error"] = "There are no history iterations left to undo.";
		return result;
	}
	result["ok"] = false;
	result["error"] = "Undo/Redo manager not instantiated properly.";
	return result;
}

Dictionary JustAMCPEditorTools::editor_redo(const Dictionary &p_args) {
	Dictionary result;
	if (editor_plugin && editor_plugin->get_editor_interface() && EditorUndoRedoManager::get_singleton()) {
		if (EditorUndoRedoManager::get_singleton()->has_redo()) {
			EditorUndoRedoManager::get_singleton()->redo();
			result["ok"] = true;
			result["message"] = "Redo step executed natively.";
			return result;
		}
		result["ok"] = false;
		result["error"] = "There are no history iterations left to redo.";
		return result;
	}
	result["ok"] = false;
	result["error"] = "Undo/Redo manager not instantiated properly.";
	return result;
}

Dictionary JustAMCPEditorTools::editor_take_screenshot(const Dictionary &p_args) {
	String path = p_args.get("path", "res://.screenshot.png");
	String sandbox_error;
	if (!justamcp_canonical_sandbox_path(path, path, sandbox_error)) {
		return MCP_INVALID_PARAMS(sandbox_error);
	}
	return _capture_scaled_screenshot(p_args, path);
}

Dictionary JustAMCPEditorTools::editor_set_main_screen(const Dictionary &p_args) {
	Dictionary result;
	String screen_name = p_args.get("screen", p_args.get("screen_name", ""));
	if (screen_name.is_empty()) {
		result["ok"] = false;
		result["error"] = "Target Workspace Screen (2D, 3D, Script, AssetLib) is required.";
		return result;
	}

	if (editor_plugin && editor_plugin->get_editor_interface()) {
		editor_plugin->get_editor_interface()->set_main_screen_editor(screen_name);
		result["ok"] = true;
		result["message"] = "Main Screen switched to structural workspace: " + screen_name;
		return result;
	}

	result["ok"] = false;
	result["error"] = "Editor Interface is unbound.";
	return result;
}

Dictionary JustAMCPEditorTools::editor_open_scene(const Dictionary &p_args) {
	Dictionary result;
	String path = p_args.get("path", "");
	if (path.is_empty()) {
		result["ok"] = false;
		result["error"] = "Requires path to a valid scene target format.";
		return result;
	}

	if (editor_plugin && editor_plugin->get_editor_interface()) {
		editor_plugin->get_editor_interface()->open_scene_from_path(path);
		result["ok"] = true;
		result["message"] = "Scene opened in editor.";
		return result;
	}

	result["ok"] = false;
	result["error"] = "Editor context is unavailable.";
	return result;
}

Dictionary JustAMCPEditorTools::editor_get_settings(const Dictionary &p_args) {
	Dictionary result;
	String setting_name = p_args.get("setting", "");

	if (setting_name.is_empty()) {
		result["ok"] = false;
		result["error"] = "Setting name is required.";
		return result;
	}

	if (editor_plugin && editor_plugin->get_editor_interface()) {
		Ref<EditorSettings> settings = editor_plugin->get_editor_interface()->get_editor_settings();
		if (settings.is_valid()) {
			if (settings->has_setting(setting_name)) {
				result["ok"] = true;
				result["value"] = settings->get_setting(setting_name);
				return result;
			} else {
				result["ok"] = false;
				result["error"] = "Editor setting not found: " + setting_name;
				return result;
			}
		}
	}

	result["ok"] = false;
	result["error"] = "Editor context is unavailable.";
	return result;
}

Dictionary JustAMCPEditorTools::editor_set_settings(const Dictionary &p_args) {
	Dictionary result;
	String setting_name = p_args.get("setting", "");
	Variant val = p_args.get("value", Variant());

	if (setting_name.is_empty()) {
		result["ok"] = false;
		result["error"] = "Setting name is required.";
		return result;
	}

	if (editor_plugin && editor_plugin->get_editor_interface()) {
		Ref<EditorSettings> settings = editor_plugin->get_editor_interface()->get_editor_settings();
		if (settings.is_valid()) {
			settings->set_setting(setting_name, val);
			result["ok"] = true;
			result["message"] = "Setting applied.";
			return result;
		}
	}

	result["ok"] = false;
	result["error"] = "Editor context is unavailable.";
	return result;
}

Dictionary JustAMCPEditorTools::editor_clear_output(const Dictionary &p_args) {
	Dictionary result;
	if (EditorNode::get_log()) {
		EditorNode::get_log()->clear();
		result["ok"] = true;
		result["message"] = "Output cleared successfully.";
		return result;
	}
	result["ok"] = false;
	result["error"] = "Editor log is unavailable.";
	return result;
}

Dictionary JustAMCPEditorTools::editor_screenshot_game(const Dictionary &p_args) {
	Dictionary result;

	if (DisplayServer::get_singleton()) {
		const int screen = DisplayServer::get_singleton()->get_primary_screen();
		Ref<Image> screenshot = DisplayServer::get_singleton()->screen_get_image(screen);
		if (screenshot.is_valid()) {
			String output_path = String(p_args.get("path", "res://.screenshot_game.png"));
			String sandbox_error;
			if (!justamcp_canonical_sandbox_path(output_path, output_path, sandbox_error)) {
				return MCP_INVALID_PARAMS(sandbox_error);
			}
			Error err = screenshot->save_png(output_path);
			if (err == OK) {
				result["ok"] = true;
				result["path"] = output_path;
				result["message"] = "Game display explicitly captured.";
				return result;
			}
		}
	}

	result["ok"] = false;
	result["error"] = "DisplayServer capture implementation unavailable.";
	return result;
}

Dictionary JustAMCPEditorTools::editor_get_output_log(const Dictionary &p_args) {
	if (p_args.has("cursor")) {
		Array all_lines;
		if (JustAMCPServer::get_singleton()) {
			Vector<String> engine_logs = JustAMCPServer::get_singleton()->get_engine_logs();
			all_lines.resize(engine_logs.size());
			for (int i = 0; i < engine_logs.size(); i++) {
				all_lines[i] = engine_logs[i];
			}
		}
		const String cursor = String(p_args["cursor"]);
		Dictionary page = justamcp_pagination_slice_array(all_lines, cursor, "logs");
		if (page.has("ok") && !bool(page.get("ok", true))) {
			Dictionary err;
			err["ok"] = false;
			err["error"] = page.get("error", "Invalid pagination cursor.");
			err["error_code"] = page.get("error_code", -32602);
			return err;
		}
		Dictionary result;
		result["ok"] = true;
		result["logs"] = page.get("logs", Array());
		result["count"] = Array(result["logs"]).size();
		if (page.has("nextCursor")) {
			result["nextCursor"] = page["nextCursor"];
		}
		return result;
	}

	Dictionary result;
	Array logs;
	int limit = p_args.get("limit", 200);
	if (JustAMCPServer::get_singleton()) {
		Vector<String> engine_logs = JustAMCPServer::get_singleton()->get_engine_logs();
		int start = MAX(0, engine_logs.size() - limit);
		for (int i = start; i < engine_logs.size(); i++) {
			logs.push_back(engine_logs[i]);
		}
	}
	result["ok"] = true;
	result["logs"] = logs;
	result["count"] = logs.size();
	return result;
}

Dictionary JustAMCPEditorTools::editor_get_errors(const Dictionary &p_args) {
	Dictionary result;
	Array errors;
	int limit = p_args.get("limit", 200);
	if (JustAMCPServer::get_singleton()) {
		Vector<String> engine_logs = JustAMCPServer::get_singleton()->get_engine_logs();
		for (int i = MAX(0, engine_logs.size() - limit); i < engine_logs.size(); i++) {
			String line = engine_logs[i];
			String lower = line.to_lower();
			if (lower.contains("error") || lower.contains("warning") || lower.contains("failed")) {
				errors.push_back(line);
			}
		}
	}
	result["ok"] = true;
	result["errors"] = errors;
	result["count"] = errors.size();
	return result;
}

Dictionary JustAMCPEditorTools::editor_reload_project(const Dictionary &p_args) {
	Dictionary result;
	if (justamcp_is_cancel_requested()) {
		result["ok"] = false;
		result["error"] = "cancelled";
		return result;
	}
	justamcp_report_progress(0, 1, "Requesting project reload");
	if (DisplayServer::get_singleton() && DisplayServer::get_singleton()->get_name() == "headless") {
		result["ok"] = false;
		result["error"] = "Project reload is unavailable in headless mode.";
		return result;
	}

	result["ok"] = false;
	result["error"] = "editor_reload_project refuses to restart the editor while MCP is serving (would drop the connection). Restart the editor process externally if a full project reload is required.";
	result["save_requested"] = bool(p_args.get("save", true));
	return result;
}

Dictionary JustAMCPEditorTools::editor_save_all_scenes(const Dictionary &p_args) {
	Dictionary result;
	if (EditorNode::get_singleton() && EditorInterface::get_singleton()) {
		EditorInterface::get_singleton()->save_all_scenes();
		result["ok"] = true;
		result["message"] = "All open scenes saved.";
		return result;
	}
	result["ok"] = false;
	result["error"] = "Editor interface unavailable.";
	return result;
}

Dictionary JustAMCPEditorTools::editor_get_signals(const Dictionary &p_args) {
	Dictionary result;
	String class_name = p_args.get("class_name", "");
	String node_path = p_args.get("node_path", "");
	Node *node = nullptr;

	if (!node_path.is_empty() && EditorInterface::get_singleton()) {
		Node *root = JustAMCPEditorSceneAccess::get_edited_root();
		if (root) {
			node = node_path == "." ? root : root->get_node_or_null(NodePath(node_path));
			if (!node && node_path.begins_with(String(root->get_name()) + "/")) {
				node = root->get_node_or_null(NodePath(node_path.substr(String(root->get_name()).length() + 1)));
			}
		}
		if (!node) {
			result["ok"] = false;
			result["error"] = "Node not found: " + node_path;
			return result;
		}
		class_name = node->get_class();
	}

	if (class_name.is_empty()) {
		result["ok"] = false;
		result["error"] = "class_name or node_path is required.";
		return result;
	}
	if (!ClassDB::class_exists(StringName(class_name))) {
		result["ok"] = false;
		result["error"] = "Class not found: " + class_name;
		return result;
	}

	List<MethodInfo> signal_list;
	ClassDB::get_signal_list(StringName(class_name), &signal_list, false);
	Array signals;
	for (const MethodInfo &signal : signal_list) {
		Dictionary item;
		item["name"] = signal.name;
		Array arguments;
		for (int i = 0; i < signal.arguments.size(); i++) {
			Dictionary arg;
			arg["name"] = signal.arguments[i].name;
			arg["type"] = Variant::get_type_name(signal.arguments[i].type);
			arguments.push_back(arg);
		}
		item["arguments"] = arguments;
		signals.push_back(item);
	}

	result["ok"] = true;
	result["class_name"] = class_name;
	result["node_path"] = node_path;
	result["signals"] = signals;
	result["count"] = signals.size();
	return result;
}

Dictionary JustAMCPEditorTools::_qa_set_paused(bool p_paused) {
	if (JustAMCPRuntime::get_singleton()) {
		Dictionary args;
		args["paused"] = p_paused;
		Dictionary runtime_res = JustAMCPRuntime::get_singleton()->execute_command("set_paused", args);
		runtime_res["ok"] = String(runtime_res.get("type", "")) != "error";
		return runtime_res;
	}
	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	Dictionary ret;
	if (!tree) {
		ret["ok"] = false;
		ret["type"] = "error";
		ret["message"] = "JustAMCPRuntime is not live and no SceneTree is available to pause.";
		return ret;
	}
	tree->set_pause(p_paused);
	ret["ok"] = true;
	ret["type"] = "paused";
	ret["paused"] = p_paused;
	return ret;
}

Dictionary JustAMCPEditorTools::_qa_eval_probes(const Array &p_probes) {
	Dictionary out;
	if (!JustAMCPRuntime::get_singleton()) {
		out["ok"] = false;
		out["error"] = "JustAMCPRuntime is not live.";
		return out;
	}
	Dictionary values;
	for (int i = 0; i < p_probes.size(); i++) {
		String expr;
		String name;
		if (p_probes[i].get_type() == Variant::DICTIONARY) {
			Dictionary probe = p_probes[i];
			expr = probe.get("expr", probe.get("expression", ""));
			name = probe.get("name", expr);
		} else {
			expr = String(p_probes[i]);
			name = expr;
		}
		Dictionary cmd;
		cmd["expr"] = expr;
		values[name] = JustAMCPRuntime::get_singleton()->execute_command("eval_expression", cmd);
	}
	out["ok"] = true;
	out["probes"] = values;
	return out;
}

Dictionary JustAMCPEditorTools::qa_start(const Dictionary &p_args) {
	Dictionary play_args = p_args;
	if (play_args.has("scene") && !play_args.has("scene_path")) {
		play_args["scene_path"] = play_args["scene"];
	}
	Dictionary started = editor_play_scene(play_args);
	if (!bool(started.get("ok", false))) {
		return started;
	}
	_wait_ms(int(p_args.get("boot_ms", 250)));
	Dictionary paused = _qa_set_paused(true);
	if (!bool(paused.get("ok", false)) || String(paused.get("type", "")) == "error") {
		editor_stop_play(Dictionary());
		Dictionary err;
		err["ok"] = false;
		err["error"] = paused.get("message", "Failed to freeze the running scene.");
		return err;
	}
	Dictionary result;
	result["ok"] = true;
	result["frozen"] = true;
	result["running_scene"] = editor_plugin && editor_plugin->get_editor_interface() ? editor_plugin->get_editor_interface()->get_playing_scene() : String();
	if (JustAMCPRuntime::get_singleton()) {
		result["root"] = JustAMCPRuntime::get_singleton()->execute_command("get_tree", Dictionary());
	}
	result["message"] = "QA session started and frozen.";
	return result;
}

Dictionary JustAMCPEditorTools::qa_act(const Dictionary &p_args) {
	if (!editor_plugin || !editor_plugin->get_editor_interface() || !editor_plugin->get_editor_interface()->is_playing_scene()) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "No QA session is running. Call qa_start first.";
		return err;
	}
	Array probes = p_args.get("probes", Array());
	Dictionary before = _qa_eval_probes(probes);
	_qa_set_paused(false);
	Array inputs = p_args.get("inputs", Array());
	if (inputs.size() > 64) {
		_qa_set_paused(true);
		return MCP_INVALID_PARAMS("inputs is capped at 64 entries.");
	}
	Array delivered;
	for (int i = 0; i < inputs.size(); i++) {
		if (inputs[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary input = inputs[i];
		delivered.push_back(_deliver_playtest_input(input, 0));
		const int hold_ms = int(input.get("hold_ms", 0));
		if (hold_ms > 0) {
			_wait_ms(hold_ms);
			delivered.push_back(_deliver_playtest_input(input, 1));
		}
	}

	int frames = 0;
	bool until_satisfied = false;
	Variant until_before;
	Variant until_after;
	const String until_expr = p_args.get("until", "");
	const int advance_frames = int(p_args.get("advance_frames", until_expr.is_empty() ? 1 : 120));
	for (int i = 0; i < advance_frames; i++) {
		frames++;
		if (!_wait_ms(16)) {
			break;
		}
		if (!until_expr.is_empty() && JustAMCPRuntime::get_singleton()) {
			Dictionary cmd;
			cmd["expr"] = until_expr;
			Dictionary eval = JustAMCPRuntime::get_singleton()->execute_command("eval_expression", cmd);
			if (i == 0) {
				until_before = eval.get("result", eval);
			}
			until_after = eval.get("result", eval);
			const Variant value = eval.has("value") ? eval["value"] : eval.get("result", false);
			if (value.get_type() != Variant::NIL && value.booleanize()) {
				until_satisfied = true;
				break;
			}
		}
	}
	_qa_set_paused(true);
	Dictionary after = _qa_eval_probes(probes);
	Dictionary until;
	until["satisfied"] = until_satisfied;
	until["frames_waited"] = frames;
	until["before"] = until_before;
	until["after"] = until_after;

	int inputs_failed = 0;
	for (int i = 0; i < delivered.size(); i++) {
		if (!bool(Dictionary(delivered[i]).get("ok", false))) {
			inputs_failed++;
		}
	}
	Dictionary result;
	result["ok"] = inputs.is_empty() || inputs_failed == 0;
	result["delivered"] = delivered;
	result["frames_advanced"] = frames;
	result["probes"] = before.get("probes", Dictionary());
	result["probes_after"] = after.get("probes", Dictionary());
	result["until"] = until;
	if (!bool(result["ok"])) {
		result["error"] = "One or more QA inputs failed to land.";
	}
	g_qa_evidence["inputs"] = delivered;
	g_qa_evidence["probes"] = after.get("probes", Dictionary());
	g_qa_evidence["until"] = until;
	return result;
}

Dictionary JustAMCPEditorTools::qa_observe(const Dictionary &p_args) {
	Dictionary result = _qa_eval_probes(p_args.get("probes", Array()));
	if (!bool(result.get("ok", false))) {
		return result;
	}
	result["frozen"] = true;
	return result;
}

Dictionary JustAMCPEditorTools::qa_watch(const Dictionary &p_args) {
	Dictionary result;
	if (!JustAMCPRuntime::get_singleton()) {
		result["ok"] = false;
		result["error"] = "JustAMCPRuntime is not live.";
		return result;
	}
	Array signals = p_args.get("signals", Array());
	if (signals.size() > 64) {
		return MCP_INVALID_PARAMS("signals is capped at 64 entries.");
	}
	Array armed;
	for (int i = 0; i < signals.size(); i++) {
		if (signals[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary watch = signals[i];
		if (watch.has("node_path") && !watch.has("node")) {
			watch["node"] = watch["node_path"];
		}
		armed.push_back(JustAMCPRuntime::get_singleton()->execute_command("watch_signal", watch));
	}
	result["ok"] = true;
	result["armed"] = armed;
	g_qa_evidence["signals"] = armed;
	return result;
}

Dictionary JustAMCPEditorTools::qa_drive(const Dictionary &p_args) {
	Dictionary result;
	if (!JustAMCPRuntime::get_singleton()) {
		result["ok"] = false;
		result["error"] = "JustAMCPRuntime is not live.";
		return result;
	}
	Dictionary cmd;
	cmd["expr"] = p_args.get("expr", p_args.get("code", ""));
	Dictionary eval = JustAMCPRuntime::get_singleton()->execute_command("eval_expression", cmd);
	result["ok"] = String(eval.get("type", "")) != "error";
	result["value"] = eval.get("value", eval.get("result", Variant()));
	result["result"] = eval.get("result", Variant());
	if (!bool(result["ok"])) {
		result["error"] = eval.get("message", "Expression failed.");
	}
	return result;
}

Dictionary JustAMCPEditorTools::qa_stop(const Dictionary &p_args) {
	_qa_set_paused(false);
	Dictionary stopped = editor_stop_play(p_args);
	g_qa_evidence["stopped"] = true;
	Ref<FileAccess> file = FileAccess::open("user://justamcp_qa_last.json", FileAccess::WRITE);
	if (file.is_valid()) {
		file->store_string(JSON::stringify(g_qa_evidence));
		file->close();
		stopped["evidence_path"] = "user://justamcp_qa_last.json";
	}
	stopped["evidence"] = g_qa_evidence;
	g_qa_evidence.clear();
	return stopped;
}

Dictionary JustAMCPEditorTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "editor_play_scene") {
		return editor_play_scene(p_args);
	}
	if (p_tool_name == "editor_play_main") {
		return editor_play_main(p_args);
	}
	if (p_tool_name == "editor_stop_play") {
		return editor_stop_play(p_args);
	}
	if (p_tool_name == "editor_is_playing") {
		return editor_is_playing(p_args);
	}
	if (p_tool_name == "editor_select_node") {
		return editor_select_node(p_args);
	}
	if (p_tool_name == "editor_get_selected") {
		return editor_get_selected(p_args);
	}
	if (p_tool_name == "editor_undo") {
		return editor_undo(p_args);
	}
	if (p_tool_name == "editor_redo") {
		return editor_redo(p_args);
	}
	if (p_tool_name == "editor_take_screenshot") {
		return editor_take_screenshot(p_args);
	}
	if (p_tool_name == "editor_set_main_screen") {
		return editor_set_main_screen(p_args);
	}
	if (p_tool_name == "editor_open_scene") {
		return editor_open_scene(p_args);
	}
	if (p_tool_name == "editor_get_settings") {
		return editor_get_settings(p_args);
	}
	if (p_tool_name == "editor_set_settings") {
		return editor_set_settings(p_args);
	}
	if (p_tool_name == "editor_clear_output") {
		return editor_clear_output(p_args);
	}
	if (p_tool_name == "editor_screenshot_game") {
		return editor_screenshot_game(p_args);
	}
	if (p_tool_name == "editor_get_output_log") {
		return editor_get_output_log(p_args);
	}
	if (p_tool_name == "editor_get_errors") {
		return editor_get_errors(p_args);
	}
	if (p_tool_name == "editor_reload_project") {
		return editor_reload_project(p_args);
	}
	if (p_tool_name == "editor_save_all_scenes") {
		return editor_save_all_scenes(p_args);
	}
	if (p_tool_name == "editor_get_signals") {
		return editor_get_signals(p_args);
	}
	if (p_tool_name == "editor_run_scene") {
		return editor_play_scene(p_args);
	}
	if (p_tool_name == "scene_tree_dump") {
		return justamcp_scene_tree_dump(p_args);
	}
	if (p_tool_name == "open_in_blazium") {
		const String path = p_args.get("path", p_args.get("file_path", ""));
		Dictionary open_args;
		open_args["path"] = path;
		open_args["line"] = p_args.get("line", -1);
		if (path.ends_with(".tscn") || path.ends_with(".scn")) {
			return editor_open_scene(open_args);
		}
		Dictionary open_result;
		Ref<Resource> resource = ResourceLoader::load(path);
		if (resource.is_valid() && EditorInterface::get_singleton()) {
			EditorInterface::get_singleton()->edit_resource(resource);
			open_result["ok"] = true;
			open_result["path"] = path;
			open_result["message"] = "Resource opened in the editor inspector.";
			return open_result;
		}
		open_result["ok"] = false;
		open_result["error"] = "Unsupported or missing resource path: " + path;
		return open_result;
	}
	if (p_tool_name == "rescan_filesystem") {
		Dictionary ret;
		if (EditorFileSystem::get_singleton()) {
			EditorFileSystem::get_singleton()->call_deferred(SNAME("scan"));
			ret["ok"] = true;
			ret["message"] = "Editor filesystem rescan requested.";
			return ret;
		}
		ret["ok"] = false;
		ret["error"] = "Editor filesystem unavailable.";
		return ret;
	}
	if (p_tool_name == "qa_start") {
		return qa_start(p_args);
	}
	if (p_tool_name == "qa_act") {
		return qa_act(p_args);
	}
	if (p_tool_name == "qa_observe") {
		return qa_observe(p_args);
	}
	if (p_tool_name == "qa_watch") {
		return qa_watch(p_args);
	}
	if (p_tool_name == "qa_drive") {
		return qa_drive(p_args);
	}
	if (p_tool_name == "qa_stop") {
		return qa_stop(p_args);
	}
	if (p_tool_name == "logs_read") {
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
		Dictionary log_args;
		log_args["limit"] = p_args.get("limit", 200);
		log_args["cursor"] = cursor;
		Dictionary logs = editor_get_output_log(log_args);
		if (!bool(logs.get("ok", true))) {
			return logs;
		}
		Array lines = logs.get("logs", Array());
		int since_index = p_args.get("since_index", 0);
		Array sliced;
		for (int i = MAX(0, since_index); i < lines.size(); i++) {
			sliced.push_back(lines[i]);
		}
		Dictionary ret;
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

	return Dictionary();
}

#endif
