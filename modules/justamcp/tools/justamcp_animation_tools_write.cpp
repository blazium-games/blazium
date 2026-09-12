/**************************************************************************/
/*  justamcp_animation_tools_write.cpp                                    */
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
#include "editor/editor_file_system.h"
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

#include "scene/2d/navigation_agent_2d.h"
#include "scene/2d/navigation_region_2d.h"
#include "scene/3d/navigation_agent_3d.h"
#include "scene/3d/navigation_region_3d.h"
#include "scene/resources/2d/navigation_polygon.h"
#include "scene/resources/navigation_mesh.h"

Dictionary JustAMCPAnimationTools::create_animation(const Dictionary &p_args) {
	String scene_path = _ensure_res_path(p_args.get("scenePath", ""));
	String player_node_path = p_args.get("playerNodePath", ".");
	String animation_name = p_args.get("animationName", "");
	String loop_mode_name = p_args.get("loopMode", "none");

	if (scene_path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Missing scenePath";
		return ret;
	}
	if (animation_name.strip_edges().is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Missing animationName";
		return ret;
	}

	Array loaded = _load_scene(scene_path);
	Dictionary err = loaded[1];
	if (!err.is_empty()) {
		return err;
	}

	Node *scene_root = Object::cast_to<Node>(loaded[0]);
	AnimationPlayer *player = Object::cast_to<AnimationPlayer>(_find_node(scene_root, player_node_path));
	if (!player) {
		memdelete(scene_root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "AnimationPlayer not found at: " + player_node_path;
		return ret;
	}

	Ref<AnimationLibrary> anim_lib = _get_default_animation_library(player);
	if (anim_lib.is_null()) {
		memdelete(scene_root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to create default AnimationLibrary";
		return ret;
	}
	if (anim_lib->has_animation(StringName(animation_name))) {
		memdelete(scene_root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Animation already exists: " + animation_name;
		return ret;
	}

	Animation::LoopMode loop_mode = Animation::LOOP_NONE;
	if (loop_mode_name == "linear") {
		loop_mode = Animation::LOOP_LINEAR;
	} else if (loop_mode_name == "pingpong") {
		loop_mode = Animation::LOOP_PINGPONG;
	}

	Ref<Animation> anim;
	anim.instantiate();
	anim->set_length(p_args.get("length", 1.0));
	anim->set_loop_mode(loop_mode);
	anim->set_step(p_args.get("step", 0.1));

	Error add_err = anim_lib->add_animation(StringName(animation_name), anim);
	if (add_err != OK) {
		memdelete(scene_root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to add animation: " + itos(add_err);
		return ret;
	}

	Dictionary save_err = _save_scene(scene_root, scene_path);
	if (!save_err.is_empty()) {
		return save_err;
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["animationName"] = animation_name;
	ret["length"] = anim->get_length();
	ret["loopMode"] = loop_mode_name;
	return ret;
}

Dictionary JustAMCPAnimationTools::set_animation_keyframe(const Dictionary &p_args) {
	String scene_path = _ensure_res_path(p_args.get("scenePath", ""));
	String player_node_path = p_args.get("playerNodePath", ".");
	String animation_name = p_args.get("animationName", "");
	String node_path = p_args.get("nodePath", "");
	String property = p_args.get("property", "");
	double time = p_args.get("time", 0.0);
	Variant value = _parse_value(_parse_json_maybe(p_args.get("value", Variant())));

	if (scene_path == "res://" || animation_name.is_empty() || node_path.is_empty() || property.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "scenePath, animationName, nodePath and property are required.";
		return ret;
	}

	Array loaded = _load_scene(scene_path);
	Dictionary err = loaded[1];
	if (!err.is_empty()) {
		return err;
	}
	Node *scene_root = Object::cast_to<Node>(loaded[0]);
	AnimationPlayer *player = Object::cast_to<AnimationPlayer>(_find_node(scene_root, player_node_path));
	if (!player || !player->has_animation(animation_name)) {
		memdelete(scene_root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "AnimationPlayer or animation not found.";
		return ret;
	}

	Ref<Animation> anim = player->get_animation(animation_name);
	NodePath track_path(node_path + ":" + property);
	int track_idx = -1;
	for (int i = 0; i < anim->get_track_count(); i++) {
		if (anim->track_get_path(i) == track_path) {
			track_idx = i;
			break;
		}
	}
	if (track_idx < 0) {
		track_idx = anim->add_track(Animation::TYPE_VALUE);
		anim->track_set_path(track_idx, track_path);
	}
	int key_idx = anim->track_insert_key(track_idx, time, value);

	Dictionary save_err = _save_scene(scene_root, scene_path);
	if (!save_err.is_empty()) {
		return save_err;
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["animationName"] = animation_name;
	ret["trackIndex"] = track_idx;
	ret["keyIndex"] = key_idx;
	ret["time"] = time;
	return ret;
}

Dictionary JustAMCPAnimationTools::remove_animation(const Dictionary &p_args) {
	String scene_path = _ensure_res_path(p_args.get("scenePath", ""));
	String player_node_path = p_args.get("playerNodePath", ".");
	String animation_name = p_args.get("animationName", "");
	if (scene_path == "res://" || animation_name.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "scenePath and animationName are required.";
		return ret;
	}

	Array loaded = _load_scene(scene_path);
	Dictionary err = loaded[1];
	if (!err.is_empty()) {
		return err;
	}
	Node *scene_root = Object::cast_to<Node>(loaded[0]);
	AnimationPlayer *player = Object::cast_to<AnimationPlayer>(_find_node(scene_root, player_node_path));
	Ref<AnimationLibrary> anim_lib = player ? player->get_animation_library("") : Ref<AnimationLibrary>();
	if (anim_lib.is_null() || !anim_lib->has_animation(animation_name)) {
		memdelete(scene_root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Animation not found: " + animation_name;
		return ret;
	}
	anim_lib->remove_animation(animation_name);

	Dictionary save_err = _save_scene(scene_root, scene_path);
	if (!save_err.is_empty()) {
		return save_err;
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["animationName"] = animation_name;
	ret["removed"] = true;
	return ret;
}

Dictionary JustAMCPAnimationTools::add_animation_track(const Dictionary &p_args) {
	String scene_path = _ensure_res_path(p_args.get("scenePath", ""));
	String player_node_path = p_args.get("playerNodePath", ".");
	String animation_name = p_args.get("animationName", "");
	Dictionary track = p_args.get("track", Dictionary());

	if (scene_path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Missing scenePath";
		return ret;
	}
	if (animation_name.strip_edges().is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Missing animationName";
		return ret;
	}
	if (track.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Missing track";
		return ret;
	}

	Array loaded = _load_scene(scene_path);
	Dictionary err = loaded[1];
	if (!err.is_empty()) {
		return err;
	}

	Node *scene_root = Object::cast_to<Node>(loaded[0]);
	AnimationPlayer *player = Object::cast_to<AnimationPlayer>(_find_node(scene_root, player_node_path));
	if (!player) {
		memdelete(scene_root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "AnimationPlayer not found at: " + player_node_path;
		return ret;
	}

	Ref<AnimationLibrary> anim_lib;
	if (player->has_animation_library("")) {
		anim_lib = player->get_animation_library("");
	}
	if (anim_lib.is_null()) {
		memdelete(scene_root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Default AnimationLibrary not found";
		return ret;
	}

	Ref<Animation> anim;
	if (anim_lib->has_animation(StringName(animation_name))) {
		anim = anim_lib->get_animation(StringName(animation_name));
	}
	if (anim.is_null()) {
		memdelete(scene_root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Animation not found: " + animation_name;
		return ret;
	}

	String track_type = track.get("type", "");
	int track_idx = -1;
	Array keyframes = track.get("keyframes", Array());

	if (track_type == "property") {
		String node_path_str = track.get("nodePath", "");
		String prop_name = track.get("property", "");
		if (prop_name.is_empty()) {
			memdelete(scene_root);
			Dictionary ret;
			ret["ok"] = false;
			ret["error"] = "track.property is required for property track";
			return ret;
		}
		track_idx = anim->add_track(Animation::TYPE_VALUE);
		anim->track_set_path(track_idx, NodePath(node_path_str + ":" + prop_name));
		for (int i = 0; i < keyframes.size(); i++) {
			if (keyframes[i].get_type() != Variant::DICTIONARY) {
				continue;
			}
			Dictionary keyframe = keyframes[i];
			Variant raw_value = keyframe.get("value", Variant());
			Variant parsed_value = (raw_value.get_type() == Variant::STRING) ? _parse_json_maybe(raw_value) : raw_value;
			anim->track_insert_key(track_idx, (float)keyframe.get("time", 0.0), _parse_value(parsed_value));
		}
	} else if (track_type == "method") {
		String method_node_path = track.get("nodePath", "");
		String method_name = track.get("method", "");
		if (method_name.is_empty()) {
			memdelete(scene_root);
			Dictionary ret;
			ret["ok"] = false;
			ret["error"] = "track.method is required for method track";
			return ret;
		}
		track_idx = anim->add_track(Animation::TYPE_METHOD);
		anim->track_set_path(track_idx, NodePath(method_node_path));
		for (int i = 0; i < keyframes.size(); i++) {
			if (keyframes[i].get_type() != Variant::DICTIONARY) {
				continue;
			}
			Dictionary keyframe = keyframes[i];
			Dictionary m_dict;
			m_dict["method"] = method_name;
			m_dict["args"] = _parse_method_args(keyframe.get("args", Array()));
			anim->track_insert_key(track_idx, (float)keyframe.get("time", 0.0), m_dict);
		}
	} else {
		memdelete(scene_root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Unsupported track.type: " + track_type;
		return ret;
	}

	Dictionary save_err = _save_scene(scene_root, scene_path);
	if (!save_err.is_empty()) {
		return save_err;
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["trackType"] = track_type;
	ret["trackIndex"] = track_idx;
	return ret;
}
