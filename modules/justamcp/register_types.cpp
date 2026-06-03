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
#include "tools/justamcp_shader_tools.h"
#include "tools/justamcp_task_manager.h"
#include "tools/justamcp_theme_tools.h"
#include "tools/justamcp_tilemap_tools.h"
#include "tools/justamcp_tool_executor.h"
#include "tools/prompts/justamcp_prompt.h"
#include "tools/prompts/justamcp_prompt_blazium_context.h"
#include "tools/prompts/justamcp_prompt_blazium_workflows.h"
#include "tools/prompts/justamcp_prompt_editor_state.h"
#include "tools/prompts/justamcp_prompt_project_info.h"
#include "tools/resources/justamcp_resource.h"
#include "tools/resources/justamcp_resource_project_file.h"
#include "tools/resources/justamcp_resource_system_logs.h"
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
static void _register_justamcp_project_settings() {
	GLOBAL_DEF_BASIC("blazium/justamcp/override_editor_settings", false);
	GLOBAL_DEF_BASIC("blazium/justamcp/server_enabled", false);
	GLOBAL_DEF_BASIC("blazium/justamcp/server_port", 6506);
	GLOBAL_DEF_BASIC("blazium/justamcp/oauth_enabled", false);
	GLOBAL_DEF_BASIC("blazium/justamcp/client_id", String());
	GLOBAL_DEF_BASIC("blazium/justamcp/client_secret", String());
	GLOBAL_DEF_BASIC("blazium/justamcp/z_mcp_config", String());
	GLOBAL_DEF_BASIC("blazium/justamcp/enable_debug_logging", true);
	GLOBAL_DEF_BASIC("blazium/justamcp/forward_engine_logs", true);
	GLOBAL_DEF_BASIC("blazium/justamcp/list_page_size", 50);
	GLOBAL_DEF_BASIC("blazium/justamcp/mcp_log_buffer_size", 500);
	GLOBAL_DEF_BASIC("blazium/justamcp/task_default_ttl_ms", 600000);
	GLOBAL_DEF_BASIC("blazium/justamcp/task_poll_interval_ms", 1000);
	GLOBAL_DEF_BASIC("blazium/justamcp/task_max_concurrent", 16);
	GLOBAL_DEF_BASIC("blazium/justamcp/bind_to_localhost_only", true);

	JustAMCPToolExecutor::register_tool_settings();
	JustAMCPPromptExecutor::register_settings();
	JustAMCPResourceExecutor::register_settings();
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
		GDREGISTER_CLASS(JustAMCPPromptExecutor);
		GDREGISTER_CLASS(JustAMCPResource);
		GDREGISTER_CLASS(JustAMCPResourceSystemLogs);
		GDREGISTER_CLASS(JustAMCPResourceProjectFile);
		GDREGISTER_CLASS(JustAMCPResourceExecutor);
		GDREGISTER_CLASS(JustAMCPTaskManager);
	}
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		_register_justamcp_project_settings();
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
}
