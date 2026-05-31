/**************************************************************************/
/*  justamcp_tool_executor.cpp                                            */
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

#include "justamcp_tool_executor.h"
#include "../justamcp_runtime.h"
#include "../justamcp_server.h"
#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/os/os.h"
#include "justamcp_analysis_tools.h"
#include "justamcp_animation_tools.h"
#include "justamcp_audio_tools.h"
#include "justamcp_batch_tools.h"
#include "justamcp_documentation_tools.h"
#include "justamcp_editor_tools.h"
#include "justamcp_export_tools.h"
#include "justamcp_input_tools.h"
#include "justamcp_networking_tools.h"
#include "justamcp_node_tools.h"
#include "justamcp_particle_tools.h"
#include "justamcp_physics_tools.h"
#include "justamcp_profiling_tools.h"
#include "justamcp_project_tools.h"
#include "justamcp_resource_executor.h"
#include "justamcp_resource_tools.h"
#include "justamcp_runtime_tools.h"
#include "justamcp_scene_3d_tools.h"
#include "justamcp_scene_tools.h"
#include "justamcp_script_tools.h"
#include "justamcp_shader_tools.h"
#include "justamcp_spatial_tools.h"
#include "justamcp_theme_tools.h"
#include "justamcp_tilemap_tools.h"

#ifdef TOOLS_ENABLED
#include "../justamcp_editor_plugin.h"
#include "editor/editor_file_system.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "scene/main/node.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "scene/resources/texture.h"
#endif

#ifdef MODULE_AUTOWORK_ENABLED
#include "justamcp_autowork_tools.h"
#endif

#ifdef MODULE_MULTIUSER_EDITOR_ENABLED
#include "justamcp_multiuser_tools.h"
#endif

void JustAMCPToolExecutor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("execute_tool", "tool_name", "args"), &JustAMCPToolExecutor::execute_tool);
	ClassDB::bind_static_method("JustAMCPToolExecutor", D_METHOD("get_tool_schemas", "register_only", "ignore_settings"), &JustAMCPToolExecutor::get_tool_schemas, DEFVAL(false), DEFVAL(false));
	ClassDB::bind_static_method("JustAMCPToolExecutor", D_METHOD("set_test_scene_root", "node"), &JustAMCPToolExecutor::set_test_scene_root);
}

Node *JustAMCPToolExecutor::test_scene_root = nullptr;
void JustAMCPToolExecutor::set_test_scene_root(Node *p_node) {
	test_scene_root = p_node;
}
Node *JustAMCPToolExecutor::get_test_scene_root() {
	return test_scene_root;
}

#ifdef TOOLS_ENABLED
static Node *_justamcp_get_edited_root() {
	if (JustAMCPToolExecutor::get_test_scene_root()) {
		return JustAMCPToolExecutor::get_test_scene_root();
	}
	if (EditorNode::get_singleton() && EditorInterface::get_singleton()) {
		return EditorInterface::get_singleton()->get_edited_scene_root();
	}
	return nullptr;
}

static Node *_justamcp_find_node(const String &p_path) {
	Node *root = _justamcp_get_edited_root();
	if (!root) {
		return nullptr;
	}
	if (p_path.is_empty() || p_path == "." || p_path == root->get_name()) {
		return root;
	}
	if (root->has_node(p_path)) {
		return root->get_node(p_path);
	}
	if (p_path.begins_with(String(root->get_name()) + "/")) {
		String rel = p_path.substr(String(root->get_name()).length() + 1);
		if (root->has_node(rel)) {
			return root->get_node(rel);
		}
	}
	if (p_path.begins_with("/root/") && root->get_tree()) {
		return root->get_tree()->get_root()->get_node_or_null(NodePath(p_path.substr(6)));
	}
	return nullptr;
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

static Dictionary _justamcp_scene_tree_dump() {
	Dictionary result;
	Node *root = _justamcp_get_edited_root();
	if (!root) {
		result["ok"] = false;
		result["error"] = "No scene is currently open.";
		return result;
	}
	Array nodes;
	List<Node *> stack;
	stack.push_back(root);
	while (!stack.is_empty()) {
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
		for (int i = node->get_child_count() - 1; i >= 0; i--) {
			stack.push_front(node->get_child(i));
		}
	}
	result["ok"] = true;
	result["root"] = root->get_name();
	result["scene_path"] = root->get_scene_file_path();
	result["nodes"] = nodes;
	result["count"] = nodes.size();
	return result;
}
#endif

JustAMCPToolExecutor::JustAMCPToolExecutor() {
	_init_tools();
}

JustAMCPToolExecutor::~JustAMCPToolExecutor() {
	if (scene_tools) {
		memdelete(scene_tools);
	}
	if (editor_tools) {
		memdelete(editor_tools);
	}
	if (networking_tools) {
		memdelete(networking_tools);
	}
	if (analysis_tools) {
		memdelete(analysis_tools);
	}
	if (resource_tools) {
		memdelete(resource_tools);
	}
	if (animation_tools) {
		memdelete(animation_tools);
	}
	if (project_tools) {
		memdelete(project_tools);
	}
	if (profiling_tools) {
		memdelete(profiling_tools);
	}
	if (export_tools) {
		memdelete(export_tools);
	}
	if (spatial_tools) {
		memdelete(spatial_tools);
	}
	if (runtime_tools) {
		memdelete(runtime_tools);
	}
	if (batch_tools) {
		memdelete(batch_tools);
	}
	if (documentation_tools) {
		memdelete(documentation_tools);
	}
	if (script_tools) {
		memdelete(script_tools);
	}
	if (node_tools) {
		memdelete(node_tools);
	}
	if (audio_tools) {
		memdelete(audio_tools);
	}
	if (blueprint_tools) {
		memdelete(blueprint_tools);
	}
	if (input_tools) {
		memdelete(input_tools);
	}
	if (particle_tools) {
		memdelete(particle_tools);
	}
	if (physics_tools) {
		memdelete(physics_tools);
	}
	if (scene_3d_tools) {
		memdelete(scene_3d_tools);
	}
	if (shader_tools) {
		memdelete(shader_tools);
	}
	if (theme_tools) {
		memdelete(theme_tools);
	}
	if (tilemap_tools) {
		memdelete(tilemap_tools);
	}
#ifdef MODULE_AUTOWORK_ENABLED
	if (autowork_tools) {
		memdelete(autowork_tools);
	}
#endif
#ifdef MODULE_MULTIUSER_EDITOR_ENABLED
	if (multiuser_tools) {
		memdelete(multiuser_tools);
	}
#endif
	if (asset_tools) {
		memdelete(asset_tools);
	}
	if (draw_tools) {
		memdelete(draw_tools);
	}
	if (environment_tools) {
		memdelete(environment_tools);
	}
}

void JustAMCPToolExecutor::set_editor_plugin(JustAMCPEditorPlugin *p_plugin) {
	editor_plugin = p_plugin;

	if (scene_tools) {
		scene_tools->set_editor_plugin(p_plugin);
	}
	if (editor_tools) {
		editor_tools->set_editor_plugin(p_plugin);
	}
	if (networking_tools) {
		networking_tools->set_editor_plugin(p_plugin);
	}
	if (resource_tools) {
		resource_tools->set_editor_plugin(p_plugin);
	}
	if (animation_tools) {
		animation_tools->set_editor_plugin(p_plugin);
	}
	if (project_tools) {
		project_tools->set_editor_plugin(p_plugin);
	}
	if (profiling_tools) {
		profiling_tools->set_editor_plugin(p_plugin);
	}
	if (export_tools) {
		export_tools->set_editor_plugin(p_plugin);
	}
	if (spatial_tools) {
		spatial_tools->set_editor_plugin(p_plugin);
	}
	if (runtime_tools) {
		runtime_tools->set_editor_plugin(p_plugin);
	}
	if (batch_tools) {
		batch_tools->set_editor_plugin(p_plugin);
	}
	if (script_tools) {
		script_tools->set_editor_plugin(p_plugin);
	}
	if (node_tools) {
		node_tools->set_editor_plugin(p_plugin);
	}
	if (audio_tools) {
		audio_tools->set_editor_plugin(p_plugin);
	}
	if (input_tools) {
		input_tools->set_editor_plugin(p_plugin);
	}
	if (particle_tools) {
		particle_tools->set_editor_plugin(p_plugin);
	}
	if (analysis_tools) {
		analysis_tools->set_editor_plugin(p_plugin);
	}
	if (physics_tools) {
		physics_tools->set_editor_plugin(p_plugin);
	}
	if (scene_3d_tools) {
		scene_3d_tools->set_editor_plugin(p_plugin);
	}
	if (shader_tools) {
		shader_tools->set_editor_plugin(p_plugin);
	}
	if (theme_tools) {
		theme_tools->set_editor_plugin(p_plugin);
	}
	if (tilemap_tools) {
		tilemap_tools->set_editor_plugin(p_plugin);
	}
#ifdef MODULE_AUTOWORK_ENABLED
	if (autowork_tools) {
		autowork_tools->set_editor_plugin(p_plugin);
	}
#endif
#ifdef MODULE_MULTIUSER_EDITOR_ENABLED
	if (multiuser_tools) {
		multiuser_tools->set_editor_plugin(p_plugin);
	}
#endif
	if (asset_tools) {
		asset_tools->set_editor_plugin(p_plugin);
	}
	if (blueprint_tools) {
		blueprint_tools->set_editor_plugin(p_plugin);
	}
	if (draw_tools) {
		draw_tools->set_editor_plugin(p_plugin);
	}
	if (environment_tools) {
		environment_tools->set_editor_plugin(p_plugin);
	}
}

void JustAMCPToolExecutor::_init_tools() {
	if (initialized) {
		return;
	}
	initialized = true;

	scene_tools = memnew(JustAMCPSceneTools);
	editor_tools = memnew(JustAMCPEditorTools);
	networking_tools = memnew(JustAMCPNetworkingTools);
	analysis_tools = memnew(JustAMCPAnalysisTools);
	resource_tools = memnew(JustAMCPResourceTools);
	animation_tools = memnew(JustAMCPAnimationTools);
	project_tools = memnew(JustAMCPProjectTools);
	profiling_tools = memnew(JustAMCPProfilingTools);
	export_tools = memnew(JustAMCPExportTools);
	spatial_tools = memnew(JustAMCPSpatialTools);
	runtime_tools = memnew(JustAMCPRuntimeTools);
	batch_tools = memnew(JustAMCPBatchTools);
	documentation_tools = memnew(JustAMCPDocumentationTools);
	script_tools = memnew(JustAMCPScriptTools);
	node_tools = memnew(JustAMCPNodeTools);
	audio_tools = memnew(JustAMCPAudioTools);
	blueprint_tools = memnew(JustAMCPBlueprintTools);
	input_tools = memnew(JustAMCPInputTools);
	particle_tools = memnew(JustAMCPParticleTools);
	physics_tools = memnew(JustAMCPPhysicsTools);
	asset_tools = memnew(JustAMCPAssetTools);
	draw_tools = memnew(JustAMCPDrawTools);
	environment_tools = memnew(JustAMCPEnvironmentTools);
	scene_3d_tools = memnew(JustAMCPScene3DTools);
	shader_tools = memnew(JustAMCPShaderTools);
	theme_tools = memnew(JustAMCPThemeTools);
	tilemap_tools = memnew(JustAMCPTileMapTools);

#ifdef MODULE_AUTOWORK_ENABLED
	autowork_tools = memnew(JustAMCPAutoworkTools);
#endif
#ifdef MODULE_MULTIUSER_EDITOR_ENABLED
	multiuser_tools = memnew(JustAMCPMultiuserTools);
#endif

	if (editor_plugin) {
		set_editor_plugin(editor_plugin);
	}
}

void JustAMCPToolExecutor::register_tool_settings() {
	get_tool_schemas(true);
}

Array JustAMCPToolExecutor::get_tool_schemas(bool p_register_only, bool p_ignore_settings) {
	Array tools;
	String current_category = "";
	bool is_core = false;

	auto add_schema = [&](const String &p_name, const String &p_desc, const Vector<String> &p_props, const Vector<String> &p_req) {
		String full_name = "blazium_" + p_name;

		if (p_register_only) {
			if (current_category.is_empty()) {
				// Don't register meta tools as toggles in the UI, they are unconditionally available
				return;
			}
			String cat_path = "blazium/justamcp/tools/" + current_category;
			String tool_path = "blazium/justamcp/tools/" + current_category + "/" + full_name;

			GLOBAL_DEF_BASIC(PropertyInfo(Variant::BOOL, cat_path), is_core);
			GLOBAL_DEF_BASIC(PropertyInfo(Variant::BOOL, tool_path), true);

#ifdef TOOLS_ENABLED
			if (EditorSettings::get_singleton()) {
				EDITOR_DEF_BASIC(cat_path, is_core);
				EDITOR_DEF_BASIC(tool_path, true);
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, cat_path));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, tool_path));
			}
#endif
			return;
		}

		if (!current_category.is_empty() && !p_ignore_settings) {
			bool cat_enabled = true;
			bool tool_enabled = true;

			bool use_project_override = false;
			if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/justamcp/override_editor_settings")) {
				use_project_override = GLOBAL_GET("blazium/justamcp/override_editor_settings");
			}

			if (use_project_override || !EditorSettings::get_singleton()) {
				if (ProjectSettings::get_singleton()) {
					if (ProjectSettings::get_singleton()->has_setting("blazium/justamcp/tools/" + current_category)) {
						cat_enabled = GLOBAL_GET("blazium/justamcp/tools/" + current_category);
					}
					if (ProjectSettings::get_singleton()->has_setting("blazium/justamcp/tools/" + current_category + "/" + full_name)) {
						tool_enabled = GLOBAL_GET("blazium/justamcp/tools/" + current_category + "/" + full_name);
					}
				}
			} else {
				if (EditorSettings::get_singleton()->has_setting("blazium/justamcp/tools/" + current_category)) {
					cat_enabled = EDITOR_GET("blazium/justamcp/tools/" + current_category);
				}
				if (EditorSettings::get_singleton()->has_setting("blazium/justamcp/tools/" + current_category + "/" + full_name)) {
					tool_enabled = EDITOR_GET("blazium/justamcp/tools/" + current_category + "/" + full_name);
				}
			}

			if (!cat_enabled || !tool_enabled) {
				return;
			}
		}

		Dictionary t;
		t["name"] = full_name;
		t["description"] = p_desc;
		Dictionary schema;
		schema["type"] = "object";
		Dictionary props;

		if (!p_props.is_empty()) {
			for (int i = 0; i < p_props.size(); i += 2) {
				Dictionary p;
				p["type"] = p_props[i + 1];
				if (p_props[i + 1] == "any") {
					p["type"] = "string";
				} else if (p_props[i + 1] == "object") {
					p["properties"] = Dictionary();
				} else if (p_props[i + 1] == "array") {
					Dictionary items_dict;
					items_dict["type"] = "object";
					items_dict["properties"] = Dictionary();
					p["items"] = items_dict;
				}
				props[p_props[i]] = p;
			}
		}

		schema["properties"] = props;

		Array req;
		for (int i = 0; i < p_req.size(); i++) {
			req.push_back(p_req[i]);
		}
		if (!req.is_empty()) {
			schema["required"] = req;
		}
		t["inputSchema"] = schema;
		tools.push_back(t);
	};

	// Meta Tools
	add_schema("search_tools", "Searches the engine native capabilities for a specific tool name matching your needs.",
			Vector<String>{ "query", "string" }, Vector<String>{ "query" });
	add_schema("execute_tool", "Dynamically bypasses context omission to execute ANY tool in the engine by name.",
			Vector<String>{ "tool_name", "string", "arguments", "string" }, Vector<String>{ "tool_name", "arguments" });
	add_schema("get_guide", "Lists or reads built-in JustAMCP workflow guides mirrored from godot-mcp resources.",
			Vector<String>{ "topic", "string", "slug", "string" }, Vector<String>{});

	// Editor Tools
	current_category = "editor_tools";
	is_core = false;
	add_schema("editor_play_scene", "Runs the currently active or specified scene.",
			Vector<String>{ "scene_path", "string" }, Vector<String>{});
	add_schema("editor_play_main", "Runs the project's main scene.",
			Vector<String>{}, Vector<String>{});
	add_schema("editor_stop_play", "Terminates an active play session.",
			Vector<String>{}, Vector<String>{});
	add_schema("editor_is_playing", "Checks if a play session is currently active natively.",
			Vector<String>{}, Vector<String>{});
	add_schema("editor_select_node", "Changes the user's active selection in the Scene Tree dock.",
			Vector<String>{ "node_paths", "array" }, Vector<String>{ "node_paths" });
	add_schema("editor_get_selected", "Retrieves the currently selected nodes.",
			Vector<String>{}, Vector<String>{});
	add_schema("editor_undo", "Triggers Editor global undo action.",
			Vector<String>{}, Vector<String>{});
	add_schema("editor_redo", "Triggers Editor global redo action.",
			Vector<String>{}, Vector<String>{});
	add_schema("editor_take_screenshot", "Captures the editor or runtime viewport.",
			Vector<String>{}, Vector<String>{});
	add_schema("editor_set_main_screen", "Switches between 2D, 3D, Script, and AssetLib views.",
			Vector<String>{ "screen_name", "string" }, Vector<String>{ "screen_name" });
	add_schema("editor_open_scene", "Invokes the editor natively to swap active tabs opening a given `.tscn` file onto the viewport.",
			Vector<String>{ "path", "string" }, Vector<String>{ "path" });
	add_schema("editor_get_settings", "Inspects current user-level settings configurations applied in EditorSettings directly.",
			Vector<String>{ "setting", "string" }, Vector<String>{ "setting" });
	add_schema("editor_set_settings", "Manipulates current user-level Editor configurations dynamically applying layout changes instantly.",
			Vector<String>{ "setting", "string", "value", "any" }, Vector<String>{ "setting", "value" });
	add_schema("editor_clear_output", "Clears the editor output panel through the native command palette.",
			Vector<String>{}, Vector<String>{});
	add_schema("editor_screenshot_game", "Captures the visible game display to a PNG file.",
			Vector<String>{}, Vector<String>{});
	add_schema("editor_get_output_log", "Returns recent output lines captured by the JustAMCP engine log hook.",
			Vector<String>{ "limit", "number" }, Vector<String>{});
	add_schema("editor_get_errors", "Returns recent error and warning lines captured by the JustAMCP engine log hook.",
			Vector<String>{ "limit", "number" }, Vector<String>{});
	add_schema("logs_read", "Reads recent JustAMCP/editor log lines with optional filtering.",
			Vector<String>{ "limit", "number", "since_index", "number", "source", "string" }, Vector<String>{});
	add_schema("editor_reload_project", "Requests an editor restart to reload the project.",
			Vector<String>{ "save", "boolean" }, Vector<String>{});
	add_schema("editor_save_all_scenes", "Saves all open editor scenes.",
			Vector<String>{}, Vector<String>{});
	add_schema("editor_get_signals", "Lists signals for a class or node in the edited scene.",
			Vector<String>{ "class_name", "string", "node_path", "string" }, Vector<String>{});
	add_schema("open_in_godot", "Opens a script or scene path in the Blazium editor.",
			Vector<String>{ "path", "string", "line", "number" }, Vector<String>{ "path" });
	add_schema("rescan_filesystem", "Triggers the editor filesystem to rescan project files.",
			Vector<String>{}, Vector<String>{});
	add_schema("scene_tree_dump", "Dumps the currently edited scene tree with node names, types, scripts, and paths.",
			Vector<String>{}, Vector<String>{});

	// Documentation Tools
	current_category = "documentation_tools";
	is_core = true;
	add_schema("docs_list_classes", "Lists internally registered Blazium class documentation summaries from the editor documentation database.",
			Vector<String>{ "query", "string", "limit", "number" }, Vector<String>{});
	add_schema("docs_search", "Searches internal Blazium documentation across classes, methods, properties, signals, and constants.",
			Vector<String>{ "query", "string", "member_type", "string", "include_members", "boolean", "limit", "number" }, Vector<String>{ "query" });
	add_schema("docs_get_class", "Reads full internal documentation for a Blazium class, optionally including all members.",
			Vector<String>{ "class_name", "string", "include_members", "boolean" }, Vector<String>{ "class_name" });
	add_schema("docs_get_member", "Reads internal documentation for a specific class member such as a method, property, signal, constant, enum, or annotation.",
			Vector<String>{ "class_name", "string", "member_type", "string", "member_name", "string" }, Vector<String>{ "class_name", "member_type", "member_name" });
	add_schema("classdb_query", "Queries ClassDB for properties, methods, signals, constants, and inheritance for a class.",
			Vector<String>{ "class_name", "string", "query", "string", "include_virtual", "boolean" }, Vector<String>{ "class_name" });

	// Networking Tools
	current_category = "networking_tools";
	is_core = false;
	add_schema("networking_create_http_request", "Spawns and configures an HTTPRequest node.",
			Vector<String>{ "parent_path", "string", "name", "string", "timeout", "number" }, Vector<String>{ "parent_path" });
	add_schema("networking_setup_websocket", "Configures WebSocket connection boilerplate.",
			Vector<String>{ "parent_path", "string", "mode", "string", "name", "string" }, Vector<String>{ "parent_path" });
	add_schema("networking_setup_multiplayer", "Instantiates a MultiplayerManager structure.",
			Vector<String>{ "parent_path", "string", "transport", "string", "mode", "string", "address", "string", "port", "number", "max_clients", "number" }, Vector<String>{ "parent_path" });
	add_schema("networking_setup_rpc", "Configures RPC (Remote Procedure Call) capabilities onto nodes appending execution mappings natively.",
			Vector<String>{ "node_path", "string", "method_name", "string", "rpc_mode", "string", "transfer_mode", "string", "call_local", "boolean", "channel", "number" }, Vector<String>{ "node_path", "method_name" });
	add_schema("networking_setup_sync", "Instantiates a MultiplayerSynchronizer evaluating a properties array to replicate values efficiently.",
			Vector<String>{ "parent_path", "string", "name", "string", "properties", "array" }, Vector<String>{ "parent_path", "properties" });
	add_schema("networking_get_info", "Returns current multiplayer peer and connection information.",
			Vector<String>{}, Vector<String>{});

#ifdef MODULE_MULTIUSER_EDITOR_ENABLED
	// Multiuser Collaborative Teams Orchestration Tools
	current_category = "multiuser_tools";
	is_core = false;
	add_schema("multiuser_get_status", "Returns the active collaborative session status, including the local peer ID and connection state natively out of the Multiplayer network node.",
			Vector<String>{}, Vector<String>{});
	add_schema("multiuser_send_chat", "Dispatches a global text string natively to all uniquely connected remote peer instances across the Editor environment.",
			Vector<String>{ "message", "string" }, Vector<String>{ "message" });
	add_schema("multiuser_kick_peer", "Removes a target user peer explicitly from the host environment by ID (Action is silently ignored if the AI host agent lacks active networking privileges).",
			Vector<String>{ "peer_id", "string" }, Vector<String>{ "peer_id" });
	add_schema("multiuser_trigger_autowork", "Issues a distributed RPC signaling all securely connected peers to automatically boot Godot's Autowork pipeline evaluating unit tests concurrently traversing the network.",
			Vector<String>{}, Vector<String>{});
#endif

	// Spatial Tools
	current_category = "spatial_tools";
	is_core = false;
	add_schema("spatial_analyze_layout", "Analyzes node density and spacing in a target area.",
			Vector<String>{ "node_path", "string", "include_2d", "boolean", "include_3d", "boolean" }, Vector<String>{});
	add_schema("spatial_suggest_placement", "Calculates empty coordinates suitable for placing nodes.",
			Vector<String>{ "node_type", "string", "context", "string", "parent_path", "string" }, Vector<String>{ "parent_path" });
	add_schema("spatial_detect_overlaps", "Identifies intersecting 3d nodes based on a distance threshold.",
			Vector<String>{ "node_path", "string", "threshold", "number" }, Vector<String>{});
	add_schema("spatial_measure_distance", "Calculates precise pathing distances between nodes.",
			Vector<String>{ "from_path", "string", "to_path", "string" }, Vector<String>{ "from_path", "to_path" });
	add_schema("spatial_bake_navigation", "Asynchronously forces an Editor NavRegion to bake its internal navigation polygons or meshes.",
			Vector<String>{ "node_path", "string", "on_thread", "boolean" }, Vector<String>{ "node_path" });
	add_schema("navigation_set_layers", "Reconfigures navigational layers routing specific bitmask mapping directly over active NavRegions accurately.",
			Vector<String>{ "node_path", "string", "layers", "number" }, Vector<String>{ "node_path", "layers" });
	add_schema("navigation_get_info", "Lists navigation regions and agents below a scene node.",
			Vector<String>{ "node_path", "string" }, Vector<String>{});

	// Runtime Tools
	current_category = "runtime_tools";
	is_core = false;
	add_schema("execute_gdscript_snippet", "Dynamically evaluates raw GDScript source strings overriding static execution bypassing compiled states natively mapping memory contexts locally.",
			Vector<String>{ "code", "string", "target_node", "string" }, Vector<String>{ "code" });
	add_schema("signal_emit", "Fires Godot Event bindings internally passing variable parameter objects mapping signal arguments natively across local instances.",
			Vector<String>{ "node_path", "string", "signal_name", "string", "args", "array" }, Vector<String>{ "node_path", "signal_name" });
	add_schema("runtime_capture_output", "Grabs standard native string output streams logged during active execution natively exposing debug logs rapidly.",
			Vector<String>{}, Vector<String>{});
	add_schema("runtime_compare_screenshots", "Evaluates frame buffer pixel matches defining explicit rendering consistencies comparing layout tests natively validating UI shifts.",
			Vector<String>{}, Vector<String>{});
	add_schema("runtime_record_video", "Initializes MP4 rendering capturing frame by frame runtime layouts dynamically tracing rendering tests natively.",
			Vector<String>{}, Vector<String>{});
	add_schema("take_game_screenshot", "Captures the running game's viewport as a PNG base64 blob natively from the active viewport.",
			Vector<String>{}, Vector<String>{});
	add_schema("runtime_info", "Snapshots active engine state including FPS, current scene, and frame counters.",
			Vector<String>{}, Vector<String>{});
	add_schema("runtime_get_errors", "Polls the engine's internal error and warning logs incrementally.",
			Vector<String>{ "since_index", "number" }, Vector<String>{});
	add_schema("runtime_capabilities", "Lists all blazium_ commands currently exposed by this MCP session.",
			Vector<String>{}, Vector<String>{});
	add_schema("eval_expression", "Evaluates a sandboxed GDScript expression for rapid diagnostic querying.",
			Vector<String>{ "expr", "string" }, Vector<String>{ "expr" });
	add_schema("runtime_quit", "Forcibly terminates the running game process synchronously.",
			Vector<String>{}, Vector<String>{});
	add_schema("get_network_info", "Retrieves multiplayer state including peer IDs and server status natively.",
			Vector<String>{}, Vector<String>{});
	add_schema("get_audio_info", "Snapshots the current AudioServer bus layout including volumes and mute states.",
			Vector<String>{}, Vector<String>{});
	add_schema("run_custom_command", "Invokes a user-registered custom command on the JustAMCPRuntime singleton.",
			Vector<String>{ "name", "string", "args", "array" }, Vector<String>{ "name" });
	add_schema("runtime_get_tree", "Returns a serialized runtime scene tree from the JustAMCP runtime singleton.",
			Vector<String>{ "root", "string", "max_depth", "number", "include_properties", "boolean" }, Vector<String>{});
	add_schema("runtime_inspect_node", "Returns runtime node metadata and properties for a specific node path.",
			Vector<String>{ "node", "string", "include_properties", "boolean" }, Vector<String>{ "node" });
	add_schema("runtime_run_gut_tests", "Returns native guidance for running GUT tests from the editor or command line.",
			Vector<String>{ "test_script", "string" }, Vector<String>{});
	add_schema("runtime_get_test_results", "Returns the currently available JustAMCP runtime test result snapshot.",
			Vector<String>{}, Vector<String>{});
	add_schema("wait", "Sleeps server-side for a bounded interval to let runtime/editor state settle.",
			Vector<String>{ "ms", "number", "seconds", "number" }, Vector<String>{});
	add_schema("get_runtime_status", "Returns editor play state and runtime bridge availability.",
			Vector<String>{}, Vector<String>{});
	add_schema("get_runtime_log", "Returns recent JustAMCP runtime/editor log lines.",
			Vector<String>{ "limit", "number" }, Vector<String>{});
	add_schema("query_runtime_node", "Reads a live runtime node through JustAMCPRuntime when the bridge is active.",
			Vector<String>{ "node_path", "string", "properties", "array", "include_children", "boolean" }, Vector<String>{ "node_path" });
	add_schema("runtime_get_autoload", "Reads runtime autoload configuration and loaded singleton node state.",
			Vector<String>{ "name", "string" }, Vector<String>{ "name" });
	add_schema("runtime_find_nodes_by_script", "Finds live runtime nodes whose script path or global class matches.",
			Vector<String>{ "script_path", "string", "class_name", "string", "limit", "number" }, Vector<String>{});
	add_schema("runtime_batch_get_properties", "Reads multiple properties from multiple live runtime nodes.",
			Vector<String>{ "node_paths", "array", "properties", "array" }, Vector<String>{ "node_paths", "properties" });
	add_schema("runtime_find_ui_elements", "Finds visible Control nodes by text and/or class.",
			Vector<String>{ "text", "string", "ui_type", "string", "visible_only", "boolean", "limit", "number" }, Vector<String>{});
	add_schema("runtime_click_button_by_text", "Triggers a live BaseButton whose text matches the query.",
			Vector<String>{ "text", "string" }, Vector<String>{ "text" });
	add_schema("runtime_move_node", "Sets a live runtime node's position-like property.",
			Vector<String>{ "node", "string", "position", "object", "property", "string" }, Vector<String>{ "node", "position" });
	add_schema("runtime_monitor_properties", "Snapshots selected live runtime node properties.",
			Vector<String>{ "node", "string", "properties", "array" }, Vector<String>{ "node", "properties" });

	// Scene Tools
	current_category = "scene_tools";
	is_core = true;
	add_schema("create_scene", "Creates a new hierarchy branch root instantiating complex node definitions mapped onto empty templates efficiently initializing standard root dependencies safely.",
			Vector<String>{ "projectPath", "string", "scenePath", "string", "rootNodeType", "string", "properties", "object" }, Vector<String>{ "scenePath" });
	add_schema("scene_create_inherited", "Constructs an explicitly derived scene cloning base structural settings automatically generating inheritance graphs locally bypassing standard empty node creations.",
			Vector<String>{ "projectPath", "string", "baseScenePath", "string", "newScenePath", "string" }, Vector<String>{ "baseScenePath", "newScenePath" });
	add_schema("list_scene_nodes", "Lists the hierarchical node structure of a given scene.",
			Vector<String>{ "scene_path", "string", "depth", "number" }, Vector<String>{ "scene_path" });
	add_schema("get_scene_file_content", "Reads the raw scene file text for inspection.",
			Vector<String>{ "scenePath", "string", "path", "string" }, Vector<String>{});
	add_schema("delete_scene", "Deletes a scene file from the project.",
			Vector<String>{ "scenePath", "string", "path", "string" }, Vector<String>{});
	add_schema("get_scene_exports", "Lists exported/editor-visible properties on nodes in a scene.",
			Vector<String>{ "scenePath", "string", "path", "string" }, Vector<String>{});
	add_schema("scene_get_current", "Returns the currently edited scene root and path.",
			Vector<String>{}, Vector<String>{});
	add_schema("scene_list_open", "Lists open editor scene tabs and the active scene.",
			Vector<String>{}, Vector<String>{});
	add_schema("scene_set_current", "Switches the active editor scene tab by opening a scene path.",
			Vector<String>{ "path", "string", "scenePath", "string" }, Vector<String>{});
	add_schema("scene_reload", "Reloads a scene path or the current edited scene.",
			Vector<String>{ "path", "string", "scenePath", "string" }, Vector<String>{});
	add_schema("scene_duplicate_file", "Copies a scene file to a new project path.",
			Vector<String>{ "source_path", "string", "dest_path", "string" }, Vector<String>{ "source_path", "dest_path" });
	add_schema("scene_close", "Reports native scene-tab close support status.",
			Vector<String>{ "path", "string", "scenePath", "string" }, Vector<String>{});
	add_schema("add_node", "Adds a new node to a scene tree structure.",
			Vector<String>{ "scene_path", "string", "parent_path", "string", "node_type", "string", "node_name", "string", "properties", "object" },
			Vector<String>{ "scene_path", "parent_path", "node_type" });
	add_schema("find_nodes", "Deep searches the scene tree by name, type, and/or group simultaneously.",
			Vector<String>{ "name", "string", "type", "string", "group", "string", "limit", "number" }, Vector<String>{});
	add_schema("get_node_property", "Reads a targeted property from a specific node.",
			Vector<String>{ "node", "string", "property", "string" }, Vector<String>{ "node", "property" });
	add_schema("call_node_method", "Safely invokes a method on a node with arguments.",
			Vector<String>{ "node", "string", "method", "string", "args", "array" }, Vector<String>{ "node", "method" });
	add_schema("wait_for_property", "Polls until a node property matches an expected value or confirms the current state.",
			Vector<String>{ "node", "string", "property", "string", "value", "any" }, Vector<String>{ "node", "property", "value" });
	add_schema("press_button", "Finds a BaseButton-derived node by name and forcibly triggers its pressed signal.",
			Vector<String>{ "name", "string" }, Vector<String>{ "name" });
	add_schema("delete_node", "Removes a specific node from a scene hierarchy.",
			Vector<String>{ "scene_path", "string", "node_path", "string" }, Vector<String>{ "scene_path", "node_path" });
	add_schema("duplicate_node", "Duplicates an existing node within a scene.",
			Vector<String>{ "scene_path", "string", "node_path", "string", "new_name", "string" }, Vector<String>{ "scene_path", "node_path" });
	add_schema("reparent_node", "Changes the parent of a specific node within a scene.",
			Vector<String>{ "scene_path", "string", "node_path", "string", "new_parent_path", "string" }, Vector<String>{ "scene_path", "node_path", "new_parent_path" });
	add_schema("set_node_properties", "Modifies the internal properties of a specified node.",
			Vector<String>{ "scene_path", "string", "node_path", "string", "properties", "object" }, Vector<String>{ "scene_path", "node_path", "properties" });
	add_schema("modify_node_property", "Compatibility wrapper for setting one property on a scene node.",
			Vector<String>{ "scene_path", "string", "node_path", "string", "property", "string", "value", "any" }, Vector<String>{ "node_path", "property", "value" });
	add_schema("get_node_properties", "Reads local serialized properties excluding Godot defaults directly resolving node attributes.",
			Vector<String>{ "node_path", "string", "include_defaults", "boolean" }, Vector<String>{ "node_path" });
	add_schema("create_area_2d", "Creates an Area2D wrapper explicitly configured with standard scene routing.",
			Vector<String>{ "node_name", "string", "parent_node_path", "string", "properties", "object" }, Vector<String>{ "node_name" });
	add_schema("create_line_2d", "Instantiates a Line2D vector series translating simple JS coordinate arrays smoothly.",
			Vector<String>{ "node_name", "string", "parent_node_path", "string", "points", "array", "properties", "object" }, Vector<String>{ "node_name" });
	add_schema("create_polygon_2d", "Shapes a geometric Polygon2D boundary instantiating properly across the parent region context.",
			Vector<String>{ "node_name", "string", "parent_node_path", "string", "points", "array", "properties", "object" }, Vector<String>{ "node_name" });
	add_schema("create_csg_shape", "Generates CSG primitives (CSGBox3D, CSGSphere3D) translating explicit dimensional inputs into 3D vectors intelligently.",
			Vector<String>{ "node_name", "string", "parent_node_path", "string", "shape_type", "string", "width", "number", "height", "number", "depth", "number", "radius", "number", "properties", "object" }, Vector<String>{ "node_name" });
	add_schema("instance_scene", "Spawns an active nested Hierarchy loading an external `.tscn` packed resource.",
			Vector<String>{ "projectPath", "string", "scenePath", "string", "instanceScenePath", "string", "parentNodePath", "string", "nodeName", "string", "properties", "object" }, Vector<String>{ "instanceScenePath" });
	add_schema("setup_camera_2d", "Generates Camera2D structure setting zoom ratios or smoothing filters directly.",
			Vector<String>{ "projectPath", "string", "scenePath", "string", "parentNodePath", "string", "nodeName", "string", "zoom", "number", "smoothing", "boolean", "properties", "object" }, Vector<String>{});
	add_schema("setup_parallax_2d", "Initializes empty ParallaxBackground arrays managing nested tracking.",
			Vector<String>{ "projectPath", "string", "scenePath", "string", "parentNodePath", "string", "nodeName", "string", "properties", "object" }, Vector<String>{});
	add_schema("create_multimesh", "Binds a MultiMeshInstance3D array enabling batch optimizations natively.",
			Vector<String>{ "projectPath", "string", "scenePath", "string", "parentNodePath", "string", "nodeName", "string", "properties", "object" }, Vector<String>{});
	add_schema("setup_skeleton", "Rigs a unified Skeleton3D core attaching animated skeletal bones systematically.",
			Vector<String>{ "projectPath", "string", "scenePath", "string", "parentNodePath", "string", "nodeName", "string", "properties", "object" }, Vector<String>{});
	add_schema("setup_occlusion", "Builds OccluderInstance3D buffers mitigating excess renders strictly mapping defined properties.",
			Vector<String>{ "projectPath", "string", "scenePath", "string", "parentNodePath", "string", "nodeName", "string", "properties", "object" }, Vector<String>{});
	add_schema("load_sprite", "Loads and sets a texture onto a Sprite node.",
			Vector<String>{ "scene_path", "string", "node_path", "string", "texture_path", "string" }, Vector<String>{ "scene_path", "node_path", "texture_path" });
	add_schema("save_scene", "Saves the current state of a scene to disk.",
			Vector<String>{ "scene_path", "string" }, Vector<String>{ "scene_path" });
	add_schema("connect_signal", "Connects a Godot signal dynamically.",
			Vector<String>{ "scene_path", "string", "node_path", "string", "signal_name", "string", "target_path", "string", "method_name", "string" },
			Vector<String>{ "scene_path", "node_path", "signal_name", "target_path", "method_name" });
	add_schema("disconnect_signal", "Disconnects an existing signal binding.",
			Vector<String>{ "scene_path", "string", "node_path", "string", "signal_name", "string", "target_path", "string", "method_name", "string" },
			Vector<String>{ "scene_path", "node_path", "signal_name", "target_path", "method_name" });
	add_schema("list_connections", "Lists active connections of a specified node.",
			Vector<String>{ "scene_path", "string", "node_path", "string" }, Vector<String>{ "scene_path", "node_path" });
	add_schema("list_node_signals", "Lists signals declared on a node in a scene.",
			Vector<String>{ "scenePath", "string", "nodePath", "string" }, Vector<String>{ "scenePath" });
	add_schema("has_signal_connection", "Checks if a specific signal connection exists in a scene.",
			Vector<String>{ "scenePath", "string", "sourceNodePath", "string", "signalName", "string", "targetNodePath", "string", "methodName", "string" }, Vector<String>{ "scenePath", "sourceNodePath", "signalName", "targetNodePath", "methodName" });
	add_schema("set_mesh", "Loads a Mesh resource and assigns it to a node mesh property.",
			Vector<String>{ "node_path", "string", "mesh_path", "string", "property", "string" }, Vector<String>{ "node_path", "mesh_path" });
	add_schema("set_material", "Loads a Material resource and assigns it to a node material property.",
			Vector<String>{ "node_path", "string", "material_path", "string", "property", "string" }, Vector<String>{ "node_path", "material_path" });
	add_schema("set_sprite_texture", "Loads a Texture2D resource and assigns it to a sprite/control texture property.",
			Vector<String>{ "node_path", "string", "texture_path", "string", "property", "string" }, Vector<String>{ "node_path", "texture_path" });
	add_schema("set_collision_shape", "Loads a Shape resource and assigns it to a CollisionShape node shape property.",
			Vector<String>{ "node_path", "string", "shape_path", "string", "property", "string" }, Vector<String>{ "node_path", "shape_path" });
	add_schema("set_resource_property", "Sets a resource or raw value on a node property.",
			Vector<String>{ "node_path", "string", "property", "string", "resource_path", "string", "value", "any" }, Vector<String>{ "node_path", "property" });
	add_schema("save_resource_to_file", "Saves a node resource property to a .tres/.res file.",
			Vector<String>{ "node_path", "string", "property", "string", "save_path", "string" }, Vector<String>{ "node_path", "property", "save_path" });

	// Resource Tools
	current_category = "resource_tools";
	is_core = false;
	add_schema("create_resource", "Creates a generic Godot resource.",
			Vector<String>{ "resource_path", "string", "resource_type", "string" }, Vector<String>{ "resource_path", "resource_type" });
	add_schema("modify_resource", "Modifies an existing resource.",
			Vector<String>{ "resource_path", "string", "properties", "object" }, Vector<String>{ "resource_path", "properties" });
	add_schema("read_resource_file", "Reads a resource file as text or returns basic resource metadata.",
			Vector<String>{ "path", "string", "resource_path", "string" }, Vector<String>{});
	add_schema("edit_resource_file", "Edits a resource file by replacing text content or setting resource properties.",
			Vector<String>{ "path", "string", "resource_path", "string", "content", "string", "properties", "object" }, Vector<String>{});
	add_schema("get_resource_preview", "Returns lightweight preview metadata for a resource.",
			Vector<String>{ "path", "string", "resource_path", "string" }, Vector<String>{});
	add_schema("list_resource_files", "Recursively lists resource files with optional class filtering.",
			Vector<String>{ "path", "string", "type_filter", "string" }, Vector<String>{});
	add_schema("save_resource_as", "Saves an existing resource to a new project path.",
			Vector<String>{ "resource_path", "string", "source_path", "string", "dest_path", "string", "save_path", "string" }, Vector<String>{});
	add_schema("get_resource_dependencies", "Lists ResourceLoader dependencies for a resource path.",
			Vector<String>{ "path", "string" }, Vector<String>{ "path" });
	add_schema("import_asset_copy", "Copies an external asset into the project and refreshes the filesystem.",
			Vector<String>{ "source_path", "string", "dest_path", "string" }, Vector<String>{ "source_path", "dest_path" });
	add_schema("manage_resource_autoloads", "Lists, adds, or removes project autoload entries through resource-style arguments.",
			Vector<String>{ "action", "string", "operation", "string", "name", "string", "path", "string" }, Vector<String>{});
	add_schema("create_material", "Creates a material resource natively.",
			Vector<String>{ "resource_path", "string", "material_type", "string", "properties", "object" }, Vector<String>{ "resource_path", "material_type" });
	add_schema("create_shader_template", "Creates a shader file from code or one of the resource shader templates.",
			Vector<String>{ "shaderPath", "string", "shaderType", "string", "code", "string", "template", "string" }, Vector<String>{ "shaderPath" });
	add_schema("create_tileset", "Creates a standard tileset natively.",
			Vector<String>{ "resource_path", "string" }, Vector<String>{ "resource_path" });
	add_schema("set_tilemap_cells", "Modifies bulk tilemap data.",
			Vector<String>{ "scene_path", "string", "node_path", "string", "cells", "array" }, Vector<String>{ "scene_path", "node_path", "cells" });
	add_schema("set_theme_resource_color", "Configures theme colors natively.",
			Vector<String>{ "resource_path", "string", "theme_type", "string", "color_name", "string", "color", "object" }, Vector<String>{ "resource_path", "color_name" });
	add_schema("set_theme_resource_font_size", "Configures theme font sizes natively.",
			Vector<String>{ "resource_path", "string", "theme_type", "string", "font_size_name", "string", "size", "number" }, Vector<String>{ "resource_path", "font_size_name" });
	add_schema("apply_theme_shader", "Applies shaders to UI theme elements natively.",
			Vector<String>{ "resource_path", "string", "shader_path", "string" }, Vector<String>{ "resource_path", "shader_path" });
	add_schema("resource_import_asset", "Triggers the EditorFileSystem to reimport a resource asynchronously.",
			Vector<String>{ "path", "string" }, Vector<String>{ "path" });
	add_schema("get_resource_info", "Returns basic information about a resource class or resource file.",
			Vector<String>{ "type", "string", "path", "string", "resource_path", "string" }, Vector<String>{});

	// Animation Tools
	current_category = "animation_tools";
	is_core = false;
	add_schema("create_animation", "Creates an animation data resource natively.",
			Vector<String>{ "resource_path", "string", "animation_name", "string", "length", "number" }, Vector<String>{ "resource_path", "animation_name" });
	add_schema("set_animation_keyframe", "Sets or creates a value-track keyframe on an AnimationPlayer animation.",
			Vector<String>{ "scenePath", "string", "playerNodePath", "string", "animationName", "string", "nodePath", "string", "property", "string", "time", "number", "value", "any" }, Vector<String>{ "scenePath", "animationName", "nodePath", "property" });
	add_schema("get_animation_info", "Lists AnimationPlayer animations and track metadata.",
			Vector<String>{ "scenePath", "string", "playerNodePath", "string", "animationName", "string" }, Vector<String>{ "scenePath" });
	add_schema("remove_animation", "Removes an animation from an AnimationPlayer's default library.",
			Vector<String>{ "scenePath", "string", "playerNodePath", "string", "animationName", "string" }, Vector<String>{ "scenePath", "animationName" });
	add_schema("list_animations", "Lists animation names and metadata from an AnimationPlayer.",
			Vector<String>{ "scenePath", "string", "playerNodePath", "string" }, Vector<String>{ "scenePath" });
	add_schema("add_animation_track", "Injects animation tracks into an existing track layout.",
			Vector<String>{ "resource_path", "string", "animation_name", "string", "track_type", "string", "node_path", "string" }, Vector<String>{ "resource_path", "animation_name", "track_type", "node_path" });
	add_schema("create_animation_tree", "Creates an animation tree container.",
			Vector<String>{ "scene_path", "string", "parent_path", "string", "tree_name", "string" }, Vector<String>{ "scene_path", "parent_path" });
	add_schema("get_animation_tree_structure", "Reads an AnimationTree state machine or blend tree structure.",
			Vector<String>{ "scenePath", "string", "animTreePath", "string" }, Vector<String>{ "scenePath", "animTreePath" });
	add_schema("add_animation_state", "Injects structural states to node graphs natively.",
			Vector<String>{ "resource_path", "string", "state_name", "string", "animation_name", "string" }, Vector<String>{ "resource_path", "state_name" });
	add_schema("remove_animation_state", "Removes a state from an AnimationTree state machine.",
			Vector<String>{ "scenePath", "string", "animTreePath", "string", "stateName", "string", "stateMachinePath", "string" }, Vector<String>{ "scenePath", "animTreePath", "stateName" });
	add_schema("connect_animation_states", "Binds state machine states together via transitions.",
			Vector<String>{ "resource_path", "string", "from_state", "string", "to_state", "string" }, Vector<String>{ "resource_path", "from_state", "to_state" });
	add_schema("remove_animation_transition", "Removes an AnimationTree state machine transition by index or endpoint states.",
			Vector<String>{ "scenePath", "string", "animTreePath", "string", "fromState", "string", "toState", "string", "transitionIndex", "number" }, Vector<String>{ "scenePath", "animTreePath" });
	add_schema("set_animation_tree_parameter", "Sets an AnimationTree runtime parameter value in a scene resource.",
			Vector<String>{ "scenePath", "string", "animTreePath", "string", "parameter", "string", "value", "any" }, Vector<String>{ "scenePath", "animTreePath", "parameter" });
	add_schema("set_blend_tree_node", "Adds or replaces an AnimationNodeAnimation inside an AnimationNodeBlendTree.",
			Vector<String>{ "scenePath", "string", "animTreePath", "string", "blendTreePath", "string", "nodeName", "string", "animationName", "string", "position", "object" }, Vector<String>{ "scenePath", "animTreePath", "nodeName" });
	add_schema("create_navigation_region", "Injects a navigation region structurally natively.",
			Vector<String>{ "scene_path", "string", "parent_path", "string", "region_name", "string" }, Vector<String>{ "scene_path", "parent_path" });
	add_schema("create_navigation_agent", "Injects a navigation agent node natively.",
			Vector<String>{ "scene_path", "string", "parent_path", "string", "agent_name", "string" }, Vector<String>{ "scene_path", "parent_path" });

	// Project Tools
	current_category = "project_tools";
	is_core = true;
	add_schema("get_project_info", "Returns metadata about the project, renderer, viewport, and autoloads.", Vector<String>{}, Vector<String>{});
	add_schema("get_filesystem_tree", "Returns a recursive file tree with an option to filter by extension.",
			Vector<String>{ "path", "string", "filter", "string", "max_depth", "number" }, Vector<String>{});
	add_schema("search_files", "Searches the file system with a glob or fuzzy query.",
			Vector<String>{ "query", "string", "path", "string", "file_type", "string", "max_results", "number" }, Vector<String>{ "query" });
	add_schema("search_in_files", "Searches file contents using string matching or regex.",
			Vector<String>{ "query", "string", "path", "string", "max_results", "number", "regex", "boolean", "file_type", "string" }, Vector<String>{ "query" });
	add_schema("set_project_setting", "Sets a specific global setting in project.godot.",
			Vector<String>{ "key", "string", "value", "string" }, Vector<String>{ "key", "value" });
	add_schema("uid_to_project_path", "Resolves a Godot UID representation to an absolute res:// path.",
			Vector<String>{ "uid", "string" }, Vector<String>{ "uid" });
	add_schema("project_path_to_uid", "Converts a godot absolute res:// path to its UUID equivalent.",
			Vector<String>{ "path", "string" }, Vector<String>{ "path" });
	add_schema("add_autoload", "Injects a singleton scene or script as an autoload in project scope.",
			Vector<String>{ "name", "string", "path", "string" }, Vector<String>{ "name", "path" });
	add_schema("remove_autoload", "Detaches a singleton autoload from project scope.",
			Vector<String>{ "name", "string" }, Vector<String>{ "name" });
	add_schema("project_get_input_actions", "Retrieves the engine InputMap event bindings natively.",
			Vector<String>{}, Vector<String>{});
	add_schema("project_set_input_action", "Binds a custom InputMap action to an event mapping.",
			Vector<String>{ "action", "string", "events", "string" }, Vector<String>{ "action", "events" });
	add_schema("project_remove_input_action", "Erases an InputMap action definition from project map.",
			Vector<String>{ "action", "string" }, Vector<String>{ "action" });
	add_schema("project_run", "Runs the project or a scene with optional save-all before launch.",
			Vector<String>{ "scene_path", "string", "main", "boolean", "autosave", "boolean" }, Vector<String>{});
	add_schema("inject_drag", "Executes a rapid mouse drag-and-drop event from start to end coordinates.",
			Vector<String>{ "from", "array", "to", "array" }, Vector<String>{ "from", "to" });
	add_schema("inject_scroll", "Triggers a viewport scroll event at specified coordinates with a given delta.",
			Vector<String>{ "x", "number", "y", "number", "delta", "number" }, Vector<String>{ "x", "y" });
	add_schema("inject_gesture", "Injects high-level touch gestures like pinch or swipe into the engine.",
			Vector<String>{ "type", "string", "params", "object" }, Vector<String>{ "type" });
	add_schema("inject_gamepad", "Simulates joypad events including button presses and axis motions.",
			Vector<String>{ "device", "number", "type", "string", "index", "number", "value", "number", "pressed", "boolean" }, Vector<String>{ "type" });

	// Profiling Tools
	current_category = "profiling_tools";
	is_core = false;
	add_schema("get_performance_monitors", "Retrieves all performance monitors related to memory, FPS, navigation, rendering.",
			Vector<String>{ "category", "string" }, Vector<String>{});
	add_schema("get_editor_performance", "Retrieves a compact structural overview of the performance footprint of the godot editor process.",
			Vector<String>{}, Vector<String>{});
	add_schema("profiling_detect_bottlenecks", "Analyzes current Performance counters for common bottlenecks.",
			Vector<String>{}, Vector<String>{});
	add_schema("profiling_monitor", "Compares current Performance counters against caller-provided thresholds.",
			Vector<String>{ "fps_min", "number", "frame_time_max_ms", "number", "memory_max_mb", "number", "draw_calls_max", "number" }, Vector<String>{});

	// Export Tools
	current_category = "export_tools";
	is_core = false;
	add_schema("list_export_presets", "Reads and returns all export presets from export_presets.cfg.",
			Vector<String>{}, Vector<String>{});
	add_schema("export_project", "Triggers a headless Godot export operation.",
			Vector<String>{ "preset_index", "number", "preset_name", "string", "debug", "boolean" }, Vector<String>{});
	add_schema("get_export_info", "Returns metadata regarding absolute template directions and binary configurations.",
			Vector<String>{}, Vector<String>{});
	add_schema("list_android_devices", "Lists Android devices visible to adb.",
			Vector<String>{}, Vector<String>{});
	add_schema("get_android_preset_info", "Reads Android export preset metadata.",
			Vector<String>{ "preset_name", "string", "preset_index", "number" }, Vector<String>{});
	add_schema("deploy_to_android", "Exports, installs, and optionally launches an Android build through adb.",
			Vector<String>{ "preset_name", "string", "preset_index", "number", "device_serial", "string", "debug", "boolean", "skip_export", "boolean", "launch", "boolean" }, Vector<String>{});

	// Batch Tools
	current_category = "batch_tools";
	is_core = false;
	add_schema("find_nodes_by_type", "Recursively scans a scene hierarchy looking for class type matches.",
			Vector<String>{ "type", "string", "recursive", "boolean" }, Vector<String>{ "type" });
	add_schema("find_signal_connections", "Collects connections and signal maps spanning a particular node criteria.",
			Vector<String>{ "signal_name", "string", "node_path", "string" }, Vector<String>{});
	add_schema("batch_set_property", "Finds nodes of a class and assigns a batch property mutation dynamically.",
			Vector<String>{ "type", "string", "property", "string", "value", "any" }, Vector<String>{ "type", "property", "value" });
	add_schema("batch_add_nodes", "Adds multiple nodes to the currently edited scene in one call.",
			Vector<String>{ "nodes", "array" }, Vector<String>{ "nodes" });
	add_schema("batch_execute", "Executes multiple JustAMCP tools sequentially with optional undo on failure.",
			Vector<String>{ "steps", "array", "stop_on_error", "boolean", "undo_on_error", "boolean" }, Vector<String>{ "steps" });
	add_schema("find_node_references", "Searches file system recursively for reference nodes by string pattern.",
			Vector<String>{ "pattern", "string" }, Vector<String>{ "pattern" });
	add_schema("cross_scene_set_property", "Modifies properties identically across ALL scene files matching parameters saving changes into file system.",
			Vector<String>{ "type", "string", "property", "string", "value", "any", "path_filter", "string", "exclude_addons", "boolean" }, Vector<String>{ "type", "property", "value" });
	add_schema("get_scene_dependencies", "Interrogates internal Godot dependencies structure string paths via ResourceLoader.",
			Vector<String>{ "path", "string" }, Vector<String>{ "path" });

	// Script Tools
	current_category = "script_tools";
	is_core = false;
	add_schema("list_scripts", "Locates .gd, .cs, .gdshader scripts within the project hierarchy.",
			Vector<String>{ "path", "string", "recursive", "boolean" }, Vector<String>{});
	add_schema("read_script", "Fetches source text directly from a godot script file.",
			Vector<String>{ "path", "string" }, Vector<String>{ "path" });
	add_schema("create_script", "Writes a script file providing a default extending template out of the box.",
			Vector<String>{ "path", "string", "content", "string", "extends", "string", "class_name", "string" }, Vector<String>{ "path" });
	add_schema("edit_script", "Modifies an existing script intelligently mapping regex replacements or direct injection.",
			Vector<String>{ "path", "string", "content", "string", "insert_at_line", "number", "text", "string", "replacements", "array" }, Vector<String>{ "path" });
	add_schema("attach_script", "Binds a target Godot Resource Script onto a Scene Node dynamically.",
			Vector<String>{ "node_path", "string", "script_path", "string" }, Vector<String>{ "node_path", "script_path" });
	add_schema("delete_script", "Deletes a script file from the project.",
			Vector<String>{ "path", "string" }, Vector<String>{ "path" });
	add_schema("detach_script", "Removes a script from a node in the currently edited scene.",
			Vector<String>{ "node_path", "string" }, Vector<String>{ "node_path" });
	add_schema("get_open_scripts", "Maps what files are actively opened within Godot's script editor GUI.",
			Vector<String>{}, Vector<String>{});
	add_schema("open_script_in_editor", "Opens a script in the editor script workspace.",
			Vector<String>{ "path", "string", "line", "number" }, Vector<String>{ "path" });
	add_schema("get_script_errors", "Returns lightweight script validation error information.",
			Vector<String>{ "path", "string" }, Vector<String>{ "path" });
	add_schema("search_in_scripts", "Searches project script files for a literal string.",
			Vector<String>{ "path", "string", "pattern", "string" }, Vector<String>{ "pattern" });
	add_schema("find_script_symbols", "Extracts classes, functions, variables, signals, constants, and enum symbols from scripts.",
			Vector<String>{ "path", "string" }, Vector<String>{});
	add_schema("patch_script", "Patches a script by replacing or inserting around an anchor.",
			Vector<String>{ "path", "string", "anchor", "string", "search", "string", "replacement", "string", "replace", "string", "insert_before", "string", "insert_after", "string" }, Vector<String>{ "path" });
	add_schema("validate_script", "Compiles a GDScript implicitly returning if valid or syntax errors mapped out.",
			Vector<String>{ "path", "string" }, Vector<String>{ "path" });

	// Node Tools
	current_category = "node_tools";
	is_core = false;
	add_schema("node_add", "Spawns a godot node natively onto a target structural anchor.",
			Vector<String>{ "type", "string", "parent_path", "string", "name", "string", "properties", "object" }, Vector<String>{ "type" });
	add_schema("node_delete", "Removes a godot node structural anchor.",
			Vector<String>{ "node_path", "string" }, Vector<String>{ "node_path" });
	add_schema("node_duplicate", "Clones a node struct into the scene graph dynamically.",
			Vector<String>{ "node_path", "string", "name", "string" }, Vector<String>{ "node_path" });
	add_schema("node_move", "Alters structural ownership hierarchy within the Scene Tree.",
			Vector<String>{ "node_path", "string", "new_parent_path", "string" }, Vector<String>{ "node_path", "new_parent_path" });
	add_schema("node_update_property", "Explicitly manages individual Variant properties assigned onto a node.",
			Vector<String>{ "node_path", "string", "property", "string", "value", "any" }, Vector<String>{ "node_path", "property", "value" });
	add_schema("node_get_properties", "Interrogates internal parameters within Node bindings structurally.",
			Vector<String>{ "node_path", "string", "category", "string" }, Vector<String>{ "node_path" });
	add_schema("node_add_resource", "Embeds a godot Resource directly assigned directly onto a node structure natively.",
			Vector<String>{ "node_path", "string", "property", "string", "resource_type", "string", "resource_properties", "object" }, Vector<String>{ "node_path", "property", "resource_type" });
	add_schema("node_set_anchor_preset", "Enables UI layout modifications bound towards native godot PRESET flags.",
			Vector<String>{ "node_path", "string", "preset", "string", "keep_offsets", "boolean" }, Vector<String>{ "node_path", "preset" });
	add_schema("node_rename", "Edits node semantic names directly within godot structures.",
			Vector<String>{ "node_path", "string", "new_name", "string" }, Vector<String>{ "node_path", "new_name" });
	add_schema("node_connect_signal", "Binds callable events between source nodes bounding into target instances natively.",
			Vector<String>{ "source_path", "string", "signal_name", "string", "target_path", "string", "method_name", "string" }, Vector<String>{ "source_path", "signal_name", "target_path", "method_name" });
	add_schema("node_disconnect_signal", "Snaps off existing bounding callable events recursively mapped over nodes.",
			Vector<String>{ "source_path", "string", "signal_name", "string", "target_path", "string", "method_name", "string" }, Vector<String>{ "source_path", "signal_name", "target_path", "method_name" });
	add_schema("node_get_groups", "Aggregates internal Godot Groups strings assigned over standard instances.",
			Vector<String>{ "node_path", "string" }, Vector<String>{ "node_path" });
	add_schema("node_set_groups", "Mutates overlapping assignment instances mapped per godot string node groups.",
			Vector<String>{ "node_path", "string", "groups", "array" }, Vector<String>{ "node_path", "groups" });
	add_schema("node_find_in_group", "Provides lookup access globally towards Godot instance sets natively spanning specific groups.",
			Vector<String>{ "group", "string" }, Vector<String>{ "group" });

	// Audio Tools
	current_category = "audio_tools";
	is_core = false;
	add_schema("get_audio_bus_layout", "Returns native godot AudioServer state containing all available buses and their effects.",
			Vector<String>{}, Vector<String>{});
	add_schema("add_audio_bus", "Binds a new structural audio bus dynamically into the godot runtime.",
			Vector<String>{ "name", "string", "at_position", "number", "volume_db", "number", "send", "string", "solo", "boolean", "mute", "boolean" }, Vector<String>{ "name" });
	add_schema("set_audio_bus", "Modifies specific index Godot audio buses layout.",
			Vector<String>{ "name", "string", "volume_db", "number", "solo", "boolean", "mute", "boolean", "bypass_effects", "boolean", "send", "string", "rename", "string" }, Vector<String>{ "name" });
	add_schema("add_audio_bus_effect", "Maps Godot AudioEffects via reflection bridging directly onto buses.",
			Vector<String>{ "bus", "string", "effect_type", "string", "params", "object", "at_position", "number" }, Vector<String>{ "bus", "effect_type" });
	add_schema("add_audio_player", "Injects structural AudioStreamPlayer instances directly mapped inside Godot trees.",
			Vector<String>{ "node_path", "string", "name", "string", "type", "string", "stream", "string", "volume_db", "number", "bus", "string", "autoplay", "boolean", "max_distance", "number", "attenuation", "number", "attenuation_model", "number", "unit_size", "number" }, Vector<String>{ "node_path", "name" });
	add_schema("audio_get_players_info", "Returns hierarchical node maps wrapping natively instantiated stream wrappers.",
			Vector<String>{ "node_path", "string" }, Vector<String>{ "node_path" });

	// Input Tools
	current_category = "input_tools";
	is_core = false;
	add_schema("simulate_key", "Mocks native godot InputEventKey structs sent across IPC towards godot debug runtimes.",
			Vector<String>{ "keycode", "string", "pressed", "boolean", "shift", "boolean", "ctrl", "boolean", "alt", "boolean" }, Vector<String>{ "keycode" });
	add_schema("simulate_mouse_click", "Mocks native godot InputEventMouseButton bounding structurally across OS IPC streams.",
			Vector<String>{ "button", "number", "pressed", "boolean", "double_click", "boolean", "auto_release", "boolean", "x", "number", "y", "number" }, Vector<String>{});
	add_schema("simulate_mouse_move", "Mocks native godot InputEventMouseMotion translating structural frames over OS IPC streams.",
			Vector<String>{ "x", "number", "y", "number", "relative_x", "number", "relative_y", "number", "button_mask", "number", "unhandled", "boolean" }, Vector<String>{});
	add_schema("simulate_action", "Triggers artificial input action payloads universally mimicking defined map shortcuts across states natively.",
			Vector<String>{ "action", "string", "pressed", "boolean", "strength", "number" }, Vector<String>{ "action" });
	add_schema("simulate_touch", "Mocks screen touch interactions passing accurate index, position, and tap evaluation vectors cleanly bypassing OS screens.",
			Vector<String>{ "action", "string", "position", "object", "x", "number", "y", "number", "pressed", "boolean", "double_tap", "boolean", "index", "number" }, Vector<String>{});
	add_schema("simulate_gamepad", "Fakes joypad signals pushing button index/pressure or axis overrides for active game controller states.",
			Vector<String>{ "device", "number", "button_index", "number", "button", "number", "axis", "number", "axis_value", "number", "pressed", "boolean", "pressure", "number" }, Vector<String>{});
	add_schema("simulate_sequence", "Chains an array of defined inputs mapping deterministic simulation timings dynamically.",
			Vector<String>{ "events", "array", "frame_delay", "number" }, Vector<String>{ "events" });
	add_schema("input_record", "Start listening over input sequence frame loops saving complex vectors natively capturing user simulations optimally.",
			Vector<String>{ "state", "boolean" }, Vector<String>{ "state" });
	add_schema("input_replay", "Streams input buffer traces driving Godot inputs mapped deterministically accurately replaying test routines safely.",
			Vector<String>{ "sequence_buffer_id", "string" }, Vector<String>{ "sequence_buffer_id" });

	// Particle Tools
	current_category = "particle_tools";
	is_core = false;
	add_schema("create_particles", "Instantiates a new particle emitter structure onto a godot node.",
			Vector<String>{ "parent_path", "string", "name", "string", "is_3d", "boolean", "amount", "number", "lifetime", "number", "one_shot", "boolean", "explosiveness", "number", "randomness", "number", "emitting", "boolean" }, Vector<String>{ "parent_path" });
	add_schema("set_particle_material", "Modifies inner Godot struct material values bounding Godot Variant types for particles.",
			Vector<String>{ "node_path", "string", "direction", "any", "spread", "number", "initial_velocity_min", "number", "initial_velocity_max", "number", "gravity", "any", "scale_min", "number", "scale_max", "number", "color", "string", "emission_shape", "string", "emission_sphere_radius", "number", "emission_box_extents", "any", "emission_ring_radius", "number", "emission_ring_inner_radius", "number", "emission_ring_height", "number" }, Vector<String>{ "node_path" });
	add_schema("set_particle_color_gradient", "Maps linear stop structs against color configurations into a proper GradientTexture1D resource natively.",
			Vector<String>{ "node_path", "string", "stops", "array" }, Vector<String>{ "node_path", "stops" });
	add_schema("apply_particle_preset", "Wraps multiple set calls interpolating variables representing default high quality VFX presets.",
			Vector<String>{ "node_path", "string", "preset", "string" }, Vector<String>{ "node_path", "preset" });
	add_schema("get_particle_info", "Interrogates godot nodes returning hierarchical configuration state back to the MCP stream.",
			Vector<String>{ "node_path", "string" }, Vector<String>{ "node_path" });

	// Physics Tools
	current_category = "physics_tools";
	is_core = false;
	add_schema("setup_collision", "Spawns and attaches native 2D/3D collision boundaries onto physical rigid bodies dynamically.",
			Vector<String>{ "node_path", "string", "shape", "string", "dimension", "string", "width", "number", "height", "number", "depth", "number", "radius", "number", "disabled", "boolean", "one_way_collision", "boolean" }, Vector<String>{ "node_path", "shape" });
	add_schema("set_physics_layers", "Controls implicit bitmask configurations assigning node interaction overlap flags natively.",
			Vector<String>{ "node_path", "string", "layer", "number", "mask", "number" }, Vector<String>{ "node_path" });
	add_schema("get_physics_layers", "Reads intrinsic properties parsing bitmask configurations exposing layers visually.",
			Vector<String>{ "node_path", "string" }, Vector<String>{ "node_path" });
	add_schema("add_raycast", "Instantiates native Godot RayCast queries pointing into logical coordinate targets asynchronously bounds.",
			Vector<String>{ "parent_path", "string", "name", "string", "dimension", "string", "target_position", "object", "enabled", "boolean", "collision_mask", "number" }, Vector<String>{ "parent_path" });
	add_schema("setup_physics_body", "Allocates pure native Object implementations defining Physics bodies bounds (Area, Character, Rigid).",
			Vector<String>{ "parent_path", "string", "body_type", "string", "name", "string", "dimension", "string", "collision_layer", "number", "collision_mask", "number" }, Vector<String>{ "parent_path", "body_type" });
	add_schema("get_collision_info", "Walks a sub-hierarchy scraping Godot shape configurations for runtime representations.",
			Vector<String>{ "node_path", "string" }, Vector<String>{ "node_path" });

	// Scene3D Tools
	current_category = "scene3d_tools";
	is_core = false;
	add_schema("add_mesh_instance", "Mints and injects a MeshInstance3D primitive directly into Godot Scene tree natively.",
			Vector<String>{ "parent_path", "string", "name", "string", "mesh_type", "string", "mesh_file", "string", "mesh_properties", "object", "position", "any", "rotation", "any", "scale", "any" }, Vector<String>{ "parent_path" });
	add_schema("setup_lighting", "Configures high performance Godot lights (SpotLight3D, OmniLight3D, DirectionalLight3D) dynamically into the tree.",
			Vector<String>{ "parent_path", "string", "name", "string", "light_type", "string", "preset", "string", "color", "any", "energy", "number", "shadows", "boolean", "range", "number", "attenuation", "number", "spot_angle", "number", "spot_angle_attenuation", "number", "position", "any", "rotation", "any" }, Vector<String>{ "parent_path" });
	add_schema("set_material_3d", "Assigns and computes real-time StandardMaterial3D configurations over Godot primitive and loaded meshes.",
			Vector<String>{ "node_path", "string", "surface_index", "number", "albedo_color", "any", "albedo_texture", "string", "metallic", "number", "roughness", "number", "metallic_texture", "string", "roughness_texture", "string", "normal_texture", "string", "emission", "any", "emission_color", "any", "emission_energy", "number", "emission_texture", "string", "transparency", "any", "cull_mode", "any" }, Vector<String>{ "node_path" });
	add_schema("setup_environment", "Allocates rendering environments over Godot bindings (SSAO, SSR, SDFGI, Glow, Fog).",
			Vector<String>{ "parent_path", "string", "name", "string", "node_path", "string", "background_mode", "string", "background_color", "any", "sky", "object", "ambient_light_color", "any", "ambient_light_energy", "number", "ambient_light_source", "any", "tonemap_mode", "any", "tonemap_exposure", "number", "tonemap_white", "number", "fog_enabled", "boolean", "fog_light_color", "any", "fog_density", "number", "fog_light_energy", "number", "glow_enabled", "boolean", "glow_intensity", "number", "glow_strength", "number", "glow_bloom", "number", "ssao_enabled", "boolean", "ssao_radius", "number", "ssao_intensity", "number", "ssr_enabled", "boolean", "ssr_max_steps", "number", "ssr_fade_in", "number", "ssr_fade_out", "number", "sdfgi_enabled", "boolean" }, Vector<String>{ "parent_path" });
	add_schema("setup_camera_3d", "Allocates viewport Camera3D projection mapping native structural properties inside godot runtime.",
			Vector<String>{ "parent_path", "string", "name", "string", "node_path", "string", "projection", "string", "fov", "number", "size", "number", "near", "number", "far", "number", "cull_mask", "number", "current", "boolean", "position", "any", "rotation", "any", "look_at", "any", "environment_path", "string" }, Vector<String>{ "parent_path" });
	add_schema("add_gridmap", "Mints and instantiates Godot high performance GridMap bounding memory grids locally.",
			Vector<String>{ "parent_path", "string", "name", "string", "node_path", "string", "mesh_library_path", "string", "cell_size", "any", "position", "any", "cells", "array" }, Vector<String>{ "parent_path" });

	// Shader Tools
	current_category = "shader_tools";
	is_core = false;
	add_schema("create_shader", "Mints blank Godot Shader files injecting standard structure for canvas_item, script, and spatial types.",
			Vector<String>{ "path", "string", "content", "string", "shader_type", "string" }, Vector<String>{ "path" });
	add_schema("read_shader", "Loads string source buffers straight out from .gdshader files dynamically mapped in file system.",
			Vector<String>{ "path", "string" }, Vector<String>{ "path" });
	add_schema("edit_shader", "Provides file patching replacing code strings directly inside the Shader Resource loading path.",
			Vector<String>{ "path", "string", "content", "string", "replacements", "array" }, Vector<String>{ "path" });
	add_schema("assign_shader_material", "Mints native generic ShaderMaterial instances binding active .gdshader onto target nodes.",
			Vector<String>{ "node_path", "string", "shader_path", "string" }, Vector<String>{ "node_path", "shader_path" });
	add_schema("set_shader_param", "Reflective native evaluation bounding GDScript variable evaluation parsing shader uniforms natively.",
			Vector<String>{ "node_path", "string", "param", "string", "value", "any" }, Vector<String>{ "node_path", "param" });
	add_schema("get_shader_params", "Extracts Godot shader variables directly from Inspector metadata representations natively.",
			Vector<String>{ "node_path", "string" }, Vector<String>{ "node_path" });

	// Theme Tools
	current_category = "theme_tools";
	is_core = false;
	add_schema("create_theme", "Generates a new Theme resource struct caching visual configuration layouts natively.",
			Vector<String>{ "path", "string", "default_font_size", "number" }, Vector<String>{ "path" });
	add_schema("set_control_theme_color", "Injects property mappings explicitly setting color maps inside control nodes.",
			Vector<String>{ "node_path", "string", "name", "string", "color", "string", "theme_type", "string" }, Vector<String>{ "node_path", "name", "color" });
	add_schema("set_theme_constant", "Maps godot uniform parameter spacings assigning values inside active nodes.",
			Vector<String>{ "node_path", "string", "name", "string", "value", "number" }, Vector<String>{ "node_path", "name" });
	add_schema("set_control_theme_font_size", "Overrides localized godot font sizes overriding global default assignments.",
			Vector<String>{ "node_path", "string", "name", "string", "size", "number" }, Vector<String>{ "node_path", "name" });
	add_schema("set_theme_stylebox", "Constructs standard StyleBoxFlat representations drawing boundaries over Control structures natively.",
			Vector<String>{ "node_path", "string", "name", "string", "bg_color", "string", "border_color", "string", "border_width", "number", "corner_radius", "number", "padding", "number" }, Vector<String>{ "node_path", "name" });
	add_schema("setup_control", "Automates standard Godot UI control flag assignments applying layout layouts.",
			Vector<String>{ "node_path", "string", "anchor_preset", "string", "min_size", "string", "size_flags_h", "string", "size_flags_v", "string", "margins", "object", "separation", "number", "grow_h", "string", "grow_v", "string" }, Vector<String>{ "node_path" });
	add_schema("get_theme_info", "Interrogates godot Inspector bindings wrapping metadata and dynamic lists natively.",
			Vector<String>{ "node_path", "string" }, Vector<String>{ "node_path" });

	// TileMap Tools
	current_category = "tilemap_tools";
	is_core = false;
	add_schema("tilemap_set_cell", "Sets physical cell identifiers onto Grid memory iterations inside TileMapLayers natively.",
			Vector<String>{ "node_path", "string", "x", "number", "y", "number", "source_id", "number", "atlas_x", "number", "atlas_y", "number", "alternative", "number" }, Vector<String>{ "node_path" });
	add_schema("tilemap_fill_rect", "Calculates ranges iterating assignment coordinates spanning bounds uniformly for TileMapLayers natively.",
			Vector<String>{ "node_path", "string", "x1", "number", "y1", "number", "x2", "number", "y2", "number", "source_id", "number", "atlas_x", "number", "atlas_y", "number", "alternative", "number" }, Vector<String>{ "node_path" });
	add_schema("tilemap_get_cell", "Queries active TileSet state identifiers parsing out structural metadata from TileMapLayers natively.",
			Vector<String>{ "node_path", "string", "x", "number", "y", "number" }, Vector<String>{ "node_path" });
	add_schema("tilemap_clear", "Flushes runtime cell structs flushing grids locally for TileMapLayers natively.",
			Vector<String>{ "node_path", "string" }, Vector<String>{ "node_path" });
	add_schema("tilemap_get_info", "Reads memory maps dumping resource state representations across TileSets dynamically natively.",
			Vector<String>{ "node_path", "string" }, Vector<String>{ "node_path" });
	add_schema("tilemap_get_used_cells", "Calculates bounds pulling physical active grid locations via vector pointers across TileMapLayers natively.",
			Vector<String>{ "node_path", "string", "max_count", "number" }, Vector<String>{ "node_path" });

	// Project Tools
	current_category = "project_tools";
	is_core = false;
	add_schema("project_map_project", "Crawls the physical file system mapping GdScript inheritance, signal connectors, and resource preloads structural graphs natively.",
			Vector<String>{ "root", "string", "include_addons", "boolean", "lod", "number" }, Vector<String>{});
	add_schema("project_map_scenes", "Parses .tscn scene tree representations identifying node types and resource dependencies across scene graphs natively.",
			Vector<String>{ "root", "string", "include_addons", "boolean" }, Vector<String>{});
	add_schema("project_list_settings", "Queries active Godot ProjectSettings dumping categorized key-value pairs with serialization logic.",
			Vector<String>{ "category", "string" }, Vector<String>{});
	add_schema("project_update_settings", "Persists localized Godot ProjectSettings mapping dynamic dictionaries onto global configuration buffers recursively.",
			Vector<String>{ "settings", "object" }, Vector<String>{ "settings" });
	add_schema("project_manage_autoloads", "Automates Godot Autoload registrations minting or deleting global project singletons dynamically.",
			Vector<String>{ "operation", "string", "name", "string", "path", "string" }, Vector<String>{ "operation" });
	add_schema("project_get_collision_layers", "Dumps named Godot physics layers extracting user-defined metadata from ProjectSettings configuration natively.",
			Vector<String>{}, Vector<String>{});
	// Asset Tools
	current_category = "asset_tools";
	is_core = false;
	add_schema("asset_generate_2d_asset", "Renders raw SVG code directly into Godot Image instances saving PNG assets to disk and scanning filesystem repositories natively.",
			Vector<String>{ "svg_code", "string", "filename", "string", "save_path", "string", "scale", "number" }, Vector<String>{ "svg_code", "filename" });

	// Blueprint Tools
	current_category = "blueprint_tools";
	is_core = false;
	add_schema("blueprint_create_particle_preset", "Applies high-level ParticleProcessMaterial blueprints (fire, smoke, explosion, etc.) including automated Gradient and QuadMesh setup.",
			Vector<String>{ "path", "string", "preset", "string", "is_3d", "boolean" }, Vector<String>{ "path", "preset" });
	add_schema("blueprint_create_material_preset", "Applies curated StandardMaterial3D blueprints (metal, glass, emissive, etc.) to target MeshInstance or Material nodes.",
			Vector<String>{ "path", "string", "preset", "string" }, Vector<String>{ "path", "preset" });
	add_schema("blueprint_setup_camera_preset", "Configures Camera2D or Camera3D nodes for specific gameplay paradigms (top-down, platformer, cinematic, action).",
			Vector<String>{ "path", "string", "preset", "string" }, Vector<String>{ "path", "preset" });

	// Draw Tools
	current_category = "draw_tools";
	is_core = false;
	add_schema("control_draw_recipe", "Stores and executes ordered CanvasItem draw operations on target Control nodes utilizing an embedded dynamic script natively.",
			Vector<String>{ "path", "string", "ops", "array", "clear_existing", "boolean" }, Vector<String>{ "path", "ops" });

	// Environment Tools
	current_category = "environment_tools";
	is_core = false;
	add_schema("environment_create", "Creates a complex Environment (+ optional Sky + ProceduralSkyMaterial) chain and assigns it to a WorldEnvironment node via presets.",
			Vector<String>{ "path", "string", "preset", "string", "sky", "boolean" }, Vector<String>{ "path" });

	// Analysis Tools
	current_category = "analysis_tools";
	is_core = false;
	add_schema("find_unused_resources", "Traverses physical file system mapping orphaned godot assets unused by .tscn scenes automatically.",
			Vector<String>{ "path", "string", "include_addons", "boolean" }, Vector<String>{});
	add_schema("analyze_signal_flow", "Dumps all complex recursive callable/signal graphs connecting dynamically natively.",
			Vector<String>{}, Vector<String>{});
	add_schema("analyze_scene_complexity", "Counts bounds estimating tree node/logic allocation and depths.",
			Vector<String>{ "path", "string" }, Vector<String>{});
	add_schema("find_script_references", "Executes strict text boundary parses across source representations verifying usage paths.",
			Vector<String>{ "query", "string", "path", "string", "include_addons", "boolean" }, Vector<String>{ "query" });
	add_schema("detect_circular_dependencies", "Detects DFS recursion cycles detecting deep graph violations dynamically.",
			Vector<String>{ "path", "string", "include_addons", "boolean" }, Vector<String>{});
	add_schema("get_project_statistics", "Calculates physical byte limits checking file distribution and global dependencies.",
			Vector<String>{ "path", "string", "include_addons", "boolean" }, Vector<String>{});
	add_schema("project_state", "Aggregates project settings, filesystem statistics, and current scene status.",
			Vector<String>{ "path", "string", "include_addons", "boolean" }, Vector<String>{});
	add_schema("project_advise", "Returns lightweight native project guidance based on current settings and scene state.",
			Vector<String>{}, Vector<String>{});
	add_schema("runtime_diagnose", "Aggregates runtime status, recent errors, and performance logs for diagnosis.",
			Vector<String>{ "limit", "number" }, Vector<String>{});
	add_schema("scene_validate", "Validates the current scene for common missing scripts, owners, and empty root issues.",
			Vector<String>{}, Vector<String>{});
	add_schema("scene_analyze", "Aggregates scene complexity and scene tree dump information.",
			Vector<String>{}, Vector<String>{});
	add_schema("script_analyze", "Searches scripts and returns script-level diagnostics for a query.",
			Vector<String>{ "query", "string", "path", "string", "include_addons", "boolean" }, Vector<String>{});
	add_schema("project_symbol_search", "Searches project scripts for classes, functions, variables, and signal symbols.",
			Vector<String>{ "query", "string", "path", "string", "include_addons", "boolean" }, Vector<String>{ "query" });
	add_schema("project_index", "Builds a lightweight native project index from mapped scripts, scenes, and statistics.",
			Vector<String>{ "path", "string", "include_addons", "boolean", "lod", "number" }, Vector<String>{});
	add_schema("scene_dependency_graph", "Returns scene dependency information using the existing batch scene dependency analyzer.",
			Vector<String>{ "scene_path", "string", "path", "string" }, Vector<String>{});

#ifdef MODULE_AUTOWORK_ENABLED
	// Autowork Tools
	current_category = "autowork_tools";
	is_core = false;
	add_schema("autowork_run_all_tests", "Recursively traverses and executes all autowork test suites natively returning structured passing/failure statistics.",
			Vector<String>{}, Vector<String>{});
	add_schema("autowork_run_tests_in_directory", "Recursively finds and executes all Godot autowork unit tests inside a given directory, returning formatted results.",
			Vector<String>{ "directory_path", "string" }, Vector<String>{ "directory_path" });
	add_schema("autowork_run_test_script", "Executes an exact test script natively against the runtime test suite framework evaluating state.",
			Vector<String>{ "script_path", "string" }, Vector<String>{ "script_path" });
	add_schema("autowork_run_test_by_name", "Performs regex lookup isolating explicit test pattern function names universally across suites for debugging single logic instances.",
			Vector<String>{ "test_name", "string" }, Vector<String>{ "test_name" });
#endif

	return tools;
}

Dictionary JustAMCPToolExecutor::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	Dictionary result;

	if (!scene_tools || !resource_tools || !animation_tools || !project_tools || !profiling_tools || !export_tools || !batch_tools || !script_tools || !node_tools || !audio_tools || !blueprint_tools || !input_tools || !particle_tools || !physics_tools || !scene_3d_tools || !shader_tools || !theme_tools || !tilemap_tools || !analysis_tools || !asset_tools || !draw_tools || !environment_tools) {
		result["ok"] = false;
		result["error"] = "Tools not initialized";
		return result;
	}

	String internal_name = p_tool_name;
	if (internal_name.begins_with("blazium_")) {
		internal_name = internal_name.substr(8);
	}
	String full_name = "blazium_" + internal_name;

	// Runtime Tool Payload Validation
	Array schemas = get_tool_schemas(false, false);
	Dictionary target_schema;
	bool tool_found = false;
	for (int i = 0; i < schemas.size(); i++) {
		Dictionary schema_entry = schemas[i];
		if (String(schema_entry["name"]) == full_name) {
			target_schema = schema_entry;
			tool_found = true;
			break;
		}
	}

	if (!tool_found) {
		Dictionary err;
		err["code"] = -32601;
		err["message"] = "Tool not found or disabled: " + p_tool_name;
		result["ok"] = false;
		result["error"] = err;
		return result;
	}

	if (target_schema.has("inputSchema")) {
		Dictionary inputSchema = target_schema["inputSchema"];
		if (inputSchema.has("required")) {
			Array req = inputSchema["required"];
			for (int i = 0; i < req.size(); i++) {
				String req_arg = req[i];
				if (!p_args.has(req_arg)) {
					Dictionary err;
					err["code"] = -32602;
					err["message"] = "Missing required parameter for tool " + p_tool_name + ": " + req_arg;
					result["ok"] = false;
					result["error"] = err;
					return result;
				}
			}
		}

		if (inputSchema.has("properties")) {
			Dictionary props = inputSchema["properties"];
			Array prop_keys = props.keys();
			for (int i = 0; i < prop_keys.size(); i++) {
				String key = prop_keys[i];
				if (!p_args.has(key)) {
					continue;
				}
				Dictionary prop_def = props[key];
				if (prop_def.has("type")) {
					String exp_type = prop_def["type"];
					Variant val = p_args[key];
					bool valid = true;
					if (exp_type == "string" && val.get_type() != Variant::STRING) {
						valid = false;
					} else if (exp_type == "number" && val.get_type() != Variant::INT && val.get_type() != Variant::FLOAT) {
						valid = false;
					} else if (exp_type == "boolean" && val.get_type() != Variant::BOOL) {
						valid = false;
					} else if (exp_type == "object" && val.get_type() != Variant::DICTIONARY) {
						valid = false;
					} else if (exp_type == "array") {
						if (val.get_type() != Variant::ARRAY && val.get_type() != Variant::PACKED_STRING_ARRAY && val.get_type() != Variant::PACKED_INT32_ARRAY && val.get_type() != Variant::PACKED_INT64_ARRAY && val.get_type() != Variant::PACKED_FLOAT32_ARRAY && val.get_type() != Variant::PACKED_FLOAT64_ARRAY && val.get_type() != Variant::PACKED_BYTE_ARRAY) {
							valid = false;
						}
					}

					if (!valid) {
						Dictionary err;
						err["code"] = -32602;
						err["message"] = "Invalid type for parameter '" + key + "'. Expected " + exp_type + ".";
						result["ok"] = false;
						result["error"] = err;
						return result;
					}
				}
			}
		}
	}

	// Meta Tools
	if (internal_name == "search_tools") {
		String query = p_args.get("query", "");
		Array all_schemas = get_tool_schemas(false, true); // true = bypass ProjectSettings UI filters
		Array matched;
		for (int i = 0; i < all_schemas.size(); i++) {
			Dictionary schema = all_schemas[i];
			String name = schema["name"];
			String desc = schema["description"];
			if (query.is_empty() || name.containsn(query) || desc.containsn(query)) {
				matched.push_back(schema);
			}
		}
		result["ok"] = true;
		result["result"] = matched;
		return result;
	}
	if (internal_name == "execute_tool") {
		String target_tool = p_args.get("tool_name", "");
		Dictionary target_args = p_args.get("arguments", Dictionary());
		return execute_tool(target_tool, target_args);
	}
	if (internal_name == "get_guide") {
		Array topics;
		topics.push_back("testing-loop");
		topics.push_back("scene-editing");
		topics.push_back("asset-generation");
		topics.push_back("troubleshooting");
		topics.push_back("tool-index");
		String topic = p_args.get("topic", p_args.get("slug", ""));
		if (topic.is_empty()) {
			result["ok"] = true;
			result["topics"] = topics;
			result["resource_template"] = "blazium://guide/{topic}";
			return result;
		}
		JustAMCPResourceExecutor resource_executor;
		Dictionary resource = resource_executor.read_resource("blazium://guide/" + topic);
		if (!resource.get("ok", false)) {
			return resource;
		}
		Array contents = resource.get("contents", Array());
		result["ok"] = true;
		result["topic"] = topic;
		result["contents"] = contents;
		if (!contents.is_empty()) {
			Dictionary first = contents[0];
			result["text"] = first.get("text", "");
			result["content"] = result["text"];
			result["mime_type"] = first.get("mimeType", "text/markdown");
		}
		return result;
	}

	// Editor Tool Routes
	if (internal_name == "editor_play_scene") {
		return editor_tools->editor_play_scene(p_args);
	}
	if (internal_name == "editor_play_main") {
		return editor_tools->editor_play_main(p_args);
	}
	if (internal_name == "editor_stop_play") {
		return editor_tools->editor_stop_play(p_args);
	}
	if (internal_name == "editor_is_playing") {
		return editor_tools->editor_is_playing(p_args);
	}
	if (internal_name == "editor_select_node") {
		return editor_tools->editor_select_node(p_args);
	}
	if (internal_name == "editor_get_selected") {
		return editor_tools->editor_get_selected(p_args);
	}
	if (internal_name == "editor_undo") {
		return editor_tools->editor_undo(p_args);
	}
	if (internal_name == "editor_redo") {
		return editor_tools->editor_redo(p_args);
	}
	if (internal_name == "editor_take_screenshot") {
		return editor_tools->editor_take_screenshot(p_args);
	}
	if (internal_name == "editor_set_main_screen") {
		return editor_tools->editor_set_main_screen(p_args);
	}
	if (internal_name == "editor_open_scene") {
		return editor_tools->editor_open_scene(p_args);
	}
	if (internal_name == "editor_get_settings") {
		return editor_tools->editor_get_settings(p_args);
	}
	if (internal_name == "editor_set_settings") {
		return editor_tools->editor_set_settings(p_args);
	}
	if (internal_name == "editor_clear_output") {
		return editor_tools->editor_clear_output(p_args);
	}
	if (internal_name == "editor_screenshot_game") {
		return editor_tools->editor_screenshot_game(p_args);
	}
	if (internal_name == "editor_get_output_log") {
		return editor_tools->editor_get_output_log(p_args);
	}
	if (internal_name == "editor_get_errors") {
		return editor_tools->editor_get_errors(p_args);
	}
	if (internal_name == "logs_read") {
		Dictionary ret;
		Dictionary log_args;
		log_args["limit"] = p_args.get("limit", 200);
		Dictionary logs = editor_tools->editor_get_output_log(log_args);
		Array lines = logs.get("logs", Array());
		int since_index = p_args.get("since_index", 0);
		Array sliced;
		for (int i = MAX(0, since_index); i < lines.size(); i++) {
			sliced.push_back(lines[i]);
		}
		ret["ok"] = true;
		ret["source"] = p_args.get("source", "editor");
		ret["logs"] = sliced;
		ret["count"] = sliced.size();
		ret["next_index"] = lines.size();
		return ret;
	}
	if (internal_name == "editor_reload_project") {
		return editor_tools->editor_reload_project(p_args);
	}
	if (internal_name == "editor_save_all_scenes") {
		return editor_tools->editor_save_all_scenes(p_args);
	}
	if (internal_name == "editor_get_signals") {
		return editor_tools->editor_get_signals(p_args);
	}
	if (internal_name == "open_in_godot") {
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
	if (internal_name == "rescan_filesystem") {
		Dictionary ret;
#ifdef TOOLS_ENABLED
		if (EditorFileSystem::get_singleton()) {
			EditorFileSystem::get_singleton()->scan();
			ret["ok"] = true;
			ret["message"] = "Editor filesystem rescan requested.";
			return ret;
		}
#endif
		ret["ok"] = false;
		ret["error"] = "Editor filesystem unavailable.";
		return ret;
	}
	if (internal_name == "scene_tree_dump") {
#ifdef TOOLS_ENABLED
		return _justamcp_scene_tree_dump();
#else
		result["ok"] = false;
		result["error"] = "Scene tree dump is only available in tools/editor builds.";
		return result;
#endif
	}

	// Networking Tool Routes
	if (internal_name == "networking_create_http_request") {
		return networking_tools->networking_create_http_request(p_args);
	}
	if (internal_name == "networking_setup_websocket") {
		return networking_tools->networking_setup_websocket(p_args);
	}
	if (internal_name == "networking_setup_multiplayer") {
		return networking_tools->networking_setup_multiplayer(p_args);
	}
	if (internal_name == "networking_setup_rpc") {
		return networking_tools->networking_setup_rpc(p_args);
	}
	if (internal_name == "networking_setup_sync") {
		return networking_tools->networking_setup_sync(p_args);
	}
	if (internal_name == "networking_get_info") {
		return networking_tools->networking_get_info(p_args);
	}

	// Spatial Tool Routes
	if (internal_name == "spatial_analyze_layout") {
		return spatial_tools->spatial_analyze_layout(p_args);
	}
	if (internal_name == "spatial_suggest_placement") {
		return spatial_tools->spatial_suggest_placement(p_args);
	}
	if (internal_name == "spatial_detect_overlaps") {
		return spatial_tools->spatial_detect_overlaps(p_args);
	}
	if (internal_name == "spatial_measure_distance") {
		return spatial_tools->spatial_measure_distance(p_args);
	}
	if (internal_name == "spatial_bake_navigation") {
		return spatial_tools->spatial_bake_navigation(p_args);
	}
	if (internal_name == "navigation_set_layers") {
		return spatial_tools->navigation_set_layers(p_args);
	}
	if (internal_name == "navigation_get_info") {
		return spatial_tools->navigation_get_info(p_args);
	}

	// Runtime Tool Routes
	if (internal_name == "execute_gdscript_snippet") {
		return runtime_tools->runtime_execute_gdscript(p_args);
	}
	if (internal_name == "signal_emit") {
		return runtime_tools->runtime_signal_emit(p_args);
	}
	if (internal_name == "runtime_capture_output") {
		return runtime_tools->runtime_capture_output(p_args);
	}
	if (internal_name == "runtime_compare_screenshots") {
		return runtime_tools->runtime_compare_screenshots(p_args);
	}
	if (internal_name == "runtime_record_video") {
		return runtime_tools->runtime_record_video(p_args);
	}

	// Project Tool Routes
	if (internal_name == "project_map_project") {
		return project_tools->execute_tool("map_project", p_args);
	}
	if (internal_name == "project_map_scenes") {
		return project_tools->execute_tool("map_scenes", p_args);
	}
	if (internal_name == "project_list_settings" || internal_name == "get_project_settings") {
		return project_tools->execute_tool("list_settings", p_args);
	}
	if (internal_name == "project_update_settings" || internal_name == "update_project_settings") {
		return project_tools->execute_tool("update_settings", p_args);
	}
	if (internal_name == "project_manage_autoloads") {
		return project_tools->execute_tool("manage_autoloads", p_args);
	}
	if (internal_name == "project_get_collision_layers") {
		return project_tools->execute_tool("get_collision_layers", p_args);
	}
	if (internal_name == "get_input_map") {
		return project_tools->execute_tool("project_get_input_actions", p_args);
	}
	if (internal_name == "configure_input_map") {
		return project_tools->execute_tool("project_set_input_action", p_args);
	}
	if (internal_name == "get_project_info") {
		return project_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "get_filesystem_tree") {
		return project_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "search_files") {
		return project_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "search_in_files") {
		return project_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "set_project_setting") {
		return project_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "uid_to_project_path") {
		return project_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "project_path_to_uid") {
		return project_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "add_autoload") {
		return project_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "remove_autoload") {
		return project_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "project_get_input_actions" || internal_name == "project_set_input_action" || internal_name == "project_remove_input_action") {
		return project_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "project_run") {
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

	// Profiling Tool Routes
	if (internal_name == "get_performance_monitors") {
		return profiling_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "get_editor_performance") {
		return profiling_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "profiling_detect_bottlenecks") {
		return profiling_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "profiling_monitor") {
		return profiling_tools->execute_tool(internal_name, p_args);
	}

	// Export Tool Routes
	if (internal_name == "list_export_presets") {
		return export_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "export_project") {
		return export_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "get_export_info") {
		return export_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "list_android_devices") {
		return export_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "get_android_preset_info") {
		return export_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "deploy_to_android") {
		return export_tools->execute_tool(internal_name, p_args);
	}

	// Resource Tool Routes
	if (internal_name == "create_resource") {
		return resource_tools->create_resource(p_args);
	}
	if (internal_name == "modify_resource") {
		return resource_tools->modify_resource(p_args);
	}
	if (internal_name == "read_resource_file") {
		return resource_tools->read_resource_file(p_args);
	}
	if (internal_name == "edit_resource_file") {
		return resource_tools->edit_resource_file(p_args);
	}
	if (internal_name == "get_resource_preview") {
		return resource_tools->get_resource_preview(p_args);
	}
	if (internal_name == "list_resource_files") {
		return resource_tools->list_resource_files(p_args);
	}
	if (internal_name == "save_resource_as") {
		return resource_tools->save_resource_as(p_args);
	}
	if (internal_name == "get_resource_dependencies") {
		return resource_tools->get_resource_dependencies(p_args);
	}
	if (internal_name == "import_asset_copy") {
		return resource_tools->import_asset_copy(p_args);
	}
	if (internal_name == "manage_resource_autoloads") {
		return resource_tools->manage_resource_autoloads(p_args);
	}
	if (internal_name == "create_material") {
		return resource_tools->create_material(p_args);
	}
	if (internal_name == "create_shader_template") {
		return resource_tools->create_shader(p_args);
	}
	if (internal_name == "create_tileset") {
		return resource_tools->create_tileset(p_args);
	}
	if (internal_name == "set_tilemap_cells") {
		return resource_tools->set_tilemap_cells(p_args);
	}
	if (internal_name == "set_theme_resource_color") {
		return resource_tools->set_theme_color(p_args);
	}
	if (internal_name == "set_theme_resource_font_size") {
		return resource_tools->set_theme_font_size(p_args);
	}
	if (internal_name == "apply_theme_shader") {
		return resource_tools->apply_theme_shader(p_args);
	}
	if (internal_name == "resource_import_asset") {
		return resource_tools->resource_import_asset(p_args);
	}
	if (internal_name == "get_resource_info") {
		return resource_tools->get_resource_info(p_args);
	}
	if (internal_name == "set_mesh") {
		return _justamcp_set_node_resource_property(p_args, "mesh", "mesh_path");
	}
	if (internal_name == "set_material") {
		return _justamcp_set_node_resource_property(p_args, "material_override", "material_path");
	}
	if (internal_name == "set_sprite_texture") {
		return _justamcp_set_node_resource_property(p_args, "texture", "texture_path");
	}
	if (internal_name == "set_collision_shape") {
		return _justamcp_set_node_resource_property(p_args, "shape", "shape_path");
	}
	if (internal_name == "set_resource_property") {
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
	if (internal_name == "save_resource_to_file") {
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

	// Batch Tool Routes
	if (internal_name == "find_nodes_by_type") {
		return batch_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "find_signal_connections") {
		return batch_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "batch_set_property") {
		return batch_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "batch_add_nodes") {
		return batch_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "batch_execute") {
		Array steps = p_args.get("steps", Array());
		bool stop_on_error = p_args.get("stop_on_error", true);
		bool undo_on_error = p_args.get("undo_on_error", false);
		Array results;
		int completed = 0;
		for (int i = 0; i < steps.size(); i++) {
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
		Dictionary ret;
		ret["ok"] = true;
		ret["results"] = results;
		ret["completed"] = completed;
		ret["count"] = results.size();
		return ret;
	}

#ifdef MODULE_AUTOWORK_ENABLED
	// Autowork Tool Routes
	if (internal_name == "autowork_run_all_tests") {
		return autowork_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "autowork_run_tests_in_directory") {
		return autowork_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "autowork_run_test_script") {
		return autowork_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "autowork_run_test_by_name") {
		return autowork_tools->execute_tool(internal_name, p_args);
	}
#endif
	if (internal_name == "find_node_references") {
		return batch_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "cross_scene_set_property") {
		return batch_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "get_scene_dependencies") {
		return batch_tools->execute_tool(internal_name, p_args);
	}

	// Script Tool Routes
	if (internal_name == "list_scripts") {
		return script_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "read_script") {
		return script_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "create_script") {
		return script_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "edit_script") {
		return script_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "delete_script") {
		return script_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "attach_script") {
		return script_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "detach_script") {
		return script_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "get_open_scripts") {
		return script_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "open_script_in_editor") {
		return script_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "get_script_errors") {
		return script_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "search_in_scripts") {
		return script_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "find_script_symbols") {
		return script_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "patch_script") {
		return script_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "validate_script") {
		return script_tools->execute_tool(internal_name, p_args);
	}

	if (internal_name == "wait") {
		int ms = p_args.get("ms", 0);
		if (ms <= 0 && p_args.has("seconds")) {
			ms = int(double(p_args.get("seconds", 0.0)) * 1000.0);
		}
		ms = CLAMP(ms, 0, 30000);
		OS::get_singleton()->delay_usec(uint64_t(ms) * 1000ULL);
		Dictionary ret;
		ret["ok"] = true;
		ret["waited_ms"] = ms;
		return ret;
	}
	if (internal_name == "get_runtime_status") {
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
	if (internal_name == "get_runtime_log") {
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

	// Blazium JustAMCP Runtime Commands (Directed to JustAMCPRuntime)
	if (internal_name == "take_game_screenshot" || internal_name == "runtime_info" ||
			internal_name == "runtime_get_errors" || internal_name == "runtime_capabilities" ||
			internal_name == "eval_expression" || internal_name == "find_nodes" ||
			internal_name == "runtime_get_tree" || internal_name == "runtime_inspect_node" ||
			internal_name == "query_runtime_node" ||
			internal_name == "get_node_property" || internal_name == "call_node_method" ||
			internal_name == "wait_for_property" || internal_name == "press_button" ||
			internal_name == "runtime_get_autoload" || internal_name == "runtime_find_nodes_by_script" ||
			internal_name == "runtime_batch_get_properties" || internal_name == "runtime_find_ui_elements" ||
			internal_name == "runtime_click_button_by_text" || internal_name == "runtime_move_node" ||
			internal_name == "runtime_monitor_properties" ||
			internal_name == "inject_drag" || internal_name == "inject_scroll" ||
			internal_name == "inject_gesture" || internal_name == "runtime_quit" ||
			internal_name == "get_network_info" || internal_name == "get_audio_info" ||
			internal_name == "inject_gamepad" || internal_name == "run_custom_command") {
		if (JustAMCPRuntime::get_singleton()) {
			String cmd = internal_name;
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
			if (internal_name == "runtime_get_tree" && runtime_args.has("max_depth") && !runtime_args.has("depth")) {
				runtime_args["depth"] = runtime_args["max_depth"];
			}
			if (internal_name == "runtime_inspect_node" && runtime_args.has("node") && !runtime_args.has("path")) {
				runtime_args["path"] = runtime_args["node"];
			}
			if (internal_name == "query_runtime_node" && runtime_args.has("node_path") && !runtime_args.has("path")) {
				runtime_args["path"] = runtime_args["node_path"];
			}
			if (internal_name == "runtime_batch_get_properties" && runtime_args.has("node_paths") && !runtime_args.has("nodes")) {
				runtime_args["nodes"] = runtime_args["node_paths"];
			}
			return JustAMCPRuntime::get_singleton()->execute_command(cmd, runtime_args);
		} else {
			Dictionary ret;
			ret["type"] = "error";
			ret["message"] = "JustAMCPRuntime not initialized or enabled. Ensure '--enable-mcp' or feature flags are active.";
			return ret;
		}
	}

	if (internal_name == "runtime_run_gut_tests") {
		Dictionary ret;
		ret["ok"] = true;
		ret["message"] = "Run GUT tests from the editor panel or with Godot command line using addons/gut/gut_cmdln.gd.";
		ret["test_script"] = p_args.get("test_script", "");
		return ret;
	}
	if (internal_name == "runtime_get_test_results") {
		Dictionary ret;
		ret["ok"] = true;
		ret["results"] = Array();
		ret["message"] = "No native GUT result bridge is active. Check editor or command-line GUT output.";
		return ret;
	}

	if (internal_name == "autowork_generate_test") {
		return script_tools->execute_tool(internal_name, p_args);
	}

	// Node Tool Routes
	if (internal_name == "node_add") {
		return node_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "node_delete") {
		return node_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "node_duplicate") {
		return node_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "node_move") {
		return node_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "node_update_property") {
		return node_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "node_get_properties") {
		return node_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "node_add_resource") {
		return node_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "node_set_anchor_preset") {
		return node_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "node_rename") {
		return node_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "node_connect_signal") {
		return node_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "node_disconnect_signal") {
		return node_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "node_get_groups") {
		return node_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "node_set_groups") {
		return node_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "node_find_in_group") {
		return node_tools->execute_tool(internal_name, p_args);
	}

	// Audio Tool Routes
	if (internal_name == "get_audio_bus_layout") {
		return audio_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "add_audio_bus") {
		return audio_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "set_audio_bus") {
		return audio_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "add_audio_bus_effect") {
		return audio_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "add_audio_player") {
		return audio_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "audio_get_players_info") {
		return audio_tools->execute_tool(internal_name, p_args);
	}

	// Input Tool Routes
	if (internal_name == "simulate_key") {
		return input_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "simulate_mouse_click") {
		return input_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "simulate_mouse_move") {
		return input_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "simulate_action") {
		return input_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "simulate_gamepad") {
		return input_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "simulate_sequence") {
		return input_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "input_record") {
		return input_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "input_replay") {
		return input_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "simulate_touch") {
		return input_tools->execute_tool(internal_name, p_args);
	}

	// Documentation Tool Routes
	if (internal_name == "docs_list_classes" || internal_name == "docs_search" ||
			internal_name == "docs_get_class" || internal_name == "docs_get_member") {
		return documentation_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "classdb_query") {
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

	// Particle Tool Routes
	if (internal_name == "create_particles") {
		return particle_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "set_particle_material") {
		return particle_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "set_particle_color_gradient") {
		return particle_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "apply_particle_preset") {
		return particle_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "get_particle_info") {
		return particle_tools->execute_tool(internal_name, p_args);
	}

	// Physics Tool Routes
	if (internal_name == "setup_collision") {
		return physics_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "set_physics_layers") {
		return physics_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "get_physics_layers") {
		return physics_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "add_raycast") {
		return physics_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "setup_physics_body") {
		return physics_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "get_collision_info") {
		return physics_tools->execute_tool(internal_name, p_args);
	}

	// Scene3D Tool Routes
	if (internal_name == "add_mesh_instance") {
		return scene_3d_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "setup_lighting") {
		return scene_3d_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "set_material_3d") {
		return scene_3d_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "setup_environment") {
		return scene_3d_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "setup_camera_3d") {
		return scene_3d_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "add_gridmap") {
		return scene_3d_tools->execute_tool(internal_name, p_args);
	}

	// Shader Tool Routes
	if (internal_name == "create_shader") {
		return shader_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "read_shader") {
		return shader_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "edit_shader") {
		return shader_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "assign_shader_material") {
		return shader_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "set_shader_param") {
		return shader_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "get_shader_params") {
		return shader_tools->execute_tool(internal_name, p_args);
	}

	// Theme Tool Routes
	if (internal_name == "create_theme") {
		return theme_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "set_control_theme_color") {
		return theme_tools->execute_tool("set_theme_color", p_args);
	}
	if (internal_name == "set_theme_constant") {
		return theme_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "set_control_theme_font_size") {
		return theme_tools->execute_tool("set_theme_font_size", p_args);
	}
	if (internal_name == "set_theme_stylebox") {
		return theme_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "setup_control") {
		return theme_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "get_theme_info") {
		return theme_tools->execute_tool(internal_name, p_args);
	}

	// TileMap Tool Routes
	if (internal_name == "tilemap_set_cell") {
		return tilemap_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "tilemap_fill_rect") {
		return tilemap_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "tilemap_get_cell") {
		return tilemap_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "tilemap_clear") {
		return tilemap_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "tilemap_get_info") {
		return tilemap_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "tilemap_get_used_cells") {
		return tilemap_tools->execute_tool(internal_name, p_args);
	}

	// Analysis Tool Routes
	if (internal_name == "find_unused_resources") {
		return analysis_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "analyze_signal_flow") {
		return analysis_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "analyze_scene_complexity") {
		return analysis_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "find_script_references") {
		return analysis_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "detect_circular_dependencies") {
		return analysis_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "get_project_statistics") {
		return analysis_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "project_state") {
		Dictionary ret;
		ret["ok"] = true;
		ret["statistics"] = analysis_tools->execute_tool("get_project_statistics", p_args);
		ret["settings"] = project_tools->execute_tool("list_settings", Dictionary());
		ret["current_scene"] = scene_tools->get_current_scene(Dictionary());
		return ret;
	}
	if (internal_name == "project_advise") {
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
	if (internal_name == "runtime_diagnose") {
		Dictionary ret;
		Dictionary errors_args;
		errors_args["limit"] = p_args.get("limit", 100);
		ret["ok"] = true;
		ret["status"] = execute_tool("blazium_get_runtime_status", Dictionary());
		ret["errors"] = editor_tools->editor_get_errors(errors_args);
		ret["performance"] = profiling_tools->execute_tool("get_performance_monitors", Dictionary());
		return ret;
	}
	if (internal_name == "scene_validate") {
		Dictionary ret;
		Array issues;
#ifdef TOOLS_ENABLED
		Node *root = _justamcp_get_edited_root();
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
	if (internal_name == "scene_analyze") {
		Dictionary ret;
		ret["ok"] = true;
		ret["tree"] = execute_tool("blazium_scene_tree_dump", Dictionary());
		ret["complexity"] = analysis_tools->execute_tool("analyze_scene_complexity", p_args);
		return ret;
	}
	if (internal_name == "script_analyze") {
		Dictionary script_args = p_args;
		if (script_args.has("query") && !script_args.has("pattern")) {
			script_args["pattern"] = script_args["query"];
		}
		return script_tools->execute_tool("search_in_scripts", script_args);
	}
	if (internal_name == "project_symbol_search") {
		Dictionary script_args;
		script_args["query"] = p_args.get("query", "");
		script_args["path"] = p_args.get("path", "res://");
		script_args["include_addons"] = p_args.get("include_addons", false);
		return analysis_tools->execute_tool("find_script_references", script_args);
	}
	if (internal_name == "project_index") {
		Dictionary ret;
		ret["ok"] = true;
		ret["scripts"] = project_tools->execute_tool("map_project", p_args);
		ret["scenes"] = project_tools->execute_tool("map_scenes", p_args);
		ret["statistics"] = analysis_tools->execute_tool("get_project_statistics", p_args);
		return ret;
	}
	if (internal_name == "scene_dependency_graph") {
		Dictionary deps_args;
		deps_args["path"] = p_args.get("scene_path", p_args.get("path", ""));
		return batch_tools->execute_tool("get_scene_dependencies", deps_args);
	}

	// Scene Tool Routes
	if (internal_name == "create_scene") {
		return scene_tools->create_scene(p_args);
	}
	if (internal_name == "scene_create_inherited") {
		return scene_tools->create_inherited_scene(p_args);
	}
	if (internal_name == "list_scene_nodes") {
		return scene_tools->list_scene_nodes(p_args);
	}
	if (internal_name == "get_scene_file_content") {
		return scene_tools->get_scene_file_content(p_args);
	}
	if (internal_name == "delete_scene") {
		return scene_tools->delete_scene_file(p_args);
	}
	if (internal_name == "get_scene_exports") {
		return scene_tools->get_scene_exports(p_args);
	}
	if (internal_name == "scene_get_current") {
		return scene_tools->get_current_scene(p_args);
	}
	if (internal_name == "scene_list_open") {
		return scene_tools->list_open_scenes(p_args);
	}
	if (internal_name == "scene_set_current") {
		return scene_tools->set_current_scene(p_args);
	}
	if (internal_name == "scene_reload") {
		return scene_tools->reload_scene(p_args);
	}
	if (internal_name == "scene_duplicate_file") {
		return scene_tools->duplicate_scene_file(p_args);
	}
	if (internal_name == "scene_close") {
		return scene_tools->close_scene(p_args);
	}
	if (internal_name == "add_node") {
		return scene_tools->add_node(p_args);
	}
	if (internal_name == "delete_node") {
		return scene_tools->delete_node(p_args);
	}
	if (internal_name == "duplicate_node") {
		return scene_tools->duplicate_node(p_args);
	}
	if (internal_name == "reparent_node") {
		return scene_tools->reparent_node(p_args);
	}
	if (internal_name == "modify_node_property") {
		Dictionary wrapper_args = p_args;
		Dictionary properties;
		properties[p_args.get("property", "")] = p_args.get("value", Variant());
		wrapper_args["properties"] = properties;
		return scene_tools->set_node_properties(wrapper_args);
	}
	if (internal_name == "set_node_properties") {
		return scene_tools->set_node_properties(p_args);
	}
	if (internal_name == "get_node_properties") {
		return scene_tools->get_node_properties(p_args);
	}
	if (internal_name == "create_area_2d") {
		return scene_tools->create_area_2d(p_args);
	}
	if (internal_name == "create_line_2d") {
		return scene_tools->create_line_2d(p_args);
	}
	if (internal_name == "create_polygon_2d") {
		return scene_tools->create_polygon_2d(p_args);
	}
	if (internal_name == "create_csg_shape") {
		return scene_tools->create_csg_shape(p_args);
	}
	if (internal_name == "instance_scene") {
		return scene_tools->instance_scene(p_args);
	}
	if (internal_name == "setup_camera_2d") {
		return scene_tools->setup_camera_2d(p_args);
	}
	if (internal_name == "setup_parallax_2d") {
		return scene_tools->setup_parallax_2d(p_args);
	}
	if (internal_name == "create_multimesh") {
		return scene_tools->create_multimesh(p_args);
	}
	if (internal_name == "setup_skeleton") {
		return scene_tools->setup_skeleton(p_args);
	}
	if (internal_name == "setup_occlusion") {
		return scene_tools->setup_occlusion(p_args);
	}
	if (internal_name == "load_sprite") {
		return scene_tools->load_sprite(p_args);
	}
	if (internal_name == "save_scene") {
		return scene_tools->save_scene(p_args);
	}
	if (internal_name == "connect_signal") {
		return scene_tools->connect_signal(p_args);
	}
	if (internal_name == "disconnect_signal") {
		return scene_tools->disconnect_signal(p_args);
	}
	if (internal_name == "list_connections") {
		return scene_tools->list_connections(p_args);
	}
	if (internal_name == "list_node_signals") {
		return scene_tools->list_node_signals(p_args);
	}
	if (internal_name == "has_signal_connection") {
		return scene_tools->has_signal_connection(p_args);
	}

	if (internal_name == "create_resource") {
		return resource_tools->create_resource(p_args);
	}
	if (internal_name == "modify_resource") {
		return resource_tools->modify_resource(p_args);
	}
	if (internal_name == "create_material") {
		return resource_tools->create_material(p_args);
	}
	if (internal_name == "create_shader_template") {
		return resource_tools->create_shader(p_args);
	}
	if (internal_name == "create_tileset") {
		return resource_tools->create_tileset(p_args);
	}
	if (internal_name == "set_tilemap_cells") {
		return resource_tools->set_tilemap_cells(p_args);
	}
	if (internal_name == "set_theme_resource_color") {
		return resource_tools->set_theme_color(p_args);
	}
	if (internal_name == "set_theme_resource_font_size") {
		return resource_tools->set_theme_font_size(p_args);
	}
	if (internal_name == "apply_theme_shader") {
		return resource_tools->apply_theme_shader(p_args);
	}

	if (internal_name == "create_animation") {
		return animation_tools->create_animation(p_args);
	}
	if (internal_name == "set_animation_keyframe") {
		return animation_tools->set_animation_keyframe(p_args);
	}
	if (internal_name == "get_animation_info") {
		return animation_tools->get_animation_info(p_args);
	}
	if (internal_name == "list_animations") {
		return animation_tools->list_animations(p_args);
	}
	if (internal_name == "remove_animation") {
		return animation_tools->remove_animation(p_args);
	}
	if (internal_name == "add_animation_track") {
		return animation_tools->add_animation_track(p_args);
	}
	if (internal_name == "create_animation_tree") {
		return animation_tools->create_animation_tree(p_args);
	}
	if (internal_name == "get_animation_tree_structure") {
		return animation_tools->get_animation_tree_structure(p_args);
	}
	if (internal_name == "add_animation_state") {
		return animation_tools->add_animation_state(p_args);
	}
	if (internal_name == "remove_animation_state") {
		return animation_tools->remove_animation_state(p_args);
	}
	if (internal_name == "connect_animation_states") {
		return animation_tools->connect_animation_states(p_args);
	}
	if (internal_name == "remove_animation_transition") {
		return animation_tools->remove_animation_transition(p_args);
	}
	if (internal_name == "set_animation_tree_parameter") {
		return animation_tools->set_animation_tree_parameter(p_args);
	}
	if (internal_name == "set_blend_tree_node") {
		return animation_tools->set_blend_tree_node(p_args);
	}
	if (internal_name == "create_navigation_region") {
		return animation_tools->create_navigation_region(p_args);
	}
	if (internal_name == "create_navigation_agent") {
		return animation_tools->create_navigation_agent(p_args);
	}

	if (internal_name.begins_with("blueprint_")) {
		return blueprint_tools->execute_tool(internal_name.substr(String("blueprint_").length()), p_args);
	}
	if (internal_name == "control_draw_recipe") {
		return draw_tools->execute_tool(internal_name, p_args);
	}
	if (internal_name == "environment_create") {
		return environment_tools->execute_tool("create_environment", p_args);
	}
	if (internal_name.begins_with("asset_")) {
		return asset_tools->execute_tool(internal_name.substr(String("asset_").length()), p_args);
	}

	if (internal_name.begins_with("editor_")) {
		if (internal_name == "editor_play_scene") {
			return editor_tools->editor_play_scene(p_args);
		}
		if (internal_name == "editor_play_main") {
			return editor_tools->editor_play_main(p_args);
		}
		if (internal_name == "editor_stop_play") {
			return editor_tools->editor_stop_play(p_args);
		}
		if (internal_name == "editor_is_playing") {
			return editor_tools->editor_is_playing(p_args);
		}
		if (internal_name == "editor_select_node") {
			return editor_tools->editor_select_node(p_args);
		}
		if (internal_name == "editor_get_selected") {
			return editor_tools->editor_get_selected(p_args);
		}
		if (internal_name == "editor_undo") {
			return editor_tools->editor_undo(p_args);
		}
		if (internal_name == "editor_redo") {
			return editor_tools->editor_redo(p_args);
		}
		if (internal_name == "editor_take_screenshot") {
			return editor_tools->editor_take_screenshot(p_args);
		}
		if (internal_name == "editor_set_main_screen") {
			return editor_tools->editor_set_main_screen(p_args);
		}
		if (internal_name == "editor_open_scene") {
			return editor_tools->editor_open_scene(p_args);
		}
		if (internal_name == "editor_get_settings") {
			return editor_tools->editor_get_settings(p_args);
		}
		if (internal_name == "editor_set_settings") {
			return editor_tools->editor_set_settings(p_args);
		}
		if (internal_name == "editor_clear_output") {
			return editor_tools->editor_clear_output(p_args);
		}
		if (internal_name == "editor_screenshot_game") {
			return editor_tools->editor_screenshot_game(p_args);
		}
		if (internal_name == "editor_get_output_log") {
			return editor_tools->editor_get_output_log(p_args);
		}
		if (internal_name == "editor_get_errors") {
			return editor_tools->editor_get_errors(p_args);
		}
		if (internal_name == "editor_reload_project") {
			return editor_tools->editor_reload_project(p_args);
		}
		if (internal_name == "editor_save_all_scenes") {
			return editor_tools->editor_save_all_scenes(p_args);
		}
		if (internal_name == "editor_get_signals") {
			return editor_tools->editor_get_signals(p_args);
		}
	}

#ifdef MODULE_MULTIUSER_EDITOR_ENABLED
	if (multiuser_tools) {
		if (internal_name == "multiuser_get_status") {
			return multiuser_tools->multiuser_get_status(p_args);
		}
		if (internal_name == "multiuser_send_chat") {
			return multiuser_tools->multiuser_send_chat(p_args);
		}
		if (internal_name == "multiuser_kick_peer") {
			return multiuser_tools->multiuser_kick_peer(p_args);
		}
		if (internal_name == "multiuser_trigger_autowork") {
			return multiuser_tools->multiuser_trigger_autowork(p_args);
		}
	}
#endif

	result["ok"] = false;
	result["error"] = "Unknown tool: " + p_tool_name;
	return result;
}
