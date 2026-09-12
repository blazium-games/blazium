/**************************************************************************/
/*  justamcp_animation_tools_read.cpp                                     */
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

#include "../justamcp_editor_filesystem.h"
#include "../justamcp_editor_plugin.h"
#include "../justamcp_editor_scene_access.h"
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

void JustAMCPAnimationTools::_refresh_and_reload(const String &p_scene_path) {
	_refresh_filesystem(p_scene_path);
	_reload_scene_in_editor(p_scene_path);
}

void JustAMCPAnimationTools::_refresh_filesystem(const String &p_path) {
	JustAMCPEditorFilesystem::refresh_path(p_path);
}

void JustAMCPAnimationTools::_reload_scene_in_editor(const String &p_scene_path) {
	if (!editor_plugin) {
		return;
	}
	Node *edited = JustAMCPEditorSceneAccess::get_edited_root();
	if (edited && edited->get_scene_file_path() == p_scene_path) {
		EditorInterface::get_singleton()->reload_scene_from_path(p_scene_path);
	}
}

Array JustAMCPAnimationTools::_load_scene(const String &p_scene_path) {
	Array ret;
	ret.resize(2);
	ret[0] = (Object *)nullptr;
	Dictionary err;

	if (!FileAccess::exists(p_scene_path)) {
		err["ok"] = false;
		err["error"] = "Scene not found: " + p_scene_path;
		ret[1] = err;
		return ret;
	}

	Ref<PackedScene> packed = ResourceLoader::load(p_scene_path);
	if (packed.is_null()) {
		err["ok"] = false;
		err["error"] = "Failed to load: " + p_scene_path;
		ret[1] = err;
		return ret;
	}

	Node *root = packed->instantiate();
	if (!root) {
		err["ok"] = false;
		err["error"] = "Failed to instantiate: " + p_scene_path;
		ret[1] = err;
		return ret;
	}

	ret[0] = root;
	ret[1] = Dictionary();
	return ret;
}

Dictionary JustAMCPAnimationTools::_save_scene(Node *p_scene_root, const String &p_scene_path) {
	Dictionary ret;
	Ref<PackedScene> packed;
	packed.instantiate();
	if (packed->pack(p_scene_root) != OK) {
		memdelete(p_scene_root);
		ret["ok"] = false;
		ret["error"] = "Failed to pack scene";
		return ret;
	}
	if (ResourceSaver::save(packed, p_scene_path) != OK) {
		memdelete(p_scene_root);
		ret["ok"] = false;
		ret["error"] = "Failed to save scene";
		return ret;
	}
	memdelete(p_scene_root);
	_refresh_and_reload(p_scene_path);
	return Dictionary();
}

Node *JustAMCPAnimationTools::_find_node(Node *p_root, const String &p_path) {
	if (p_path == "." || p_path.is_empty()) {
		return p_root;
	}
	return p_root->get_node_or_null(p_path);
}

Variant JustAMCPAnimationTools::_parse_value(const Variant &p_value) {
	if (p_value.get_type() == Variant::DICTIONARY) {
		Dictionary value = p_value;
		String t;
		if (value.has("type")) {
			t = value["type"];
		} else if (value.has("_type")) {
			t = value["_type"];
		}
		if (!t.is_empty()) {
			if (t == "Vector2") {
				return Vector2(value.get("x", 0), value.get("y", 0));
			}
			if (t == "Vector3") {
				return Vector3(value.get("x", 0), value.get("y", 0), value.get("z", 0));
			}
			if (t == "Color") {
				return Color(value.get("r", 1), value.get("g", 1), value.get("b", 1), value.get("a", 1));
			}
		}
	} else if (p_value.get_type() == Variant::ARRAY) {
		Array arr = p_value;
		Array result;
		for (int i = 0; i < arr.size(); i++) {
			result.push_back(_parse_value(arr[i]));
		}
		return result;
	}
	return p_value;
}

Variant JustAMCPAnimationTools::_parse_json_maybe(const Variant &p_value) {
	if (p_value.get_type() != Variant::STRING) {
		return p_value;
	}
	String text = p_value;
	Ref<JSON> json;
	json.instantiate();
	if (json->parse(text) == OK) {
		return json->get_data();
	}
	if (text == "null") {
		return Variant();
	}
	return p_value;
}

Array JustAMCPAnimationTools::_parse_method_args(const Array &p_raw_args) {
	Array parsed_args;
	for (int i = 0; i < p_raw_args.size(); i++) {
		Variant parsed = _parse_json_maybe(p_raw_args[i]);
		parsed_args.push_back(_parse_value(parsed));
	}
	return parsed_args;
}

Ref<Resource> JustAMCPAnimationTools::_get_default_animation_library(Node *p_player) {
	AnimationPlayer *player = Object::cast_to<AnimationPlayer>(p_player);
	if (!player) {
		return Ref<Resource>();
	}

	Ref<AnimationLibrary> anim_lib;
	if (player->has_animation_library("")) {
		anim_lib = player->get_animation_library("");
		if (anim_lib.is_valid()) {
			return anim_lib;
		}
	}

	anim_lib.instantiate();
	Error err = player->add_animation_library("", anim_lib);
	if (err != OK) {
		return Ref<Resource>();
	}
	return anim_lib;
}

Ref<Resource> JustAMCPAnimationTools::_get_state_machine(Node *p_anim_tree, const String &p_state_machine_path) {
	AnimationTree *anim_tree = Object::cast_to<AnimationTree>(p_anim_tree);
	if (!anim_tree) {
		return Ref<Resource>();
	}

	Ref<AnimationNode> current = anim_tree->get_root_animation_node();
	if (p_state_machine_path.is_empty() || p_state_machine_path == "root") {
		return current;
	}

	Vector<String> segments = p_state_machine_path.split("/", false);
	for (int i = 0; i < segments.size(); i++) {
		String segment = segments[i];
		if (segment.is_empty()) {
			continue;
		}

		Ref<AnimationNodeStateMachine> sm = current;
		Ref<AnimationNodeBlendTree> bt = current;

		if (sm.is_valid()) {
			if (sm->has_node(StringName(segment))) {
				current = sm->get_node(StringName(segment));
			} else {
				return Ref<Resource>();
			}
		} else if (bt.is_valid()) {
			if (bt->has_node(StringName(segment))) {
				current = bt->get_node(StringName(segment));
			} else {
				return Ref<Resource>();
			}
		} else {
			return Ref<Resource>();
		}
	}
	return current;
}

Dictionary JustAMCPAnimationTools::_serialize_animation_node(const Ref<AnimationNode> &p_node) {
	Dictionary info;
	if (p_node.is_null()) {
		info["type"] = "null";
		return info;
	}

	Ref<AnimationNodeStateMachine> sm = p_node;
	if (sm.is_valid()) {
		return _serialize_state_machine(sm);
	}

	Ref<AnimationNodeBlendTree> bt = p_node;
	if (bt.is_valid()) {
		return _serialize_blend_tree(bt);
	}

	Ref<AnimationNodeAnimation> anim = p_node;
	info["type"] = p_node->get_class();
	if (anim.is_valid()) {
		info["animation"] = String(anim->get_animation());
	}
	return info;
}

Dictionary JustAMCPAnimationTools::_serialize_state_machine(const Ref<AnimationNodeStateMachine> &p_state_machine) {
	Dictionary info;
	info["type"] = "AnimationNodeStateMachine";

	Array states;
	List<StringName> node_names;
	p_state_machine->get_node_list(&node_names);
	for (const StringName &name : node_names) {
		Ref<AnimationNode> child = p_state_machine->get_node(name);
		Dictionary state = _serialize_animation_node(child);
		state["name"] = String(name);
		Vector2 position = p_state_machine->get_node_position(name);
		Dictionary pos;
		pos["x"] = position.x;
		pos["y"] = position.y;
		state["position"] = pos;
		states.push_back(state);
	}

	Array transitions;
	for (int i = 0; i < p_state_machine->get_transition_count(); i++) {
		Ref<AnimationNodeStateMachineTransition> transition = p_state_machine->get_transition(i);
		Dictionary item;
		item["index"] = i;
		item["from"] = String(p_state_machine->get_transition_from(i));
		item["to"] = String(p_state_machine->get_transition_to(i));
		if (transition.is_valid()) {
			item["switch_mode"] = int(transition->get_switch_mode());
			item["advance_mode"] = int(transition->get_advance_mode());
			item["advance_condition"] = String(transition->get_advance_condition());
			item["advance_expression"] = transition->get_advance_expression();
		}
		transitions.push_back(item);
	}

	info["states"] = states;
	info["transitions"] = transitions;
	return info;
}

Dictionary JustAMCPAnimationTools::_serialize_blend_tree(const Ref<AnimationNodeBlendTree> &p_blend_tree) {
	Dictionary info;
	info["type"] = "AnimationNodeBlendTree";

	Array nodes;
	List<StringName> node_names;
	p_blend_tree->get_node_list(&node_names);
	for (const StringName &name : node_names) {
		Ref<AnimationNode> child = p_blend_tree->get_node(name);
		Dictionary node = _serialize_animation_node(child);
		node["name"] = String(name);
		Vector2 position = p_blend_tree->get_node_position(name);
		Dictionary pos;
		pos["x"] = position.x;
		pos["y"] = position.y;
		node["position"] = pos;
		nodes.push_back(node);
	}

	info["nodes"] = nodes;
	return info;
}

Dictionary JustAMCPAnimationTools::get_animation_info(const Dictionary &p_args) {
	String scene_path = _ensure_res_path(p_args.get("scenePath", ""));
	String player_node_path = p_args.get("playerNodePath", ".");
	String animation_name = p_args.get("animationName", "");
	if (scene_path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "scenePath is required.";
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
		ret["error"] = "AnimationPlayer not found.";
		return ret;
	}

	List<StringName> names;
	player->get_animation_list(&names);
	Array animations;
	for (const StringName &name : names) {
		if (!animation_name.is_empty() && String(name) != animation_name) {
			continue;
		}
		Ref<Animation> anim = player->get_animation(name);
		Dictionary info;
		info["name"] = String(name);
		info["length"] = anim.is_valid() ? anim->get_length() : 0.0;
		info["track_count"] = anim.is_valid() ? anim->get_track_count() : 0;
		info["loop_mode"] = anim.is_valid() ? int(anim->get_loop_mode()) : 0;
		Array tracks;
		if (anim.is_valid()) {
			for (int i = 0; i < anim->get_track_count(); i++) {
				Dictionary t;
				t["index"] = i;
				t["path"] = String(anim->track_get_path(i));
				t["type"] = int(anim->track_get_type(i));
				t["key_count"] = anim->track_get_key_count(i);
				tracks.push_back(t);
			}
		}
		info["tracks"] = tracks;
		animations.push_back(info);
	}
	memdelete(scene_root);

	Dictionary ret;
	ret["ok"] = true;
	ret["playerNodePath"] = player_node_path;
	ret["animations"] = animations;
	ret["count"] = animations.size();
	return ret;
}

Dictionary JustAMCPAnimationTools::list_animations(const Dictionary &p_args) {
	Dictionary info_args = p_args;
	info_args.erase("animationName");
	Dictionary ret = get_animation_info(info_args);
	if (!ret.get("ok", false)) {
		return ret;
	}

	Array names;
	Array animations = ret.get("animations", Array());
	for (int i = 0; i < animations.size(); i++) {
		Dictionary animation = animations[i];
		names.push_back(animation.get("name", ""));
	}
	ret["names"] = names;
	return ret;
}

Dictionary JustAMCPAnimationTools::get_animation_tree_structure(const Dictionary &p_args) {
	String scene_path = _ensure_res_path(p_args.get("scenePath", p_args.get("scene_path", "")));
	String anim_tree_path = p_args.get("animTreePath", p_args.get("node_path", ""));
	if (scene_path == "res://" || anim_tree_path.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "scenePath and animTreePath are required.";
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

	Ref<AnimationNode> root = anim_tree->get_root_animation_node();
	Dictionary structure = _serialize_animation_node(root);
	structure["node_path"] = anim_tree_path;
	structure["anim_player"] = String(anim_tree->get_animation_player());
	structure["active"] = anim_tree->is_active();
	memdelete(scene_root);

	Dictionary ret;
	ret["ok"] = true;
	ret["structure"] = structure;
	return ret;
}
