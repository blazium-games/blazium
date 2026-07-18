/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"
#include "justamcp_project_settings.h"

#include "justamcp_runtime.h"

#ifdef TOOLS_ENABLED
#include "core/config/project_settings.h"
#include "core/os/os.h"
#include "editor/editor_settings.h"
#include "justamcp_editor_plugin.h"
#include "justamcp_server.h"
#include "tools/justamcp_analysis_tools.h"
#include "tools/justamcp_animation_tools.h"
#include "tools/justamcp_audio_tools.h"
#include "tools/justamcp_batch_tools.h"
#include "tools/justamcp_documentation_tools.h"
#include "tools/justamcp_export_tools.h"
#include "tools/justamcp_input_tools.h"
#ifdef MODULE_MULTIUSER_EDITOR_ENABLED
#include "tools/justamcp_multiuser_tools.h"
#endif
#include "tools/justamcp_asset_tags_tools.h"
#include "tools/justamcp_category_registry.h"
#include "tools/justamcp_mcp_client_bridge.h"
#include "tools/justamcp_node_tools.h"
#include "tools/justamcp_particle_tools.h"
#include "tools/justamcp_physics_tools.h"
#include "tools/justamcp_profiling_tools.h"
#include "tools/justamcp_project_tools.h"
#include "tools/justamcp_prompt_executor.h"
#include "tools/justamcp_resource_executor.h"
#include "tools/justamcp_resource_tools.h"
#include "tools/justamcp_scene_3d_tools.h"
#include "tools/justamcp_scene_tools.h"
#include "tools/justamcp_script_tools.h"
#include "tools/justamcp_semantic_search_tools.h"
#include "tools/justamcp_shader_tools.h"
#include "tools/justamcp_task_manager.h"
#include "tools/justamcp_theme_tools.h"
#include "tools/justamcp_tilemap_tools.h"
#include "tools/justamcp_tool_category_bridge.h"
#include "tools/justamcp_tool_executor.h"
#include "tools/justamcp_tool_schema_cache.h"
#include "tools/justamcp_toolset_registry.h"
#include "tools/prompts/justamcp_prompt.h"
#include "tools/prompts/justamcp_prompt_asset_tagging_workflow.h"
#include "tools/prompts/justamcp_prompt_blazium_context.h"
#include "tools/prompts/justamcp_prompt_blazium_workflows.h"
#include "tools/prompts/justamcp_prompt_editor_state.h"
#include "tools/prompts/justamcp_prompt_project_info.h"
#include "tools/resources/justamcp_resource.h"
#include "tools/resources/justamcp_resource_project_file.h"
#endif

#ifdef TESTS_ENABLED
#include "tests/test_justamcp_analysis_read_cap.cpp"
#include "tests/test_justamcp_asset_tags_tools.cpp"
#include "tests/test_justamcp_autowork_read_cap.cpp"
#include "tests/test_justamcp_bridge_execute_no_main_block.cpp"
#include "tests/test_justamcp_bridge_no_main_wait.cpp"
#include "tests/test_justamcp_bridge_no_sync_fallback_on_main.cpp"
#include "tests/test_justamcp_bridge_task_path.cpp"
#include "tests/test_justamcp_bridge_url_allow_list.cpp"
#include "tests/test_justamcp_cancel_deadline_ignores_late_complete.cpp"
#include "tests/test_justamcp_category_handled.cpp"
#include "tests/test_justamcp_complete_current_respects_tombstone.cpp"
#include "tests/test_justamcp_cors_non_localhost_policy.cpp"
#include "tests/test_justamcp_delete_origin.cpp"
#include "tests/test_justamcp_domain_smoke.cpp"
#include "tests/test_justamcp_dual_accept_get_does_not_steal.cpp"
#include "tests/test_justamcp_enqueue_rate_limit.cpp"
#include "tests/test_justamcp_export_task_support_required.cpp"
#include "tests/test_justamcp_get_sse_does_not_steal_post_pending.cpp"
#include "tests/test_justamcp_http_get_sse.cpp"
#include "tests/test_justamcp_http_get_sse_replay.cpp"
#include "tests/test_justamcp_http_integration.cpp"
#include "tests/test_justamcp_json_rpc_router.cpp"
#include "tests/test_justamcp_json_rpc_transport.cpp"
#include "tests/test_justamcp_legacy_message.cpp"
#include "tests/test_justamcp_list_bridges_redacts_token.cpp"
#include "tests/test_justamcp_message_oauth.cpp"
#include "tests/test_justamcp_non_object_json_400.cpp"
#include "tests/test_justamcp_origin_host_strict.cpp"
#include "tests/test_justamcp_pending_survives_get_close.cpp"
#include "tests/test_justamcp_phase_k.cpp"
#include "tests/test_justamcp_post_sse_async_keeps_open.cpp"
#include "tests/test_justamcp_registry_dispatch.cpp"
#include "tests/test_justamcp_resource_providers.cpp"
#include "tests/test_justamcp_resource_read_caps.cpp"
#include "tests/test_justamcp_routing_dispatch.cpp"
#include "tests/test_justamcp_runtime_smoke.cpp"
#include "tests/test_justamcp_schema_cache_concurrent.cpp"
#include "tests/test_justamcp_schema_dirty_per_cache_key.cpp"
#include "tests/test_justamcp_semantic_tools_async.cpp"
#include "tests/test_justamcp_server_queue_lifecycle.cpp"
#include "tests/test_justamcp_session_manager.cpp"
#include "tests/test_justamcp_session_survives_stream_close.cpp"
#include "tests/test_justamcp_settings_resolver.cpp"
#include "tests/test_justamcp_sse_no_acao_star.cpp"
#include "tests/test_justamcp_stateless_task_dispatch.cpp"
#include "tests/test_justamcp_task_augmented_no_block.cpp"
#include "tests/test_justamcp_task_manager.cpp"
#include "tests/test_justamcp_tool_dispatcher.cpp"
#include "tests/test_justamcp_tool_schema_builder.cpp"
#include "tests/test_justamcp_tool_schema_cache.cpp"
#endif

#ifndef TOOLS_ENABLED
#include "core/config/project_settings.h"
#include "core/os/os.h"
#endif

#include "servers/display_server.h"

static bool _is_headless() {
	if (DisplayServer::get_singleton() != nullptr) {
		return DisplayServer::get_singleton()->get_name() == "headless";
	}
	if (OS::get_singleton() && OS::get_singleton()->get_cmdline_args().find("--headless")) {
		return true;
	}
	return false;
}

static bool _is_justamcp_enabled() {
	if (OS::get_singleton()->get_cmdline_args().find("--enable-mcp")) {
		return true;
	}

	bool use_project_override = false;
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/justamcp/override_editor_settings")) {
		use_project_override = GLOBAL_GET("blazium/justamcp/override_editor_settings");
	}

	if (_is_headless()) {
		use_project_override = true;
	}

#ifdef TOOLS_ENABLED
	if (use_project_override || !EditorSettings::get_singleton()) {
#else
	{
#endif
		if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/justamcp/server_enabled")) {
			return GLOBAL_GET("blazium/justamcp/server_enabled");
		}
#ifdef TOOLS_ENABLED
	} else if (EditorSettings::get_singleton() && EditorSettings::get_singleton()->has_setting("blazium/justamcp/server_enabled")) {
		return EditorSettings::get_singleton()->get_setting("blazium/justamcp/server_enabled");
#endif
	}

	return false;
}

#ifdef TOOLS_ENABLED
static void _register_category_toolset(const String &p_name, const String &p_category, const String &p_description) {
	if (!JustAMCPToolsetRegistry::get_singleton()) {
		return;
	}
	JustAMCPToolCategoryBridge *bridge = memnew(JustAMCPToolCategoryBridge);
	bridge->setup(p_name, p_category, p_description);
	JustAMCPToolsetRegistry::get_singleton()->register_toolset_with_owner(
			p_name,
			p_description,
			callable_mp(bridge, &JustAMCPToolCategoryBridge::provide_tool_schemas),
			callable_mp(bridge, &JustAMCPToolCategoryBridge::execute_tool),
			bridge,
			p_category);
}

static void _register_all_toolsets() {
	for (int i = 0; i < JustAMCPCategoryRegistry::get_entry_count(); i++) {
		const JustAMCPCategoryRegistryEntry &entry = JustAMCPCategoryRegistry::get_entry(i);
		if (entry.requires_multiuser) {
#ifdef MODULE_MULTIUSER_EDITOR_ENABLED
			_register_category_toolset(entry.display_name, entry.category_id, entry.description);
#endif
			continue;
		}
		if (entry.requires_autowork) {
#ifdef MODULE_AUTOWORK_ENABLED
			_register_category_toolset(entry.display_name, entry.category_id, entry.description);
#endif
			continue;
		}
		_register_category_toolset(entry.display_name, entry.category_id, entry.description);
	}
}
#endif

void initialize_justamcp_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(JustAMCPRuntime);
		if (_is_justamcp_enabled()) {
			JustAMCPRuntime *runtime = memnew(JustAMCPRuntime);
			Engine::get_singleton()->add_singleton(Engine::Singleton("JustAMCPRuntime", runtime));
		}
#ifdef TOOLS_ENABLED
		GLOBAL_DEF_BASIC("blazium/justamcp/game_control_enabled", false);
		GLOBAL_DEF_BASIC("blazium/justamcp/disable_game_mcp", false);
#endif
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(JustAMCPServer);
		GDREGISTER_CLASS(JustAMCPToolExecutor);
		GDREGISTER_CLASS(JustAMCPSceneTools);
		GDREGISTER_CLASS(JustAMCPResourceTools);
		GDREGISTER_CLASS(JustAMCPAnimationTools);
		GDREGISTER_CLASS(JustAMCPAnalysisTools);
		GDREGISTER_CLASS(JustAMCPAudioTools);
		GDREGISTER_CLASS(JustAMCPBatchTools);
		GDREGISTER_CLASS(JustAMCPDocumentationTools);
		GDREGISTER_CLASS(JustAMCPExportTools);
		GDREGISTER_CLASS(JustAMCPInputTools);
#ifdef MODULE_MULTIUSER_EDITOR_ENABLED
		GDREGISTER_CLASS(JustAMCPMultiuserTools);
#endif
		GDREGISTER_CLASS(JustAMCPNodeTools);
		GDREGISTER_CLASS(JustAMCPParticleTools);
		GDREGISTER_CLASS(JustAMCPPhysicsTools);
		GDREGISTER_CLASS(JustAMCPProfilingTools);
		GDREGISTER_CLASS(JustAMCPProjectTools);
		GDREGISTER_CLASS(JustAMCPScene3DTools);
		GDREGISTER_CLASS(JustAMCPScriptTools);
		GDREGISTER_CLASS(JustAMCPShaderTools);
		GDREGISTER_CLASS(JustAMCPThemeTools);
		GDREGISTER_CLASS(JustAMCPTileMapTools);
		GDREGISTER_CLASS(JustAMCPPrompt);
		GDREGISTER_CLASS(JustAMCPPromptBlaziumContext);
		GDREGISTER_CLASS(JustAMCPPromptBlaziumWorkflow);
		GDREGISTER_CLASS(JustAMCPPromptProjectInfo);
		GDREGISTER_CLASS(JustAMCPPromptEditorState);
		GDREGISTER_CLASS(JustAMCPPromptAssetTaggingWorkflow);
		GDREGISTER_CLASS(JustAMCPPromptExecutor);
		GDREGISTER_CLASS(JustAMCPResource);
		GDREGISTER_CLASS(JustAMCPResourceProjectFile);
		GDREGISTER_CLASS(JustAMCPResourceExecutor);
		GDREGISTER_CLASS(JustAMCPTaskManager);
		GDREGISTER_CLASS(JustAMCPToolsetRegistry);
		GDREGISTER_CLASS(JustAMCPAssetTagsTools);
		GDREGISTER_CLASS(JustAMCPToolCategoryBridge);
		GDREGISTER_CLASS(JustAMCPMCPClientBridge);
		GDREGISTER_CLASS(JustAMCPSemanticSearchTools);
	}
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		JustAMCPProjectSettings::register_project_settings();
		if (EditorSettings::get_singleton() && !EditorSettings::get_singleton()->has_setting("blazium/justamcp/enable_toolset_discovery")) {
			EditorSettings::get_singleton()->set_setting("blazium/justamcp/enable_toolset_discovery", true);
		}
		if (!JustAMCPToolsetRegistry::get_singleton()) {
			memnew(JustAMCPToolsetRegistry);
		}
#ifdef MODULE_ASSETTAGS_ENABLED
		if (JustAMCPToolsetRegistry::get_singleton()) {
			JustAMCPAssetTagsTools *asset_tags_bridge = memnew(JustAMCPAssetTagsTools);
			JustAMCPToolsetRegistry::get_singleton()->register_toolset_with_owner(
					"AssetTags",
					"Project asset tag dictionary, assignment, and search tools.",
					callable_mp(asset_tags_bridge, &JustAMCPAssetTagsTools::provide_tool_schemas),
					callable_mp(asset_tags_bridge, &JustAMCPAssetTagsTools::execute_tool),
					asset_tags_bridge,
					"asset_tags_tools");
		}
#endif
#ifdef MODULE_SEMANTICSEARCH_ENABLED
		if (JustAMCPToolsetRegistry::get_singleton()) {
			JustAMCPSemanticSearchTools *semantic_bridge = memnew(JustAMCPSemanticSearchTools);
			JustAMCPToolsetRegistry::get_singleton()->register_toolset_with_owner(
					"SemanticSearch",
					"Semantic asset search and similarity tools (lexical/embedding/hybrid).",
					callable_mp(semantic_bridge, &JustAMCPSemanticSearchTools::provide_tool_schemas),
					callable_mp(semantic_bridge, &JustAMCPSemanticSearchTools::execute_tool),
					semantic_bridge,
					"semantic_search_tools");
		}
#endif
		if (JustAMCPToolsetRegistry::get_singleton()) {
			JustAMCPMCPClientBridge *mcp_client_bridge = memnew(JustAMCPMCPClientBridge);
			JustAMCPToolsetRegistry::get_singleton()->register_toolset_with_owner(
					"MCPClient",
					"Outbound MCP client bridges to external MCP servers.",
					callable_mp(mcp_client_bridge, &JustAMCPMCPClientBridge::provide_tool_schemas),
					callable_mp(mcp_client_bridge, &JustAMCPMCPClientBridge::execute_tool),
					mcp_client_bridge,
					"mcp_client_tools");
		}
		_register_all_toolsets();
		JustAMCPToolSchemaCache::get_schemas(false, false, false, false);
		EditorPlugins::add_by_type<JustAMCPEditorPlugin>();
	}
#endif
}

void uninitialize_justamcp_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		if (Engine::get_singleton()->has_singleton("JustAMCPRuntime")) {
			if (JustAMCPRuntime *runtime = Object::cast_to<JustAMCPRuntime>(Engine::get_singleton()->get_singleton_object("JustAMCPRuntime"))) {
				Engine::get_singleton()->remove_singleton("JustAMCPRuntime");
				memdelete(runtime);
			}
		}
	}
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		if (JustAMCPToolsetRegistry *registry = JustAMCPToolsetRegistry::get_singleton()) {
			memdelete(registry);
		}
	}
#endif
}
