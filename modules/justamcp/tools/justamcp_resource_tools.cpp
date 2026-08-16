/**************************************************************************/
/*  justamcp_resource_tools.cpp                                           */
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

#include "justamcp_resource_tools.h"

#include "../justamcp_editor_plugin.h" // IWYU pragma: keep
#include "../justamcp_read_limits.h" // IWYU pragma: keep

#include "core/config/project_settings.h" // IWYU pragma: keep
#include "core/io/resource_loader.h" // IWYU pragma: keep

#ifdef MODULE_ASSETTAGS_ENABLED
#include "modules/assettags/asset_tag_registry.h" // IWYU pragma: keep
#endif
#include "core/io/dir_access.h" // IWYU pragma: keep
#include "core/io/file_access.h" // IWYU pragma: keep
#include "core/io/image_loader.h" // IWYU pragma: keep
#include "core/io/json.h" // IWYU pragma: keep
#include "core/io/resource_loader.h" // IWYU pragma: keep
#include "core/io/resource_saver.h" // IWYU pragma: keep
#include "editor/editor_interface.h" // IWYU pragma: keep
#include "editor/editor_node.h" // IWYU pragma: keep
#include "editor/editor_undo_redo_manager.h" // IWYU pragma: keep
#include "editor/file_system/editor_file_system.h" // IWYU pragma: keep
#include "scene/2d/sprite_2d.h" // IWYU pragma: keep
#include "scene/2d/tile_map.h" // IWYU pragma: keep
#include "scene/3d/mesh_instance_3d.h" // IWYU pragma: keep
#include "scene/3d/sprite_3d.h" // IWYU pragma: keep
#include "scene/resources/2d/tile_set.h" // IWYU pragma: keep
#include "scene/resources/material.h" // IWYU pragma: keep
#include "scene/resources/packed_scene.h" // IWYU pragma: keep
#include "scene/resources/shader.h" // IWYU pragma: keep
#include "scene/resources/texture.h" // IWYU pragma: keep
#include "scene/resources/theme.h" // IWYU pragma: keep

void JustAMCPResourceTools::_bind_methods() {
}

JustAMCPResourceTools::JustAMCPResourceTools() {
}

JustAMCPResourceTools::~JustAMCPResourceTools() {
}

Dictionary JustAMCPResourceTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "create_resource") {
		return create_resource(p_args);
	}
	if (p_tool_name == "modify_resource") {
		return modify_resource(p_args);
	}
	if (p_tool_name == "read_resource_file") {
		return read_resource_file(p_args);
	}
	if (p_tool_name == "edit_resource_file") {
		return edit_resource_file(p_args);
	}
	if (p_tool_name == "get_resource_preview") {
		return get_resource_preview(p_args);
	}
	if (p_tool_name == "list_resource_files") {
		return list_resource_files(p_args);
	}
	if (p_tool_name == "save_resource_as") {
		return save_resource_as(p_args);
	}
	if (p_tool_name == "get_resource_dependencies") {
		return get_resource_dependencies(p_args);
	}
	if (p_tool_name == "import_asset_copy") {
		return import_asset_copy(p_args);
	}
	if (p_tool_name == "manage_resource_autoloads") {
		return manage_resource_autoloads(p_args);
	}
	if (p_tool_name == "create_material") {
		return create_material(p_args);
	}
	if (p_tool_name == "create_shader_template") {
		return create_shader(p_args);
	}
	if (p_tool_name == "create_tileset") {
		return create_tileset(p_args);
	}
	if (p_tool_name == "set_tilemap_cells") {
		return set_tilemap_cells(p_args);
	}
	if (p_tool_name == "set_theme_resource_color") {
		return set_theme_color(p_args);
	}
	if (p_tool_name == "set_theme_resource_font_size") {
		return set_theme_font_size(p_args);
	}
	if (p_tool_name == "apply_theme_shader") {
		return apply_theme_shader(p_args);
	}
	if (p_tool_name == "resource_import_asset") {
		return resource_import_asset(p_args);
	}
	if (p_tool_name == "get_resource_info") {
		return get_resource_info(p_args);
	}
	return Dictionary();
}

#endif
