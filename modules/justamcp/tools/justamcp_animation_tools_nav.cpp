/**************************************************************************/
/*  justamcp_animation_tools_nav.cpp                                      */
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

Dictionary JustAMCPAnimationTools::create_navigation_region(const Dictionary &p_args) {
	String scene_path = _ensure_res_path(p_args.get("scenePath", ""));
	String parent_path = p_args.get("parentPath", ".");
	String node_name = p_args.get("nodeName", "NavigationRegion");
	bool is_3d = p_args.get("is3D", false);

	if (scene_path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Missing scenePath";
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

	Node *nav = nullptr;
	if (is_3d) {
		NavigationRegion3D *nav3d = memnew(NavigationRegion3D);
		Ref<NavigationMesh> nmesh;
		nmesh.instantiate();
		nav3d->set_navigation_mesh(nmesh);
		nav = nav3d;
	} else {
		NavigationRegion2D *nav2d = memnew(NavigationRegion2D);
		Ref<NavigationPolygon> npoly;
		npoly.instantiate();
		nav2d->set_navigation_polygon(npoly);
		nav = nav2d;
	}

	nav->set_name(node_name);
	parent->add_child(nav);
	nav->set_owner(scene_root);

	Dictionary save_err = _save_scene(scene_root, scene_path);
	if (!save_err.is_empty()) {
		memdelete(scene_root);
		return save_err;
	}

	memdelete(scene_root);
	Dictionary ret;
	ret["ok"] = true;
	ret["nodeName"] = node_name;
	ret["is3D"] = is_3d;
	return ret;
}

Dictionary JustAMCPAnimationTools::create_navigation_agent(const Dictionary &p_args) {
	String scene_path = _ensure_res_path(p_args.get("scenePath", ""));
	String parent_path = p_args.get("parentPath", ".");
	String node_name = p_args.get("nodeName", "NavigationAgent");
	bool is_3d = p_args.get("is3D", false);

	if (scene_path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Missing scenePath";
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

	Node *agent = nullptr;
	if (is_3d) {
		NavigationAgent3D *agent3d = memnew(NavigationAgent3D);
		agent3d->set_name(node_name);
		if (p_args.has("pathDesiredDistance")) {
			agent3d->set_path_desired_distance((float)p_args["pathDesiredDistance"]);
		}
		if (p_args.has("targetDesiredDistance")) {
			agent3d->set_target_desired_distance((float)p_args["targetDesiredDistance"]);
		}
		agent = agent3d;
	} else {
		NavigationAgent2D *agent2d = memnew(NavigationAgent2D);
		agent2d->set_name(node_name);
		if (p_args.has("pathDesiredDistance")) {
			agent2d->set_path_desired_distance((float)p_args["pathDesiredDistance"]);
		}
		if (p_args.has("targetDesiredDistance")) {
			agent2d->set_target_desired_distance((float)p_args["targetDesiredDistance"]);
		}
		agent = agent2d;
	}

	parent->add_child(agent);
	agent->set_owner(scene_root);

	Dictionary save_err = _save_scene(scene_root, scene_path);
	if (!save_err.is_empty()) {
		memdelete(scene_root);
		return save_err;
	}

	memdelete(scene_root);
	Dictionary ret;
	ret["ok"] = true;
	ret["nodeName"] = node_name;
	ret["is3D"] = is_3d;
	return ret;
}

Dictionary JustAMCPAnimationTools::create_tween(const Dictionary &p_args) {
	String node_path = p_args.get("nodePath", ".");
	String property = p_args.get("property", "");
	Variant final_value = _parse_value(p_args.get("finalValue", Variant()));
	float duration = p_args.get("duration", 1.0);

	if (property.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Missing property";
		return ret;
	}

	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!tree) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "No SceneTree available at runtime";
		return ret;
	}

	Node *root = tree->get_root();
	Node *target = root->get_node_or_null(NodePath(node_path));
	if (!target) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Node not found at Path: " + node_path;
		return ret;
	}

	Ref<Tween> tween = target->create_tween();
	if (tween.is_null()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to create tween";
		return ret;
	}

	String trans_name = p_args.get("transitionType", "linear");
	String ease_name = p_args.get("easeType", "in_out");

	Tween::TransitionType trans = Tween::TRANS_LINEAR;
	if (trans_name == "sine") {
		trans = Tween::TRANS_SINE;
	} else if (trans_name == "quint") {
		trans = Tween::TRANS_QUINT;
	} else if (trans_name == "quart") {
		trans = Tween::TRANS_QUART;
	} else if (trans_name == "quad") {
		trans = Tween::TRANS_QUAD;
	} else if (trans_name == "expo") {
		trans = Tween::TRANS_EXPO;
	} else if (trans_name == "elastic") {
		trans = Tween::TRANS_ELASTIC;
	} else if (trans_name == "cubic") {
		trans = Tween::TRANS_CUBIC;
	} else if (trans_name == "circ") {
		trans = Tween::TRANS_CIRC;
	} else if (trans_name == "bounce") {
		trans = Tween::TRANS_BOUNCE;
	} else if (trans_name == "back") {
		trans = Tween::TRANS_BACK;
	}

	Tween::EaseType ease = Tween::EASE_IN_OUT;
	if (ease_name == "in") {
		ease = Tween::EASE_IN;
	} else if (ease_name == "out") {
		ease = Tween::EASE_OUT;
	} else if (ease_name == "out_in") {
		ease = Tween::EASE_OUT_IN;
	}

	tween->set_trans(trans);
	tween->set_ease(ease);
	tween->tween_property(target, NodePath(property), final_value, duration);

	Dictionary ret;
	ret["ok"] = true;
	ret["message"] = vformat("Tweening %s:%s to %s over %f seconds", node_path, property, String(final_value), duration);
	return ret;
}
