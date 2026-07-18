/**************************************************************************/
/*  justamcp_project_resource_provider.cpp                                */
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

#include "justamcp_project_resource_provider.h"
#include "../../justamcp_editor_scene_access.h"

#include "../../justamcp_server.h"
#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/input/input_event.h"
#include "core/input/input_map.h"
#include "core/io/json.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "main/performance.h"
#include "scene/main/node.h"

#ifdef MODULE_ASSETTAGS_ENABLED
#include "core/templates/hash_set.h"
#include "modules/assettags/asset_tag_manager.h"
#endif

static Dictionary _project_json_contents(const String &p_uri, const Dictionary &p_payload) {
	Dictionary result;
	result["ok"] = true;
	Array contents;
	Dictionary content;
	content["uri"] = p_uri;
	content["mimeType"] = "application/json";
	content["text"] = JSON::stringify(p_payload, "\t");
	contents.push_back(content);
	result["contents"] = contents;
	return result;
}

static Node *_project_edited_root() {
	if (EditorNode::get_singleton() && EditorInterface::get_singleton()) {
		return JustAMCPEditorSceneAccess::get_edited_root();
	}
	return nullptr;
}

bool JustAMCPProjectResourceProvider::can_read(const String &p_canonical_uri) {
	return p_canonical_uri == "blazium://project/info" ||
			p_canonical_uri == "blazium://project/settings" ||
			p_canonical_uri == "blazium://editor/state" ||
			p_canonical_uri == "blazium://input_map" ||
			p_canonical_uri == "blazium://performance";
}

Dictionary JustAMCPProjectResourceProvider::read(const String &p_uri, const String &p_canonical_uri) {
	Node *root = _project_edited_root();
	const bool editor_ready = EditorNode::get_singleton() && EditorInterface::get_singleton();

	if (p_canonical_uri == "blazium://project/info") {
		Dictionary payload;
		payload["session_id"] = "justamcp-editor";
		payload["godot_version"] = Engine::get_singleton()->get_version_info().get("string", "unknown");
		payload["project_path"] = ProjectSettings::get_singleton()->get_resource_path();
		payload["project_name"] = ProjectSettings::get_singleton()->get_setting("application/config/name", "");
		payload["current_scene"] = root ? root->get_scene_file_path() : String();
		payload["is_playing"] = editor_ready ? EditorInterface::get_singleton()->is_playing_scene() : false;
		payload["is_active"] = JustAMCPServer::get_singleton() ? JustAMCPServer::get_singleton()->is_server_started() : true;
		payload["readiness"] = !root ? "no_scene" : (bool(payload["is_playing"]) ? "playing" : "ready");
#ifdef MODULE_ASSETTAGS_ENABLED
		if (AssetTagManager *tag_manager = AssetTagManager::get_singleton()) {
			PackedStringArray all_tags = tag_manager->list_all_tags();
			payload["asset_tag_count"] = all_tags.size();
			HashSet<String> namespaces;
			for (int i = 0; i < all_tags.size(); i++) {
				namespaces.insert(all_tags[i].get_slice(".", 0));
			}
			Array top_namespaces;
			for (const String &ns : namespaces) {
				top_namespaces.push_back(ns);
			}
			top_namespaces.sort();
			payload["asset_tag_namespaces"] = top_namespaces;
		}
#endif
		return _project_json_contents(p_uri, payload);
	}

	if (p_canonical_uri == "blazium://project/settings") {
		static const char *common_settings[] = {
			"application/config/name",
			"application/config/description",
			"application/run/main_scene",
			"display/window/size/viewport_width",
			"display/window/size/viewport_height",
			"rendering/renderer/rendering_method",
			"physics/2d/default_gravity",
			"physics/3d/default_gravity",
		};
		Dictionary settings;
		for (const char *key : common_settings) {
			settings[key] = ProjectSettings::get_singleton()->get_setting(key, Variant());
		}
		Dictionary payload;
		payload["settings"] = settings;
		payload["errors"] = Variant();
		return _project_json_contents(p_uri, payload);
	}

	if (p_canonical_uri == "blazium://editor/state") {
		Dictionary payload;
		payload["godot_version"] = Engine::get_singleton()->get_version_info().get("string", "unknown");
		payload["project_name"] = ProjectSettings::get_singleton()->get_setting("application/config/name", "");
		payload["current_scene"] = root ? root->get_scene_file_path() : String();
		payload["is_playing"] = editor_ready ? EditorInterface::get_singleton()->is_playing_scene() : false;
		payload["readiness"] = !root ? "no_scene" : (bool(payload["is_playing"]) ? "playing" : "ready");
		return _project_json_contents(p_uri, payload);
	}

	if (p_canonical_uri == "blazium://input_map") {
		Dictionary actions;
		if (InputMap::get_singleton()) {
			List<StringName> action_names = InputMap::get_singleton()->get_actions();
			for (const StringName &action_name : action_names) {
				String action = action_name;
				if (action.begins_with("ui_")) {
					continue;
				}
				Array events;
				const List<Ref<InputEvent>> *action_events = InputMap::get_singleton()->action_get_events(action_name);
				if (action_events) {
					for (const Ref<InputEvent> &event : *action_events) {
						if (event.is_valid()) {
							events.push_back(event->as_text());
						}
					}
				}
				Dictionary info;
				info["deadzone"] = InputMap::get_singleton()->action_get_deadzone(action_name);
				info["events"] = events;
				actions[action] = info;
			}
		}
		Dictionary payload;
		payload["actions"] = actions;
		payload["count"] = actions.size();
		return _project_json_contents(p_uri, payload);
	}

	if (p_canonical_uri == "blazium://performance") {
		Dictionary monitors;
		if (Performance::get_singleton()) {
			monitors["time/fps"] = Performance::get_singleton()->get_monitor(Performance::TIME_FPS);
			monitors["time/process"] = Performance::get_singleton()->get_monitor(Performance::TIME_PROCESS);
			monitors["time/physics_process"] = Performance::get_singleton()->get_monitor(Performance::TIME_PHYSICS_PROCESS);
			monitors["memory/static"] = Performance::get_singleton()->get_monitor(Performance::MEMORY_STATIC);
			monitors["memory/static_max"] = Performance::get_singleton()->get_monitor(Performance::MEMORY_STATIC_MAX);
			monitors["object/count"] = Performance::get_singleton()->get_monitor(Performance::OBJECT_COUNT);
			monitors["object/resource_count"] = Performance::get_singleton()->get_monitor(Performance::OBJECT_RESOURCE_COUNT);
			monitors["object/node_count"] = Performance::get_singleton()->get_monitor(Performance::OBJECT_NODE_COUNT);
			monitors["render/total_draw_calls_in_frame"] = Performance::get_singleton()->get_monitor(Performance::RENDER_TOTAL_DRAW_CALLS_IN_FRAME);
		}
		Dictionary payload;
		payload["monitors"] = monitors;
		payload["missing"] = Array();
		return _project_json_contents(p_uri, payload);
	}

	Dictionary err;
	err["ok"] = false;
	err["error_code"] = -32602;
	err["error"] = "Unknown project resource URI: " + p_uri;
	err["uri"] = p_uri;
	return err;
}

#endif
