/**************************************************************************/
/*  entity_assembler.cpp                                                  */
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

#include "entity_assembler.h"

#include "modules/trenchbroom/fgd/blazium_fgd_point_class.h"
#include "modules/trenchbroom/fgd/blazium_fgd_solid_class.h"
#include "modules/trenchbroom/trenchbroom_map.h"
#include "modules/trenchbroom/trenchbroom_map_settings.h"
#include "modules/trenchbroom/util/trenchbroom_util.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/3d/occluder_instance_3d.h"
#include "scene/3d/physics/collision_object_3d.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/resources/packed_scene.h"

static bool _node_has_property(Node *p_node, const StringName &p_property_name) {
	List<PropertyInfo> property_list;
	p_node->get_property_list(&property_list);
	for (const List<PropertyInfo>::Element *element = property_list.front(); element; element = element->next()) {
		if (element->get().name == p_property_name) {
			return true;
		}
	}
	return false;
}

void TrenchbroomEntityAssembler::_bind_methods() {
}

TrenchbroomEntityAssembler::TrenchbroomEntityAssembler(const TrenchbroomMapSettings *p_settings) {
	map_settings = p_settings;
}

Ref<Script> TrenchbroomEntityAssembler::get_script_by_class_name(const String &p_class_name) {
	if (ResourceLoader::exists(p_class_name, "Script")) {
		return ResourceLoader::load(p_class_name);
	}

	const TypedArray<Dictionary> global_classes = ProjectSettings::get_singleton()->get_global_class_list();
	for (int i = 0; i < global_classes.size(); i++) {
		const Dictionary global_class = global_classes[i];
		if (String(global_class["class"]) == p_class_name) {
			return ResourceLoader::load(String(global_class["path"]));
		}
	}

	return Ref<Script>();
}

Node3D *TrenchbroomEntityAssembler::generate_group_node(GroupData &p_group_data) {
	Node3D *group_node = memnew(Node3D);
	group_node->set_name(p_group_data.name);
	p_group_data.node = group_node;
	return group_node;
}

Node *TrenchbroomEntityAssembler::generate_solid_entity_node(Node *p_node, const String &p_node_name, EntityData &p_data, const BlaziumFGDSolidClass *p_definition) {
	ERR_FAIL_NULL_V(p_definition, nullptr);

	if (p_definition->get_spawn_type() == BlaziumFGDSolidClass::SPAWN_MERGE_WORLDSPAWN) {
		return nullptr;
	}

	String node_name = p_node_name;
	Node *node = p_node;

	if (!p_definition->get_node_class().is_empty()) {
		if (ClassDB::class_exists(p_definition->get_node_class())) {
			node = Object::cast_to<Node>(ClassDB::instantiate(p_definition->get_node_class()));
		} else {
			const Ref<Script> node_script = get_script_by_class_name(p_definition->get_node_class());
			if (node_script.is_valid() && node_script->can_instantiate()) {
				Object *script_object = ClassDB::instantiate(node_script->get_instance_base_type());
				node_script->instance_create(script_object);
				node = Object::cast_to<Node>(script_object);
			}
		}
	} else {
		node = memnew(Node3D);
	}

	ERR_FAIL_NULL_V(node, nullptr);

	if (node_name.begins_with("%")) {
		node_name = node_name.trim_prefix("%");
		node->set_name(node_name);
		node->set_unique_name_in_owner(true);
	} else {
		node->set_name(node_name);
	}

	node_name = node_name.trim_suffix(p_definition->get_classname()).trim_suffix("_");

	if (p_data.mesh.is_valid()) {
		MeshInstance3D *mesh_instance = memnew(MeshInstance3D);
		mesh_instance->set_name(node_name + "_mesh_instance");
		mesh_instance->set_mesh(p_data.mesh);
		mesh_instance->set_gi_mode(p_definition->get_global_illumination_mode());
		mesh_instance->set_cast_shadows_setting(p_definition->get_shadow_casting_setting());
		mesh_instance->set_layer_mask(p_definition->get_render_layers());
		node->add_child(mesh_instance);
		p_data.mesh_instance = mesh_instance;

		if (p_definition->get_build_occlusion() && p_data.mesh.is_valid()) {
			PackedVector3Array verts;
			PackedInt32Array indices;
			int index = 0;
			for (int surf_idx = 0; surf_idx < p_data.mesh->get_surface_count(); surf_idx++) {
				const int vert_count = verts.size();
				const Array surf_array = p_data.mesh->surface_get_arrays(surf_idx);
				const PackedVector3Array surf_vertices = surf_array[Mesh::ARRAY_VERTEX];
				const PackedInt32Array surf_indices = surf_array[Mesh::ARRAY_INDEX];
				for (int vert_i = 0; vert_i < surf_vertices.size(); vert_i++) {
					verts.push_back(surf_vertices[vert_i]);
				}
				const int old_size = indices.size();
				indices.resize(old_size + surf_indices.size());
				for (int new_index_i = 0; new_index_i < surf_indices.size(); new_index_i++) {
					indices.set(index, surf_indices[new_index_i] + vert_count);
					index++;
				}
			}

			Ref<ArrayOccluder3D> occluder;
			occluder.instantiate();
			occluder->set_arrays(verts, indices);
			OccluderInstance3D *occluder_instance = memnew(OccluderInstance3D);
			occluder_instance->set_name(node_name + "_occluder_instance");
			occluder_instance->set_occluder(occluder);
			node->add_child(occluder_instance);
			p_data.occluder_instance = occluder_instance;
		}

		if (!(build_flags & TrenchbroomMap::DISABLE_SMOOTHING) &&
				p_data.is_smooth_shaded(map_settings->get_entity_smoothing_property())) {
			Ref<ArrayMesh> smoothed_mesh = TrenchbroomUtil::smooth_mesh_by_angle(
					p_data.mesh,
					p_data.get_smoothing_angle(map_settings->get_entity_smoothing_angle_property()));
			mesh_instance->set_mesh(smoothed_mesh);

			if (p_data.is_gi_enabled() && (build_flags & TrenchbroomMap::UNWRAP_UV2)) {
				Ref<ArrayMesh> array_mesh = mesh_instance->get_mesh();
				if (array_mesh.is_valid()) {
					array_mesh->lightmap_unwrap(
							Transform3D(),
							map_settings->get_uv_unwrap_texel_size() * map_settings->get_scale_factor());
					mesh_instance->set_mesh(array_mesh);
				}
			}
		}
	}

	CollisionObject3D *collision_object = Object::cast_to<CollisionObject3D>(node);
	if (!p_data.shapes.is_empty() && collision_object) {
		collision_object->set_collision_layer(p_definition->get_collision_layer());
		collision_object->set_collision_mask(p_definition->get_collision_mask());
		collision_object->set_collision_priority(p_definition->get_collision_priority());

		Vector<PackedInt32Array> shape_to_face_array;
		if (p_data.mesh_metadata.has("shape_to_face_array")) {
			const Array metadata_array = p_data.mesh_metadata["shape_to_face_array"];
			for (int i = 0; i < metadata_array.size(); i++) {
				shape_to_face_array.push_back(metadata_array[i]);
			}
			p_data.mesh_metadata.erase("shape_to_face_array");
		}

		Dictionary face_index_metadata;
		for (uint32_t shape_index = 0; shape_index < p_data.shapes.size(); shape_index++) {
			Ref<Shape3D> shape = p_data.shapes[shape_index];
			CollisionShape3D *collision_shape = memnew(CollisionShape3D);
			if (p_definition->get_collision_shape_type() == BlaziumFGDSolidClass::COLLISION_CONCAVE) {
				collision_shape->set_name(node_name + "_collision_shape");
			} else {
				collision_shape->set_name(vformat("%s_brush_%s_collision_shape", node_name, shape_index));
			}
			shape->set_margin(p_definition->get_collision_shape_margin());
			collision_shape->set_shape(shape);
			collision_shape->set_owner(node->get_owner());
			node->add_child(collision_shape);
			p_data.collision_shapes.push_back(collision_shape);

			if ((int)shape_index < shape_to_face_array.size()) {
				face_index_metadata[collision_shape->get_name()] = shape_to_face_array[shape_index];
			}
		}

		if (p_definition->get_add_collision_shape_to_face_indices_metadata()) {
			p_data.mesh_metadata["collision_shape_to_face_indices_map"] = face_index_metadata;
		}
	}

	if (_node_has_property(node, "position")) {
		const Variant position_value = node->get("position");
		if (position_value.get_type() == Variant::VECTOR3) {
			node->set("position", TrenchbroomUtil::id_to_opengl(p_data.origin));
		} else if (position_value.get_type() == Variant::VECTOR2 && map_settings) {
			node->set("position", Vector2(p_data.origin.z, -p_data.origin.y) * map_settings->get_inverse_scale_factor());
		}
	}

	if (!p_data.mesh_metadata.is_empty()) {
		node->set_meta("trenchbroom_mesh_data", p_data.mesh_metadata);
	}

	return node;
}

Node *TrenchbroomEntityAssembler::generate_point_entity_node(Node *p_node, const String &p_node_name, Dictionary &p_properties, const BlaziumFGDPointClass *p_definition) {
	ERR_FAIL_NULL_V(p_definition, nullptr);

	Node *node = p_node;
	const String classname = p_properties.has("classname") ? String(p_properties["classname"]) : String();

	if (p_definition->get_scene_file().is_valid()) {
		PackedScene::GenEditState flag = PackedScene::GEN_EDIT_STATE_DISABLED;
#ifdef TOOLS_ENABLED
		if (Engine::get_singleton()->is_editor_hint()) {
			flag = PackedScene::GEN_EDIT_STATE_INSTANCE;
		}
#endif
		node = p_definition->get_scene_file()->instantiate(flag);
	} else if (!p_definition->get_node_class().is_empty()) {
		if (ClassDB::class_exists(p_definition->get_node_class())) {
			node = Object::cast_to<Node>(ClassDB::instantiate(p_definition->get_node_class()));
		} else {
			const Ref<Script> node_script = get_script_by_class_name(p_definition->get_node_class());
			if (node_script.is_valid() && node_script->can_instantiate()) {
				Object *script_object = ClassDB::instantiate(node_script->get_instance_base_type());
				node_script->instance_create(script_object);
				node = Object::cast_to<Node>(script_object);
			}
		}
	}

	if (!node) {
		node = memnew(Node3D);
	}

	node->set_name(p_node_name);

	if (_node_has_property(node, "rotation_degrees") && p_definition->get_apply_rotation_on_map_build()) {
		Vector3 angles;
		if (p_properties.has("angles") || p_properties.has("mangle")) {
			const String key = p_properties.has("angles") ? "angles" : "mangle";
			Vector<double> parsed_angles;
			const Variant angles_raw = p_properties[key];
			if (angles_raw.get_type() == Variant::STRING) {
				parsed_angles = String(angles_raw).split_floats(" ", false);
				if (parsed_angles.size() < 3) {
					ERR_PRINT(vformat("Invalid vector format for \"%s\" in entity \"%s\"", key, classname));
				}
			} else if (angles_raw.get_type() == Variant::PACKED_FLOAT64_ARRAY) {
				parsed_angles = angles_raw;
			} else if (angles_raw.get_type() == Variant::ARRAY) {
				const Array angle_array = angles_raw;
				for (int angle_index = 0; angle_index < angle_array.size(); angle_index++) {
					parsed_angles.push_back(angle_array[angle_index]);
				}
			}

			if (parsed_angles.size() >= 3) {
				angles = Vector3(-parsed_angles[0], parsed_angles[1], -parsed_angles[2]);
				if (key == "mangle") {
					if (classname.begins_with("light")) {
						angles = Vector3(parsed_angles[1], parsed_angles[0], -parsed_angles[2]);
					} else if (classname == "info_intermission") {
						angles = Vector3(parsed_angles[0], parsed_angles[1], -parsed_angles[2]);
					}
				}
			}
		} else if (p_properties.has("angle")) {
			const real_t angle = p_properties["angle"];
			if (Math::is_equal_approx((double)angle, -1.0)) {
				angles.x = 90.0;
			} else if (Math::is_equal_approx((double)angle, -2.0)) {
				angles.x = -90.0;
			} else {
				angles.y += angle;
			}
		}
		angles.y += 180.0;
		node->set("rotation_degrees", angles);
	}

	if (_node_has_property(node, "scale") && p_definition->get_apply_scale_on_map_build() && p_properties.has("scale")) {
		Variant scale_prop = p_properties["scale"];
		if (scale_prop.get_type() == Variant::STRING) {
			const PackedStringArray scale_arr = String(scale_prop).split(" ", false);
			switch (scale_arr.size()) {
				case 1:
					scale_prop = scale_arr[0].to_float();
					break;
				case 3:
					scale_prop = Vector3(scale_arr[1].to_float(), scale_arr[2].to_float(), scale_arr[0].to_float());
					break;
				case 2:
					scale_prop = Vector2(scale_arr[0].to_float(), scale_arr[0].to_float());
					break;
				default:
					break;
			}
		}

		const Variant node_scale = node->get("scale");
		if (scale_prop.is_num()) {
			const real_t scale_factor = scale_prop;
			if (node_scale.get_type() == Variant::VECTOR3) {
				node->set("scale", Vector3(node_scale) * scale_factor);
			} else if (node_scale.get_type() == Variant::VECTOR2) {
				node->set("scale", Vector2(node_scale) * scale_factor);
			} else if (node_scale.is_num()) {
				node->set("scale", (real_t)node_scale * scale_factor);
			}
		} else if (node_scale.get_type() == Variant::VECTOR3 &&
				(scale_prop.get_type() == Variant::VECTOR3 || scale_prop.get_type() == Variant::VECTOR3I)) {
			node->set("scale", Vector3(node_scale) * Vector3(scale_prop));
		} else if (node_scale.get_type() == Variant::VECTOR2 &&
				(scale_prop.get_type() == Variant::VECTOR2 || scale_prop.get_type() == Variant::VECTOR2I)) {
			node->set("scale", Vector2(node_scale) * Vector2(scale_prop));
		}
	}

	if (p_properties.has("origin") && map_settings) {
		Vector3 origin_vec;
		const Variant origin_prop = p_properties["origin"];
		if (origin_prop.get_type() == Variant::VECTOR3) {
			const Vector3 origin_vector = origin_prop;
			origin_vec = Vector3(origin_vector.y, origin_vector.z, origin_vector.x);
		} else if (origin_prop.get_type() == Variant::STRING) {
			const Vector<double> origin_comps = String(origin_prop).split_floats(" ", false);
			if (origin_comps.size() > 2) {
				origin_vec = Vector3(origin_comps[1], origin_comps[2], origin_comps[0]);
			} else {
				ERR_PRINT(vformat("Invalid vector format for \"origin\" in %s", p_node_name));
			}
		} else {
			ERR_PRINT(vformat("Invalid vector format for \"origin\" in %s", p_node_name));
		}

		if (_node_has_property(node, "position")) {
			const Variant position_value = node->get("position");
			if (position_value.get_type() == Variant::VECTOR3) {
				node->set("position", origin_vec * map_settings->get_scale_factor());
			} else if (position_value.get_type() == Variant::VECTOR2) {
				node->set("position", Vector2(origin_vec.z, -origin_vec.y));
			}
		}
	}

	return node;
}

void TrenchbroomEntityAssembler::apply_entity_properties(Node *p_node, EntityData &p_data) {
	ERR_FAIL_NULL(p_node);

	const Dictionary properties = p_data.properties;
	const BlaziumFGDEntityClass *entity_def = p_data.definition.ptr();

	if (entity_def && entity_def->get_auto_apply_to_matching_node_properties()) {
		const Array property_keys = properties.keys();
		for (int i = 0; i < property_keys.size(); i++) {
			const String property = property_keys[i];
			const BlaziumFGDPointClass *point_def = Object::cast_to<BlaziumFGDPointClass>(entity_def);
			if (property == "scale" && point_def && point_def->get_apply_scale_on_map_build()) {
				continue;
			}
			if (_node_has_property(p_node, property)) {
				const Variant node_value = p_node->get(property);
				const Variant property_value = properties[property];
				if (node_value.get_type() == property_value.get_type()) {
					p_node->set(property, property_value);
				} else {
					ERR_PRINT(vformat("Entity %s property '%s' type mismatch with matching generated node property.", p_node->get_name(), property));
				}
			}
		}
	}

	if (_node_has_property(p_node, "trenchbroom_properties")) {
		p_node->set("trenchbroom_properties", properties);
	}

	if (_node_has_property(p_node, "func_godot_properties")) {
		p_node->set("func_godot_properties", properties);
	}

	if (p_node->has_method("_trenchbroom_apply_properties")) {
		Callable::CallError err;
		const Variant args[] = { properties };
		const Variant *argptrs[] = { &args[0] };
		p_node->callp("_trenchbroom_apply_properties", argptrs, 1, err);
	} else if (p_node->has_method("_func_godot_apply_properties")) {
		Callable::CallError err;
		const Variant args[] = { properties };
		const Variant *argptrs[] = { &args[0] };
		p_node->callp("_func_godot_apply_properties", argptrs, 1, err);
	}

	if (p_node->has_method("_trenchbroom_build_complete")) {
		p_node->call_deferred("_trenchbroom_build_complete");
	} else if (p_node->has_method("_func_godot_build_complete")) {
		p_node->call_deferred("_func_godot_build_complete");
	}
}

Node *TrenchbroomEntityAssembler::generate_entity_node(EntityData &p_entity_data, int p_entity_index) {
	Node *node = nullptr;
	String node_name = vformat("entity_%s", p_entity_index);
	const Dictionary properties = p_entity_data.properties;
	const BlaziumFGDEntityClass *entity_def = p_entity_data.definition.ptr();
	ERR_FAIL_NULL_V(entity_def, nullptr);

	if (properties.has("classname")) {
		node_name += "_" + String(properties["classname"]);
	}

	String name_prop;
	if (!entity_def->get_name_property().is_empty() && properties.has(entity_def->get_name_property())) {
		name_prop = String(properties[entity_def->get_name_property()]);
	} else if (map_settings && !map_settings->get_entity_name_property().is_empty() &&
			properties.has(map_settings->get_entity_name_property())) {
		name_prop = String(properties[map_settings->get_entity_name_property()]);
	}

	if (!name_prop.is_empty()) {
		if (name_prop.begins_with("%")) {
			node_name = name_prop;
		} else {
			node_name = "entity_" + name_prop;
		}
	}

	const BlaziumFGDSolidClass *solid_def = Object::cast_to<BlaziumFGDSolidClass>(entity_def);
	const BlaziumFGDPointClass *point_def = Object::cast_to<BlaziumFGDPointClass>(entity_def);
	if (solid_def) {
		node = generate_solid_entity_node(node, node_name, p_entity_data, solid_def);
	} else if (point_def) {
		node = generate_point_entity_node(node, node_name, p_entity_data.properties, point_def);
		if (node && p_entity_data.mesh.is_valid()) {
			MeshInstance3D *mesh_instance = memnew(MeshInstance3D);
			mesh_instance->set_name(node_name + "_mesh_instance");
			mesh_instance->set_mesh(p_entity_data.mesh);
			node->add_child(mesh_instance);
			p_entity_data.mesh_instance = mesh_instance;
		}
	}

	if (node) {
		Ref<Script> script_class;
		if (point_def) {
			script_class = point_def->get_script_class();
		} else if (solid_def) {
			script_class = solid_def->get_script_class();
		}
		if (script_class.is_valid()) {
			node->set_script(script_class);
		}

		TypedArray<String> node_groups;
		if (map_settings) {
			node_groups = map_settings->get_entity_node_groups();
		}
		const Vector<String> entity_groups = entity_def->get_node_groups();
		for (int i = 0; i < entity_groups.size(); i++) {
			node_groups.push_back(entity_groups[i]);
		}
		for (int i = 0; i < node_groups.size(); i++) {
			const String node_group = node_groups[i];
			if (!node_group.is_empty()) {
				node->add_to_group(node_group, true);
			}
		}
	}

	return node;
}

void TrenchbroomEntityAssembler::build(TrenchbroomMap *p_map_node, LocalVector<EntityData> &p_entities, LocalVector<GroupData> &p_groups) {
	ERR_FAIL_NULL(p_map_node);
	ERR_FAIL_NULL(map_settings);

	Node *scene_root = p_map_node->is_inside_tree() ? p_map_node->get_tree()->get_edited_scene_root() : p_map_node;
	build_flags = p_map_node->get_build_flags();
	const bool show_profile = build_flags & TrenchbroomMap::SHOW_PROFILE_INFO;

	if (map_settings->get_use_groups_hierarchy()) {
		if (show_profile) {
			TrenchbroomUtil::print_profile_info(vformat("Generating %d groups", p_groups.size()), "[ASM]");
		}
		for (uint32_t group_index = 0; group_index < p_groups.size(); group_index++) {
			generate_group_node(p_groups[group_index]);
		}

		for (uint32_t group_index = 0; group_index < p_groups.size(); group_index++) {
			GroupData &group = p_groups[group_index];
			if (group.parent_id < 0) {
				p_map_node->add_child(group.node);
				group.node->set_owner(scene_root);
			} else {
				for (uint32_t parent_index = 0; parent_index < p_groups.size(); parent_index++) {
					GroupData &parent = p_groups[parent_index];
					if (group.parent_id == parent.id) {
						parent.node->add_child(group.node);
						group.node->set_owner(scene_root);
						break;
					}
				}
			}
		}
	}

	if (show_profile) {
		TrenchbroomUtil::print_profile_info(vformat("Assembling %d entities", p_entities.size()), "[ASM]");
	}
	for (int entity_index = 0; entity_index < (int)p_entities.size(); entity_index++) {
		EntityData &entity_data = p_entities[entity_index];
		if (entity_data.properties.has("classname") &&
				String(entity_data.properties["classname"]) == "_cordon_volume" &&
				!(build_flags & TrenchbroomMap::INCLUDE_CORDON_VOLUMES)) {
			continue;
		}
		Node *entity_node = generate_entity_node(entity_data, entity_index);
		if (!entity_node) {
			continue;
		}

		if (!map_settings->get_use_groups_hierarchy() || !entity_data.group) {
			p_map_node->add_child(entity_node);
			if (entity_index == 0) {
				p_map_node->move_child(entity_node, 0);
			}
		} else {
			for (uint32_t group_index = 0; group_index < p_groups.size(); group_index++) {
				GroupData &group = p_groups[group_index];
				if (entity_data.group->id == group.id) {
					group.node->add_child(entity_node);
					break;
				}
			}
		}

		entity_node->set_owner(scene_root);
		if (entity_data.mesh_instance) {
			entity_data.mesh_instance->set_owner(scene_root);
		}
		for (uint32_t shape_index = 0; shape_index < entity_data.collision_shapes.size(); shape_index++) {
			CollisionShape3D *collision_shape = entity_data.collision_shapes[shape_index];
			if (collision_shape) {
				collision_shape->set_owner(scene_root);
			}
		}
		if (entity_data.occluder_instance) {
			entity_data.occluder_instance->set_owner(scene_root);
		}

		apply_entity_properties(entity_node, entity_data);
	}

	if (show_profile) {
		TrenchbroomUtil::print_profile_info("Entity assembly complete", "[ASM]");
	}
}
