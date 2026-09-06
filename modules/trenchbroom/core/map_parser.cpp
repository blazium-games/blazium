/**************************************************************************/
/*  map_parser.cpp                                                        */
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

#include "modules/trenchbroom/fgd/blazium_fgd_file.h"
#include "modules/trenchbroom/fgd/blazium_fgd_point_class.h"
#include "modules/trenchbroom/fgd/blazium_fgd_solid_class.h"
#include "modules/trenchbroom/import/quake_map_file.h"
#include "modules/trenchbroom/trenchbroom_map_settings.h"
#include "modules/trenchbroom/util/trenchbroom_util.h"

#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_uid.h"
#include "core/object/class_db.h"
#include "core/string/print_string.h"

void TrenchbroomMapParser::_bind_methods() {
	ClassDB::bind_method(D_METHOD("parse_map_data", "map_file", "map_settings"), &TrenchbroomMapParser::parse_map_data_dict);
}

int TrenchbroomMapParser::_find_unescaped_quote(const String &p_text, int p_start) const {
	int index = MAX(p_start, 0);
	while (index < p_text.length()) {
		if (p_text[index] == '"') {
			int backslash_count = 0;
			int check = index - 1;
			while (check >= 0 && p_text[check] == '\\') {
				backslash_count++;
				check--;
			}
			if (backslash_count % 2 == 0) {
				return index;
			}
		}
		index++;
	}
	return -1;
}

Dictionary TrenchbroomMapParser::_parse_quoted_key_value_line(const String &p_line) const {
	Dictionary property_data;
	property_data["valid"] = false;
	property_data["complete"] = false;
	property_data["key"] = String();
	property_data["value"] = String();
	property_data["trailing"] = String();

	if (!p_line.begins_with("\"")) {
		return property_data;
	}

	const int key_end = _find_unescaped_quote(p_line, 1);
	if (key_end < 0) {
		return property_data;
	}
	property_data["key"] = p_line.substr(1, key_end - 1);

	int value_open = key_end + 1;
	while (value_open < p_line.length() && p_line[value_open] <= 32) {
		value_open++;
	}
	if (value_open >= p_line.length() || p_line[value_open] != '"') {
		return property_data;
	}
	value_open++;

	const int value_end = _find_unescaped_quote(p_line, value_open);
	property_data["valid"] = true;
	if (value_end < 0) {
		property_data["value"] = p_line.substr(value_open);
		return property_data;
	}

	property_data["complete"] = true;
	property_data["value"] = p_line.substr(value_open, value_end - value_open);
	property_data["trailing"] = p_line.substr(value_end + 1).strip_edges();
	return property_data;
}

void TrenchbroomMapParser::_convert_property_types(EntityData &r_entity, const BlaziumFGDEntityClass *p_def) {
	if (!p_def) {
		return;
	}

	Dictionary properties = r_entity.properties;
	Array property_keys = properties.keys();
	for (int i = 0; i < property_keys.size(); i++) {
		const String property = property_keys[i];
		const String prop_string = properties[property];
		const Dictionary class_properties = p_def->get_class_properties();
		if (!class_properties.has(property)) {
			continue;
		}

		const Variant prop_default = class_properties[property];
		switch (prop_default.get_type()) {
			case Variant::INT:
				properties[property] = prop_string.to_int();
				break;
			case Variant::FLOAT:
				properties[property] = prop_string.to_float();
				break;
			case Variant::BOOL:
				properties[property] = bool(prop_string.to_int());
				break;
			case Variant::VECTOR3: {
				const Vector<double> prop_comps = prop_string.split_floats(" ", false);
				if (prop_comps.size() > 2) {
					properties[property] = Vector3(prop_comps[0], prop_comps[1], prop_comps[2]);
				} else {
					ERR_PRINT(vformat("Invalid Vector3 format for '%s' in entity '%s': %s", property, p_def->get_classname(), prop_string));
					properties[property] = prop_default;
				}
			} break;
			case Variant::VECTOR3I: {
				Vector3i prop_vec = prop_default;
				const PackedStringArray prop_comps = prop_string.split(" ", false);
				if (prop_comps.size() > 2) {
					for (int v = 0; v < 3; v++) {
						prop_vec[v] = prop_comps[v].to_int();
					}
				} else {
					ERR_PRINT(vformat("Invalid Vector3i format for '%s' in entity '%s': %s", property, p_def->get_classname(), prop_string));
				}
				properties[property] = prop_vec;
			} break;
			case Variant::COLOR: {
				Color prop_color = prop_default;
				const PackedStringArray prop_comps = prop_string.split(" ", false);
				if (prop_comps.size() > 2) {
					prop_color.set_r8(prop_comps[0].to_int());
					prop_color.set_g8(prop_comps[1].to_int());
					prop_color.set_b8(prop_comps[2].to_int());
					prop_color.a = 1.0;
				} else {
					ERR_PRINT(vformat("Invalid Color format for '%s' in entity '%s': %s", property, p_def->get_classname(), prop_string));
				}
				properties[property] = prop_color;
			} break;
			case Variant::DICTIONARY: {
				const Dictionary class_property_descriptions = p_def->get_class_property_descriptions();
				const Variant prop_desc = class_property_descriptions.get(property, Variant());
				if (prop_desc.get_type() == Variant::ARRAY) {
					const Array desc_array = prop_desc;
					if (desc_array.size() > 1 && desc_array[1].get_type() == Variant::INT) {
						properties[property] = prop_string.to_int();
					}
				}
			} break;
			case Variant::ARRAY:
				properties[property] = prop_string.to_int();
				break;
			case Variant::VECTOR2: {
				const Vector<double> prop_comps = prop_string.split_floats(" ", false);
				if (prop_comps.size() > 1) {
					properties[property] = Vector2(prop_comps[0], prop_comps[1]);
				} else {
					ERR_PRINT(vformat("Invalid Vector2 format for '%s' in entity '%s': %s", property, p_def->get_classname(), prop_string));
					properties[property] = prop_default;
				}
			} break;
			case Variant::VECTOR2I: {
				Vector2i prop_vec = prop_default;
				const PackedStringArray prop_comps = prop_string.split(" ", false);
				if (prop_comps.size() > 1) {
					for (int v = 0; v < 2; v++) {
						prop_vec[v] = prop_comps[v].to_int();
					}
				} else {
					ERR_PRINT(vformat("Invalid Vector2i format for '%s' in entity '%s': %s", property, p_def->get_classname(), prop_string));
				}
				properties[property] = prop_vec;
			} break;
			case Variant::VECTOR4: {
				const Vector<double> prop_comps = prop_string.split_floats(" ", false);
				if (prop_comps.size() > 3) {
					properties[property] = Vector4(prop_comps[0], prop_comps[1], prop_comps[2], prop_comps[3]);
				} else {
					ERR_PRINT(vformat("Invalid Vector4 format for '%s' in entity '%s': %s", property, p_def->get_classname(), prop_string));
					properties[property] = prop_default;
				}
			} break;
			case Variant::VECTOR4I: {
				Vector4i prop_vec = prop_default;
				const PackedStringArray prop_comps = prop_string.split(" ", false);
				if (prop_comps.size() > 3) {
					for (int v = 0; v < 4; v++) {
						prop_vec[v] = prop_comps[v].to_int();
					}
				} else {
					ERR_PRINT(vformat("Invalid Vector4i format for '%s' in entity '%s': %s", property, p_def->get_classname(), prop_string));
				}
				properties[property] = prop_vec;
			} break;
			case Variant::STRING_NAME:
				properties[property] = StringName(prop_string);
				break;
			case Variant::NODE_PATH:
				if (prop_string.begins_with("$") || prop_string.begins_with("%")) {
					properties[property] = NodePath(prop_string);
				} else {
					properties[property] = prop_string;
				}
				break;
			case Variant::OBJECT:
				properties[property] = prop_string;
				break;
			default:
				break;
		}
	}

	r_entity.properties = properties;
}

void TrenchbroomMapParser::_apply_default_properties(EntityData &r_entity, const BlaziumFGDEntityClass *p_def, HashMap<String, Dictionary> &p_prop_defaults_cache, HashMap<String, Dictionary> &p_prop_descriptions_cache) {
	if (!p_def) {
		return;
	}

	Dictionary def_properties;
	if (p_prop_defaults_cache.has(p_def->get_classname())) {
		def_properties = p_prop_defaults_cache[p_def->get_classname()];
	} else {
		def_properties = p_def->retrieve_all_class_properties();
		p_prop_defaults_cache[p_def->get_classname()] = def_properties;
	}

	Dictionary def_descriptions;
	if (p_prop_descriptions_cache.has(p_def->get_classname())) {
		def_descriptions = p_prop_descriptions_cache[p_def->get_classname()];
	} else {
		def_descriptions = p_def->retrieve_all_class_property_descriptions();
		p_prop_descriptions_cache[p_def->get_classname()] = def_descriptions;
	}

	Dictionary properties = r_entity.properties;
	Array def_property_keys = def_properties.keys();
	for (int i = 0; i < def_property_keys.size(); i++) {
		const String property = def_property_keys[i];
		if (properties.has(property)) {
			continue;
		}

		const Variant prop_default = def_properties[property];
		if (prop_default.get_type() == Variant::ARRAY) {
			int prop_flags_sum = 0;
			const Array prop_default_array = prop_default;
			for (int f = 0; f < prop_default_array.size(); f++) {
				const Variant prop_flag = prop_default_array[f];
				if (prop_flag.get_type() == Variant::ARRAY) {
					const Array flag_array = prop_flag;
					if (flag_array.size() > 2 && flag_array[2].operator bool() && flag_array[1].get_type() == Variant::INT) {
						prop_flags_sum += (int)flag_array[1];
					}
				}
			}
			properties[property] = prop_flags_sum;
		} else if (prop_default.get_type() == Variant::DICTIONARY) {
			const Variant prop_desc = def_descriptions.get(property, Variant());
			if (prop_desc.get_type() == Variant::ARRAY) {
				const Array desc_array = prop_desc;
				if (desc_array.size() > 1 && (desc_array[1].get_type() == Variant::INT || desc_array[1].get_type() == Variant::STRING)) {
					properties[property] = desc_array[1];
				} else {
					const Dictionary default_dict = prop_default;
					if (default_dict.size() > 0) {
						Array dict_keys = default_dict.keys();
						properties[property] = default_dict[dict_keys[0]];
					} else {
						properties[property] = 0;
					}
				}
			} else {
				const Dictionary default_dict = prop_default;
				if (default_dict.size() > 0) {
					Array dict_keys = default_dict.keys();
					properties[property] = default_dict[dict_keys[0]];
				} else {
					properties[property] = 0;
				}
			}
		} else if (prop_default.get_type() == Variant::OBJECT) {
			Ref<Resource> resource = prop_default;
			if (resource.is_valid()) {
				properties[property] = resource->get_path();
			} else {
				properties[property] = String();
			}
		} else if (prop_default.get_type() == Variant::NODE_PATH || prop_default.get_type() == Variant::NIL) {
			properties[property] = String();
		} else {
			properties[property] = prop_default;
		}
	}

	r_entity.properties = properties;
}

ParseData TrenchbroomMapParser::parse_map_data(const String &p_map_file, const TrenchbroomMapSettings *p_map_settings, bool p_show_profile, bool p_resolve_vmf_instances) {
	PackedStringArray map_data;
	ParseData parse_data;
	String map_file = p_map_file;

	if (p_show_profile) {
		TrenchbroomUtil::print_profile_info(vformat("Loading map file %s", map_file), "[PRS]");
	}

	if (map_file.begins_with("uid://")) {
		const ResourceUID::ID uid = ResourceUID::get_singleton()->text_to_id(map_file);
		if (!ResourceUID::get_singleton()->has_id(uid)) {
			ERR_PRINT(vformat("Error: failed to retrieve path for UID (%s)", map_file));
			return parse_data;
		}
		map_file = ResourceUID::get_singleton()->get_id_path(uid);
	}

	Ref<FileAccess> file = FileAccess::open(map_file, FileAccess::READ);
	if (file.is_null()) {
		file = FileAccess::open(map_file + ".import", FileAccess::READ);
		if (file.is_valid()) {
			map_file += ".import";
		} else {
			ERR_PRINT("Error: Failed to open map file (" + map_file + ")");
			return parse_data;
		}
	}

	if (map_file.ends_with(".import")) {
		while (!file->eof_reached()) {
			const String import_line = file->get_line();
			if (import_line.begins_with("path")) {
				file.unref();
				String path_line = import_line.replace("path=", "");
				path_line = path_line.replace("\"", "");
				Ref<QuakeMapFile> quake_map = ResourceLoader::load(path_line);
				if (quake_map.is_null() || quake_map->get_map_data().is_empty()) {
					ERR_PRINT("Error: Failed to open map file (" + path_line + ")");
					return parse_data;
				}
				const String data = quake_map->get_map_data().replace("\r", "");
				map_data = data.split("\n");
				break;
			}
		}
	} else {
		while (!file->eof_reached()) {
			map_data.push_back(file->get_line());
		}
	}

	bool parse_success = false;
	const String map_file_lower = map_file.to_lower();
	if (map_file_lower.ends_with(".map")) {
		if (p_show_profile) {
			TrenchbroomUtil::print_profile_info("Parsing as Quake MAP", "[PRS]");
		}
		parse_success = _parse_quake_map(map_data, p_map_settings, parse_data);
	} else if (map_file_lower.ends_with(".vmf")) {
		if (p_show_profile) {
			TrenchbroomUtil::print_profile_info("Parsing as Source VMF", "[PRS]");
		}
		parse_success = _parse_vmf(map_data, p_map_settings, parse_data);
		if (parse_success && p_resolve_vmf_instances) {
			_resolve_vmf_instances(parse_data, map_file, p_map_settings);
		}
	}

	if (!parse_success) {
		ERR_PRINT(vformat("Error: Failed to parse map file (%s)", map_file));
		return ParseData();
	}

	if (p_show_profile) {
		TrenchbroomUtil::print_profile_info("Determining groups hierarchy", "[PRS]");
	}
	for (uint32_t g = 0; g < parse_data.groups.size(); g++) {
		GroupData &group = parse_data.groups[g];
		if (group.parent_id != -1) {
			for (uint32_t p = 0; p < parse_data.groups.size(); p++) {
				if (parse_data.groups[p].id == group.parent_id) {
					group.parent = &parse_data.groups[p];
					break;
				}
			}
		}
	}

	Dictionary entity_defs;
	if (p_map_settings && p_map_settings->get_entity_fgd().is_valid()) {
		entity_defs = p_map_settings->get_entity_fgd()->get_entity_definitions();
	}

	Ref<BlaziumFGDPointClass> default_point_class;
	default_point_class.instantiate();
	default_point_class->set_node_class("Marker3D");

	Ref<BlaziumFGDSolidClass> default_solid_class;
	default_solid_class.instantiate();
	default_solid_class->set_spawn_type(BlaziumFGDSolidClass::SPAWN_ENTITY);
	default_solid_class->set_build_occlusion(false);
	default_solid_class->set_collision_shape_type(BlaziumFGDSolidClass::COLLISION_NONE);
	default_solid_class->set_origin_type(BlaziumFGDSolidClass::ORIGIN_BRUSH);

	PackedStringArray missing_defs;
	HashMap<String, Dictionary> prop_defaults_cache;
	HashMap<String, Dictionary> prop_descriptions_cache;

	if (p_show_profile) {
		TrenchbroomUtil::print_profile_info("Checking entity omission, definition status, and property types", "[PRS]");
	}

	for (int i = (int)parse_data.entities.size() - 1; i >= 0; i--) {
		EntityData &entity = parse_data.entities[i];

		if (entity.group != nullptr && entity.group->omit) {
			parse_data.entities.remove_at(i);
			continue;
		}

		if (entity.properties.has("classname")) {
			const String classname = entity.properties["classname"];
			if (entity_defs.has(classname)) {
				entity.definition = entity_defs[classname];
				if (!Object::cast_to<BlaziumFGDSolidClass>(entity.definition.ptr()) &&
						!Object::cast_to<BlaziumFGDPointClass>(entity.definition.ptr())) {
					if (missing_defs.find(classname) == -1) {
						ERR_PRINT(vformat("Invalid entity definition for \"%s\". Entity definition must be Solid Class or Point Class.", classname));
						missing_defs.push_back(classname);
					}
					entity.definition.unref();
				}
			} else if (missing_defs.find(classname) == -1) {
				ERR_PRINT(vformat("No entity definition found for \"%s\"", classname));
				missing_defs.push_back(classname);
			}
		}

		if (!entity.definition.is_valid()) {
			if (entity.brushes.is_empty() && entity.patches.is_empty()) {
				entity.definition = default_point_class;
			} else {
				entity.definition = default_solid_class;
			}
		}

		_convert_property_types(entity, entity.definition.ptr());
		_apply_default_properties(entity, entity.definition.ptr(), prop_defaults_cache, prop_descriptions_cache);
	}

	if (p_show_profile) {
		TrenchbroomUtil::print_profile_info("Merging worldspawn entities", "[PRS]");
	}
	int worldspawn_index = -1;
	for (uint32_t i = 0; i < parse_data.entities.size(); i++) {
		const BlaziumFGDSolidClass *solid_def = Object::cast_to<BlaziumFGDSolidClass>(parse_data.entities[i].definition.ptr());
		if (solid_def && solid_def->get_spawn_type() == BlaziumFGDSolidClass::SPAWN_WORLDSPAWN) {
			worldspawn_index = (int)i;
			break;
		}
		if (parse_data.entities[i].properties.has("classname") &&
				String(parse_data.entities[i].properties["classname"]) == "worldspawn") {
			worldspawn_index = (int)i;
			break;
		}
	}
	if (worldspawn_index >= 0) {
		EntityData &worldspawn = parse_data.entities[worldspawn_index];
		for (int i = (int)parse_data.entities.size() - 1; i >= 0; i--) {
			if (i == worldspawn_index) {
				continue;
			}
			const BlaziumFGDSolidClass *solid_def = Object::cast_to<BlaziumFGDSolidClass>(parse_data.entities[i].definition.ptr());
			if (!solid_def || solid_def->get_spawn_type() != BlaziumFGDSolidClass::SPAWN_MERGE_WORLDSPAWN) {
				continue;
			}
			EntityData &merge_entity = parse_data.entities[i];
			for (uint32_t brush_index = 0; brush_index < merge_entity.brushes.size(); brush_index++) {
				worldspawn.brushes.push_back(merge_entity.brushes[brush_index]);
			}
			for (uint32_t patch_index = 0; patch_index < merge_entity.patches.size(); patch_index++) {
				worldspawn.patches.push_back(merge_entity.patches[patch_index]);
			}
			parse_data.entities.remove_at(i);
			if (i < worldspawn_index) {
				worldspawn_index--;
			}
		}
	}

	if (p_show_profile) {
		TrenchbroomUtil::print_profile_info("Removing omitted layers and groups", "[PRS]");
	}
	for (int i = (int)parse_data.groups.size() - 1; i >= 0; i--) {
		if (parse_data.groups[i].omit) {
			parse_data.groups.remove_at(i);
		}
	}

	if (p_show_profile) {
		TrenchbroomUtil::print_profile_info("Map parsing complete", "[PRS]");
	}
	return parse_data;
}

Dictionary TrenchbroomMapParser::parse_map_data_dict(const String &p_map_file, const Ref<TrenchbroomMapSettings> &p_map_settings) {
	const ParseData parse_data = parse_map_data(p_map_file, p_map_settings.ptr());
	Dictionary result;
	Array entities;
	for (uint32_t i = 0; i < parse_data.entities.size(); i++) {
		const EntityData &entity = parse_data.entities[i];
		Dictionary ent;
		ent["properties"] = entity.properties;
		ent["brush_count"] = (int)entity.brushes.size();
		ent["patch_count"] = (int)entity.patches.size();
		ent["overlay_count"] = (int)entity.overlays.size();
		int displacement_face_count = 0;
		int disp_distance_count = 0;
		for (uint32_t brush_index = 0; brush_index < entity.brushes.size(); brush_index++) {
			const BrushData &brush = entity.brushes[brush_index];
			for (uint32_t face_index = 0; face_index < brush.faces.size(); face_index++) {
				const FaceData &face = brush.faces[face_index];
				if (face.disp.valid) {
					displacement_face_count++;
					disp_distance_count = MAX(disp_distance_count, face.disp.distances.size());
				}
			}
		}
		ent["displacement_face_count"] = displacement_face_count;
		ent["disp_distance_count"] = disp_distance_count;
		ent["has_displacement"] = displacement_face_count > 0;
		entities.push_back(ent);
	}
	Array groups;
	for (uint32_t i = 0; i < parse_data.groups.size(); i++) {
		const GroupData &group = parse_data.groups[i];
		Dictionary grp;
		grp["id"] = group.id;
		grp["name"] = group.name;
		grp["omit"] = group.omit;
		groups.push_back(grp);
	}
	result["entities"] = entities;
	result["groups"] = groups;
	return result;
}
