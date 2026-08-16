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
#include "../justamcp_pagination.h"
#include "../justamcp_server.h"
#include "../justamcp_tool_context.h"

#include "core/io/file_access.h"
#include "core/object/class_db.h"
#include "editor/editor_data.h"
#include "editor/editor_interface.h"
#include "editor/editor_log.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/settings/editor_settings.h"
#include "scene/main/viewport.h" // IWYU pragma: keep
#include "scene/main/window.h" // IWYU pragma: keep
#include "servers/display/display_server.h"

void JustAMCPEditorTools::_bind_methods() {}

void JustAMCPEditorTools::set_editor_plugin(JustAMCPEditorPlugin *p_plugin) {
	editor_plugin = p_plugin;
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

	if (scene_path.is_empty()) {
		if (editor_plugin && editor_plugin->get_editor_interface()) {
			editor_plugin->get_editor_interface()->play_current_scene();
			result["ok"] = true;
			result["message"] = "Playing current scene.";
			return result;
		}
	} else {
		if (FileAccess::exists(scene_path)) {
			if (editor_plugin && editor_plugin->get_editor_interface()) {
				editor_plugin->get_editor_interface()->play_custom_scene(scene_path);
				result["ok"] = true;
				result["message"] = "Playing scene: " + scene_path;
				return result;
			}
		} else {
			result["ok"] = false;
			result["error"] = "Target scene file not found.";
			return result;
		}
	}

	result["ok"] = false;
	result["error"] = "Failed to evaluate play request.";
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
			for (const KeyValue<ObjectID, Object *> &E : selection->get_selection()) {
				Node *node = Object::cast_to<Node>(E.value);
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
	Dictionary result;

	if (DisplayServer::get_singleton()) {
		const int screen = DisplayServer::get_singleton()->get_primary_screen();
		Ref<Image> screenshot = DisplayServer::get_singleton()->screen_get_image(screen);
		if (screenshot.is_valid()) {
			String output_path = "res://.screenshot.png";
			Error err = screenshot->save_png(output_path);
			if (err == OK) {
				result["ok"] = true;
				result["path"] = output_path;
				result["message"] = "Editor viewport captured to Output Path.";
				return result;
			} else {
				result["ok"] = false;
				result["error"] = "Could not stream screenshot pixel data into OS buffers.";
			}
		}
	}
	result["ok"] = false;
	result["error"] = "DisplayServer capture fallback unavailable.";
	return result;
}

Dictionary JustAMCPEditorTools::editor_set_main_screen(const Dictionary &p_args) {
	Dictionary result;
	String screen_name = p_args.get("screen", "");
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
			String output_path = "res://.screenshot_game.png";
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

	return Dictionary();
}

#endif
