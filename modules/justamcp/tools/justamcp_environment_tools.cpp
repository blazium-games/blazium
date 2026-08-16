/**************************************************************************/
/*  justamcp_environment_tools.cpp                                        */
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

#include "justamcp_environment_tools.h"

#include "../justamcp_editor_plugin.h"
#include "../justamcp_editor_scene_access.h"

#include "editor/editor_interface.h"
#include "editor/editor_undo_redo_manager.h"
#include "scene/3d/world_environment.h"
#include "scene/main/node.h"
#include "scene/main/viewport.h" // IWYU pragma: keep
#include "scene/resources/3d/sky_material.h"
#include "scene/resources/environment.h"
#include "scene/resources/sky.h"

void JustAMCPEnvironmentTools::_bind_methods() {}

Dictionary JustAMCPEnvironmentTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "create_environment" || p_tool_name == "environment_create") {
		return create_environment(p_args);
	}

	return Dictionary();
}

Dictionary JustAMCPEnvironmentTools::create_environment(const Dictionary &p_args) {
	String node_path = p_args.get("path", "");
	String preset = p_args.get("preset", "default");
	bool want_sky = p_args.get("sky", true);

	Node *scene_root = nullptr;
	if (editor_plugin && editor_plugin->get_editor_interface()) {
		scene_root = JustAMCPEditorSceneAccess::get_edited_root();
	}

	if (!scene_root) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "No open scene.";
		return ret;
	}

	Node *node = scene_root->get_node_or_null(NodePath(node_path));
	if (!node) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Node not found at path: " + node_path;
		return ret;
	}

	WorldEnvironment *we = Object::cast_to<WorldEnvironment>(node);
	if (!we) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Target node must be a WorldEnvironment, got " + node->get_class();
		return ret;
	}

	Ref<Environment> env;
	env.instantiate();

	Ref<ProceduralSkyMaterial> sky_mat;
	Ref<Sky> sky;

	if (want_sky) {
		sky_mat.instantiate();
		sky.instantiate();
		sky->set_material(sky_mat);
		env->set_background(Environment::BG_SKY);
		env->set_sky(sky);
	} else {
		env->set_background(Environment::BG_CLEAR_COLOR);
	}

	if (preset == "sunset") {
		if (sky_mat.is_valid()) {
			sky_mat->set_sky_top_color(Color(0.25, 0.3, 0.55));
			sky_mat->set_sky_horizon_color(Color(1.0, 0.55, 0.3));
			sky_mat->set_ground_horizon_color(Color(0.85, 0.4, 0.25));
			sky_mat->set_ground_bottom_color(Color(0.2, 0.12, 0.1));
		}
		env->set_ambient_source(Environment::AMBIENT_SOURCE_SKY);
		env->set_ambient_light_color(Color(1.0, 0.75, 0.55));
		env->set_ambient_light_energy(0.8);
	} else if (preset == "night") {
		if (sky_mat.is_valid()) {
			sky_mat->set_sky_top_color(Color(0.02, 0.02, 0.07));
			sky_mat->set_sky_horizon_color(Color(0.05, 0.07, 0.15));
			sky_mat->set_ground_horizon_color(Color(0.04, 0.05, 0.1));
			sky_mat->set_ground_bottom_color(Color(0.0, 0.0, 0.02));
		}
		env->set_ambient_source(Environment::AMBIENT_SOURCE_COLOR);
		env->set_ambient_light_color(Color(0.2, 0.22, 0.35));
		env->set_ambient_light_energy(0.4);
	} else if (preset == "fog") {
		if (sky_mat.is_valid()) {
			sky_mat->set_sky_top_color(Color(0.65, 0.65, 0.7));
			sky_mat->set_sky_horizon_color(Color(0.8, 0.8, 0.82));
			sky_mat->set_ground_horizon_color(Color(0.7, 0.7, 0.72));
			sky_mat->set_ground_bottom_color(Color(0.3, 0.3, 0.32));
		}
		env->set_ambient_source(Environment::AMBIENT_SOURCE_SKY);
		env->set_ambient_light_energy(0.7);
		env->set_volumetric_fog_enabled(true);
		env->set_volumetric_fog_density(0.03);
	} else {
		if (sky_mat.is_valid()) {
			sky_mat->set_sky_top_color(Color(0.38, 0.45, 0.55));
			sky_mat->set_sky_horizon_color(Color(0.65, 0.67, 0.7));
			sky_mat->set_ground_horizon_color(Color(0.65, 0.67, 0.7));
			sky_mat->set_ground_bottom_color(Color(0.2, 0.17, 0.13));
		}
		env->set_ambient_source(Environment::AMBIENT_SOURCE_SKY);
		env->set_ambient_light_energy(1.0);
	}

	EditorUndoRedoManager *ur = nullptr;
	if (editor_plugin && editor_plugin->get_editor_interface()) {
		ur = editor_plugin->get_editor_interface()->get_editor_undo_redo();
	}

	if (ur) {
		ur->create_action("Set Environment Preset: " + preset);
		ur->add_do_property(we, "environment", env);
		ur->add_undo_property(we, "environment", we->get_environment());
		ur->commit_action();
	} else {
		we->set_environment(env);
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["preset"] = preset;
	ret["message"] = "Applied environment preset '" + preset + "' to " + node_path;
	return ret;
}

JustAMCPEnvironmentTools::JustAMCPEnvironmentTools() {}
JustAMCPEnvironmentTools::~JustAMCPEnvironmentTools() {}
