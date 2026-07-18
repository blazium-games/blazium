/**************************************************************************/
/*  justamcp_category_registry.cpp                                        */
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

#include "justamcp_category_registry.h"

#include "core/templates/hash_map.h"

static const JustAMCPCategoryRegistryEntry g_category_registry[] = {
	{ "Editor", "editor_tools", "Editor playback, selection, settings, and workspace tools." },
	{ "Documentation", "documentation_tools", "Documentation and guide lookup tools." },
	{ "Networking", "networking_tools", "HTTP and networking helper tools." },
	{ "Multiuser", "multiuser_tools", "Multiuser editor session tools.", true },
	{ "Spatial", "spatial_tools", "Spatial mapping and navigation tools." },
	{ "Runtime", "runtime_tools", "Runtime diagnostics and game control tools." },
	{ "Scene", "scene_tools", "Scene creation, node editing, and signal tools." },
	{ "Resource", "resource_tools", "Resource import, inspection, and file tools." },
	{ "Animation", "animation_tools", "Animation tree and playback tools." },
	{ "Project", "project_tools", "Project settings, export metadata, and info tools." },
	{ "Profiling", "profiling_tools", "Profiler and performance inspection tools." },
	{ "Export", "export_tools", "Export preset and packaging tools." },
	{ "Batch", "batch_tools", "Batch scene and dependency analysis tools." },
	{ "Script", "script_tools", "Script editing and search tools." },
	{ "Node", "node_tools", "Node property and hierarchy tools." },
	{ "Audio", "audio_tools", "Audio bus and playback tools." },
	{ "Input", "input_tools", "Input map and action tools." },
	{ "Particle", "particle_tools", "Particle system tools." },
	{ "Physics", "physics_tools", "Physics layer and collision tools." },
	{ "Scene3D", "scene3d_tools", "3D scene and environment tools." },
	{ "Shader", "shader_tools", "Shader file and material tools." },
	{ "Theme", "theme_tools", "Theme and UI styling tools." },
	{ "TileMap", "tilemap_tools", "TileMap editing tools." },
	{ "Asset", "asset_tools", "Generic asset manipulation tools." },
	{ "Blueprint", "blueprint_tools", "Blueprint workflow tools." },
	{ "Draw", "draw_tools", "2D draw and canvas tools." },
	{ "Environment", "environment_tools", "World environment tools." },
	{ "Analysis", "analysis_tools", "Project analysis and validation tools." },
	{ "Autowork", "autowork_tools", "Autowork test runner tools.", false, true },
};

int JustAMCPCategoryRegistry::get_entry_count() {
	return int(sizeof(g_category_registry) / sizeof(g_category_registry[0]));
}

const JustAMCPCategoryRegistryEntry &JustAMCPCategoryRegistry::get_entry(int p_index) {
	ERR_FAIL_INDEX_V(p_index, get_entry_count(), g_category_registry[0]);
	return g_category_registry[p_index];
}

bool JustAMCPCategoryRegistry::is_registered_category(const String &p_category) {
	for (int i = 0; i < get_entry_count(); i++) {
		if (p_category == g_category_registry[i].category_id) {
			return true;
		}
	}
	return false;
}

#endif
