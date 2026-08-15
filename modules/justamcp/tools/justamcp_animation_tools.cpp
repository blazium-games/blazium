/**************************************************************************/
/*  justamcp_animation_tools.cpp                                          */
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

#include "justamcp_animation_tools.h"
#include "../justamcp_editor_plugin.h"

#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "scene/resources/packed_scene.h"

#include "scene/animation/animation_blend_space_1d.h"
#include "scene/animation/animation_blend_space_2d.h"
#include "scene/animation/animation_blend_tree.h"
#include "scene/animation/animation_node_state_machine.h"
#include "scene/animation/animation_player.h"
#include "scene/animation/animation_tree.h"
#include "scene/resources/animation_library.h"

#include "scene/2d/navigation/navigation_agent_2d.h"
#include "scene/2d/navigation/navigation_region_2d.h"
#include "scene/3d/navigation/navigation_agent_3d.h"
#include "scene/3d/navigation/navigation_region_3d.h"
#include "scene/resources/2d/navigation_polygon.h"
#include "scene/resources/navigation_mesh.h"

Dictionary JustAMCPAnimationTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "create_animation") {
		return create_animation(p_args);
	}
	if (p_tool_name == "set_animation_keyframe") {
		return set_animation_keyframe(p_args);
	}
	if (p_tool_name == "get_animation_info") {
		return get_animation_info(p_args);
	}
	if (p_tool_name == "list_animations") {
		return list_animations(p_args);
	}
	if (p_tool_name == "remove_animation") {
		return remove_animation(p_args);
	}
	if (p_tool_name == "add_animation_track") {
		return add_animation_track(p_args);
	}
	if (p_tool_name == "create_animation_tree") {
		return create_animation_tree(p_args);
	}
	if (p_tool_name == "get_animation_tree_structure") {
		return get_animation_tree_structure(p_args);
	}
	if (p_tool_name == "add_animation_state") {
		return add_animation_state(p_args);
	}
	if (p_tool_name == "remove_animation_state") {
		return remove_animation_state(p_args);
	}
	if (p_tool_name == "connect_animation_states") {
		return connect_animation_states(p_args);
	}
	if (p_tool_name == "remove_animation_transition") {
		return remove_animation_transition(p_args);
	}
	if (p_tool_name == "set_animation_tree_parameter") {
		return set_animation_tree_parameter(p_args);
	}
	if (p_tool_name == "set_blend_tree_node") {
		return set_blend_tree_node(p_args);
	}
	if (p_tool_name == "create_navigation_region") {
		return create_navigation_region(p_args);
	}
	if (p_tool_name == "create_navigation_agent") {
		return create_navigation_agent(p_args);
	}
	if (p_tool_name == "create_tween") {
		return create_tween(p_args);
	}

	return Dictionary();
}

void JustAMCPAnimationTools::_bind_methods() {
}

JustAMCPAnimationTools::JustAMCPAnimationTools() {
}

JustAMCPAnimationTools::~JustAMCPAnimationTools() {
}

String JustAMCPAnimationTools::_ensure_res_path(const String &p_path) {
	if (!p_path.begins_with("res://")) {
		return "res://" + p_path;
	}
	return p_path;
}
