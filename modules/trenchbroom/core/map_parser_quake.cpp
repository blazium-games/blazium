/**************************************************************************/
/*  map_parser_quake.cpp                                                  */
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

#include "modules/trenchbroom/fgd/blazium_fgd_entity_class.h"
#include "modules/trenchbroom/trenchbroom_map_settings.h"
#include "modules/trenchbroom/util/trenchbroom_util.h"

#include "core/string/print_string.h"

bool TrenchbroomMapParser::_parse_quake_map(const PackedStringArray &p_map_data, const TrenchbroomMapSettings *p_map_settings, ParseData &r_parse_data) {
	LocalVector<EntityData> &entities_data = r_parse_data.entities;
	LocalVector<GroupData> &groups_data = r_parse_data.groups;

	EntityData *ent = nullptr;
	BrushData *brush = nullptr;
	PatchData *patch = nullptr;
	EntityData entity_storage;
	PatchData patch_storage;
	int scope = 0;
	bool multiline_property_active = false;
	String multiline_property_key;
	String multiline_property_value;
	int multiline_property_line = -1;

	for (int line_index = 0; line_index < p_map_data.size(); line_index++) {
		String line = p_map_data[line_index];
		const int line_number = line_index + 1;
		line = line.replace("\t", "").replace("\r", "");

		if (multiline_property_active) {
			const int value_end = _find_unescaped_quote(line);
			if (value_end == -1) {
				multiline_property_value += "\n" + line;
			} else {
				multiline_property_value += "\n" + line.substr(0, value_end);
				const String trailing = line.substr(value_end + 1).strip_edges();
				if (!trailing.is_empty()) {
					WARN_PRINT(vformat("Unexpected trailing data after multiline property at line %d, ignoring: %s", line_number, trailing));
				}
				if (ent == nullptr) {
					ERR_PRINT(vformat("Malformed Quake MAP property continuation at line %d", line_number));
					return false;
				}
				ent->properties[multiline_property_key] = multiline_property_value;
				multiline_property_active = false;
				multiline_property_key = String();
				multiline_property_value = String();
				multiline_property_line = -1;
			}
			continue;
		}

		if (line.begins_with("{")) {
			if (ent == nullptr) {
				entity_storage = EntityData();
				ent = &entity_storage;
			} else if (patch == nullptr) {
				ent->brushes.push_back(BrushData());
				brush = &ent->brushes[ent->brushes.size() - 1];
			} else {
				scope++;
			}
			continue;
		}

		if (line.begins_with("}")) {
			if (brush != nullptr) {
				brush = nullptr;
			} else if (patch != nullptr) {
				if (scope > 0) {
					scope--;
				} else {
					if (ent != nullptr) {
						ent->patches.push_back(*patch);
					}
					patch = nullptr;
				}
			} else if (ent != nullptr) {
				if (ent->properties.has("classname") && String(ent->properties["classname"]) == "func_group" && ent->properties.has("_tb_type")) {
					if (entities_data.size() > 0) {
						for (uint32_t b = 0; b < ent->brushes.size(); b++) {
							entities_data[0].brushes.push_back(ent->brushes[b]);
						}
					}

					GroupData group;
					const Dictionary props = ent->properties;
					group.id = props["_tb_id"];
					if (String(props["_tb_type"]) == "_tb_layer") {
						group.type = GroupData::GROUP_TYPE_GROUP;
						group.name = "layer_";
					} else {
						group.name = "group_";
					}
					group.name = group.name + itos(group.id);
					if (String(props["_tb_name"]) != "Unnamed") {
						group.name = group.name + "_" + String(props["_tb_name"]).replace(" ", "_");
					}
					if (props.has("_tb_layer")) {
						group.parent_id = props["_tb_layer"];
					}
					if (props.has("_tb_group")) {
						group.parent_id = props["_tb_group"];
					}
					if (props.has("_tb_layer_omit_from_export")) {
						group.omit = true;
					}
					groups_data.push_back(group);
				} else {
					if (!ent->properties.is_empty() || !ent->brushes.is_empty() || !ent->patches.is_empty()) {
						entities_data.push_back(*ent);
					}
				}
				ent = nullptr;
			}
			continue;
		}

		if (ent != nullptr && brush == nullptr && patch == nullptr && line.begins_with("\"")) {
			const Dictionary property_data = _parse_quoted_key_value_line(line);
			if (!(bool)property_data["valid"]) {
				WARN_PRINT(vformat("Malformed Quake MAP property at line %d, skipping: %s", line_number, line));
				continue;
			}
			if ((bool)property_data["complete"]) {
				const String trailing = property_data["trailing"];
				if (!trailing.is_empty()) {
					WARN_PRINT(vformat("Unexpected trailing data after property at line %d, ignoring: %s", line_number, trailing));
				}
				ent->properties[property_data["key"]] = property_data["value"];
			} else {
				multiline_property_active = true;
				multiline_property_key = property_data["key"];
				multiline_property_value = property_data["value"];
				multiline_property_line = line_number;
			}
			continue;
		}

		if (brush != nullptr && line.begins_with("(")) {
			line = line.replace("(", "");
			PackedStringArray tokens = line.split(" ) ", false);
			if (tokens.size() < 4) {
				continue;
			}

			PackedVector3Array points;
			points.resize(3);
			for (int i = 0; i < 3; i++) {
				String token = tokens[i];
				token = token.trim_prefix("(");
				const Vector<double> pts = token.split_floats(" ", false);
				if (pts.size() < 3) {
					continue;
				}
				points.write[i] = Vector3(pts[0], pts[1], pts[2]) * p_map_settings->get_scale_factor();
			}

			const Plane plane(points[0], points[1], points[2]);
			brush->planes.push_back(plane);

			FaceData face;
			face.plane = plane;

			String tex;
			if (tokens[3].begins_with("\"")) {
				const int last_quote = tokens[3].rfind("\"");
				tex = tokens[3].substr(1, last_quote - 1);
				tokens = tokens[3].substr(last_quote + 2).split(" ] ", false);
			} else {
				tex = tokens[3].get_slice(" ", 0);
				tokens = tokens[3].trim_prefix(tex + " ").split(" ] ", false);
			}
			face.texture = tex;

			if (brush->faces.is_empty()) {
				if (tex == p_map_settings->get_origin_texture()) {
					brush->origin = true;
				}
			} else if (brush->origin) {
				if (tex != p_map_settings->get_origin_texture()) {
					brush->origin = false;
				}
			}

			if (tokens.size() > 1) {
				for (int i = 0; i < 2; i++) {
					String axis_token = tokens[i].trim_prefix("[ ");
					const Vector<double> coords = axis_token.split_floats(" ", false);
					if (coords.size() >= 4) {
						face.uv_axes.push_back(Vector3(coords[0], coords[1], coords[2]));
						if (i == 0) {
							face.uv.set_origin(Vector2(coords[3], face.uv.get_origin().y));
						} else {
							face.uv.set_origin(Vector2(face.uv.get_origin().x, coords[3]));
						}
					}
				}
				if (tokens.size() > 2) {
					const Vector<double> coords = tokens[2].split_floats(" ", false);
					if (coords.size() >= 3) {
						face.uv.columns[0] = Vector2(coords[1], 0.0) * p_map_settings->get_scale_factor();
						face.uv.columns[1] = Vector2(0.0, coords[2]) * p_map_settings->get_scale_factor();
					}
				}
			} else if (tokens.size() > 0) {
				const Vector<double> coords = tokens[0].split_floats(" ", false);
				if (coords.size() >= 5) {
					face.uv.set_origin(Vector2(coords[0], coords[1]));
					const real_t r = Math::deg_to_rad(coords[2]);
					face.uv.columns[0] = Vector2(Math::cos(r), -Math::sin(r)) * coords[3] * p_map_settings->get_scale_factor();
					face.uv.columns[1] = Vector2(Math::sin(r), Math::cos(r)) * coords[4] * p_map_settings->get_scale_factor();
				}
			}

			brush->faces.push_back(face);
			continue;
		}

		if (patch != nullptr) {
			if (line.begins_with("(")) {
				line = line.replace("( ", "");
				if (patch->size.size() > 0) {
					PackedStringArray tokens = line.replace("(", "").split(" )", false);
					for (int i = 0; i < tokens.size(); i++) {
						const Vector<double> subtokens = tokens[i].split_floats(" ", false);
						if (subtokens.size() >= 5) {
							patch->points.push_back(Vector3(subtokens[0], subtokens[1], subtokens[2]));
							patch->uvs.push_back(Vector2(subtokens[3], subtokens[4]));
						}
					}
				} else {
					PackedStringArray tokens = line.replace(")", "").split(" ", false);
					patch->size.resize(tokens.size());
					for (int i = 0; i < tokens.size(); i++) {
						patch->size.write[i] = tokens[i].to_int();
					}
				}
			} else if (!line.begins_with(")")) {
				patch->texture = line.replace("\"", "");
			}
		}

		if (line.begins_with("patchDef")) {
			brush = nullptr;
			patch_storage = PatchData();
			patch = &patch_storage;
			continue;
		}
	}

	if (multiline_property_active) {
		ERR_PRINT(vformat("Unterminated multiline property \"%s\" starting at line %d", multiline_property_key, multiline_property_line));
		return false;
	}

	for (uint32_t i = 0; i < entities_data.size(); i++) {
		EntityData &entity = entities_data[i];
		int group_id = -1;
		if (entity.properties.has("_tb_layer")) {
			group_id = entity.properties["_tb_layer"];
		} else if (entity.properties.has("_tb_group")) {
			group_id = entity.properties["_tb_group"];
		}
		if (group_id != -1) {
			for (uint32_t g = 0; g < groups_data.size(); g++) {
				if (groups_data[g].id == group_id) {
					entity.group = &groups_data[g];
					break;
				}
			}
		}
	}

	return true;
}
