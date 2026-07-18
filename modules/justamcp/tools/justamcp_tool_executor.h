/**************************************************************************/
/*  justamcp_tool_executor.h                                              */
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

#pragma once

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "scene/main/node.h"

class JustAMCPEditorPlugin;
class JustAMCPAnalysisTools;
#include "justamcp_analysis_tools.h"
#include "justamcp_animation_tools.h"
#include "justamcp_asset_tools.h"
#include "justamcp_audio_tools.h"
#include "justamcp_batch_tools.h"
#include "justamcp_blueprint_tools.h"
#include "justamcp_documentation_tools.h"
#include "justamcp_draw_tools.h"
#include "justamcp_editor_tools.h"
#include "justamcp_environment_tools.h"
#include "justamcp_export_tools.h"
#include "justamcp_input_tools.h"
#include "justamcp_networking_tools.h"
#include "justamcp_node_tools.h"
#include "justamcp_particle_tools.h"
#include "justamcp_physics_tools.h"
#include "justamcp_project_tools.h"
#include "justamcp_resource_tools.h"
#include "justamcp_scene_tools.h"
class JustAMCPProfilingTools;
class JustAMCPSpatialTools;
class JustAMCPRuntimeTools;
class JustAMCPExportTools;
class JustAMCPBatchTools;
class JustAMCPScriptTools;
class JustAMCPNodeTools;
class JustAMCPAudioTools;
class JustAMCPInputTools;
class JustAMCPParticleTools;
class JustAMCPPhysicsTools;
class JustAMCPScene3DTools;
class JustAMCPShaderTools;
class JustAMCPThemeTools;
class JustAMCPTileMapTools;
class JustAMCPAutoworkTools;
class JustAMCPMultiuserTools;

class JustAMCPToolExecutor : public Object {
	GDCLASS(JustAMCPToolExecutor, Object);

	friend class JustAMCPCategoryExecutorDispatch;

private:
	JustAMCPEditorPlugin *editor_plugin = nullptr;

	JustAMCPAnalysisTools *analysis_tools = nullptr;
	JustAMCPAnimationTools *animation_tools = nullptr;
	JustAMCPAssetTools *asset_tools = nullptr;
	JustAMCPBlueprintTools *blueprint_tools = nullptr;
	JustAMCPAudioTools *audio_tools = nullptr;
	JustAMCPBatchTools *batch_tools = nullptr;
	JustAMCPDocumentationTools *documentation_tools = nullptr;
	JustAMCPDrawTools *draw_tools = nullptr;
	JustAMCPEditorTools *editor_tools = nullptr;
	JustAMCPEnvironmentTools *environment_tools = nullptr;
	JustAMCPExportTools *export_tools = nullptr;
	JustAMCPInputTools *input_tools = nullptr;
	JustAMCPNetworkingTools *networking_tools = nullptr;
	JustAMCPNodeTools *node_tools = nullptr;
	JustAMCPParticleTools *particle_tools = nullptr;
	JustAMCPPhysicsTools *physics_tools = nullptr;
	JustAMCPProjectTools *project_tools = nullptr;
	JustAMCPProfilingTools *profiling_tools = nullptr;
	JustAMCPResourceTools *resource_tools = nullptr;
	JustAMCPRuntimeTools *runtime_tools = nullptr;
	JustAMCPScene3DTools *scene_3d_tools = nullptr;
	JustAMCPSceneTools *scene_tools = nullptr;
	JustAMCPScriptTools *script_tools = nullptr;
	JustAMCPShaderTools *shader_tools = nullptr;
	JustAMCPSpatialTools *spatial_tools = nullptr;
	JustAMCPThemeTools *theme_tools = nullptr;
	JustAMCPTileMapTools *tilemap_tools = nullptr;
	JustAMCPAutoworkTools *autowork_tools = nullptr;
	JustAMCPMultiuserTools *multiuser_tools = nullptr;

	bool initialized = false;
	bool allow_disabled_dispatch = false;

	void _init_tools();

protected:
	static void _bind_methods();

public:
	void set_editor_plugin(JustAMCPEditorPlugin *p_plugin);
	Dictionary execute_tool(const String &p_tool_name, const Dictionary &p_args);
	Dictionary execute_composite_tool(const String &p_internal_name, const Dictionary &p_args);
	Dictionary execute_tool_direct(const String &p_tool_name, const Dictionary &p_args);
	Dictionary execute_registry_category_tool(const String &p_category, const String &p_tool_name, const Dictionary &p_args);
	Dictionary execute_module_category_tool(const String &p_category, const String &p_internal_name, const Dictionary &p_args);

	static void register_tool_settings();
	static Array get_tool_schemas(bool p_register_only = false, bool p_ignore_settings = false, bool p_apply_discovery_filter = true, bool p_include_disabled_tools = false);
	static Array collect_tool_schemas_for_category(const String &p_category, bool p_register_only = false, bool p_ignore_settings = false, bool p_include_disabled_tools = false);
	static Array get_tool_schemas_for_category(const String &p_category, bool p_register_only = false, bool p_ignore_settings = false, bool p_include_disabled_tools = false);
	static Dictionary list_tools(const String &p_cursor = "");

	static JustAMCPToolExecutor *active_instance;
	static JustAMCPToolExecutor *get_active_instance();
	void set_as_active_instance();

	static Node *test_scene_root;
	static void set_test_scene_root(Node *p_node);
	static Node *get_test_scene_root();

	JustAMCPToolExecutor();
	~JustAMCPToolExecutor();
};
