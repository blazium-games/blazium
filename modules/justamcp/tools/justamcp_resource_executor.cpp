/**************************************************************************/
/*  justamcp_resource_executor.cpp                                        */
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

#include "justamcp_resource_executor.h"
#include "../justamcp_pagination.h"
#include "../justamcp_server.h"
#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/input/input_event.h"
#include "core/input/input_map.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "justamcp_documentation_tools.h"
#include "main/performance.h"
#include "resources/justamcp_resource_autowork_results.h"
#include "resources/justamcp_resource_project_file.h"
#include "resources/justamcp_resource_system_logs.h"
#include "resources/justamcp_resource_video_recordings.h"
#include "scene/main/node.h"
#include "scene/resources/material.h"

void JustAMCPResourceExecutor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("list_resources", "cursor"), &JustAMCPResourceExecutor::list_resources, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("list_resource_templates", "cursor"), &JustAMCPResourceExecutor::list_resource_templates, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("read_resource", "uri"), &JustAMCPResourceExecutor::read_resource);
	ClassDB::bind_method(D_METHOD("add_resource", "resource"), &JustAMCPResourceExecutor::add_resource);
}

void JustAMCPResourceExecutor::register_settings() {
	JustAMCPResourceExecutor exec;

	Dictionary resources_dict = exec.list_resources();
	if (resources_dict.has("resources")) {
		Array resources = resources_dict["resources"];
		for (int i = 0; i < resources.size(); i++) {
			Dictionary res = resources[i];
			String name = res["name"];
			String desc = res["description"];
			String path = "blazium/justamcp/resources/" + name;

			GLOBAL_DEF_NOVAL_BASIC(PropertyInfo(Variant::STRING, path, PROPERTY_HINT_MULTILINE_TEXT, desc, PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_READ_ONLY), String());
			if (EditorSettings::get_singleton()) {
				EDITOR_DEF_BASIC(path, String());
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, path, PROPERTY_HINT_MULTILINE_TEXT, desc, PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_READ_ONLY));
			}
		}
	}

	Dictionary templates_dict = exec.list_resource_templates();
	if (templates_dict.has("resourceTemplates")) {
		Array templates = templates_dict["resourceTemplates"];
		for (int i = 0; i < templates.size(); i++) {
			Dictionary templ = templates[i];
			String name = templ["name"];
			String desc = templ["description"];
			String path = "blazium/justamcp/resources/" + name; // group templates under resources so it creates 1 accordion

			GLOBAL_DEF_NOVAL_BASIC(PropertyInfo(Variant::STRING, path, PROPERTY_HINT_MULTILINE_TEXT, desc, PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_READ_ONLY), String());
			if (EditorSettings::get_singleton()) {
				EDITOR_DEF_BASIC(path, String());
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, path, PROPERTY_HINT_MULTILINE_TEXT, desc, PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_READ_ONLY));
			}
		}
	}
}

JustAMCPResourceExecutor::JustAMCPResourceExecutor() {
	add_resource(memnew(JustAMCPResourceProjectFile));
	add_resource(memnew(JustAMCPResourceVideoRecordings));
	add_resource(memnew(JustAMCPResourceAutoworkResults));
}

JustAMCPResourceExecutor::~JustAMCPResourceExecutor() {}

void JustAMCPResourceExecutor::add_resource(const Ref<JustAMCPResource> &p_resource) {
	if (p_resource.is_valid()) {
		registered_resources.push_back(p_resource);
	}
}

Dictionary JustAMCPResourceExecutor::_make_resource_schema(const String &p_uri, const String &p_name, const String &p_description, const String &p_mime_type) const {
	Dictionary resource;
	resource["uri"] = p_uri;
	resource["name"] = p_name;
	resource["description"] = p_description;
	resource["mimeType"] = p_mime_type;
	return resource;
}

Dictionary JustAMCPResourceExecutor::_make_resource_template_schema(const String &p_uri_template, const String &p_name, const String &p_description, const String &p_mime_type) const {
	Dictionary resource;
	resource["uriTemplate"] = p_uri_template;
	resource["name"] = p_name;
	resource["description"] = p_description;
	resource["mimeType"] = p_mime_type;
	return resource;
}

Dictionary JustAMCPResourceExecutor::_make_json_contents(const String &p_uri, const Dictionary &p_payload) const {
	Dictionary result;
	result["ok"] = true;

	Array contents;
	Dictionary content;
	content["uri"] = p_uri;
	content["mimeType"] = "application/json";
	content["text"] = JSON::stringify(p_payload);
	contents.push_back(content);
	result["contents"] = contents;
	return result;
}

Dictionary JustAMCPResourceExecutor::_make_text_contents(const String &p_uri, const String &p_text, const String &p_mime_type) const {
	Dictionary result;
	result["ok"] = true;

	Array contents;
	Dictionary content;
	content["uri"] = p_uri;
	content["mimeType"] = p_mime_type;
	content["text"] = p_text;
	contents.push_back(content);
	result["contents"] = contents;
	return result;
}

Dictionary JustAMCPResourceExecutor::_make_json_error_payload(const String &p_uri, const String &p_error) const {
	Dictionary payload;
	payload["connected"] = false;
	payload["error"] = p_error;
	return _make_json_contents(p_uri, payload);
}

String JustAMCPResourceExecutor::_canonicalize_resource_uri(const String &p_uri) const {
	if (p_uri.begins_with("godot://")) {
		return "blazium://" + p_uri.substr(String("godot://").length());
	}
	if (p_uri.begins_with("godot-mcp://guide/")) {
		return "blazium://guide/" + p_uri.substr(String("godot-mcp://guide/").length());
	}
	return p_uri;
}

static bool _justamcp_try_extract_log_cursor(const String &p_uri, const String &p_prefix, String &r_cursor) {
	if (p_uri == p_prefix) {
		r_cursor = String();
		return true;
	}
	const String prefix_slash = p_prefix + "/";
	if (p_uri.begins_with(prefix_slash)) {
		r_cursor = justamcp_pagination_cursor_from_uri_suffix(p_uri.substr(prefix_slash.length()));
		return true;
	}
	return false;
}

Node *JustAMCPResourceExecutor::_get_edited_root() const {
	if (EditorNode::get_singleton() && EditorInterface::get_singleton()) {
		return EditorInterface::get_singleton()->get_edited_scene_root();
	}
	return nullptr;
}

Node *JustAMCPResourceExecutor::_find_node_by_resource_path(const String &p_path) const {
	Node *root = _get_edited_root();
	if (!root) {
		return nullptr;
	}

	String path = p_path;
	if (path.is_empty() || path == "." || path == "/" || path == String("/") + root->get_name()) {
		return root;
	}

	if (path.begins_with("/")) {
		String root_prefix = String("/") + root->get_name() + "/";
		if (path.begins_with(root_prefix)) {
			path = path.substr(root_prefix.length());
		} else {
			path = path.substr(1);
		}
	}

	return root->get_node_or_null(NodePath(path));
}

Variant JustAMCPResourceExecutor::_serialize_value(const Variant &p_value) const {
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

Dictionary JustAMCPResourceExecutor::_serialize_node_brief(Node *p_node, Node *p_root) const {
	Dictionary node;
	node["name"] = p_node->get_name();
	node["type"] = p_node->get_class();
	node["path"] = p_node == p_root ? String("/") + p_root->get_name() : String("/") + p_root->get_name() + "/" + String(p_root->get_path_to(p_node));
	node["child_count"] = p_node->get_child_count();
	return node;
}

void JustAMCPResourceExecutor::_append_node_tree(Node *p_node, Node *p_root, int p_depth, int p_max_depth, Array &r_nodes) const {
	if (!p_node || p_depth > p_max_depth) {
		return;
	}
	r_nodes.push_back(_serialize_node_brief(p_node, p_root));
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_append_node_tree(p_node->get_child(i), p_root, p_depth + 1, p_max_depth, r_nodes);
	}
}

void JustAMCPResourceExecutor::_collect_materials(const String &p_path, Array &r_materials) const {
	Ref<DirAccess> dir = DirAccess::open(p_path);
	if (dir.is_null()) {
		return;
	}

	dir->list_dir_begin();
	String name = dir->get_next();
	while (!name.is_empty()) {
		if (name.begins_with(".")) {
			name = dir->get_next();
			continue;
		}
		String full_path = p_path.path_join(name);
		if (dir->current_is_dir()) {
			_collect_materials(full_path, r_materials);
		} else if (name.ends_with(".tres") || name.ends_with(".res") || name.ends_with(".material")) {
			Ref<Resource> loaded = ResourceLoader::load(full_path);
			Ref<Material> material = loaded;
			if (material.is_valid()) {
				Dictionary info;
				info["path"] = full_path;
				info["type"] = material->get_class();
				r_materials.push_back(info);
			}
		}
		name = dir->get_next();
	}
	dir->list_dir_end();
}

Dictionary JustAMCPResourceExecutor::list_resources(const String &cursor) {
	Array resources;

	for (int i = 0; i < registered_resources.size(); i++) {
		if (registered_resources[i].is_valid() && !registered_resources[i]->is_template()) {
			resources.push_back(registered_resources[i]->get_schema());
		}
	}

	resources.push_back(_make_resource_schema("blazium://scene/current", "Current Scene", "Current scene path and play state."));
	resources.push_back(_make_resource_schema("blazium://scene/hierarchy", "Scene Hierarchy", "Flattened hierarchy for the active edited scene."));
	resources.push_back(_make_resource_schema("blazium://selection/current", "Current Selection", "Currently selected editor nodes."));
	resources.push_back(_make_resource_schema("blazium://project/info", "Project Info", "Project name, engine version, paths, active scene, and play state."));
	resources.push_back(_make_resource_schema("blazium://project/settings", "Project Settings", "Common project settings subset."));
	resources.push_back(_make_resource_schema("blazium://logs/recent", "Recent Logs", "Recent JustAMCP engine log lines."));
	resources.push_back(_make_resource_schema("blazium://materials", "Materials", "Material resources found under res://."));
	resources.push_back(_make_resource_schema("blazium://input_map", "Input Map", "Project input actions and their configured events."));
	resources.push_back(_make_resource_schema("blazium://performance", "Performance", "Performance singleton snapshot."));
	resources.push_back(_make_resource_schema("blazium://docs/classes", "Blazium Documentation Classes", "Internal class documentation summaries from the editor documentation database."));
	resources.push_back(_make_resource_schema("blazium://guide/testing-loop", "Testing Loop Guide", "How to run, drive, inspect, screenshot, and stop a running game through JustAMCP.", "text/markdown"));
	resources.push_back(_make_resource_schema("blazium://guide/scene-editing", "Scene Editing Guide", "How to choose scene, node, resource, signal, and group editing tools.", "text/markdown"));
	resources.push_back(_make_resource_schema("blazium://guide/asset-generation", "Asset Generation Guide", "How SVG-to-PNG asset generation works and how sizing options are applied.", "text/markdown"));
	resources.push_back(_make_resource_schema("blazium://guide/troubleshooting", "Troubleshooting Guide", "Common failures and recovery steps for runtime, project, and tool workflows.", "text/markdown"));
	resources.push_back(_make_resource_schema("blazium://guide/tool-index", "Tool Index Guide", "Goal-oriented guide to the main JustAMCP tool families.", "text/markdown"));
	resources.push_back(_make_resource_schema("blazium://system/logs", "System Logs", "Recent engine log lines captured by JustAMCP."));
	resources.push_back(_make_resource_schema("blazium://editor/state", "Editor State", "Current editor play mode, active scene, and selection summary."));
	resources.push_back(_make_resource_schema("blazium://test/results", "Test Results", "Latest Autowork test results when available."));

	return justamcp_pagination_slice_array(resources, cursor, "resources");
}

Dictionary JustAMCPResourceExecutor::list_resource_templates(const String &cursor) {
	Dictionary result;
	Array templates;

	for (int i = 0; i < registered_resources.size(); i++) {
		if (registered_resources[i].is_valid() && registered_resources[i]->is_template()) {
			templates.push_back(registered_resources[i]->get_schema());
		}
	}

	templates.push_back(_make_resource_template_schema("blazium://node/{path}/properties", "Node Properties", "Editor-visible properties for a node path such as blazium://node/Main/Camera3D/properties."));
	templates.push_back(_make_resource_template_schema("blazium://node/{path}/children", "Node Children", "Direct children for a node path such as blazium://node/Main/children."));
	templates.push_back(_make_resource_template_schema("blazium://node/{path}/groups", "Node Groups", "Groups assigned to a node path such as blazium://node/Main/Enemy/groups."));
	templates.push_back(_make_resource_template_schema("blazium://script/{path}", "Script Source", "Read a script by omitting the res:// prefix, such as blazium://script/scripts/player.gd."));
	templates.push_back(_make_resource_template_schema("blazium://docs/search/{query}", "Documentation Search", "Search internal Blazium class and member documentation."));
	templates.push_back(_make_resource_template_schema("blazium://docs/class/{class_name}", "Class Documentation", "Read internal documentation for one Blazium class."));
	templates.push_back(_make_resource_template_schema("blazium://docs/member/{class_name}/{member_type}/{member_name}", "Member Documentation", "Read internal documentation for one method, property, signal, constant, enum, annotation, or theme property."));
	templates.push_back(_make_resource_template_schema("blazium://logs/mcp/{cursor}", "MCP Log Notifications", "Paginated buffered MCP notifications/message history. Use blazium://logs/mcp/start for the first page."));
	templates.push_back(_make_resource_template_schema("blazium://logs/recent/{cursor}", "Recent Engine Logs", "Paginated recent engine log lines captured by JustAMCP. Use blazium://logs/recent/start for the first page."));
	templates.push_back(_make_resource_template_schema("blazium://system/logs/{cursor}", "System Logs (Paginated)", "Paginated engine log lines as JSON. Use blazium://system/logs/start for the first page."));

	return justamcp_pagination_slice_array(templates, cursor, "resourceTemplates");
}

Dictionary JustAMCPResourceExecutor::read_resource(const String &p_uri) {
	String canonical_uri = _canonicalize_resource_uri(p_uri);
	for (int i = 0; i < registered_resources.size(); i++) {
		if (registered_resources[i].is_valid()) {
			bool match = false;
			if (registered_resources[i]->is_template()) {
				// Naive match for "res://" if the template is "res://{path}"
				String tmpl = registered_resources[i]->get_uri();
				if (tmpl.begins_with("res://") && p_uri.begins_with("res://")) {
					match = true;
				}
			} else if (registered_resources[i]->get_uri() == p_uri || registered_resources[i]->get_uri() == canonical_uri) {
				match = true;
			}

			if (match) {
				Dictionary res = registered_resources[i]->read_resource(p_uri);
				if (res.has("ok") && bool(Variant(res["ok"]))) {
					return res; // Successfully read!
				}
			}
		}
	}

	if (canonical_uri.begins_with("blazium://")) {
		Dictionary result = _read_blazium_resource(p_uri);
		if (result.get("ok", false)) {
			return result;
		}
	}

	Dictionary result;
	result["ok"] = false;
	result["error_code"] = -32602;
	result["error"] = "Unknown resource URI: " + p_uri;
	return result;
}

Dictionary JustAMCPResourceExecutor::_read_node_resource(const String &p_uri, const String &p_suffix) const {
	String prefix = p_uri.begins_with("blazium://node/") ? "blazium://node/" : "godot://node/";
	String path = p_uri.substr(prefix.length());
	path = path.substr(0, path.length() - p_suffix.length());
	Node *root = _get_edited_root();
	if (!root) {
		return _make_json_error_payload(p_uri, "No scene is currently open");
	}
	Node *node = _find_node_by_resource_path(path);
	if (!node) {
		return _make_json_error_payload(p_uri, "Node not found: /" + path);
	}

	Dictionary payload;
	payload["path"] = String("/") + root->get_name() + (node == root ? String() : String("/") + String(root->get_path_to(node)));

	if (p_suffix == "/properties") {
		Dictionary props;
		List<PropertyInfo> prop_list;
		node->get_property_list(&prop_list);
		for (const PropertyInfo &prop : prop_list) {
			if ((prop.usage & PROPERTY_USAGE_EDITOR) && !String(prop.name).begins_with("_")) {
				props[prop.name] = _serialize_value(node->get(prop.name));
			}
		}
		payload["type"] = node->get_class();
		payload["properties"] = props;
	} else if (p_suffix == "/children") {
		Array children;
		for (int i = 0; i < node->get_child_count(); i++) {
			children.push_back(_serialize_node_brief(node->get_child(i), root));
		}
		payload["children"] = children;
		payload["count"] = children.size();
	} else if (p_suffix == "/groups") {
		Array groups;
		List<Node::GroupInfo> group_info;
		node->get_groups(&group_info);
		for (const Node::GroupInfo &group : group_info) {
			String group_name = group.name;
			if (!group_name.begins_with("_")) {
				groups.push_back(group_name);
			}
		}
		payload["groups"] = groups;
		payload["count"] = groups.size();
	}

	return _make_json_contents(p_uri, payload);
}

Dictionary JustAMCPResourceExecutor::_read_script_resource(const String &p_uri) const {
	String prefix = p_uri.begins_with("blazium://script/") ? "blazium://script/" : "godot://script/";
	String script_path = p_uri.substr(prefix.length());
	if (!script_path.begins_with("res://")) {
		script_path = "res://" + script_path;
	}
	if (!FileAccess::exists(script_path)) {
		return _make_json_error_payload(p_uri, "Script not found: " + script_path);
	}
	Ref<FileAccess> file = FileAccess::open(script_path, FileAccess::READ);
	if (file.is_null()) {
		return _make_json_error_payload(p_uri, "Cannot read script: " + script_path);
	}

	String content = file->get_as_text();
	file->close();

	Dictionary payload;
	payload["path"] = script_path;
	payload["content"] = content;
	payload["line_count"] = content.get_slice_count("\n");
	payload["size"] = content.length();
	return _make_json_contents(p_uri, payload);
}

Dictionary JustAMCPResourceExecutor::_read_guide_resource(const String &p_uri) const {
	String canonical_uri = _canonicalize_resource_uri(p_uri);
	String slug = canonical_uri.substr(String("blazium://guide/").length());
	String title;
	String body;

	if (slug == "testing-loop") {
		title = "Testing a Running Blazium Game from MCP";
		body = "1. Start the game with `blazium_editor_play_scene` or `blazium_editor_play_main`.\n"
			   "2. Check `blazium_editor_is_playing`, `blazium_runtime_info`, and `blazium_editor_get_errors` before sending input.\n"
			   "3. Drive input with `blazium_simulate_key`, `blazium_simulate_mouse_click`, `blazium_simulate_action`, or `blazium_send_input` when a runtime bridge is active.\n"
			   "4. Use `blazium_wait` between actions, then inspect state with `blazium_runtime_inspect_node`, `blazium_query_runtime_node`, or `blazium_take_game_screenshot`.\n"
			   "5. Stop with `blazium_editor_stop_play` before editing scripts that run every frame.\n";
	} else if (slug == "scene-editing") {
		title = "Scene Editing Patterns";
		body = "- Create scenes with `blazium_create_scene` and add nodes with `blazium_add_node`.\n"
			   "- Use `blazium_set_node_properties` for multiple node values and `blazium_set_resource_property` for nested resources.\n"
			   "- Replace common node resources with `blazium_set_collision_shape`, `blazium_set_sprite_texture`, `blazium_set_mesh`, and `blazium_set_material`.\n"
			   "- Persist node-attached resources with `blazium_save_resource_to_file`.\n"
			   "- Verify signal wiring with `blazium_list_connections`, `blazium_list_node_signals`, and `blazium_has_signal_connection`.\n";
	} else if (slug == "asset-generation") {
		title = "Generating 2D Assets";
		body = "`blazium_asset_generate_2d_asset` renders SVG directly through the engine image loader and saves a PNG under the project. "
			   "Prefer explicit SVG width, height, and viewBox values for predictable output. Use width, height, or scale arguments to control rasterization size when available.\n";
	} else if (slug == "troubleshooting") {
		title = "Troubleshooting";
		body = "- If runtime inspection fails, confirm the game is running and the JustAMCP runtime bridge is enabled.\n"
			   "- If new files do not appear, call `blazium_rescan_filesystem` or reopen the editor filesystem dock.\n"
			   "- If an API call fails, verify the target class with `blazium_classdb_query` or `blazium_docs_get_class` before editing code.\n"
			   "- If project settings were edited, restart or reload the project when the setting is only read during startup.\n";
	} else if (slug == "tool-index") {
		title = "JustAMCP Quick Tool Index by Goal";
		body = "JustAMCP groups common Blazium editor, runtime, documentation, and asset workflows into these tool families:\n"
			   "- Files and scripts: `blazium_read_file`, `blazium_create_script`, `blazium_edit_script`, `blazium_validate_script`, `blazium_search_in_scripts`.\n"
			   "- Scenes and nodes: `blazium_create_scene`, `blazium_list_scene_nodes`, `blazium_add_node`, `blazium_duplicate_node`, `blazium_scene_tree_dump`.\n"
			   "- Project: `blazium_project_list_settings`, `blazium_project_update_settings`, `blazium_project_get_input_actions`, `blazium_project_set_input_action`.\n"
			   "- Runtime: `blazium_editor_play_scene`, `blazium_runtime_info`, `blazium_wait`, `blazium_take_game_screenshot`, `blazium_editor_stop_play`.\n"
			   "- Docs: `blazium_docs_search`, `blazium_docs_get_class`, `blazium_classdb_query`.\n";
	} else {
		return _make_json_error_payload(p_uri, "Unknown guide: " + slug);
	}

	return _make_text_contents(p_uri, "# " + title + "\n\n" + body);
}

Dictionary JustAMCPResourceExecutor::_read_blazium_resource(const String &p_uri) const {
	Node *root = _get_edited_root();
	String canonical_uri = _canonicalize_resource_uri(p_uri);
	bool editor_ready = EditorNode::get_singleton() && EditorInterface::get_singleton();

	if (canonical_uri.begins_with("blazium://guide/")) {
		return _read_guide_resource(p_uri);
	}

	if (canonical_uri == "blazium://sessions") {
		Dictionary session;
		session["session_id"] = "justamcp-editor";
		session["godot_version"] = Engine::get_singleton()->get_version_info().get("string", "unknown");
		session["project_path"] = ProjectSettings::get_singleton()->get_resource_path();
		session["project_name"] = ProjectSettings::get_singleton()->get_setting("application/config/name", "");
		session["is_active"] = JustAMCPServer::get_singleton() ? JustAMCPServer::get_singleton()->is_server_started() : true;

		Array sessions;
		sessions.push_back(session);
		Dictionary payload;
		payload["count"] = sessions.size();
		payload["sessions"] = sessions;
		return _make_json_contents(p_uri, payload);
	}

	if (canonical_uri == "blazium://scene/current") {
		Dictionary payload;
		payload["current_scene"] = root && root->get_scene_file_path().is_empty() ? String() : (root ? root->get_scene_file_path() : String());
		payload["root"] = root ? _serialize_node_brief(root, root) : Dictionary();
		payload["project_name"] = ProjectSettings::get_singleton()->get_setting("application/config/name", "");
		payload["is_playing"] = editor_ready ? EditorInterface::get_singleton()->is_playing_scene() : false;
		return _make_json_contents(p_uri, payload);
	}

	if (canonical_uri == "blazium://scene/hierarchy") {
		if (!root) {
			return _make_json_error_payload(p_uri, "No scene is currently open");
		}
		Array nodes;
		_append_node_tree(root, root, 0, 10, nodes);
		Dictionary payload;
		payload["nodes"] = nodes;
		payload["total_count"] = nodes.size();
		return _make_json_contents(p_uri, payload);
	}

	if (canonical_uri == "blazium://selection/current") {
		Array selected_paths;
		Array selected_nodes;
		if (editor_ready && EditorInterface::get_singleton()->get_selection()) {
			Array selected = EditorInterface::get_singleton()->get_selection()->get_selected_nodes();
			for (int i = 0; i < selected.size(); i++) {
				Node *node = Object::cast_to<Node>(selected[i]);
				if (node) {
					selected_paths.push_back(String(node->get_path()));
					Dictionary entry;
					entry["name"] = node->get_name();
					entry["type"] = node->get_class();
					entry["path"] = String(node->get_path());
					selected_nodes.push_back(entry);
				}
			}
		}
		Dictionary payload;
		payload["selected_paths"] = selected_paths;
		payload["nodes"] = selected_nodes;
		payload["count"] = selected_paths.size();
		return _make_json_contents(p_uri, payload);
	}

	if (canonical_uri == "blazium://project/info") {
		Dictionary payload;
		payload["session_id"] = "justamcp-editor";
		payload["godot_version"] = Engine::get_singleton()->get_version_info().get("string", "unknown");
		payload["project_path"] = ProjectSettings::get_singleton()->get_resource_path();
		payload["project_name"] = ProjectSettings::get_singleton()->get_setting("application/config/name", "");
		payload["current_scene"] = root ? root->get_scene_file_path() : String();
		payload["is_playing"] = editor_ready ? EditorInterface::get_singleton()->is_playing_scene() : false;
		payload["is_active"] = JustAMCPServer::get_singleton() ? JustAMCPServer::get_singleton()->is_server_started() : true;
		payload["readiness"] = !root ? "no_scene" : (bool(payload["is_playing"]) ? "playing" : "ready");
		return _make_json_contents(p_uri, payload);
	}

	if (canonical_uri == "blazium://project/settings") {
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
		return _make_json_contents(p_uri, payload);
	}

	{
		String log_cursor;
		if (_justamcp_try_extract_log_cursor(canonical_uri, "blazium://logs/mcp", log_cursor)) {
			if (!JustAMCPServer::get_singleton()) {
				Dictionary payload;
				payload["notifications"] = Array();
				payload["count"] = 0;
				return _make_json_contents(p_uri, payload);
			}
			Dictionary page = JustAMCPServer::get_singleton()->get_mcp_notification_log_page(log_cursor);
			if (page.has("ok") && !bool(page.get("ok", true))) {
				Dictionary payload;
				payload["error"] = page.get("error", "Invalid pagination cursor.");
				return _make_json_contents(p_uri, payload);
			}
			Dictionary payload;
			payload["notifications"] = page.get("notifications", Array());
			payload["count"] = Array(payload["notifications"]).size();
			if (page.has("nextCursor")) {
				payload["nextCursor"] = page["nextCursor"];
				payload["nextUri"] = "blazium://logs/mcp/" + String(page["nextCursor"]);
			}
			return _make_json_contents(p_uri, payload);
		}
	}

	{
		String log_cursor;
		if (_justamcp_try_extract_log_cursor(canonical_uri, "blazium://logs/recent", log_cursor)) {
			Array all_lines;
			if (JustAMCPServer::get_singleton()) {
				Vector<String> logs = JustAMCPServer::get_singleton()->get_engine_logs();
				all_lines.resize(logs.size());
				for (int i = 0; i < logs.size(); i++) {
					all_lines[i] = logs[i];
				}
			}
			Dictionary page = justamcp_pagination_slice_array(all_lines, log_cursor, "lines");
			if (page.has("ok") && !bool(page.get("ok", true))) {
				Dictionary payload;
				payload["error"] = page.get("error", "Invalid pagination cursor.");
				return _make_json_contents(p_uri, payload);
			}
			Dictionary payload;
			payload["lines"] = page.get("lines", Array());
			payload["count"] = Array(payload["lines"]).size();
			if (page.has("nextCursor")) {
				payload["nextCursor"] = page["nextCursor"];
				payload["nextUri"] = "blazium://logs/recent/" + String(page["nextCursor"]);
			}
			return _make_json_contents(p_uri, payload);
		}
	}

	{
		String log_cursor;
		if (_justamcp_try_extract_log_cursor(canonical_uri, "blazium://system/logs", log_cursor)) {
			Array all_lines;
			if (JustAMCPServer::get_singleton()) {
				Vector<String> logs = JustAMCPServer::get_singleton()->get_engine_logs();
				all_lines.resize(logs.size());
				for (int i = 0; i < logs.size(); i++) {
					all_lines[i] = logs[i];
				}
			}
			Dictionary page = justamcp_pagination_slice_array(all_lines, log_cursor, "lines");
			if (page.has("ok") && !bool(page.get("ok", true))) {
				Dictionary payload;
				payload["error"] = page.get("error", "Invalid pagination cursor.");
				return _make_json_contents(p_uri, payload);
			}
			Dictionary payload;
			payload["lines"] = page.get("lines", Array());
			payload["count"] = Array(payload["lines"]).size();
			if (page.has("nextCursor")) {
				payload["nextCursor"] = page["nextCursor"];
				payload["nextUri"] = "blazium://system/logs/" + String(page["nextCursor"]);
			}
			return _make_json_contents(p_uri, payload);
		}
	}

	if (canonical_uri == "blazium://editor/state") {
		Dictionary payload;
		payload["godot_version"] = Engine::get_singleton()->get_version_info().get("string", "unknown");
		payload["project_name"] = ProjectSettings::get_singleton()->get_setting("application/config/name", "");
		payload["current_scene"] = root ? root->get_scene_file_path() : String();
		payload["is_playing"] = editor_ready ? EditorInterface::get_singleton()->is_playing_scene() : false;
		payload["readiness"] = !root ? "no_scene" : (bool(payload["is_playing"]) ? "playing" : "ready");
		return _make_json_contents(p_uri, payload);
	}

	if (canonical_uri == "blazium://materials") {
		Array materials;
		_collect_materials("res://", materials);
		Dictionary payload;
		payload["materials"] = materials;
		payload["count"] = materials.size();
		return _make_json_contents(p_uri, payload);
	}

	if (canonical_uri == "blazium://input_map") {
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
		return _make_json_contents(p_uri, payload);
	}

	if (canonical_uri == "blazium://performance") {
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
		return _make_json_contents(p_uri, payload);
	}

	if (canonical_uri == "blazium://test/results") {
		Dictionary payload;
		payload["available"] = false;
		payload["source"] = "autowork";
		payload["content"] = String();
		String json_path = "user://autowork_results.json";
		String xml_path = "user://autowork_results.xml";
		String result_path;
		if (FileAccess::exists(json_path)) {
			result_path = json_path;
		} else if (FileAccess::exists(xml_path)) {
			result_path = xml_path;
		}
		if (!result_path.is_empty()) {
			Ref<FileAccess> file = FileAccess::open(result_path, FileAccess::READ);
			if (file.is_valid()) {
				payload["available"] = true;
				payload["path"] = result_path;
				payload["content"] = file->get_as_utf8_string();
			}
		}
		return _make_json_contents(p_uri, payload);
	}

	if (canonical_uri == "blazium://docs/classes") {
		JustAMCPDocumentationTools docs;
		Dictionary args;
		Dictionary payload = docs.list_classes(args);
		if (!payload.get("ok", false)) {
			return _make_json_error_payload(p_uri, payload.get("error", "Failed to read documentation classes."));
		}
		payload.erase("ok");
		return _make_json_contents(p_uri, payload);
	}

	if (canonical_uri.begins_with("blazium://docs/search/")) {
		String query = canonical_uri.substr(String("blazium://docs/search/").length()).replace("%20", " ");
		JustAMCPDocumentationTools docs;
		Dictionary args;
		args["query"] = query;
		args["limit"] = 50;
		Dictionary payload = docs.search_documentation(args);
		if (!payload.get("ok", false)) {
			return _make_json_error_payload(p_uri, payload.get("error", "Failed to search documentation."));
		}
		payload.erase("ok");
		return _make_json_contents(p_uri, payload);
	}

	if (canonical_uri.begins_with("blazium://docs/class/")) {
		String class_name = canonical_uri.substr(String("blazium://docs/class/").length());
		JustAMCPDocumentationTools docs;
		Dictionary args;
		args["class_name"] = class_name;
		args["include_members"] = true;
		Dictionary payload = docs.get_class_documentation(args);
		if (!payload.get("ok", false)) {
			return _make_json_error_payload(p_uri, payload.get("error", "Failed to read class documentation."));
		}
		payload.erase("ok");
		return _make_json_contents(p_uri, payload);
	}

	if (canonical_uri.begins_with("blazium://docs/member/")) {
		String member_path = canonical_uri.substr(String("blazium://docs/member/").length());
		if (member_path.get_slice_count("/") < 3) {
			return _make_json_error_payload(p_uri, "Member documentation URI requires class_name/member_type/member_name.");
		}
		JustAMCPDocumentationTools docs;
		Dictionary args;
		args["class_name"] = member_path.get_slice("/", 0);
		args["member_type"] = member_path.get_slice("/", 1);
		args["member_name"] = member_path.get_slice("/", 2);
		Dictionary payload = docs.get_member_documentation(args);
		if (!payload.get("ok", false)) {
			return _make_json_error_payload(p_uri, payload.get("error", "Failed to read member documentation."));
		}
		payload.erase("ok");
		return _make_json_contents(p_uri, payload);
	}

	if ((p_uri.begins_with("blazium://node/") || p_uri.begins_with("godot://node/")) && p_uri.ends_with("/properties")) {
		return _read_node_resource(p_uri, "/properties");
	}
	if ((p_uri.begins_with("blazium://node/") || p_uri.begins_with("godot://node/")) && p_uri.ends_with("/children")) {
		return _read_node_resource(p_uri, "/children");
	}
	if ((p_uri.begins_with("blazium://node/") || p_uri.begins_with("godot://node/")) && p_uri.ends_with("/groups")) {
		return _read_node_resource(p_uri, "/groups");
	}
	if (p_uri.begins_with("blazium://script/") || p_uri.begins_with("godot://script/")) {
		return _read_script_resource(p_uri);
	}

	Dictionary result;
	result["ok"] = false;
	return result;
}

#endif // TOOLS_ENABLED
