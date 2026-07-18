/**************************************************************************/
/*  justamcp_blueprint_tools.cpp                                          */
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

#include "justamcp_blueprint_tools.h"
#include "../justamcp_editor_plugin.h"
#include "../justamcp_editor_scene_access.h"

#include "modules/noise/fastnoise_lite.h"
#include "modules/noise/noise_texture_2d.h"
#include "scene/2d/camera_2d.h"
#include "scene/2d/gpu_particles_2d.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/gpu_particles_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/curve_texture.h"
#include "scene/resources/gradient_texture.h"
#include "scene/resources/material.h"
#include "scene/resources/particle_process_material.h"

#include "editor/editor_interface.h"
#include "editor/editor_undo_redo_manager.h"

void JustAMCPBlueprintTools::_bind_methods() {}

Dictionary JustAMCPBlueprintTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	String tool_name = p_tool_name;
	if (tool_name.begins_with("blueprint_")) {
		tool_name = tool_name.substr(String("blueprint_").length());
	}
	if (tool_name == "create_particle_preset") {
		return create_particle_preset(p_args);
	}
	if (tool_name == "create_material_preset") {
		return create_material_preset(p_args);
	}
	if (tool_name == "setup_camera_preset") {
		return setup_camera_preset(p_args);
	}
	if (p_tool_name == "create_texture_preset") {
		return create_texture_preset(p_args);
	}

	return Dictionary();
}

Dictionary JustAMCPBlueprintTools::create_particle_preset(const Dictionary &p_args) {
	String node_path = p_args.get("path", "");
	String preset = p_args.get("preset", "fire");
	bool is_3d = p_args.get("is_3d", true);

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
		ret["error"] = "Node not found: " + node_path;
		return ret;
	}

	Ref<ParticleProcessMaterial> mat;
	mat.instantiate();
	float lifetime = 1.0;
	int amount = 50;
	bool one_shot = false;

	if (preset == "fire") {
		amount = 80;
		lifetime = 1.2;
		mat->set_direction(Vector3(0, 1, 0));
		mat->set_spread(15.0);
		mat->set_param_min(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, 2.0);
		mat->set_param_max(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, 4.0);
		mat->set_gravity(Vector3(0, 1, 0));
		mat->set_param_min(ParticleProcessMaterial::PARAM_SCALE, 0.4);
		mat->set_param_max(ParticleProcessMaterial::PARAM_SCALE, 0.8);
		Ref<Gradient> grad;
		grad.instantiate();
		grad->add_point(0.0, Color(1.0, 1.0, 0.9));
		grad->add_point(0.3, Color(1.0, 0.6, 0.1));
		grad->add_point(0.7, Color(0.8, 0.1, 0.05, 0.7));
		grad->add_point(1.0, Color(0.2, 0.05, 0.05, 0.0));
		Ref<GradientTexture1D> gt;
		gt.instantiate();
		gt->set_gradient(grad);
		mat->set_color_ramp(gt);
	} else if (preset == "smoke") {
		amount = 40;
		lifetime = 3.0;
		mat->set_direction(Vector3(0, 1, 0));
		mat->set_spread(20.0);
		mat->set_param_min(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, 0.5);
		mat->set_param_max(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, 1.5);
		mat->set_gravity(Vector3(0, 0.2, 0));
		mat->set_param_min(ParticleProcessMaterial::PARAM_SCALE, 0.6);
		mat->set_param_max(ParticleProcessMaterial::PARAM_SCALE, 1.4);
		Ref<Gradient> grad;
		grad.instantiate();
		grad->add_point(0.0, Color(0.3, 0.3, 0.3, 0.0));
		grad->add_point(0.2, Color(0.35, 0.35, 0.35, 0.7));
		grad->add_point(1.0, Color(0.1, 0.1, 0.1, 0.0));
		Ref<GradientTexture1D> gt;
		gt.instantiate();
		gt->set_gradient(grad);
		mat->set_color_ramp(gt);
	} else if (preset == "explosion") {
		amount = 200;
		lifetime = 1.5;
		one_shot = true;
		mat->set_spread(180.0);
		mat->set_param_min(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, 6.0);
		mat->set_param_max(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, 10.0);
		mat->set_gravity(Vector3(0, -4.0, 0));
		Ref<Gradient> grad;
		grad.instantiate();
		grad->add_point(0.0, Color(1.0, 0.95, 0.5));
		grad->add_point(0.2, Color(1.0, 0.4, 0.1));
		grad->add_point(1.0, Color(0.1, 0.1, 0.1, 0.0));
		Ref<GradientTexture1D> gt;
		gt.instantiate();
		gt->set_gradient(grad);
		mat->set_color_ramp(gt);
	} else if (preset == "rain") {
		amount = 500;
		lifetime = 1.5;
		mat->set_direction(Vector3(0, -1, 0));
		mat->set_spread(2.0);
		mat->set_param_min(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, 15.0);
		mat->set_param_max(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, 18.0);
		mat->set_gravity(Vector3(0, -2.0, 0));
		mat->set_emission_shape(ParticleProcessMaterial::EMISSION_SHAPE_BOX);
		mat->set_emission_box_extents(Vector3(10, 0.1, 10));
		mat->set_param_min(ParticleProcessMaterial::PARAM_SCALE, 0.02);
		mat->set_param_max(ParticleProcessMaterial::PARAM_SCALE, 0.04);
	} else if (preset == "lightning") {
		amount = 40;
		lifetime = 0.35;
		one_shot = true;
		mat->set_direction(Vector3(0, -1, 0));
		mat->set_spread(8.0);
		mat->set_param_min(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, 18.0);
		mat->set_param_max(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, 28.0);
		mat->set_param_min(ParticleProcessMaterial::PARAM_SCALE, 0.08);
		mat->set_param_max(ParticleProcessMaterial::PARAM_SCALE, 0.18);
		Ref<Gradient> grad;
		grad.instantiate();
		grad->add_point(0.0, Color(1, 1, 1));
		grad->add_point(0.2, Color(0.6, 0.85, 1.0));
		grad->add_point(1.0, Color(0.1, 0.2, 0.7, 0.0));
		Ref<GradientTexture1D> gt;
		gt.instantiate();
		gt->set_gradient(grad);
		mat->set_color_ramp(gt);
	}

	EditorUndoRedoManager *ur = nullptr;
	if (editor_plugin && editor_plugin->get_editor_interface()) {
		ur = editor_plugin->get_editor_interface()->get_editor_undo_redo();
	}

	if (is_3d) {
		GPUParticles3D *p = Object::cast_to<GPUParticles3D>(node);
		if (p) {
			if (ur) {
				ur->create_action("Apply Particle Preset: " + preset);
				ur->add_do_property(p, "process_material", mat);
				ur->add_do_property(p, "amount", amount);
				ur->add_do_property(p, "lifetime", lifetime);
				ur->add_do_property(p, "one_shot", one_shot);
				ur->add_undo_property(p, "process_material", p->get_process_material());
				ur->add_undo_property(p, "amount", p->get_amount());
				ur->add_undo_property(p, "lifetime", p->get_lifetime());
				ur->add_undo_property(p, "one_shot", p->get_one_shot());
				if (p->get_draw_passes() == 0 || p->get_draw_pass_mesh(0).is_null()) {
					Ref<QuadMesh> qm;
					qm.instantiate();
					Ref<StandardMaterial3D> sm;
					sm.instantiate();
					sm->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
					sm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
					if (preset == "fire" || preset == "explosion" || preset == "lightning") {
						sm->set_blend_mode(BaseMaterial3D::BLEND_MODE_ADD);
					}
					qm->set_material(sm);
					ur->add_do_property(p, "draw_pass_1", qm);
				}
				ur->commit_action();
			} else {
				p->set_process_material(mat);
				p->set_amount(amount);
				p->set_lifetime(lifetime);
				p->set_one_shot(one_shot);
			}
		}
	} else {
		GPUParticles2D *p = Object::cast_to<GPUParticles2D>(node);
		if (p) {
			if (ur) {
				ur->create_action("Apply Particle Preset: " + preset);
				ur->add_do_property(p, "process_material", mat);
				ur->add_do_property(p, "amount", amount);
				ur->add_do_property(p, "lifetime", lifetime);
				ur->add_do_property(p, "one_shot", one_shot);
				ur->add_undo_property(p, "process_material", p->get_process_material());
				ur->add_undo_property(p, "amount", p->get_amount());
				ur->add_undo_property(p, "lifetime", p->get_lifetime());
				ur->add_undo_property(p, "one_shot", p->get_one_shot());
				ur->commit_action();
			} else {
				p->set_process_material(mat);
				p->set_amount(amount);
				p->set_lifetime(lifetime);
				p->set_one_shot(one_shot);
			}
		}
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["message"] = "Applied particle preset '" + preset + "' to " + node_path;
	return ret;
}

Dictionary JustAMCPBlueprintTools::create_material_preset(const Dictionary &p_args) {
	String node_path = p_args.get("path", "");
	String preset = p_args.get("preset", "metal");

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
		ret["error"] = "Node not found: " + node_path;
		return ret;
	}

	Ref<StandardMaterial3D> mat;
	mat.instantiate();
	if (preset == "metal") {
		mat->set_metallic(1.0);
		mat->set_roughness(0.25);
		mat->set_albedo(Color(0.85, 0.85, 0.88));
	} else if (preset == "glass") {
		mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		mat->set_albedo(Color(0.9, 0.95, 1.0, 0.3));
		mat->set_metallic(0.0);
		mat->set_roughness(0.05);
	} else if (preset == "emissive") {
		mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
		mat->set_emission(Color(1, 1, 1));
		mat->set_emission_energy_multiplier(3.0);
	} else if (preset == "matte") {
		mat->set_roughness(1.0);
		mat->set_metallic(0.0);
		mat->set_albedo(Color(0.7, 0.7, 0.7));
	} else if (preset == "ceramic") {
		mat->set_roughness(0.4);
		mat->set_metallic(0.0);
		mat->set_feature(BaseMaterial3D::FEATURE_CLEARCOAT, true);
		mat->set_clearcoat(0.7);
		mat->set_clearcoat_roughness(0.15);
	}

	EditorUndoRedoManager *ur = nullptr;
	if (editor_plugin && editor_plugin->get_editor_interface()) {
		ur = editor_plugin->get_editor_interface()->get_editor_undo_redo();
	}

	if (node->has_method("set_material")) {
		if (ur) {
			ur->create_action("Apply Material Preset: " + preset);
			ur->add_do_method(node, "set_material", mat);
			ur->commit_action();
		} else {
			node->call("set_material", mat);
		}
	} else if (node->has_method("set_surface_override_material")) {
		if (ur) {
			ur->create_action("Apply Material Preset: " + preset);
			ur->add_do_method(node, "set_surface_override_material", 0, mat);
			ur->commit_action();
		} else {
			node->call("set_surface_override_material", 0, mat);
		}
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["message"] = "Applied material preset '" + preset + "' to " + node_path;
	return ret;
}

Dictionary JustAMCPBlueprintTools::setup_camera_preset(const Dictionary &p_args) {
	String node_path = p_args.get("path", "");
	String preset = p_args.get("preset", "action_3d");

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
		ret["error"] = "Node not found: " + node_path;
		return ret;
	}

	EditorUndoRedoManager *ur = nullptr;
	if (editor_plugin && editor_plugin->get_editor_interface()) {
		ur = editor_plugin->get_editor_interface()->get_editor_undo_redo();
	}

	if (preset.ends_with("_2d")) {
		Camera2D *cam = Object::cast_to<Camera2D>(node);
		if (cam) {
			if (ur) {
				ur->create_action("Camera Preset: " + preset);
				if (preset == "topdown_2d") {
					ur->add_do_property(cam, "zoom", Vector2(2, 2));
					ur->add_do_property(cam, "position_smoothing_enabled", true);
					ur->add_do_property(cam, "position_smoothing_speed", 5.0);
				} else if (preset == "platformer_2d") {
					ur->add_do_property(cam, "zoom", Vector2(1.5, 1.5));
					ur->add_do_property(cam, "position_smoothing_enabled", true);
					ur->add_do_property(cam, "position_smoothing_speed", 8.0);
				}
				ur->commit_action();
			} else {
				if (preset == "topdown_2d") {
					cam->set_zoom(Vector2(2, 2));
					cam->set_position_smoothing_enabled(true);
				} else if (preset == "platformer_2d") {
					cam->set_zoom(Vector2(1.5, 1.5));
					cam->set_position_smoothing_enabled(true);
				}
			}
		}
	} else {
		Camera3D *cam = Object::cast_to<Camera3D>(node);
		if (cam) {
			if (ur) {
				ur->create_action("Camera Preset: " + preset);
				if (preset == "cinematic_3d") {
					ur->add_do_property(cam, "fov", 40.0);
					ur->add_do_property(cam, "near", 0.1);
					ur->add_do_property(cam, "far", 500.0);
				} else if (preset == "action_3d") {
					ur->add_do_property(cam, "fov", 70.0);
					ur->add_do_property(cam, "near", 0.1);
					ur->add_do_property(cam, "far", 200.0);
				}
				ur->commit_action();
			} else {
				if (preset == "cinematic_3d") {
					cam->set_fov(40.0);
					cam->set_near(0.1);
					cam->set_far(500.0);
				} else if (preset == "action_3d") {
					cam->set_fov(70.0);
					cam->set_near(0.1);
					cam->set_far(200.0);
				}
			}
		}
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["message"] = "Applied camera preset '" + preset + "' to " + node_path;
	return ret;
}

Dictionary JustAMCPBlueprintTools::create_texture_preset(const Dictionary &p_args) {
	String node_path = p_args.get("path", "");
	String property = p_args.get("property", "");
	String preset = p_args.get("preset", "gradient");

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
		ret["error"] = "Node not found: " + node_path;
		return ret;
	}

	Ref<Resource> tex;
	if (preset == "gradient") {
		Ref<GradientTexture2D> gt;
		gt.instantiate();
		Ref<Gradient> grad;
		grad.instantiate();
		grad->add_point(0.0, Color(0, 0, 0));
		grad->add_point(1.0, Color(1, 1, 1));
		gt->set_gradient(grad);
		tex = gt;
	} else if (preset == "noise") {
		Ref<NoiseTexture2D> nt;
		nt.instantiate();
		Ref<FastNoiseLite> noise;
		noise.instantiate();
		noise->set_noise_type(FastNoiseLite::TYPE_SIMPLEX);
		nt->set_noise(noise);
		tex = nt;
	}

	EditorUndoRedoManager *ur = nullptr;
	if (editor_plugin && editor_plugin->get_editor_interface()) {
		ur = editor_plugin->get_editor_interface()->get_editor_undo_redo();
	}

	if (ur) {
		ur->create_action("Setup Texture Preset: " + preset);
		ur->add_do_property(node, property, tex);
		ur->add_undo_property(node, property, node->get(property));
		ur->add_do_reference(tex.ptr());
		ur->commit_action();
	} else {
		node->set(property, tex);
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["message"] = "Setup texture '" + preset + "' on " + node_path + "." + property;
	return ret;
}

JustAMCPBlueprintTools::JustAMCPBlueprintTools() {}
JustAMCPBlueprintTools::~JustAMCPBlueprintTools() {}
