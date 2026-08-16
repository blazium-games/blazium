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

#include "../justamcp_mcp_spec.h"
#include "../justamcp_pagination.h"
#include "../justamcp_runtime.h"
#include "../justamcp_server.h"
#include "../justamcp_tool_context.h"
#include "justamcp_analysis_tools.h"
#include "justamcp_animation_tools.h"
#include "justamcp_asset_tags_tools.h"
#include "justamcp_audio_tools.h"
#include "justamcp_batch_tools.h"
#include "justamcp_category_dispatch.h"
#include "justamcp_category_executor_dispatch.h"
#include "justamcp_category_registry.h"
#include "justamcp_category_schemas.h"
#include "justamcp_documentation_tools.h"
#include "justamcp_editor_tools.h"
#include "justamcp_export_tools.h"
#include "justamcp_input_tools.h"
#include "justamcp_mcp_client_bridge.h"
#include "justamcp_meta_tools.h"
#include "justamcp_networking_tools.h"
#include "justamcp_node_tools.h"
#include "justamcp_particle_tools.h"
#include "justamcp_physics_tools.h"
#include "justamcp_profiling_tools.h"
#include "justamcp_project_tools.h"
#include "justamcp_readonly_tools.h"
#include "justamcp_resource_executor.h"
#include "justamcp_resource_tools.h"
#include "justamcp_runtime_tools.h"
#include "justamcp_scene_3d_tools.h"
#include "justamcp_scene_tools.h"
#include "justamcp_script_tools.h"
#include "justamcp_semantic_search_tools.h"
#include "justamcp_shader_tools.h"
#include "justamcp_spatial_tools.h"
#include "justamcp_theme_tools.h"
#include "justamcp_tilemap_tools.h"
#include "justamcp_tool_dispatcher.h"
#include "justamcp_tool_schema_builder.h"
#include "justamcp_tool_schema_cache.h"
#include "justamcp_toolset_registry.h"

#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/templates/hash_set.h"

#ifdef TOOLS_ENABLED
#include "../justamcp_editor_plugin.h"

#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/settings/editor_settings.h"
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
	ClassDB::bind_static_method("JustAMCPToolExecutor", D_METHOD("get_tool_schemas", "register_only", "ignore_settings", "apply_discovery_filter", "include_disabled_tools"), &JustAMCPToolExecutor::get_tool_schemas, DEFVAL(false), DEFVAL(false), DEFVAL(true), DEFVAL(false));
	ClassDB::bind_static_method("JustAMCPToolExecutor", D_METHOD("list_tools", "cursor"), &JustAMCPToolExecutor::list_tools, DEFVAL(""));
	ClassDB::bind_static_method("JustAMCPToolExecutor", D_METHOD("set_test_scene_root", "node"), &JustAMCPToolExecutor::set_test_scene_root);
}

Node *JustAMCPToolExecutor::test_scene_root = nullptr;
JustAMCPToolExecutor *JustAMCPToolExecutor::active_instance = nullptr;

JustAMCPToolExecutor *JustAMCPToolExecutor::get_active_instance() {
	return active_instance;
}

void JustAMCPToolExecutor::set_as_active_instance() {
	_init_tools();
	active_instance = this;
}

void JustAMCPToolExecutor::track_worker_task(WorkerThreadPool::TaskID p_task_id) {
	if (p_task_id == WorkerThreadPool::INVALID_TASK_ID) {
		return;
	}
	MutexLock lock(pending_worker_task_mutex);
	pending_worker_task_ids.push_back(p_task_id);
}

void JustAMCPToolExecutor::_wait_for_tracked_worker_tasks() {
	Vector<WorkerThreadPool::TaskID> ids;
	{
		MutexLock lock(pending_worker_task_mutex);
		ids = pending_worker_task_ids;
		pending_worker_task_ids.clear();
	}
	WorkerThreadPool *pool = WorkerThreadPool::get_singleton();
	if (!pool) {
		return;
	}
	for (int i = 0; i < ids.size(); i++) {
		pool->wait_for_task_completion(ids[i]);
	}
}

void JustAMCPToolExecutor::set_test_scene_root(Node *p_node) {
	test_scene_root = p_node;
}
Node *JustAMCPToolExecutor::get_test_scene_root() {
	return test_scene_root;
}

JustAMCPToolExecutor::JustAMCPToolExecutor() {
#ifdef TESTS_ENABLED

	_init_tools();
#endif
}

JustAMCPToolExecutor::~JustAMCPToolExecutor() {
	if (active_instance == this) {
		active_instance = nullptr;
	}
	_wait_for_tracked_worker_tasks();
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
	multiuser_tools.unref();
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
	_init_tools();
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
	if (multiuser_tools.is_valid()) {
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

static Array _collect_tool_schemas(bool p_register_only, bool p_ignore_settings, bool p_apply_discovery_filter, bool p_include_disabled_tools, const String &p_category_only) {
	Array tools;
	String current_category = "";
	bool is_core = false;
	HashSet<String> invalidated_categories;

	auto category_matches = [&]() -> bool {
		return p_category_only.is_empty() || current_category == p_category_only;
	};

	auto add_schema = [&](const String &p_name, const String &p_desc, const Vector<String> &p_props, const Vector<String> &p_req, const String &p_task_support = "forbidden", const String &p_thread_affinity = "") {
		String full_name = "blazium_" + p_name;
		if (!justamcp_is_valid_mcp_tool_name(full_name)) {
			WARN_PRINT("JustAMCP: skipping invalid tool name at registration: " + full_name);
			return;
		}

		if (p_register_only) {
			if (current_category.is_empty() || !category_matches()) {
				return;
			}
			JustAMCPToolSchemaBuilder::register_tool_settings(current_category, full_name, is_core);
			if (!invalidated_categories.has(current_category)) {
				JustAMCPToolSchemaCache::invalidate_category(current_category);
				invalidated_categories.insert(current_category);
			}
			return;
		}

		if (!category_matches()) {
			return;
		}

		bool cat_enabled = true;
		bool tool_enabled = true;
		if (!current_category.is_empty() && !JustAMCPToolSchemaBuilder::resolve_tool_enabled(current_category, full_name, p_ignore_settings, p_include_disabled_tools, cat_enabled, tool_enabled)) {
			return;
		}

		Dictionary t;
		t["name"] = full_name;
		t["description"] = p_desc;
		if (!current_category.is_empty()) {
			Dictionary meta;
			meta["category"] = current_category;
			meta["enabled"] = cat_enabled && tool_enabled;
			if (p_thread_affinity == "worker") {
				meta["threadAffinity"] = "worker";
			}
			t["_meta"] = meta;
		}
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
		justamcp_attach_icons(t);
		if (p_task_support != "forbidden" || p_thread_affinity == "worker") {
			Dictionary execution;
			if (p_task_support != "forbidden") {
				execution["taskSupport"] = p_task_support;
			}
			if (p_thread_affinity == "worker") {
				execution["threadAffinity"] = "worker";
			}
			t["execution"] = execution;
		}
		tools.push_back(t);
	};

	if (p_category_only.is_empty()) {
		add_schema("search_tools", "Searches the engine native capabilities for a specific tool name matching your needs.",
				Vector<String>{ "query", "string" }, Vector<String>{ "query" }, "forbidden", "worker");
		add_schema("execute_tool", "Dynamically bypasses context omission to execute ANY tool in the engine by name.",
				Vector<String>{ "tool_name", "string", "arguments", "object" }, Vector<String>{ "tool_name", "arguments" });
		add_schema("get_guide", "Lists or reads built-in JustAMCP workflow guides mirrored from godot-mcp resources.",
				Vector<String>{ "topic", "string", "slug", "string" }, Vector<String>{}, "forbidden", "worker");
		add_schema("list_toolsets", "Lists registered JustAMCP toolsets when lazy tool discovery is enabled.",
				Vector<String>{}, Vector<String>{}, "forbidden", "worker");
		add_schema("describe_toolset", "Returns the tool schemas for a registered JustAMCP toolset.",
				Vector<String>{ "toolset_name", "string" }, Vector<String>{ "toolset_name" }, "forbidden", "worker");
		add_schema("call_toolset", "Executes a tool from a registered JustAMCP toolset.",
				Vector<String>{ "toolset_name", "string", "tool_name", "string", "arguments", "object" }, Vector<String>{ "toolset_name", "tool_name" });
	}

	JustAMCPCategorySchemas::JustAMCPCategorySchemaContext category_ctx;
	category_ctx.current_category = &current_category;
	category_ctx.is_core = &is_core;
	category_ctx.add_schema = add_schema;
	JustAMCPCategorySchemas::register_category_schemas(category_ctx);
#ifdef MODULE_ASSETTAGS_ENABLED
	if (p_category_only.is_empty() || p_category_only == "asset_tags_tools") {
		if (JustAMCPToolsetRegistry::get_singleton()) {
			Array tag_schemas = JustAMCPToolsetRegistry::get_singleton()->collect_tool_schemas(
					"AssetTags", p_register_only, p_ignore_settings, false);
			if (!tag_schemas.is_empty()) {
				current_category = "asset_tags_tools";
				is_core = true;
				if (p_register_only) {
					JustAMCPToolSchemaCache::invalidate_category(current_category);
				}
				for (int i = 0; i < tag_schemas.size(); i++) {
					tools.push_back(tag_schemas[i]);
				}
			}
		}
	}
#endif

	if (p_category_only.is_empty() || p_category_only == "mcp_client_tools") {
		current_category = "mcp_client_tools";
		is_core = false;
		{
			Array client_schemas = JustAMCPMCPClientBridge::get_tool_schemas(p_register_only, p_ignore_settings);
			for (int i = 0; i < client_schemas.size(); i++) {
				tools.push_back(client_schemas[i]);
			}
		}
	}

#ifdef MODULE_SEMANTICSEARCH_ENABLED
	if (p_category_only.is_empty() || p_category_only == "semantic_search_tools") {
		if (JustAMCPToolsetRegistry::get_singleton()) {
			Array semantic_schemas = JustAMCPToolsetRegistry::get_singleton()->collect_tool_schemas(
					"SemanticSearch", p_register_only, p_ignore_settings, false);
			if (!semantic_schemas.is_empty()) {
				current_category = "semantic_search_tools";
				is_core = false;
				if (p_register_only) {
					JustAMCPToolSchemaCache::invalidate_category(current_category);
				}
				for (int i = 0; i < semantic_schemas.size(); i++) {
					tools.push_back(semantic_schemas[i]);
				}
			}
		}
	}
#endif

#ifdef MODULE_AUTOWORK_ENABLED
	if (p_category_only.is_empty() || p_category_only == "autowork_tools") {
		current_category = "autowork_tools";
		is_core = false;
		add_schema("autowork_run_all_tests", "Recursively traverses and executes all autowork test suites natively returning structured passing/failure statistics.",
				Vector<String>{}, Vector<String>{}, "optional");
		add_schema("autowork_run_tests_in_directory", "Recursively finds and executes all Godot autowork unit tests inside a given directory, returning formatted results.",
				Vector<String>{ "directory_path", "string" }, Vector<String>{ "directory_path" }, "optional");
		add_schema("autowork_run_test_script", "Executes an exact test script natively against the runtime test suite framework evaluating state.",
				Vector<String>{ "script_path", "string" }, Vector<String>{ "script_path" }, "optional");
		add_schema("autowork_run_test_by_name", "Performs regex lookup isolating explicit test pattern function names universally across suites for debugging single logic instances.",
				Vector<String>{ "test_name", "string" }, Vector<String>{ "test_name" }, "optional");
	}
#endif

	if (p_category_only.is_empty() && !p_register_only && p_apply_discovery_filter && !p_ignore_settings && JustAMCPToolsetRegistry::get_singleton() && JustAMCPToolsetRegistry::get_singleton()->is_discovery_enabled()) {
		Array filtered;
		static const char *discovery_tools[] = {
			"blazium_search_tools",
			"blazium_execute_tool",
			"blazium_get_guide",
			"blazium_list_toolsets",
			"blazium_describe_toolset",
			"blazium_call_toolset",
			nullptr
		};
		for (int i = 0; i < tools.size(); i++) {
			Dictionary tool = tools[i];
			String name = tool.get("name", "");
			for (int j = 0; discovery_tools[j]; j++) {
				if (name == discovery_tools[j]) {
					filtered.push_back(tool);
					break;
				}
			}
		}
		tools = filtered;
	}

	for (int i = 0; i < tools.size(); i++) {
		JustAMCPReadonlyTools::register_readonly_from_schema(tools[i]);
	}

	return tools;
}

Array JustAMCPToolExecutor::get_tool_schemas(bool p_register_only, bool p_ignore_settings, bool p_apply_discovery_filter, bool p_include_disabled_tools) {
	return _collect_tool_schemas(p_register_only, p_ignore_settings, p_apply_discovery_filter, p_include_disabled_tools, String());
}

Array JustAMCPToolExecutor::collect_tool_schemas_for_category(const String &p_category, bool p_register_only, bool p_ignore_settings, bool p_include_disabled_tools) {
	return _collect_tool_schemas(p_register_only, p_ignore_settings, false, p_include_disabled_tools, p_category);
}

Array JustAMCPToolExecutor::get_tool_schemas_for_category(const String &p_category, bool p_register_only, bool p_ignore_settings, bool p_include_disabled_tools) {
	return JustAMCPToolSchemaCache::get_category_schemas(p_category, p_register_only, p_ignore_settings, p_include_disabled_tools);
}

Dictionary JustAMCPToolExecutor::list_tools(const String &p_cursor) {
	bool apply_discovery_filter = false;
	if (JustAMCPToolsetRegistry::get_singleton() && JustAMCPToolsetRegistry::get_singleton()->is_discovery_enabled()) {
		apply_discovery_filter = true;
	}
	return justamcp_pagination_slice_array(JustAMCPToolSchemaCache::get_schemas(false, false, apply_discovery_filter, false), p_cursor, "tools");
}

Dictionary JustAMCPToolExecutor::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	Dictionary result;

	String internal_name = p_tool_name;
	if (internal_name.begins_with("blazium_")) {
		internal_name = internal_name.substr(8);
	}
	String full_name = "blazium_" + internal_name;
	if (!justamcp_is_valid_mcp_tool_name(p_tool_name) && !justamcp_is_valid_mcp_tool_name(full_name)) {
		result["ok"] = false;
		result["error"] = justamcp_invalid_mcp_tool_name_message(p_tool_name);
		return result;
	}

	if (!scene_tools || !resource_tools || !animation_tools || !project_tools || !profiling_tools || !export_tools || !batch_tools || !script_tools || !node_tools || !audio_tools || !blueprint_tools || !input_tools || !particle_tools || !physics_tools || !scene_3d_tools || !shader_tools || !theme_tools || !tilemap_tools || !analysis_tools || !asset_tools || !draw_tools || !environment_tools) {
		result["ok"] = false;
		result["error"] = "Tools not initialized";
		return result;
	}

	Dictionary target_schema = JustAMCPToolSchemaCache::find_tool_schema(full_name, true);
	bool tool_found = !target_schema.is_empty();

	if (!tool_found) {
		Dictionary err;
		err["code"] = -32601;
		err["message"] = "Tool not found or disabled: " + p_tool_name;
		result["ok"] = false;
		result["error"] = err;
		return result;
	}

	if (target_schema.has("_meta") && !allow_disabled_dispatch) {
		Dictionary meta = target_schema["_meta"];
		if (!meta.get("enabled", true)) {
			Dictionary err;
			err["code"] = -32601;
			err["message"] = "Tool is disabled: " + p_tool_name;
			result["ok"] = false;
			result["error"] = err;
			return result;
		}
	}

	if (justamcp_is_cancel_requested()) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "cancelled";
		return err;
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

#ifdef TOOLS_ENABLED
	if (JustAMCPToolDispatcher::matches_prefix_route(internal_name)) {
		return JustAMCPToolDispatcher::dispatch_prefix_tools(this, internal_name, p_args);
	}
#endif

	if (JustAMCPMetaTools::handles(internal_name)) {
		return JustAMCPMetaTools::execute(this, internal_name, p_args);
	}

	String routed_category;
	if (target_schema.has("_meta")) {
		const Dictionary meta = target_schema["_meta"];
		routed_category = meta.get("category", "");
	}
	if (!routed_category.is_empty()) {
		const Dictionary module_result = JustAMCPToolCategoryDispatch::dispatch_module_tools(this, routed_category, internal_name, p_args);
		if (bool(module_result.get("handled", false))) {
			Dictionary routed = module_result.duplicate();
			routed.erase("handled");
			return routed;
		}
	}

	Dictionary composite = execute_composite_tool(internal_name, p_args);
	if (!composite.is_empty()) {
		return composite;
	}

	result["ok"] = false;
	Dictionary err;
	err["code"] = -32601;
	err["message"] = "Unknown tool: " + p_tool_name;
	result["error"] = err;
	return result;
}

Dictionary JustAMCPToolExecutor::execute_module_category_tool(const String &p_category, const String &p_internal_name, const Dictionary &p_args) {
	return JustAMCPCategoryExecutorDispatch::dispatch(this, p_category, p_internal_name, p_args);
}

Dictionary JustAMCPToolExecutor::execute_registry_category_tool(const String &p_category, const String &p_tool_name, const Dictionary &p_args) {
	String internal_name = p_tool_name;
	if (internal_name.begins_with("blazium_")) {
		internal_name = internal_name.substr(8);
	}
	const Dictionary module_result = JustAMCPToolCategoryDispatch::dispatch_module_tools(this, p_category, internal_name, p_args);
	if (bool(module_result.get("handled", false))) {
		Dictionary routed = module_result.duplicate();
		routed.erase("handled");
		return routed;
	}
	const bool prev = allow_disabled_dispatch;
	allow_disabled_dispatch = true;
	Dictionary result = execute_tool(p_tool_name, p_args);
	allow_disabled_dispatch = prev;
	return result;
}

Dictionary JustAMCPToolExecutor::execute_tool_direct(const String &p_tool_name, const Dictionary &p_args) {
	bool allow_bypass = false;
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/justamcp/allow_execute_tool_bypass")) {
		allow_bypass = GLOBAL_GET("blazium/justamcp/allow_execute_tool_bypass");
	}
	const bool prev = allow_disabled_dispatch;
	allow_disabled_dispatch = allow_bypass;
	Dictionary result = execute_tool(p_tool_name, p_args);
	allow_disabled_dispatch = prev;
	return result;
}
