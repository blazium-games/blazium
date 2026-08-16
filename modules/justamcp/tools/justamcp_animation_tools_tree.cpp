/**************************************************************************/
/*  justamcp_animation_tools_tree.cpp                                     */
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

#include "../justamcp_editor_plugin.h"
#include "justamcp_animation_tools.h"

#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/file_system/editor_file_system.h"
#include "scene/2d/navigation/navigation_agent_2d.h"
#include "scene/2d/navigation/navigation_region_2d.h"
#include "scene/3d/navigation/navigation_agent_3d.h"
#include "scene/3d/navigation/navigation_region_3d.h"
#include "scene/animation/animation_blend_space_1d.h"
#include "scene/animation/animation_blend_space_2d.h"
#include "scene/animation/animation_blend_tree.h"
#include "scene/animation/animation_node_state_machine.h"
#include "scene/animation/animation_player.h"
#include "scene/animation/animation_tree.h"
#include "scene/resources/2d/navigation_polygon.h"
#include "scene/resources/animation_library.h"
#include "scene/resources/navigation_mesh.h"
#include "scene/resources/packed_scene.h"

Dictionary JustAMCPAnimationTools::create_animation_tree(const Dictionary &p_args) {
	String scene_path = _ensure_res_path(p_args.get("scenePath", ""));
	String parent_path = p_args.get("parentPath", ".");
	String node_name = p_args.get("nodeName", "AnimationTree");
	String anim_player_path = p_args.get("animPlayerPath", "");
	String root_type = p_args.get("rootType", "StateMachine");

	if (scene_path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Missing scenePath";
		return ret;
	}
	if (anim_player_path.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Missing animPlayerPath";
		return ret;
	}

	Array loaded = _load_scene(scene_path);
	Dictionary err = loaded[1];
	if (!err.is_empty()) {
		return err;
	}

	Node *scene_root = Object::cast_to<Node>(loaded[0]);
	Node *parent = _find_node(scene_root, parent_path);
	if (!parent) {
		memdelete(scene_root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Parent node not found: " + parent_path;
		return ret;
	}

	AnimationTree *anim_tree = memnew(AnimationTree);
	anim_tree->set_name(node_name);
	anim_tree->set_animation_player(NodePath(anim_player_path));

	Ref<AnimationRootNode> root;
	if (root_type == "StateMachine") {
		root.instantiate();
		Ref<AnimationNodeStateMachine> sm;
		sm.instantiate();
		root = sm;
	} else if (root_type == "BlendTree") {
		root.instantiate();
		Ref<AnimationNodeBlendTree> bt;
		bt.instantiate();
		root = bt;
	} else if (root_type == "BlendSpace1D") {
		root.instantiate();

		Ref<AnimationNodeBlendSpace1D> bs1d;
		bs1d.instantiate();
		root = bs1d;
	} else if (root_type == "BlendSpace2D") {
		root.instantiate();
		Ref<AnimationNodeBlendSpace2D> bs2d;
		bs2d.instantiate();
		root = bs2d;
	} else {
		memdelete(scene_root);
		memdelete(anim_tree);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Unsupported rootType: " + root_type;
		return ret;
	}

	anim_tree->set_root_animation_node(root);
	parent->add_child(anim_tree);
	anim_tree->set_owner(scene_root);

	Dictionary save_err = _save_scene(scene_root, scene_path);
	if (!save_err.is_empty()) {
		return save_err;
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["nodeName"] = node_name;
	ret["rootType"] = root_type;
	return ret;
}

Dictionary JustAMCPAnimationTools::add_animation_state(const Dictionary &p_args) {
	String scene_path = _ensure_res_path(p_args.get("scenePath", ""));
	String anim_tree_path = p_args.get("animTreePath", "");
	String state_name = p_args.get("stateName", "");
	String animation_name = p_args.get("animationName", "");
	String state_machine_path = p_args.get("stateMachinePath", "");

	if (scene_path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Missing scenePath";
		return ret;
	}
	if (anim_tree_path.is_empty() || state_name.is_empty() || animation_name.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Missing animTreePath, stateName or animationName";
		return ret;
	}

	Array loaded = _load_scene(scene_path);
	Dictionary err = loaded[1];
	if (!err.is_empty()) {
		return err;
	}

	Node *scene_root = Object::cast_to<Node>(loaded[0]);
	AnimationTree *anim_tree = Object::cast_to<AnimationTree>(_find_node(scene_root, anim_tree_path));
	if (!anim_tree) {
		memdelete(scene_root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "AnimationTree not found at: " + anim_tree_path;
		return ret;
	}

	Ref<AnimationNodeStateMachine> sm = _get_state_machine(anim_tree, state_machine_path);
	if (sm.is_null()) {
		memdelete(scene_root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "AnimationNodeStateMachine not found";
		return ret;
	}

	Ref<AnimationNodeAnimation> anim_node;
	anim_node.instantiate();
	anim_node->set_animation(StringName(animation_name));
	sm->add_node(StringName(state_name), anim_node);

	Dictionary save_err = _save_scene(scene_root, scene_path);
	if (!save_err.is_empty()) {
		return save_err;
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["stateName"] = state_name;
	ret["animationName"] = animation_name;
	return ret;
}

Dictionary JustAMCPAnimationTools::connect_animation_states(const Dictionary &p_args) {
	String scene_path = _ensure_res_path(p_args.get("scenePath", ""));
	String anim_tree_path = p_args.get("animTreePath", "");
	String from_state = p_args.get("fromState", "");
	String to_state = p_args.get("toState", "");
	String transition_type = p_args.get("transitionType", "immediate");
	String state_machine_path = p_args.get("stateMachinePath", "");
	String advance_condition = p_args.get("advanceCondition", "");

	if (scene_path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Missing scenePath";
		return ret;
	}
	if (anim_tree_path.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Missing animTreePath";
		return ret;
	}
	if (from_state.is_empty() || to_state.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Missing fromState or toState";
		return ret;
	}

	Array loaded = _load_scene(scene_path);
	Dictionary err = loaded[1];
	if (!err.is_empty()) {
		return err;
	}

	Node *scene_root = Object::cast_to<Node>(loaded[0]);
	AnimationTree *anim_tree = Object::cast_to<AnimationTree>(_find_node(scene_root, anim_tree_path));
	if (!anim_tree) {
		memdelete(scene_root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "AnimationTree not found at: " + anim_tree_path;
		return ret;
	}

	Ref<AnimationNodeStateMachine> sm = _get_state_machine(anim_tree, state_machine_path);
	if (sm.is_null()) {
		memdelete(scene_root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "AnimationNodeStateMachine not found";
		return ret;
	}

	Ref<AnimationNodeStateMachineTransition> transition;
	transition.instantiate();

	if (transition_type == "sync") {
		transition->set_switch_mode(AnimationNodeStateMachineTransition::SWITCH_MODE_SYNC);
	} else if (transition_type == "at_end") {
		transition->set_switch_mode(AnimationNodeStateMachineTransition::SWITCH_MODE_AT_END);
	} else if (transition_type == "immediate") {
		transition->set_switch_mode(AnimationNodeStateMachineTransition::SWITCH_MODE_IMMEDIATE);
	} else {
		memdelete(scene_root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Unsupported transitionType: " + transition_type;
		return ret;
	}

	if (!advance_condition.is_empty()) {
		transition->set_advance_condition(StringName(advance_condition));
	}

	sm->add_transition(StringName(from_state), StringName(to_state), transition);

	Dictionary save_err = _save_scene(scene_root, scene_path);
	if (!save_err.is_empty()) {
		return save_err;
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["from"] = from_state;
	ret["to"] = to_state;
	return ret;
}

Dictionary JustAMCPAnimationTools::remove_animation_state(const Dictionary &p_args) {
	String scene_path = _ensure_res_path(p_args.get("scenePath", p_args.get("scene_path", "")));
	String anim_tree_path = p_args.get("animTreePath", p_args.get("node_path", ""));
	String state_name = p_args.get("stateName", p_args.get("state_name", ""));
	String state_machine_path = p_args.get("stateMachinePath", p_args.get("state_machine_path", ""));
	if (scene_path == "res://" || anim_tree_path.is_empty() || state_name.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "scenePath, animTreePath, and stateName are required.";
		return ret;
	}

	Array loaded = _load_scene(scene_path);
	Dictionary err = loaded[1];
	if (!err.is_empty()) {
		return err;
	}
	Node *scene_root = Object::cast_to<Node>(loaded[0]);
	AnimationTree *anim_tree = Object::cast_to<AnimationTree>(_find_node(scene_root, anim_tree_path));
	Ref<AnimationNodeStateMachine> sm;
	if (anim_tree) {
		sm = _get_state_machine(anim_tree, state_machine_path);
	}
	if (sm.is_null() || !sm->has_node(StringName(state_name))) {
		memdelete(scene_root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "State not found: " + state_name;
		return ret;
	}

	sm->remove_node(StringName(state_name));
	Dictionary save_err = _save_scene(scene_root, scene_path);
	if (!save_err.is_empty()) {
		return save_err;
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["stateName"] = state_name;
	ret["removed"] = true;
	return ret;
}

Dictionary JustAMCPAnimationTools::remove_animation_transition(const Dictionary &p_args) {
	String scene_path = _ensure_res_path(p_args.get("scenePath", p_args.get("scene_path", "")));
	String anim_tree_path = p_args.get("animTreePath", p_args.get("node_path", ""));
	String from_state = p_args.get("fromState", p_args.get("from_state", ""));
	String to_state = p_args.get("toState", p_args.get("to_state", ""));
	int transition_index = p_args.get("transitionIndex", p_args.get("transition_index", -1));
	String state_machine_path = p_args.get("stateMachinePath", p_args.get("state_machine_path", ""));
	if (scene_path == "res://" || anim_tree_path.is_empty() || (transition_index < 0 && (from_state.is_empty() || to_state.is_empty()))) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "scenePath, animTreePath, and either transitionIndex or fromState/toState are required.";
		return ret;
	}

	Array loaded = _load_scene(scene_path);
	Dictionary err = loaded[1];
	if (!err.is_empty()) {
		return err;
	}
	Node *scene_root = Object::cast_to<Node>(loaded[0]);
	AnimationTree *anim_tree = Object::cast_to<AnimationTree>(_find_node(scene_root, anim_tree_path));
	Ref<AnimationNodeStateMachine> sm;
	if (anim_tree) {
		sm = _get_state_machine(anim_tree, state_machine_path);
	}
	if (sm.is_null()) {
		memdelete(scene_root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "AnimationNodeStateMachine not found.";
		return ret;
	}

	if (transition_index >= 0) {
		if (transition_index >= sm->get_transition_count()) {
			memdelete(scene_root);
			Dictionary ret;
			ret["ok"] = false;
			ret["error"] = "Transition index out of range.";
			return ret;
		}
		sm->remove_transition_by_index(transition_index);
	} else {
		if (!sm->has_transition(StringName(from_state), StringName(to_state))) {
			memdelete(scene_root);
			Dictionary ret;
			ret["ok"] = false;
			ret["error"] = "Transition not found.";
			return ret;
		}
		sm->remove_transition(StringName(from_state), StringName(to_state));
	}

	Dictionary save_err = _save_scene(scene_root, scene_path);
	if (!save_err.is_empty()) {
		return save_err;
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["from"] = from_state;
	ret["to"] = to_state;
	ret["transitionIndex"] = transition_index;
	ret["removed"] = true;
	return ret;
}

Dictionary JustAMCPAnimationTools::set_animation_tree_parameter(const Dictionary &p_args) {
	String scene_path = _ensure_res_path(p_args.get("scenePath", p_args.get("scene_path", "")));
	String anim_tree_path = p_args.get("animTreePath", p_args.get("node_path", ""));
	String parameter = p_args.get("parameter", p_args.get("parameter_name", ""));
	Variant value = _parse_value(_parse_json_maybe(p_args.get("value", Variant())));
	if (scene_path == "res://" || anim_tree_path.is_empty() || parameter.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "scenePath, animTreePath, and parameter are required.";
		return ret;
	}

	Array loaded = _load_scene(scene_path);
	Dictionary err = loaded[1];
	if (!err.is_empty()) {
		return err;
	}
	Node *scene_root = Object::cast_to<Node>(loaded[0]);
	AnimationTree *anim_tree = Object::cast_to<AnimationTree>(_find_node(scene_root, anim_tree_path));
	if (!anim_tree) {
		memdelete(scene_root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "AnimationTree not found at: " + anim_tree_path;
		return ret;
	}

	anim_tree->set("parameters/" + parameter, value);
	Dictionary save_err = _save_scene(scene_root, scene_path);
	if (!save_err.is_empty()) {
		return save_err;
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["parameter"] = parameter;
	ret["value"] = value;
	return ret;
}

Dictionary JustAMCPAnimationTools::set_blend_tree_node(const Dictionary &p_args) {
	String scene_path = _ensure_res_path(p_args.get("scenePath", p_args.get("scene_path", "")));
	String anim_tree_path = p_args.get("animTreePath", p_args.get("node_path", ""));
	String blend_tree_path = p_args.get("blendTreePath", p_args.get("blend_tree_path", ""));
	String node_name = p_args.get("nodeName", p_args.get("node_name", ""));
	String animation_name = p_args.get("animationName", p_args.get("animation_name", ""));
	Vector2 position;
	if (p_args.has("position")) {
		Variant parsed_position = _parse_value(p_args["position"]);
		if (parsed_position.get_type() == Variant::VECTOR2) {
			position = parsed_position;
		}
	}
	if (scene_path == "res://" || anim_tree_path.is_empty() || node_name.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "scenePath, animTreePath, and nodeName are required.";
		return ret;
	}

	Array loaded = _load_scene(scene_path);
	Dictionary err = loaded[1];
	if (!err.is_empty()) {
		return err;
	}
	Node *scene_root = Object::cast_to<Node>(loaded[0]);
	AnimationTree *anim_tree = Object::cast_to<AnimationTree>(_find_node(scene_root, anim_tree_path));
	Ref<AnimationNode> target;
	if (anim_tree) {
		target = _get_state_machine(anim_tree, blend_tree_path);
	}
	Ref<AnimationNodeBlendTree> blend_tree = target;
	if (blend_tree.is_null()) {
		memdelete(scene_root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "AnimationNodeBlendTree not found.";
		return ret;
	}

	Ref<AnimationNodeAnimation> anim_node;
	anim_node.instantiate();
	if (!animation_name.is_empty()) {
		anim_node->set_animation(StringName(animation_name));
	}
	if (blend_tree->has_node(StringName(node_name))) {
		blend_tree->remove_node(StringName(node_name));
	}
	blend_tree->add_node(StringName(node_name), anim_node, position);

	Dictionary save_err = _save_scene(scene_root, scene_path);
	if (!save_err.is_empty()) {
		return save_err;
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["nodeName"] = node_name;
	ret["animationName"] = animation_name;
	return ret;
}
