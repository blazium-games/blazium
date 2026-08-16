/**************************************************************************/
/*  justamcp_scene_3d_tools_env.cpp                                       */
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

#include "../justamcp_editor_scene_access.h"
#include "../justamcp_mcp_tool_macros.h"
#include "justamcp_scene_3d_tools.h"

#include "core/io/resource_loader.h"
#include "editor/editor_undo_redo_manager.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/light_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/world_environment.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/3d/sky_material.h"
#include "scene/resources/environment.h"
#include "scene/resources/material.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/sky.h"

#include "modules/gridmap/grid_map.h"

Dictionary JustAMCPScene3DTools::set_material_3d(const Dictionary &p_params) {
	if (!p_params.has("node_path")) {
		return MCP_INVALID_PARAMS("Missing node_path");
	}
	String node_path = p_params["node_path"];

	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No active scene");
	}

	Node *node = _find_node_by_path(node_path);
	if (!node) {
		return MCP_INVALID_PARAMS("Node not found: " + node_path);
	}

	MeshInstance3D *mesh_inst = Object::cast_to<MeshInstance3D>(node);
	if (!mesh_inst) {
		return MCP_INVALID_PARAMS("Node is not a MeshInstance3D");
	}

	int surface_index = p_params.get("surface_index", 0);

	Ref<StandardMaterial3D> mat;
	mat.instantiate();

	mat->set_albedo(_parse_color(p_params.get("albedo_color", Variant()), Color(1, 1, 1)));
	if (p_params.has("albedo_texture")) {
		String tex_path = p_params["albedo_texture"];
		if (ResourceLoader::exists(tex_path)) {
			mat->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, ResourceLoader::load(tex_path));
		}
	}

	mat->set_metallic(_optional_float(p_params, "metallic", 0.0));
	mat->set_roughness(_optional_float(p_params, "roughness", 1.0));
	if (p_params.has("metallic_texture")) {
		if (ResourceLoader::exists(p_params["metallic_texture"])) {
			mat->set_texture(BaseMaterial3D::TEXTURE_METALLIC, ResourceLoader::load(p_params["metallic_texture"]));
		}
	}
	if (p_params.has("roughness_texture")) {
		if (ResourceLoader::exists(p_params["roughness_texture"])) {
			mat->set_texture(BaseMaterial3D::TEXTURE_ROUGHNESS, ResourceLoader::load(p_params["roughness_texture"]));
		}
	}
	if (p_params.has("normal_texture")) {
		mat->set_feature(BaseMaterial3D::FEATURE_NORMAL_MAPPING, true);
		if (ResourceLoader::exists(p_params["normal_texture"])) {
			mat->set_texture(BaseMaterial3D::TEXTURE_NORMAL, ResourceLoader::load(p_params["normal_texture"]));
		}
	}

	if (p_params.has("emission") || p_params.has("emission_color")) {
		mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
		mat->set_emission(_parse_color(p_params.get("emission", p_params.get("emission_color", Variant())), Color()));
		mat->set_emission_energy_multiplier(_optional_float(p_params, "emission_energy", 1.0));
	}
	if (p_params.has("emission_texture")) {
		mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
		if (ResourceLoader::exists(p_params["emission_texture"])) {
			mat->set_texture(BaseMaterial3D::TEXTURE_EMISSION, ResourceLoader::load(p_params["emission_texture"]));
		}
	}

	if (p_params.has("transparency")) {
		String t = String(p_params["transparency"]).to_upper();
		if (t == "DISABLED" || t == "0") {
			mat->set_transparency(BaseMaterial3D::TRANSPARENCY_DISABLED);
		} else if (t == "ALPHA" || t == "1") {
			mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		} else if (t == "ALPHA_SCISSOR" || t == "2") {
			mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_SCISSOR);
		} else if (t == "ALPHA_HASH" || t == "3") {
			mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_HASH);
		} else if (t == "ALPHA_DEPTH_PRE_PASS" || t == "4") {
			mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_DEPTH_PRE_PASS);
		}
	}

	if (p_params.has("cull_mode")) {
		String c = String(p_params["cull_mode"]).to_upper();
		if (c == "BACK" || c == "0") {
			mat->set_cull_mode(BaseMaterial3D::CULL_BACK);
		} else if (c == "FRONT" || c == "1") {
			mat->set_cull_mode(BaseMaterial3D::CULL_FRONT);
		} else if (c == "DISABLED" || c == "2") {
			mat->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
		}
	}

	Ref<Material> old_mat = mesh_inst->get_surface_override_material(surface_index);
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action("MCP: Set material on " + mesh_inst->get_name());
	undo_redo->add_do_method(mesh_inst, "set_surface_override_material", surface_index, mat);
	undo_redo->add_undo_method(mesh_inst, "set_surface_override_material", surface_index, old_mat);
	undo_redo->commit_action();

	Dictionary res;
	res["node_path"] = root->get_path_to(mesh_inst);
	res["surface_index"] = surface_index;
	res["albedo_color"] = String(mat->get_albedo());
	res["metallic"] = mat->get_metallic();
	res["roughness"] = mat->get_roughness();
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPScene3DTools::setup_environment(const Dictionary &p_params) {
	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No active scene");
	}

	String parent_path = p_params.has("parent_path") ? String(p_params["parent_path"]) : ".";
	Node *parent = _find_node_by_path(parent_path);
	if (!parent) {
		return MCP_INVALID_PARAMS("Parent node not found: " + parent_path);
	}

	String node_path = p_params.has("node_path") ? String(p_params["node_path"]) : "";
	WorldEnvironment *world_env = nullptr;
	bool is_existing = false;

	if (!node_path.is_empty()) {
		Node *ex = _find_node_by_path(node_path);
		if (ex) {
			world_env = Object::cast_to<WorldEnvironment>(ex);
			is_existing = true;
		}
	}

	if (!world_env) {
		world_env = memnew(WorldEnvironment);
		world_env->set_name(p_params.get("name", "WorldEnvironment"));
	}

	Ref<Environment> env = world_env->get_environment();
	if (env.is_null()) {
		env.instantiate();
	}

	String bg_mode = p_params.get("background_mode", "sky");
	if (bg_mode == "sky") {
		env->set_background(Environment::BG_SKY);
	} else if (bg_mode == "color") {
		env->set_background(Environment::BG_COLOR);
		env->set_bg_color(_parse_color(p_params.get("background_color", Variant()), Color(0.3, 0.3, 0.3)));
	} else if (bg_mode == "canvas") {
		env->set_background(Environment::BG_CANVAS);
	} else if (bg_mode == "clear_color") {
		env->set_background(Environment::BG_CLEAR_COLOR);
	}

	if (p_params.has("sky") && p_params["sky"].get_type() == Variant::DICTIONARY) {
		Dictionary sky_params = p_params["sky"];
		Ref<ProceduralSkyMaterial> sky_mat;
		sky_mat.instantiate();
		sky_mat->set_sky_top_color(_parse_color(sky_params.get("sky_top_color", Variant()), Color(0.385, 0.454, 0.55)));
		sky_mat->set_sky_horizon_color(_parse_color(sky_params.get("sky_horizon_color", Variant()), Color(0.646, 0.654, 0.67)));
		sky_mat->set_ground_bottom_color(_parse_color(sky_params.get("ground_bottom_color", Variant()), Color(0.2, 0.169, 0.133)));
		sky_mat->set_ground_horizon_color(_parse_color(sky_params.get("ground_horizon_color", Variant()), Color(0.646, 0.654, 0.67)));

		Ref<Sky> sky;
		sky.instantiate();
		sky->set_material(sky_mat);
		env->set_sky(sky);
		env->set_background(Environment::BG_SKY);
	}

	if (p_params.has("ambient_light_color")) {
		env->set_ambient_light_color(_parse_color(p_params["ambient_light_color"], Color(1, 1, 1)));
	}
	if (p_params.has("ambient_light_energy")) {
		env->set_ambient_light_energy(p_params["ambient_light_energy"]);
	}

	if (p_params.has("tonemap_mode")) {
		String tm = String(p_params["tonemap_mode"]).to_upper();
		if (tm == "LINEAR" || tm == "0") {
			env->set_tonemapper(Environment::TONE_MAPPER_LINEAR);
		} else if (tm == "REINHARDT" || tm == "1") {
			env->set_tonemapper(Environment::TONE_MAPPER_REINHARDT);
		} else if (tm == "FILMIC" || tm == "2") {
			env->set_tonemapper(Environment::TONE_MAPPER_FILMIC);
		} else if (tm == "ACES" || tm == "3") {
			env->set_tonemapper(Environment::TONE_MAPPER_ACES);
		} else if (tm == "AGX" || tm == "4") {
			env->set_tonemapper((Environment::ToneMapper)4);
		}
	}
	if (p_params.has("tonemap_exposure")) {
		env->set_tonemap_exposure(p_params["tonemap_exposure"]);
	}
	if (p_params.has("tonemap_white")) {
		env->set_tonemap_white(p_params["tonemap_white"]);
	}

	if (p_params.has("fog_enabled")) {
		env->set_fog_enabled(p_params["fog_enabled"]);
	}
	if (env->is_fog_enabled() || p_params.has("fog_light_color")) {
		env->set_fog_light_color(_parse_color(p_params.get("fog_light_color", Variant()), Color(0.518, 0.553, 0.608)));
		if (p_params.has("fog_density")) {
			env->set_fog_density(p_params["fog_density"]);
		}
		if (p_params.has("fog_light_energy")) {
			env->set_fog_light_energy(p_params["fog_light_energy"]);
		}
	}

	if (p_params.has("glow_enabled")) {
		env->set_glow_enabled(p_params["glow_enabled"]);
	}
	if (env->is_glow_enabled()) {
		if (p_params.has("glow_intensity")) {
			env->set_glow_intensity(p_params["glow_intensity"]);
		}
		if (p_params.has("glow_strength")) {
			env->set_glow_strength(p_params["glow_strength"]);
		}
		if (p_params.has("glow_bloom")) {
			env->set_glow_bloom(p_params["glow_bloom"]);
		}
	}

	if (p_params.has("ssao_enabled")) {
		env->set_ssao_enabled(p_params["ssao_enabled"]);
	}
	if (env->is_ssao_enabled()) {
		if (p_params.has("ssao_radius")) {
			env->set_ssao_radius(p_params["ssao_radius"]);
		}
		if (p_params.has("ssao_intensity")) {
			env->set_ssao_intensity(p_params["ssao_intensity"]);
		}
	}

	if (p_params.has("ssr_enabled")) {
		env->set_ssr_enabled(p_params["ssr_enabled"]);
	}
	if (env->is_ssr_enabled()) {
		if (p_params.has("ssr_max_steps")) {
			env->set_ssr_max_steps(p_params["ssr_max_steps"]);
		}
		if (p_params.has("ssr_fade_in")) {
			env->set_ssr_fade_in(p_params["ssr_fade_in"]);
		}
		if (p_params.has("ssr_fade_out")) {
			env->set_ssr_fade_out(p_params["ssr_fade_out"]);
		}
	}

	if (p_params.has("sdfgi_enabled")) {
		env->set_sdfgi_enabled(p_params["sdfgi_enabled"]);
	}

	world_env->set_environment(env);

	if (!is_existing) {
		_add_child_with_undo(world_env, parent, root, "MCP: Add WorldEnvironment");
	}

	Array features;
	if (env->is_fog_enabled()) {
		features.push_back("fog");
	}
	if (env->is_glow_enabled()) {
		features.push_back("glow");
	}
	if (env->is_ssao_enabled()) {
		features.push_back("ssao");
	}
	if (env->is_ssr_enabled()) {
		features.push_back("ssr");
	}
	if (env->is_sdfgi_enabled()) {
		features.push_back("sdfgi");
	}

	Dictionary res;
	res["node_path"] = root->get_path_to(world_env);
	res["name"] = world_env->get_name();
	res["background_mode"] = bg_mode;
	res["features"] = features;
	res["is_existing"] = is_existing;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPScene3DTools::setup_camera_3d(const Dictionary &p_params) {
	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No active scene");
	}

	String parent_path = p_params.has("parent_path") ? String(p_params["parent_path"]) : ".";
	Node *parent = _find_node_by_path(parent_path);
	if (!parent) {
		return MCP_INVALID_PARAMS("Parent node not found: " + parent_path);
	}

	String node_path = p_params.has("node_path") ? String(p_params["node_path"]) : "";
	Camera3D *camera = nullptr;
	bool is_existing = false;

	if (!node_path.is_empty()) {
		Node *ex = _find_node_by_path(node_path);
		if (ex) {
			camera = Object::cast_to<Camera3D>(ex);
			if (!camera) {
				return MCP_INVALID_PARAMS("Node is not a Camera3D: " + node_path);
			}
			is_existing = true;
		}
	}

	if (!camera) {
		camera = memnew(Camera3D);
		camera->set_name(p_params.get("name", "Camera3D"));
	}

	String proj = p_params.has("projection") ? String(p_params["projection"]).to_lower() : "";
	if (proj == "perspective" || proj == "0") {
		camera->set_projection(Camera3D::PROJECTION_PERSPECTIVE);
	} else if (proj == "orthogonal" || proj == "orthographic" || proj == "1") {
		camera->set_projection(Camera3D::PROJECTION_ORTHOGONAL);
	} else if (proj == "frustum" || proj == "2") {
		camera->set_projection(Camera3D::PROJECTION_FRUSTUM);
	}

	if (p_params.has("fov")) {
		camera->set_fov(p_params["fov"]);
	}
	if (p_params.has("size")) {
		camera->set_size(p_params["size"]);
	}
	if (p_params.has("near")) {
		camera->set_near(p_params["near"]);
	}
	if (p_params.has("far")) {
		camera->set_far(p_params["far"]);
	}
	if (p_params.has("cull_mask")) {
		camera->set_cull_mask(p_params["cull_mask"]);
	}

	if (p_params.has("current")) {
		camera->set_current(p_params["current"]);
	}

	camera->set_position(_parse_vector3(p_params.get("position", Variant()), camera->get_position()));
	if (p_params.has("rotation")) {
		camera->set_rotation_degrees(_parse_vector3(p_params["rotation"], camera->get_rotation_degrees()));
	}
	if (p_params.has("look_at")) {
		camera->look_at(_parse_vector3(p_params["look_at"], Vector3()), Vector3(0, 1, 0));
	}

	if (p_params.has("environment_path")) {
		String ep = p_params["environment_path"];
		if (ResourceLoader::exists(ep)) {
			Ref<Environment> e = ResourceLoader::load(ep);
			if (e.is_valid()) {
				camera->set_environment(e);
			}
		}
	}

	if (!is_existing) {
		_add_child_with_undo(camera, parent, root, "MCP: Add Camera3D");
	}

	Dictionary res;
	res["node_path"] = root->get_path_to(camera);
	res["name"] = camera->get_name();
	res["projection"] = camera->get_projection() == Camera3D::PROJECTION_PERSPECTIVE ? "perspective" : "orthogonal";
	res["fov"] = camera->get_fov();
	res["position"] = String(camera->get_position());
	res["is_existing"] = is_existing;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPScene3DTools::add_gridmap(const Dictionary &p_params) {
	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No active scene");
	}

	String parent_path = p_params.has("parent_path") ? String(p_params["parent_path"]) : ".";
	Node *parent = _find_node_by_path(parent_path);
	if (!parent) {
		return MCP_INVALID_PARAMS("Parent node not found: " + parent_path);
	}

	String node_path = p_params.has("node_path") ? String(p_params["node_path"]) : "";
	GridMap *gridmap = nullptr;
	bool is_existing = false;

	if (!node_path.is_empty()) {
		Node *ex = _find_node_by_path(node_path);
		if (ex) {
			gridmap = Object::cast_to<GridMap>(ex);
			if (!gridmap) {
				return MCP_INVALID_PARAMS("Node is not a GridMap: " + node_path);
			}
			is_existing = true;
		}
	}

	if (!gridmap) {
		gridmap = memnew(GridMap);
		gridmap->set_name(p_params.get("name", "GridMap"));
	}

	if (p_params.has("mesh_library_path")) {
		String mlp = p_params["mesh_library_path"];
		if (!ResourceLoader::exists(mlp)) {
			if (!is_existing) {
				memdelete(gridmap);
			}
			return MCP_INVALID_PARAMS("Mesh library not found: " + mlp);
		}
		Ref<MeshLibrary> ml = ResourceLoader::load(mlp);
		if (ml.is_valid()) {
			gridmap->set_mesh_library(ml);
		} else {
			if (!is_existing) {
				memdelete(gridmap);
			}
			return MCP_INVALID_PARAMS("Not a mesh library: " + mlp);
		}
	}

	if (p_params.has("cell_size")) {
		gridmap->set_cell_size(_parse_vector3(p_params["cell_size"], Vector3(2, 2, 2)));
	}

	gridmap->set_position(_parse_vector3(p_params.get("position", Variant()), gridmap->get_position()));

	if (!is_existing) {
		_add_child_with_undo(gridmap, parent, root, "MCP: Add GridMap");
	}

	int cells_set = 0;
	if (p_params.has("cells") && p_params["cells"].get_type() == Variant::ARRAY) {
		Array cells = p_params["cells"];
		for (int i = 0; i < cells.size(); i++) {
			if (cells[i].get_type() == Variant::DICTIONARY) {
				Dictionary cell = cells[i];
				int x = cell.get("x", 0);
				int y = cell.get("y", 0);
				int z = cell.get("z", 0);
				int item = cell.get("item", 0);
				int orientation = cell.get("orientation", 0);
				gridmap->set_cell_item(Vector3i(x, y, z), item, orientation);
				cells_set++;
			}
		}
	}

	Dictionary res;
	res["node_path"] = root->get_path_to(gridmap);
	res["name"] = gridmap->get_name();
	res["cells_set"] = cells_set;
	res["is_existing"] = is_existing;
	res["has_mesh_library"] = gridmap->get_mesh_library().is_valid();
	return MCP_SUCCESS(res);
}

#endif
