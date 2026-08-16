/**************************************************************************/
/*  justamcp_scene_3d_tools.cpp                                           */
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

#include "justamcp_scene_3d_tools.h"

#include "../justamcp_editor_scene_access.h"
#include "../justamcp_mcp_tool_macros.h"

#include "core/io/resource_loader.h"
#include "editor/editor_undo_redo_manager.h"
#include "scene/3d/camera_3d.h" // IWYU pragma: keep
#include "scene/3d/light_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/world_environment.h" // IWYU pragma: keep
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/3d/sky_material.h" // IWYU pragma: keep
#include "scene/resources/environment.h" // IWYU pragma: keep
#include "scene/resources/material.h" // IWYU pragma: keep
#include "scene/resources/packed_scene.h"
#include "scene/resources/sky.h" // IWYU pragma: keep

#include "modules/gridmap/grid_map.h" // IWYU pragma: keep

JustAMCPScene3DTools::JustAMCPScene3DTools() {
}

JustAMCPScene3DTools::~JustAMCPScene3DTools() {
}

Node *JustAMCPScene3DTools::_find_node_by_path(const String &p_path) {
	if (p_path == "." || p_path.is_empty()) {
		return JustAMCPEditorSceneAccess::get_edited_root();
	}
	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return nullptr;
	}
	return root->get_node_or_null(NodePath(p_path));
}

void JustAMCPScene3DTools::_add_child_with_undo(Node *p_node, Node *p_parent, Node *p_root, const String &p_action_name) {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();

	undo_redo->create_action(p_action_name);
	undo_redo->add_do_method(p_parent, "add_child", p_node);
	undo_redo->add_do_method(p_node, "set_owner", p_root);
	undo_redo->add_do_reference(p_node);
	undo_redo->add_undo_method(p_parent, "remove_child", p_node);
	undo_redo->commit_action();
}

Color JustAMCPScene3DTools::_parse_color(const Variant &p_val, const Color &p_default) {
	if (p_val.get_type() == Variant::STRING) {
		return Color::html(p_val);
	} else if (p_val.get_type() == Variant::DICTIONARY) {
		Dictionary d = p_val;
		return Color(
				d.has("r") ? (float)d["r"] : p_default.r,
				d.has("g") ? (float)d["g"] : p_default.g,
				d.has("b") ? (float)d["b"] : p_default.b,
				d.has("a") ? (float)d["a"] : p_default.a);
	}
	return p_default;
}

Vector3 JustAMCPScene3DTools::_parse_vector3(const Variant &p_val, const Vector3 &p_default) {
	if (p_val.get_type() == Variant::STRING) {
		String s = p_val;
		s = s.replace("(", "").replace(")", "");
		Vector<String> parts = s.split(",");
		if (parts.size() >= 3) {
			return Vector3(parts[0].to_float(), parts[1].to_float(), parts[2].to_float());
		}
	} else if (p_val.get_type() == Variant::DICTIONARY) {
		Dictionary d = p_val;
		return Vector3(
				d.has("x") ? (float)d["x"] : p_default.x,
				d.has("y") ? (float)d["y"] : p_default.y,
				d.has("z") ? (float)d["z"] : p_default.z);
	} else if (p_val.get_type() == Variant::ARRAY) {
		Array arr = p_val;
		if (arr.size() >= 3) {
			return Vector3(arr[0], arr[1], arr[2]);
		}
	}
	return p_default;
}

float JustAMCPScene3DTools::_optional_float(const Dictionary &p_params, const String &p_key, float p_default) {
	if (p_params.has(p_key)) {
		return p_params[p_key];
	}
	return p_default;
}

Dictionary JustAMCPScene3DTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "add_mesh_instance") {
		return add_mesh_instance(p_args);
	}
	if (p_tool_name == "setup_lighting") {
		return setup_lighting(p_args);
	}
	if (p_tool_name == "set_material_3d") {
		return set_material_3d(p_args);
	}
	if (p_tool_name == "setup_environment") {
		return setup_environment(p_args);
	}
	if (p_tool_name == "setup_camera_3d") {
		return setup_camera_3d(p_args);
	}
	if (p_tool_name == "add_gridmap") {
		return add_gridmap(p_args);
	}

	return Dictionary();
}

Dictionary JustAMCPScene3DTools::add_mesh_instance(const Dictionary &p_params) {
	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No active scene");
	}

	String parent_path = p_params.has("parent_path") ? String(p_params["parent_path"]) : ".";
	Node *parent = _find_node_by_path(parent_path);
	if (!parent) {
		return MCP_INVALID_PARAMS("Parent node not found: " + parent_path);
	}

	String node_name = p_params.has("name") ? String(p_params["name"]) : "MeshInstance3D";
	String mesh_type = p_params.has("mesh_type") ? String(p_params["mesh_type"]) : "";
	String mesh_file = p_params.has("mesh_file") ? String(p_params["mesh_file"]) : "";

	if (mesh_type.is_empty() && mesh_file.is_empty()) {
		return MCP_INVALID_PARAMS("Either mesh_type or mesh_file is required.");
	}

	MeshInstance3D *mesh_instance = memnew(MeshInstance3D);
	mesh_instance->set_name(node_name);

	if (!mesh_file.is_empty()) {
		if (!ResourceLoader::exists(mesh_file)) {
			memdelete(mesh_instance);
			return MCP_INVALID_PARAMS("Mesh file not found: " + mesh_file);
		}
		Ref<Resource> loaded = ResourceLoader::load(mesh_file);
		if (loaded.is_valid()) {
			if (loaded->is_class("Mesh")) {
				mesh_instance->set_mesh(loaded);
			} else if (loaded->is_class("PackedScene")) {
				Ref<PackedScene> scene = loaded;
				Node *scene_inst = scene->instantiate();
				if (scene_inst) {
					Ref<Mesh> found_mesh;
					List<Node *> q;
					q.push_back(scene_inst);
					while (q.size()) {
						Node *n = q.front()->get();
						q.pop_front();
						MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(n);
						if (mi && mi->get_mesh().is_valid()) {
							found_mesh = mi->get_mesh();
							break;
						}
						for (int i = 0; i < n->get_child_count(); i++) {
							q.push_back(n->get_child(i));
						}
					}
					memdelete(scene_inst);
					if (found_mesh.is_null()) {
						memdelete(mesh_instance);
						return MCP_INVALID_PARAMS("No mesh found in packed scene: " + mesh_file);
					}
					mesh_instance->set_mesh(found_mesh);
				}
			} else {
				memdelete(mesh_instance);
				return MCP_INVALID_PARAMS("File is not a mesh or packed scene: " + mesh_file);
			}
		} else {
			memdelete(mesh_instance);
			return MCP_INVALID_PARAMS("Failed to load mesh file: " + mesh_file);
		}
	} else {
		Ref<Mesh> m;
		if (mesh_type == "BoxMesh") {
			m.instantiate();
		} else if (mesh_type == "BoxMesh") {
			m = memnew(BoxMesh);
		} else if (mesh_type == "SphereMesh") {
			m = memnew(SphereMesh);
		} else if (mesh_type == "CylinderMesh") {
			m = memnew(CylinderMesh);
		} else if (mesh_type == "CapsuleMesh") {
			m = memnew(CapsuleMesh);
		} else if (mesh_type == "PlaneMesh") {
			m = memnew(PlaneMesh);
		} else if (mesh_type == "PrismMesh") {
			m = memnew(PrismMesh);
		} else if (mesh_type == "TorusMesh") {
			m = memnew(TorusMesh);
		} else if (mesh_type == "QuadMesh") {
			m = memnew(QuadMesh);
		} else {
			memdelete(mesh_instance);
			return MCP_INVALID_PARAMS("Unknown mesh_type: " + mesh_type);
		}

		if (p_params.has("mesh_properties")) {
			Dictionary m_props = p_params["mesh_properties"];
			Array keys = m_props.keys();
			for (int i = 0; i < keys.size(); i++) {
				m->set(keys[i], m_props[keys[i]]);
			}
		}
		mesh_instance->set_mesh(m);
	}

	Vector3 position = _parse_vector3(p_params.get("position", Variant()), Vector3());
	Vector3 rotation = _parse_vector3(p_params.get("rotation", Variant()), Vector3());
	Vector3 scale = _parse_vector3(p_params.get("scale", Variant()), Vector3(1, 1, 1));

	mesh_instance->set_position(position);
	mesh_instance->set_rotation_degrees(rotation);
	mesh_instance->set_scale(scale);

	_add_child_with_undo(mesh_instance, parent, root, "MCP: Add MeshInstance3D");

	Dictionary res;
	res["node_path"] = root->get_path_to(mesh_instance);
	res["name"] = mesh_instance->get_name();
	res["mesh_type"] = mesh_file.is_empty() ? mesh_type : mesh_file;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPScene3DTools::setup_lighting(const Dictionary &p_params) {
	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No active scene");
	}

	String parent_path = p_params.has("parent_path") ? String(p_params["parent_path"]) : ".";
	Node *parent = _find_node_by_path(parent_path);
	if (!parent) {
		return MCP_INVALID_PARAMS("Parent node not found: " + parent_path);
	}

	String light_type = p_params.has("light_type") ? String(p_params["light_type"]) : "";
	String preset = p_params.has("preset") ? String(p_params["preset"]) : "";
	String node_name = p_params.has("name") ? String(p_params["name"]) : "";

	if (!preset.is_empty()) {
		if (preset == "sun") {
			light_type = "DirectionalLight3D";
			if (node_name.is_empty()) {
				node_name = "SunLight";
			}
		} else if (preset == "indoor") {
			light_type = "OmniLight3D";
			if (node_name.is_empty()) {
				node_name = "IndoorLight";
			}
		} else if (preset == "dramatic") {
			light_type = "SpotLight3D";
			if (node_name.is_empty()) {
				node_name = "DramaticLight";
			}
		} else {
			return MCP_INVALID_PARAMS("Unknown preset: " + preset);
		}
	}

	if (light_type.is_empty()) {
		return MCP_INVALID_PARAMS("Either light_type or preset is required.");
	}

	Light3D *light = nullptr;
	if (light_type == "DirectionalLight3D") {
		light = memnew(DirectionalLight3D);
	} else if (light_type == "OmniLight3D") {
		light = memnew(OmniLight3D);
	} else if (light_type == "SpotLight3D") {
		light = memnew(SpotLight3D);
	} else {
		return MCP_INVALID_PARAMS("Unknown light_type: " + light_type);
	}

	if (node_name.is_empty()) {
		node_name = light_type;
	}
	light->set_name(node_name);

	light->set_color(_parse_color(p_params.get("color", Variant()), Color(1, 1, 1)));
	light->set_param(Light3D::PARAM_ENERGY, _optional_float(p_params, "energy", 1.0));
	light->set_shadow(p_params.get("shadows", false));

	OmniLight3D *omni = Object::cast_to<OmniLight3D>(light);
	SpotLight3D *spot = Object::cast_to<SpotLight3D>(light);

	if (omni) {
		omni->set_param(Light3D::PARAM_RANGE, _optional_float(p_params, "range", 5.0));
		omni->set_param(Light3D::PARAM_ATTENUATION, _optional_float(p_params, "attenuation", 1.0));
	} else if (spot) {
		spot->set_param(Light3D::PARAM_RANGE, _optional_float(p_params, "range", 5.0));
		spot->set_param(Light3D::PARAM_ATTENUATION, _optional_float(p_params, "attenuation", 1.0));
		spot->set_param(Light3D::PARAM_SPOT_ANGLE, _optional_float(p_params, "spot_angle", 45.0));
		spot->set_param(Light3D::PARAM_SPOT_ATTENUATION, _optional_float(p_params, "spot_angle_attenuation", 1.0));
	}

	if (!preset.is_empty()) {
		if (preset == "sun") {
			light->set_param(Light3D::PARAM_ENERGY, _optional_float(p_params, "energy", 1.0));
			light->set_shadow(p_params.get("shadows", true));
			light->set_rotation_degrees(_parse_vector3(p_params.get("rotation", Variant()), Vector3(-45, -30, 0)));
		} else if (preset == "indoor") {
			light->set_param(Light3D::PARAM_ENERGY, _optional_float(p_params, "energy", 0.8));
			light->set_color(_parse_color(p_params.get("color", Variant()), Color(1.0, 0.95, 0.85)));
			if (omni) {
				omni->set_param(Light3D::PARAM_RANGE, _optional_float(p_params, "range", 8.0));
			}
		} else if (preset == "dramatic") {
			light->set_param(Light3D::PARAM_ENERGY, _optional_float(p_params, "energy", 2.0));
			light->set_shadow(p_params.get("shadows", true));
			if (spot) {
				spot->set_param(Light3D::PARAM_SPOT_ANGLE, _optional_float(p_params, "spot_angle", 25.0));
				spot->set_param(Light3D::PARAM_RANGE, _optional_float(p_params, "range", 10.0));
			}
		}
	}

	light->set_position(_parse_vector3(p_params.get("position", Variant()), Vector3()));
	if (p_params.has("rotation")) {
		light->set_rotation_degrees(_parse_vector3(p_params.get("rotation", Variant()), light->get_rotation_degrees()));
	}

	_add_child_with_undo(light, parent, root, "MCP: Add " + light_type);

	Dictionary res;
	res["node_path"] = root->get_path_to(light);
	res["name"] = light->get_name();
	res["light_type"] = light_type;
	res["preset"] = preset;
	return MCP_SUCCESS(res);
}

#endif
