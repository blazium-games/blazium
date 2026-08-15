/**************************************************************************/
/*  justamcp_animation_tools.h                                            */
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

#include "core/object/object.h"
#include "scene/main/node.h"

class JustAMCPEditorPlugin;
class AnimationNode;
class AnimationNodeBlendTree;
class AnimationNodeStateMachine;

class JustAMCPAnimationTools : public Object {
	GDCLASS(JustAMCPAnimationTools, Object);

private:
	JustAMCPEditorPlugin *editor_plugin = nullptr;

protected:
	static void _bind_methods();

public:
	void set_editor_plugin(JustAMCPEditorPlugin *p_plugin) { editor_plugin = p_plugin; }
	Dictionary execute_tool(const String &p_tool_name, const Dictionary &p_args);

	Dictionary create_animation(const Dictionary &p_args);
	Dictionary set_animation_keyframe(const Dictionary &p_args);
	Dictionary get_animation_info(const Dictionary &p_args);
	Dictionary list_animations(const Dictionary &p_args);
	Dictionary remove_animation(const Dictionary &p_args);
	Dictionary add_animation_track(const Dictionary &p_args);
	Dictionary create_animation_tree(const Dictionary &p_args);
	Dictionary get_animation_tree_structure(const Dictionary &p_args);
	Dictionary add_animation_state(const Dictionary &p_args);
	Dictionary remove_animation_state(const Dictionary &p_args);
	Dictionary connect_animation_states(const Dictionary &p_args);
	Dictionary remove_animation_transition(const Dictionary &p_args);
	Dictionary set_animation_tree_parameter(const Dictionary &p_args);
	Dictionary set_blend_tree_node(const Dictionary &p_args);
	Dictionary create_navigation_region(const Dictionary &p_args);
	Dictionary create_navigation_agent(const Dictionary &p_args);
	Dictionary create_tween(const Dictionary &p_args);

private:
	String _ensure_res_path(const String &p_path);
	void _refresh_and_reload(const String &p_scene_path);
	void _refresh_filesystem(const String &p_path = String());
	void _reload_scene_in_editor(const String &p_scene_path);

	Array _load_scene(const String &p_scene_path);
	Dictionary _save_scene(Node *p_scene_root, const String &p_scene_path);
	Node *_find_node(Node *p_root, const String &p_path);

	Variant _parse_value(const Variant &p_value);
	Variant _parse_json_maybe(const Variant &p_value);
	Array _parse_method_args(const Array &p_raw_args);

	Ref<Resource> _get_default_animation_library(Node *p_player);
	Ref<Resource> _get_state_machine(Node *p_anim_tree, const String &p_state_machine_path = "");
	Dictionary _serialize_animation_node(const Ref<AnimationNode> &p_node);
	Dictionary _serialize_state_machine(const Ref<AnimationNodeStateMachine> &p_state_machine);
	Dictionary _serialize_blend_tree(const Ref<AnimationNodeBlendTree> &p_blend_tree);

public:
	JustAMCPAnimationTools();
	~JustAMCPAnimationTools();
};
