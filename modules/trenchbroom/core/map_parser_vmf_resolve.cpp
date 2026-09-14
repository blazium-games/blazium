/**************************************************************************/
/*  map_parser_vmf_resolve.cpp                                            */
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

#include "map_parser.h"

#include "core/io/file_access.h"
#include "core/math/transform_3d.h"
#include "core/templates/hash_set.h"
#include "modules/trenchbroom/fgd/blazium_fgd_entity_class.h"
#include "modules/trenchbroom/trenchbroom_map_settings.h"

Vector3 TrenchbroomMapParser::_parse_vmf_vector(const String &p_value) {
	String cleaned = p_value.strip_edges();
	cleaned = cleaned.replace("[", "").replace("]", "").replace("(", "").replace(")", "");
	const Vector<double> comps = cleaned.split_floats(" ", false);
	if (comps.size() >= 3) {
		return Vector3(comps[0], comps[1], comps[2]);
	}
	return Vector3();
}

void TrenchbroomMapParser::_transform_brush(BrushData &p_brush, const Transform3D &p_xform) {
	for (uint32_t face_index = 0; face_index < p_brush.faces.size(); face_index++) {
		Plane &plane = p_brush.planes[face_index];
		const Vector3 normal = p_xform.basis.xform(plane.normal).normalized();
		const Vector3 point = p_xform.xform(plane.normal * plane.d);
		plane = Plane(normal, point);
		p_brush.faces[face_index].plane = plane;
	}
}

void TrenchbroomMapParser::_transform_patch(PatchData &p_patch, const Transform3D &p_xform) {
	for (int point_index = 0; point_index < p_patch.points.size(); point_index++) {
		p_patch.points.write[point_index] = p_xform.xform(p_patch.points[point_index]);
	}
}

void TrenchbroomMapParser::_transform_entity_origin(EntityData &p_entity, const Transform3D &p_xform) {
	if (p_entity.properties.has("origin")) {
		const Vector3 origin = _parse_vmf_vector(String(p_entity.properties["origin"]));
		const Vector3 transformed = p_xform.xform(origin);
		p_entity.properties["origin"] = vformat("%f %f %f", transformed.x, transformed.y, transformed.z);
	}
}

void TrenchbroomMapParser::_resolve_vmf_instances(ParseData &r_parse_data, const String &p_map_file, const TrenchbroomMapSettings *p_map_settings) {
	HashSet<String> resolution_stack;
	_resolve_vmf_instances_impl(r_parse_data, p_map_file, p_map_settings, resolution_stack);
}

void TrenchbroomMapParser::_resolve_vmf_instances_impl(ParseData &r_parse_data, const String &p_map_file, const TrenchbroomMapSettings *p_map_settings, HashSet<String> &p_resolution_stack) {
	const String map_dir = p_map_file.get_base_dir();

	int worldspawn_index = -1;
	for (uint32_t i = 0; i < r_parse_data.entities.size(); i++) {
		if (r_parse_data.entities[i].properties.has("classname") &&
				String(r_parse_data.entities[i].properties["classname"]) == "worldspawn") {
			worldspawn_index = (int)i;
			break;
		}
	}

	for (int entity_index = (int)r_parse_data.entities.size() - 1; entity_index >= 0; entity_index--) {
		EntityData &entity = r_parse_data.entities[entity_index];
		if (!entity.properties.has("classname")) {
			continue;
		}
		const String classname = entity.properties["classname"];
		if (classname != "func_instance" && classname != "func_instancing") {
			continue;
		}
		if (!entity.properties.has("file")) {
			continue;
		}

		String prefab_file = String(entity.properties["file"]).replace("\\", "/");
		if (!prefab_file.is_absolute_path()) {
			prefab_file = map_dir.path_join(prefab_file);
		}
		if (p_resolution_stack.has(prefab_file) || !FileAccess::exists(prefab_file)) {
			r_parse_data.entities.remove_at(entity_index);
			continue;
		}

		p_resolution_stack.insert(prefab_file);

		ParseData prefab_data = parse_map_data(prefab_file, p_map_settings, false, false);
		if (prefab_data.entities.is_empty()) {
			p_resolution_stack.erase(prefab_file);
			r_parse_data.entities.remove_at(entity_index);
			continue;
		}

		_resolve_vmf_instances_impl(prefab_data, prefab_file, p_map_settings, p_resolution_stack);

		Vector3 origin = entity.properties.has("origin") ? _parse_vmf_vector(String(entity.properties["origin"])) : Vector3();
		Vector3 angles = entity.properties.has("angles") ? _parse_vmf_vector(String(entity.properties["angles"])) : Vector3();
		const Transform3D instance_xform(Basis::from_euler(Vector3(
												 Math::deg_to_rad(angles.x),
												 Math::deg_to_rad(angles.y),
												 Math::deg_to_rad(angles.z))),
				origin);

		int prefab_world_index = 0;
		for (uint32_t prefab_index = 0; prefab_index < prefab_data.entities.size(); prefab_index++) {
			if (prefab_data.entities[prefab_index].properties.has("classname") &&
					String(prefab_data.entities[prefab_index].properties["classname"]) == "worldspawn") {
				prefab_world_index = (int)prefab_index;
				break;
			}
		}

		const EntityData &prefab_world = prefab_data.entities[prefab_world_index];
		if (worldspawn_index < 0) {
			p_resolution_stack.erase(prefab_file);
			r_parse_data.entities.remove_at(entity_index);
			continue;
		}

		EntityData &worldspawn = r_parse_data.entities[worldspawn_index];
		for (uint32_t brush_index = 0; brush_index < prefab_world.brushes.size(); brush_index++) {
			BrushData brush = prefab_world.brushes[brush_index];
			_transform_brush(brush, instance_xform);
			worldspawn.brushes.push_back(brush);
		}
		for (uint32_t patch_index = 0; patch_index < prefab_world.patches.size(); patch_index++) {
			PatchData patch = prefab_world.patches[patch_index];
			_transform_patch(patch, instance_xform);
			worldspawn.patches.push_back(patch);
		}

		for (uint32_t prefab_index = 0; prefab_index < prefab_data.entities.size(); prefab_index++) {
			if ((int)prefab_index == prefab_world_index) {
				continue;
			}
			EntityData prefab_entity = prefab_data.entities[prefab_index];
			if (prefab_entity.properties.has("classname") &&
					String(prefab_entity.properties["classname"]) == "worldspawn") {
				continue;
			}
			_transform_entity_origin(prefab_entity, instance_xform);
			if (entity.properties.has("replace") && prefab_entity.properties.has("targetname")) {
				const String replace_prefix = String(entity.properties["replace"]);
				const String targetname = String(prefab_entity.properties["targetname"]);
				if (!replace_prefix.is_empty() && targetname.begins_with(replace_prefix)) {
					prefab_entity.properties["targetname"] = targetname.substr(replace_prefix.length());
				}
			}
			r_parse_data.entities.push_back(prefab_entity);
		}

		p_resolution_stack.erase(prefab_file);
		r_parse_data.entities.remove_at(entity_index);
	}
}
