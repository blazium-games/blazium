/**************************************************************************/
/*  justamcp_particle_tools.cpp                                           */
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

#include "justamcp_particle_tools.h"

#include "../justamcp_editor_scene_access.h"
#include "../justamcp_mcp_tool_macros.h"

#include "core/math/expression.h"
#include "scene/2d/cpu_particles_2d.h" // IWYU pragma: keep
#include "scene/2d/gpu_particles_2d.h"
#include "scene/3d/cpu_particles_3d.h" // IWYU pragma: keep
#include "scene/3d/gpu_particles_3d.h"
#include "scene/resources/gradient_texture.h"
#include "scene/resources/particle_process_material.h"

void JustAMCPParticleTools::_bind_methods() {}

JustAMCPParticleTools::JustAMCPParticleTools() {}
JustAMCPParticleTools::~JustAMCPParticleTools() {}

Node *JustAMCPParticleTools::_find_node_by_path(const String &p_path) {
	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return nullptr;
	}

	if (p_path == "." || p_path == root->get_name()) {
		return root;
	}
	if (root->has_node(p_path)) {
		return root->get_node(p_path);
	}

	if (p_path.begins_with(String(root->get_name()) + "/")) {
		String rel = p_path.substr(String(root->get_name()).length() + 1);
		if (root->has_node(rel)) {
			return root->get_node(rel);
		}
	}
	return nullptr;
}

Node *JustAMCPParticleTools::_get_particles_node_any(const String &p_path) {
	Node *node = _find_node_by_path(p_path);
	if (node && (node->is_class("GPUParticles2D") || node->is_class("GPUParticles3D"))) {
		return node;
	}
	return nullptr;
}

Color JustAMCPParticleTools::_parse_color(const String &p_color_str) {
	if (p_color_str.begins_with("#")) {
		return Color::html(p_color_str);
	}
	String lower = p_color_str.to_lower();
	if (lower == "red") {
		return Color(1, 0, 0);
	}
	if (lower == "green") {
		return Color(0, 1, 0);
	}
	if (lower == "blue") {
		return Color(0, 0, 1);
	}
	if (lower == "white") {
		return Color(1, 1, 1);
	}
	if (lower == "black") {
		return Color(0, 0, 0);
	}
	if (lower == "yellow") {
		return Color(1, 1, 0);
	}
	if (lower == "orange") {
		return Color(1, 0.5, 0);
	}
	if (lower == "gray" || lower == "gray") {
		return Color(0.5, 0.5, 0.5);
	}
	if (lower == "cyan") {
		return Color(0, 1, 1);
	}
	if (lower == "magenta") {
		return Color(1, 0, 1);
	}
	if (lower == "transparent") {
		return Color(0, 0, 0, 0);
	}

	Ref<Expression> expr;
	expr.instantiate();
	if (expr->parse(p_color_str) == OK) {
		Variant parsed = expr->execute(Array(), nullptr, false, true);
		if (parsed.get_type() == Variant::COLOR) {
			return parsed;
		}
	}
	return Color(1, 1, 1);
}

Dictionary JustAMCPParticleTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "create_particles") {
		return _create_particles(p_args);
	}
	if (p_tool_name == "set_particle_material") {
		return _set_particle_material(p_args);
	}
	if (p_tool_name == "set_particle_color_gradient") {
		return _set_particle_color_gradient(p_args);
	}
	if (p_tool_name == "apply_particle_preset") {
		return _apply_particle_preset(p_args);
	}
	if (p_tool_name == "get_particle_info") {
		return _get_particle_info(p_args);
	}

	Dictionary err;
	err["code"] = -32601;
	err["message"] = "Method not found: " + p_tool_name;
	Dictionary res;
	res["error"] = err;
	return Dictionary();
}

Dictionary JustAMCPParticleTools::_create_particles(const Dictionary &p_params) {
	if (!p_params.has("parent_path")) {
		return MCP_INVALID_PARAMS("Missing param: parent_path");
	}
	String parent_path = p_params["parent_path"];

	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No scene is currently open");
	}

	Node *parent = _find_node_by_path(parent_path);
	if (!parent) {
		return MCP_NOT_FOUND("Node at '" + parent_path + "'");
	}

	String node_name = p_params.has("name") ? String(p_params["name"]) : "Particles";
	bool is_3d = p_params.has("is_3d") ? bool(p_params["is_3d"]) : false;
	int amount = p_params.has("amount") ? int(p_params["amount"]) : 16;
	float lifetime = p_params.has("lifetime") ? float(p_params["lifetime"]) : 1.0;
	bool one_shot = p_params.has("one_shot") ? bool(p_params["one_shot"]) : false;
	float explosiveness = p_params.has("explosiveness") ? float(p_params["explosiveness"]) : 0.0;
	float randomness = p_params.has("randomness") ? float(p_params["randomness"]) : 0.0;
	bool emitting = p_params.has("emitting") ? bool(p_params["emitting"]) : true;

	Node *particles_node = nullptr;
	Ref<ParticleProcessMaterial> mat;
	mat.instantiate();

	if (is_3d) {
		GPUParticles3D *p = memnew(GPUParticles3D);
		p->set_name(node_name);
		p->set_amount(amount);
		p->set_lifetime(lifetime);
		p->set_one_shot(one_shot);
		p->set_explosiveness_ratio(explosiveness);
		p->set_randomness_ratio(randomness);
		p->set_emitting(emitting);
		p->set_process_material(mat);
		particles_node = p;
	} else {
		GPUParticles2D *p = memnew(GPUParticles2D);
		p->set_name(node_name);
		p->set_amount(amount);
		p->set_lifetime(lifetime);
		p->set_one_shot(one_shot);
		p->set_explosiveness_ratio(explosiveness);
		p->set_randomness_ratio(randomness);
		p->set_emitting(emitting);
		p->set_process_material(mat);
		particles_node = p;
	}

	parent->add_child(particles_node, true);
	particles_node->set_owner(root);

	Dictionary res;
	res["name"] = particles_node->get_name();
	res["parent"] = parent_path;
	res["is_3d"] = is_3d;
	res["amount"] = amount;
	res["lifetime"] = lifetime;
	res["one_shot"] = one_shot;
	res["created"] = true;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPParticleTools::_set_particle_material(const Dictionary &p_params) {
	if (!p_params.has("node_path")) {
		return MCP_INVALID_PARAMS("Missing param: node_path");
	}
	String node_path = p_params["node_path"];

	Node *node = _get_particles_node_any(node_path);
	if (!node) {
		return MCP_NOT_FOUND("GPUParticles2D/3D at '" + node_path + "'");
	}

	Ref<ParticleProcessMaterial> mat = node->get("process_material");
	if (mat.is_null()) {
		mat.instantiate();
		node->set("process_material", mat);
	}

	Array changes;

	if (p_params.has("direction")) {
		Variant dir = p_params["direction"];
		if (dir.get_type() == Variant::DICTIONARY) {
			Dictionary d = dir;
			mat->set_direction(Vector3(float(d.get("x", 0)), float(d.get("y", 0)), float(d.get("z", 0))));
			changes.push_back("direction");
		} else if (dir.get_type() == Variant::STRING) {
			Ref<Expression> expr;
			expr.instantiate();
			if (expr->parse(dir) == OK) {
				Variant parsed = expr->execute(Array(), nullptr, false, true);
				if (parsed.get_type() == Variant::VECTOR3) {
					mat->set_direction(parsed);
					changes.push_back("direction");
				}
			}
		}
	}

	if (p_params.has("spread")) {
		mat->set_spread(float(p_params["spread"]));
		changes.push_back("spread");
	}

	if (p_params.has("initial_velocity_min")) {
		mat->set_param_min(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, float(p_params["initial_velocity_min"]));
		changes.push_back("initial_velocity_min");
	}
	if (p_params.has("initial_velocity_max")) {
		mat->set_param_max(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, float(p_params["initial_velocity_max"]));
		changes.push_back("initial_velocity_max");
	}

	if (p_params.has("gravity")) {
		Variant grav = p_params["gravity"];
		if (grav.get_type() == Variant::DICTIONARY) {
			Dictionary d = grav;
			mat->set_gravity(Vector3(float(d.get("x", 0)), float(d.get("y", 0)), float(d.get("z", 0))));
			changes.push_back("gravity");
		} else if (grav.get_type() == Variant::STRING) {
			Ref<Expression> expr;
			expr.instantiate();
			if (expr->parse(grav) == OK) {
				Variant parsed = expr->execute(Array(), nullptr, false, true);
				if (parsed.get_type() == Variant::VECTOR3) {
					mat->set_gravity(parsed);
					changes.push_back("gravity");
				}
			}
		}
	}

	if (p_params.has("scale_min")) {
		mat->set_param_min(ParticleProcessMaterial::PARAM_SCALE, float(p_params["scale_min"]));
		changes.push_back("scale_min");
	}
	if (p_params.has("scale_max")) {
		mat->set_param_max(ParticleProcessMaterial::PARAM_SCALE, float(p_params["scale_max"]));
		changes.push_back("scale_max");
	}

	if (p_params.has("color")) {
		mat->set_color(_parse_color(String(p_params["color"])));
		changes.push_back("color");
	}

	if (p_params.has("emission_shape")) {
		String shape_str = String(p_params["emission_shape"]).to_lower();
		if (shape_str == "point") {
			mat->set_emission_shape(ParticleProcessMaterial::EMISSION_SHAPE_POINT);
		} else if (shape_str == "sphere") {
			mat->set_emission_shape(ParticleProcessMaterial::EMISSION_SHAPE_SPHERE);
			if (p_params.has("emission_sphere_radius")) {
				mat->set_emission_sphere_radius(float(p_params["emission_sphere_radius"]));
			}
		} else if (shape_str == "sphere_surface") {
			mat->set_emission_shape(ParticleProcessMaterial::EMISSION_SHAPE_SPHERE_SURFACE);
			if (p_params.has("emission_sphere_radius")) {
				mat->set_emission_sphere_radius(float(p_params["emission_sphere_radius"]));
			}
		} else if (shape_str == "box") {
			mat->set_emission_shape(ParticleProcessMaterial::EMISSION_SHAPE_BOX);
			if (p_params.has("emission_box_extents")) {
				Variant ext = p_params["emission_box_extents"];
				if (ext.get_type() == Variant::DICTIONARY) {
					Dictionary d = ext;
					mat->set_emission_box_extents(Vector3(float(d.get("x", 1)), float(d.get("y", 1)), float(d.get("z", 1))));
				}
			}
		} else if (shape_str == "ring") {
			mat->set_emission_shape(ParticleProcessMaterial::EMISSION_SHAPE_RING);
			if (p_params.has("emission_ring_radius")) {
				mat->set_emission_ring_radius(float(p_params["emission_ring_radius"]));
			}
			if (p_params.has("emission_ring_inner_radius")) {
				mat->set_emission_ring_inner_radius(float(p_params["emission_ring_inner_radius"]));
			}
			if (p_params.has("emission_ring_height")) {
				mat->set_emission_ring_height(float(p_params["emission_ring_height"]));
			}
		}
		changes.push_back("emission_shape");
	}

	Dictionary res;
	res["node_path"] = node_path;
	res["changes"] = changes;
	return MCP_SUCCESS(res);
}

void JustAMCPParticleTools::_apply_gradient(Object *p_mat, const Array &p_stops) {
	ParticleProcessMaterial *mat = Object::cast_to<ParticleProcessMaterial>(p_mat);
	if (!mat) {
		return;
	}

	Ref<Gradient> gradient;
	gradient.instantiate();

	while (gradient->get_point_count() > 0) {
		gradient->remove_point(0);
	}

	for (int i = 0; i < p_stops.size(); i++) {
		Dictionary stop = p_stops[i];
		float offset = float(stop.get("offset", 0.0));
		String color_str = "";
		if (stop["color"].get_type() == Variant::COLOR) {
			Color c = stop["color"];
			gradient->add_point(offset, c);
		} else {
			color_str = stop.get("color", "#ffffff");
			gradient->add_point(offset, _parse_color(color_str));
		}
	}

	Ref<GradientTexture1D> grad_tex;
	grad_tex.instantiate();
	grad_tex->set_width(64);
	grad_tex->set_gradient(gradient);

	mat->set_deferred("color_ramp", grad_tex);
}

Dictionary JustAMCPParticleTools::_set_particle_color_gradient(const Dictionary &p_params) {
	if (!p_params.has("node_path")) {
		return MCP_INVALID_PARAMS("Missing param: node_path");
	}
	String node_path = p_params["node_path"];

	Node *node = _get_particles_node_any(node_path);
	if (!node) {
		return MCP_NOT_FOUND("GPUParticles2D/3D at '" + node_path + "'");
	}

	Ref<ParticleProcessMaterial> mat = node->get("process_material");
	if (mat.is_null()) {
		mat.instantiate();
		node->set("process_material", mat);
	}

	if (!p_params.has("stops") || p_params["stops"].get_type() != Variant::ARRAY) {
		return MCP_INVALID_PARAMS("Missing required parameter: stops (array of {offset, color})");
	}
	Array stops = p_params["stops"];
	if (stops.is_empty()) {
		return MCP_INVALID_PARAMS("stops array must not be empty");
	}

	_apply_gradient(mat.ptr(), stops);

	Dictionary res;
	res["node_path"] = node_path;
	res["stops_count"] = stops.size();
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPParticleTools::_apply_particle_preset(const Dictionary &p_params) {
	if (!p_params.has("node_path")) {
		return MCP_INVALID_PARAMS("Missing param: node_path");
	}
	String node_path = p_params["node_path"];
	if (!p_params.has("preset")) {
		return MCP_INVALID_PARAMS("Missing param: preset");
	}
	String preset = String(p_params["preset"]).to_lower();

	Node *node = _get_particles_node_any(node_path);
	if (!node) {
		return MCP_NOT_FOUND("GPUParticles2D/3D at '" + node_path + "'");
	}

	Ref<ParticleProcessMaterial> mat;
	mat.instantiate();

	bool is_2d = node->is_class("GPUParticles2D");
	Vector3 gravity_down = Vector3(0, is_2d ? 98 : 9.8, 0);
	Vector3 gravity_none = Vector3(0, 0, 0);

	if (preset == "explosion") {
		node->set("amount", 32);
		node->set("lifetime", 0.6);
		node->set("one_shot", true);
		node->set("explosiveness_ratio", 1.0);
		mat->set_direction(is_2d ? Vector3(0, -1, 0) : Vector3(0, 1, 0));
		mat->set_spread(180.0);
		mat->set_param_min(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, is_2d ? 100.0 : 5.0);
		mat->set_param_max(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, is_2d ? 200.0 : 10.0);
		mat->set_gravity(gravity_down * 0.5);
		mat->set_param_min(ParticleProcessMaterial::PARAM_DAMPING, 2.0);
		mat->set_param_max(ParticleProcessMaterial::PARAM_DAMPING, 4.0);
		mat->set_param_min(ParticleProcessMaterial::PARAM_SCALE, 0.5);
		mat->set_param_max(ParticleProcessMaterial::PARAM_SCALE, 1.5);
		mat->set_color(Color(1.0, 0.6, 0.1));

		Array stops;
		Dictionary s1;
		s1["offset"] = 0.0;
		s1["color"] = Color(1.0, 1.0, 1.0);
		stops.push_back(s1);
		Dictionary s2;
		s2["offset"] = 0.3;
		s2["color"] = Color(1.0, 0.8, 0.2);
		stops.push_back(s2);
		Dictionary s3;
		s3["offset"] = 0.7;
		s3["color"] = Color(1.0, 0.3, 0.0);
		stops.push_back(s3);
		Dictionary s4;
		s4["offset"] = 1.0;
		s4["color"] = Color(0.2, 0.0, 0.0, 0.0);
		stops.push_back(s4);
		_apply_gradient(mat.ptr(), stops);
	} else if (preset == "smoke") {
		node->set("amount", 16);
		node->set("lifetime", 3.0);
		node->set("one_shot", false);
		node->set("explosiveness_ratio", 0.0);
		mat->set_direction(is_2d ? Vector3(0, -1, 0) : Vector3(0, 1, 0));
		mat->set_spread(25.0);
		mat->set_param_min(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, is_2d ? 10.0 : 0.5);
		mat->set_param_max(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, is_2d ? 25.0 : 1.2);
		mat->set_gravity(gravity_none);
		mat->set_param_min(ParticleProcessMaterial::PARAM_SCALE, 1.5);
		mat->set_param_max(ParticleProcessMaterial::PARAM_SCALE, 3.0);
		mat->set_param_min(ParticleProcessMaterial::PARAM_DAMPING, 1.0);
		mat->set_param_max(ParticleProcessMaterial::PARAM_DAMPING, 2.0);

		Array stops;
		Dictionary s1;
		s1["offset"] = 0.0;
		s1["color"] = Color(0.5, 0.5, 0.5, 0.6);
		stops.push_back(s1);
		Dictionary s2;
		s2["offset"] = 0.5;
		s2["color"] = Color(0.6, 0.6, 0.6, 0.3);
		stops.push_back(s2);
		Dictionary s3;
		s3["offset"] = 1.0;
		s3["color"] = Color(0.7, 0.7, 0.7, 0.0);
		stops.push_back(s3);
		_apply_gradient(mat.ptr(), stops);
	} else {
		return MCP_INVALID_PARAMS("Unknown preset or not fully implemented: " + preset);
	}

	node->set("process_material", mat);

	Dictionary res;
	res["node_path"] = node_path;
	res["preset"] = preset;
	res["applied"] = true;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPParticleTools::_get_particle_info(const Dictionary &p_params) {
	if (!p_params.has("node_path")) {
		return MCP_INVALID_PARAMS("Missing param: node_path");
	}
	String node_path = p_params["node_path"];

	Node *node = _get_particles_node_any(node_path);
	if (!node) {
		return MCP_NOT_FOUND("GPUParticles2D/3D at '" + node_path + "'");
	}

	Dictionary info;
	info["node_path"] = node_path;
	info["type"] = node->get_class();
	info["amount"] = node->get("amount");
	info["lifetime"] = node->get("lifetime");
	info["one_shot"] = node->get("one_shot");
	info["explosiveness"] = node->get("explosiveness_ratio");
	info["randomness"] = node->get("randomness_ratio");
	info["emitting"] = node->get("emitting");

	Ref<ParticleProcessMaterial> mat = node->get("process_material");
	if (mat.is_valid()) {
		Dictionary mat_info;
		mat_info["direction"] = String(Variant(mat->get_direction()));
		mat_info["spread"] = mat->get_spread();
		mat_info["gravity"] = String(Variant(mat->get_gravity()));
		info["material"] = mat_info;
	} else {
		info["material"] = Variant();
	}

	return MCP_SUCCESS(info);
}
