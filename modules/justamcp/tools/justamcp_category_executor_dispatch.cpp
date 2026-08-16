/**************************************************************************/
/*  justamcp_category_executor_dispatch.cpp                               */
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

#include "justamcp_category_executor_dispatch.h"

#include "justamcp_analysis_tools.h"
#include "justamcp_animation_tools.h"
#include "justamcp_asset_tools.h"
#include "justamcp_audio_tools.h"
#include "justamcp_batch_tools.h"
#include "justamcp_blueprint_tools.h"
#include "justamcp_category_registry.h"
#include "justamcp_documentation_tools.h"
#include "justamcp_draw_tools.h"
#include "justamcp_editor_tools.h"
#include "justamcp_environment_tools.h"
#include "justamcp_export_tools.h"
#include "justamcp_input_tools.h"
#include "justamcp_mcp_client_bridge.h"
#include "justamcp_networking_tools.h"
#include "justamcp_node_tools.h"
#include "justamcp_particle_tools.h"
#include "justamcp_physics_tools.h"
#include "justamcp_profiling_tools.h"
#include "justamcp_project_tools.h"
#include "justamcp_resource_tools.h"
#include "justamcp_runtime_tools.h"
#include "justamcp_scene_3d_tools.h"
#include "justamcp_scene_tools.h"
#include "justamcp_script_tools.h"
#include "justamcp_shader_tools.h"
#include "justamcp_spatial_tools.h"
#include "justamcp_theme_tools.h"
#include "justamcp_tilemap_tools.h"
#include "justamcp_tool_executor.h"

#include "modules/modules_enabled.gen.h"
#ifdef MODULE_AUTOWORK_ENABLED
#include "justamcp_autowork_tools.h"
#endif
#ifdef MODULE_MULTIUSER_EDITOR_ENABLED
#include "justamcp_multiuser_tools.h"
#endif

Dictionary JustAMCPCategoryExecutorDispatch::dispatch(JustAMCPToolExecutor *p_executor, const String &p_category, const String &p_internal_name, const Dictionary &p_args) {
	if (!p_executor || !JustAMCPCategoryRegistry::is_registered_category(p_category)) {
		return Dictionary();
	}
	if (p_category == "editor_tools" && p_executor->editor_tools) {
		return p_executor->editor_tools->execute_tool(p_internal_name, p_args);
	}
	if (p_category == "documentation_tools" && p_executor->documentation_tools) {
		return p_executor->documentation_tools->execute_tool(p_internal_name, p_args);
	}
	if (p_category == "networking_tools" && p_executor->networking_tools) {
		return p_executor->networking_tools->execute_tool(p_internal_name, p_args);
	}
#ifdef MODULE_MULTIUSER_EDITOR_ENABLED
	if (p_category == "multiuser_tools" && p_executor->multiuser_tools.is_valid()) {
		return p_executor->multiuser_tools->execute_tool(p_internal_name, p_args);
	}
#endif
	if (p_category == "spatial_tools" && p_executor->spatial_tools) {
		return p_executor->spatial_tools->execute_tool(p_internal_name, p_args);
	}
	if (p_category == "runtime_tools" && p_executor->runtime_tools) {
		return p_executor->runtime_tools->execute_tool(p_internal_name, p_args);
	}
	if (p_category == "scene_tools" && p_executor->scene_tools) {
		return p_executor->scene_tools->execute_tool(p_internal_name, p_args);
	}
	if (p_category == "resource_tools" && p_executor->resource_tools) {
		return p_executor->resource_tools->execute_tool(p_internal_name, p_args);
	}
	if (p_category == "animation_tools" && p_executor->animation_tools) {
		return p_executor->animation_tools->execute_tool(p_internal_name, p_args);
	}
	if (p_category == "project_tools" && p_executor->project_tools) {
		return p_executor->project_tools->execute_tool(p_internal_name, p_args);
	}
	if (p_category == "profiling_tools" && p_executor->profiling_tools) {
		return p_executor->profiling_tools->execute_tool(p_internal_name, p_args);
	}
	if (p_category == "export_tools" && p_executor->export_tools) {
		return p_executor->export_tools->execute_tool(p_internal_name, p_args);
	}
	if (p_category == "batch_tools" && p_executor->batch_tools) {
		return p_executor->batch_tools->execute_tool(p_internal_name, p_args);
	}
	if (p_category == "script_tools" && p_executor->script_tools) {
		return p_executor->script_tools->execute_tool(p_internal_name, p_args);
	}
	if (p_category == "node_tools" && p_executor->node_tools) {
		return p_executor->node_tools->execute_tool(p_internal_name, p_args);
	}
	if (p_category == "audio_tools" && p_executor->audio_tools) {
		return p_executor->audio_tools->execute_tool(p_internal_name, p_args);
	}
	if (p_category == "input_tools" && p_executor->input_tools) {
		return p_executor->input_tools->execute_tool(p_internal_name, p_args);
	}
	if (p_category == "particle_tools" && p_executor->particle_tools) {
		return p_executor->particle_tools->execute_tool(p_internal_name, p_args);
	}
	if (p_category == "physics_tools" && p_executor->physics_tools) {
		return p_executor->physics_tools->execute_tool(p_internal_name, p_args);
	}
	if (p_category == "scene3d_tools" && p_executor->scene_3d_tools) {
		return p_executor->scene_3d_tools->execute_tool(p_internal_name, p_args);
	}
	if (p_category == "shader_tools" && p_executor->shader_tools) {
		return p_executor->shader_tools->execute_tool(p_internal_name, p_args);
	}
	if (p_category == "theme_tools" && p_executor->theme_tools) {
		return p_executor->theme_tools->execute_tool(p_internal_name, p_args);
	}
	if (p_category == "tilemap_tools" && p_executor->tilemap_tools) {
		return p_executor->tilemap_tools->execute_tool(p_internal_name, p_args);
	}
	if (p_category == "asset_tools" && p_executor->asset_tools) {
		return p_executor->asset_tools->execute_tool(p_internal_name, p_args);
	}
	if (p_category == "blueprint_tools" && p_executor->blueprint_tools) {
		return p_executor->blueprint_tools->execute_tool(p_internal_name, p_args);
	}
	if (p_category == "draw_tools" && p_executor->draw_tools) {
		return p_executor->draw_tools->execute_tool(p_internal_name, p_args);
	}
	if (p_category == "environment_tools" && p_executor->environment_tools) {
		return p_executor->environment_tools->execute_tool(p_internal_name, p_args);
	}
	if (p_category == "analysis_tools" && p_executor->analysis_tools) {
		return p_executor->analysis_tools->execute_tool(p_internal_name, p_args);
	}
	if (p_category == "mcp_client_tools") {
		if (JustAMCPMCPClientBridge *bridge = JustAMCPMCPClientBridge::get_singleton()) {
			return bridge->execute_tool(p_internal_name, p_args);
		}
	}
#ifdef MODULE_AUTOWORK_ENABLED
	if (p_category == "autowork_tools" && p_executor->autowork_tools) {
		return p_executor->autowork_tools->execute_tool(p_internal_name, p_args);
	}
#endif
	return Dictionary();
}

#endif
