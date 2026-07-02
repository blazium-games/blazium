/**************************************************************************/
/*  map_parser_vmf.cpp                                                    */
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

#include "map_parser_vmf_dispinfo.h"
#include "modules/trenchbroom/fgd/blazium_fgd_entity_class.h"
#include "modules/trenchbroom/trenchbroom_map_settings.h"
#include "modules/trenchbroom/util/trenchbroom_util.h"

#include "core/string/print_string.h"

bool TrenchbroomMapParser::_parse_vmf(const PackedStringArray &p_map_data, const TrenchbroomMapSettings *p_map_settings, ParseData &r_parse_data) {
	LocalVector<EntityData> &entities_data = r_parse_data.entities;
	LocalVector<GroupData> &groups_data = r_parse_data.groups;

	EntityData *ent = nullptr;
	BrushData *brush = nullptr;
	GroupData *group = nullptr;
	EntityData entity_storage;
	BrushData brush_storage;
	GroupData group_storage;
	String group_display_name;
	LocalVector<int> group_parent_stack;
	int scope = 0;
	bool cordon_mode = false;
	VmfDispinfoParser dispinfo;
	EntityData cordon_entity_storage;
	EntityData *cordon_ent = nullptr;

	auto finalize_visgroup_name = [&]() {
		if (group == nullptr) {
			return;
		}
		const String safe_name = group_display_name.replace(" ", "_");
		if (safe_name.is_empty()) {
			group->name = vformat("group_%d", group->id);
		} else {
			group->name = vformat("group_%d_%s", group->id, safe_name);
		}
	};

	for (int line_index = 0; line_index < p_map_data.size(); line_index++) {
		String line = p_map_data[line_index];
		line = line.replace("\t", "");

		if (line == "cordon") {
			cordon_entity_storage = EntityData();
			cordon_entity_storage.properties["classname"] = "_cordon_volume";
			cordon_ent = &cordon_entity_storage;
			ent = cordon_ent;
			cordon_mode = true;
			brush = nullptr;
			continue;
		}
		if (line.begins_with("entity") || line.begins_with("world")) {
			if (ent != nullptr) {
				if (ent == cordon_ent) {
					if (!ent->brushes.is_empty()) {
						entities_data.push_back(*ent);
					}
					cordon_ent = nullptr;
					cordon_mode = false;
					ent = nullptr;
				} else {
					entities_data.push_back(*ent);
				}
			}
			if (ent == nullptr) {
				entity_storage = EntityData();
				ent = &entity_storage;
			}
			brush = nullptr;
			continue;
		}
		if (line.begins_with("solid")) {
			if (ent != nullptr) {
				brush_storage = BrushData();
				brush_storage.cordon = cordon_mode;
				brush = &brush_storage;
			}
			continue;
		}
		if (line == "dispinfo") {
			FaceData *face = (brush != nullptr && brush->faces.size() > 0) ? &brush->faces[brush->faces.size() - 1] : nullptr;
			dispinfo.on_dispinfo_line(face);
			continue;
		}
		dispinfo.on_subblock_line(line);
		if (brush != nullptr && line.begins_with("{")) {
			scope++;
			dispinfo.on_brace_open(scope);
			continue;
		}
		if (line == "visgroup") {
			if (group != nullptr) {
				finalize_visgroup_name();
				groups_data.push_back(*group);
				group_parent_stack.push_back(groups_data.size() - 1);
			}
			group_storage = GroupData();
			group = &group_storage;
			group_display_name = String();
			if (group_parent_stack.size() > 0) {
				group->parent_id = groups_data[group_parent_stack[group_parent_stack.size() - 1]].id;
			}
			continue;
		}

		if (line.begins_with("}")) {
			if (scope > 0) {
				dispinfo.on_brace_close(scope);
			}
			if (scope == 0) {
				if (brush != nullptr) {
					if (ent != nullptr && !brush->faces.is_empty()) {
						ent->brushes.push_back(*brush);
					}
					brush = nullptr;
				} else if (ent != nullptr) {
					if (ent == cordon_ent) {
						if (!ent->brushes.is_empty()) {
							entities_data.push_back(*ent);
						}
						cordon_ent = nullptr;
						cordon_mode = false;
						ent = nullptr;
					} else {
						entities_data.push_back(*ent);
						ent = nullptr;
					}
				} else if (group != nullptr) {
					finalize_visgroup_name();
					groups_data.push_back(*group);
					group = nullptr;
					group_display_name = String();
				} else if (group_parent_stack.size() > 0) {
					group_parent_stack.remove_at(group_parent_stack.size() - 1);
				}
			}
			continue;
		}

		if ((ent != nullptr || group != nullptr) && line.begins_with("\"")) {
			PackedStringArray tokens = line.split("\" \"", false);
			if (tokens.size() < 2) {
				continue;
			}
			const String key = tokens[0].trim_prefix("\"");
			const String value = tokens[1].trim_suffix("\"");

			if (brush != nullptr) {
				if (scope > 1) {
					if (key == "plane") {
						PackedStringArray plane_tokens = value.replace("(", "").split(")", false);
						PackedVector3Array points;
						points.resize(3);
						for (int i = 0; i < 3; i++) {
							String token = plane_tokens[i].trim_prefix("(");
							const Vector<double> pts = token.split_floats(" ", false);
							if (pts.size() >= 3) {
								points.write[i] = Vector3(pts[0], pts[1], pts[2]) * p_map_settings->get_scale_factor();
							}
						}
						brush->planes.push_back(Plane(points[0], points[1], points[2]));
						FaceData face;
						face.plane = brush->planes[brush->planes.size() - 1];
						brush->faces.push_back(face);
					} else if (key == "material") {
						if (brush->faces.size() > 0) {
							brush->faces[brush->faces.size() - 1].texture = value;
							if (brush->faces.size() < 2) {
								if (value == p_map_settings->get_origin_texture()) {
									brush->origin = true;
								}
							} else if (brush->origin) {
								if (value != p_map_settings->get_origin_texture()) {
									brush->origin = false;
								}
							}
						}
					} else if (key == "uaxis" || key == "vaxis") {
						if (brush->faces.size() > 0) {
							String axis_value = value.replace("[", "");
							const Vector<double> vals = axis_value.replace("]", "").split_floats(" ", false);
							if (vals.size() >= 5) {
								FaceData &face = brush->faces[brush->faces.size() - 1];
								face.uv_axes.push_back(Vector3(vals[0], vals[1], vals[2]));
								if (key.begins_with("u")) {
									face.uv.set_origin(Vector2(vals[3], face.uv.get_origin().y));
									face.uv.columns[0] = face.uv.columns[0] * vals[4] * p_map_settings->get_scale_factor();
								} else {
									face.uv.set_origin(Vector2(face.uv.get_origin().x, vals[3]));
									face.uv.columns[1] = face.uv.columns[1] * vals[4] * p_map_settings->get_scale_factor();
								}
							}
						}
					} else if (key == "rotation") {
						if (brush->faces.size() > 0) {
							const real_t r = Math::deg_to_rad(value.to_float());
							FaceData &face = brush->faces[brush->faces.size() - 1];
							const Vector2 u = face.uv.columns[0];
							const Vector2 v = face.uv.columns[1];
							face.uv.columns[0] = Vector2(Math::cos(r), -Math::sin(r)) * u.length();
							face.uv.columns[1] = Vector2(Math::sin(r), Math::cos(r)) * v.length();
						}
					} else if (key == "visgroupid") {
						if (entities_data.size() > 0 && ent != nullptr && !ent->properties.has(key)) {
							ent->properties[key] = value;
						}
					} else if (brush->faces.size() > 0) {
						FaceData &face = brush->faces[brush->faces.size() - 1];
						dispinfo.handle_face_key(key, value, face, p_map_settings->get_scale_factor());
					}
				}
			} else if (ent != nullptr) {
				ent->properties[key] = value;
			} else if (group != nullptr) {
				if (key == "name") {
					group_display_name = value;
				} else if (key == "visgroupid") {
					group->id = value.to_int();
				}
			}
		}
	}

	if (ent != nullptr) {
		entities_data.push_back(*ent);
	}

	for (uint32_t i = 0; i < entities_data.size(); i++) {
		EntityData &entity = entities_data[i];
		if (entity.properties.has("visgroupid")) {
			const int group_id = entity.properties["visgroupid"];
			for (uint32_t g = 0; g < groups_data.size(); g++) {
				if (groups_data[g].id == group_id) {
					entity.group = &groups_data[g];
					break;
				}
			}
		}
		if (entity.properties.has("classname") && String(entity.properties["classname"]) == "info_overlay") {
			OverlayData overlay;
			overlay.material = entity.properties.has("material") ? String(entity.properties["material"]) : String();
			if (entity.properties.has("origin")) {
				overlay.origin = _parse_vmf_vector(String(entity.properties["origin"])) * p_map_settings->get_scale_factor();
			}
			if (entity.properties.has("BasisOrigin")) {
				overlay.origin = _parse_vmf_vector(String(entity.properties["BasisOrigin"])) * p_map_settings->get_scale_factor();
			}
			if (entity.properties.has("BasisU")) {
				overlay.basis_u = _parse_vmf_vector(String(entity.properties["BasisU"])) * p_map_settings->get_scale_factor();
			}
			if (entity.properties.has("BasisV")) {
				overlay.basis_v = _parse_vmf_vector(String(entity.properties["BasisV"])) * p_map_settings->get_scale_factor();
			}
			if (entity.properties.has("BasisNormal")) {
				overlay.basis_normal = _parse_vmf_vector(String(entity.properties["BasisNormal"])).normalized();
			} else if (overlay.basis_u.length_squared() > 0.0 && overlay.basis_v.length_squared() > 0.0) {
				overlay.basis_normal = overlay.basis_u.cross(overlay.basis_v).normalized();
			}
			entity.overlays.push_back(overlay);
		}
	}

	return true;
}
