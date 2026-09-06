/**************************************************************************/
/*  trenchbroom_map_settings.cpp                                          */
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

#include "trenchbroom_map_settings.h"

#include "core/error/error_macros.h"
#include "core/object/class_db.h"
#include "core/string/print_string.h"
#include "fgd/blazium_fgd_file.h"
#include "import/quake_wad_file.h"

void TrenchbroomMapSettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_inverse_scale_factor", "inverse_scale_factor"), &TrenchbroomMapSettings::set_inverse_scale_factor);
	ClassDB::bind_method(D_METHOD("get_inverse_scale_factor"), &TrenchbroomMapSettings::get_inverse_scale_factor);
	ClassDB::bind_method(D_METHOD("get_scale_factor"), &TrenchbroomMapSettings::get_scale_factor);
	ClassDB::bind_method(D_METHOD("set_entity_fgd", "entity_fgd"), &TrenchbroomMapSettings::set_entity_fgd);
	ClassDB::bind_method(D_METHOD("get_entity_fgd"), &TrenchbroomMapSettings::get_entity_fgd);
	ClassDB::bind_method(D_METHOD("set_use_groups_hierarchy", "use_groups_hierarchy"), &TrenchbroomMapSettings::set_use_groups_hierarchy);
	ClassDB::bind_method(D_METHOD("get_use_groups_hierarchy"), &TrenchbroomMapSettings::get_use_groups_hierarchy);
	ClassDB::bind_method(D_METHOD("set_uv_unwrap_texel_size", "uv_unwrap_texel_size"), &TrenchbroomMapSettings::set_uv_unwrap_texel_size);
	ClassDB::bind_method(D_METHOD("get_uv_unwrap_texel_size"), &TrenchbroomMapSettings::get_uv_unwrap_texel_size);
	ClassDB::bind_method(D_METHOD("set_entity_node_groups", "entity_node_groups"), &TrenchbroomMapSettings::set_entity_node_groups);
	ClassDB::bind_method(D_METHOD("get_entity_node_groups"), &TrenchbroomMapSettings::get_entity_node_groups);
	ClassDB::bind_method(D_METHOD("set_entity_name_property", "entity_name_property"), &TrenchbroomMapSettings::set_entity_name_property);
	ClassDB::bind_method(D_METHOD("get_entity_name_property"), &TrenchbroomMapSettings::get_entity_name_property);
	ClassDB::bind_method(D_METHOD("set_entity_smoothing_property", "entity_smoothing_property"), &TrenchbroomMapSettings::set_entity_smoothing_property);
	ClassDB::bind_method(D_METHOD("get_entity_smoothing_property"), &TrenchbroomMapSettings::get_entity_smoothing_property);
	ClassDB::bind_method(D_METHOD("set_entity_smoothing_angle_property", "entity_smoothing_angle_property"), &TrenchbroomMapSettings::set_entity_smoothing_angle_property);
	ClassDB::bind_method(D_METHOD("get_entity_smoothing_angle_property"), &TrenchbroomMapSettings::get_entity_smoothing_angle_property);
	ClassDB::bind_method(D_METHOD("set_vertex_merge_distance_property", "vertex_merge_distance_property"), &TrenchbroomMapSettings::set_vertex_merge_distance_property);
	ClassDB::bind_method(D_METHOD("get_vertex_merge_distance_property"), &TrenchbroomMapSettings::get_vertex_merge_distance_property);
	ClassDB::bind_method(D_METHOD("set_cull_interior_faces_property", "cull_interior_faces_property"), &TrenchbroomMapSettings::set_cull_interior_faces_property);
	ClassDB::bind_method(D_METHOD("get_cull_interior_faces_property"), &TrenchbroomMapSettings::get_cull_interior_faces_property);
	ClassDB::bind_method(D_METHOD("set_base_texture_dir", "base_texture_dir"), &TrenchbroomMapSettings::set_base_texture_dir);
	ClassDB::bind_method(D_METHOD("get_base_texture_dir"), &TrenchbroomMapSettings::get_base_texture_dir);
	ClassDB::bind_method(D_METHOD("set_texture_file_extensions", "texture_file_extensions"), &TrenchbroomMapSettings::set_texture_file_extensions);
	ClassDB::bind_method(D_METHOD("get_texture_file_extensions"), &TrenchbroomMapSettings::get_texture_file_extensions);
	ClassDB::bind_method(D_METHOD("set_clip_texture", "clip_texture"), &TrenchbroomMapSettings::set_clip_texture);
	ClassDB::bind_method(D_METHOD("get_clip_texture"), &TrenchbroomMapSettings::get_clip_texture);
	ClassDB::bind_method(D_METHOD("set_skip_texture", "skip_texture"), &TrenchbroomMapSettings::set_skip_texture);
	ClassDB::bind_method(D_METHOD("get_skip_texture"), &TrenchbroomMapSettings::get_skip_texture);
	ClassDB::bind_method(D_METHOD("set_origin_texture", "origin_texture"), &TrenchbroomMapSettings::set_origin_texture);
	ClassDB::bind_method(D_METHOD("get_origin_texture"), &TrenchbroomMapSettings::get_origin_texture);
	ClassDB::bind_method(D_METHOD("set_texture_wads", "texture_wads"), &TrenchbroomMapSettings::set_texture_wads);
	ClassDB::bind_method(D_METHOD("get_texture_wads"), &TrenchbroomMapSettings::get_texture_wads);
	ClassDB::bind_method(D_METHOD("set_base_material_dir", "base_material_dir"), &TrenchbroomMapSettings::set_base_material_dir);
	ClassDB::bind_method(D_METHOD("get_base_material_dir"), &TrenchbroomMapSettings::get_base_material_dir);
	ClassDB::bind_method(D_METHOD("set_material_file_extension", "material_file_extension"), &TrenchbroomMapSettings::set_material_file_extension);
	ClassDB::bind_method(D_METHOD("get_material_file_extension"), &TrenchbroomMapSettings::get_material_file_extension);
	ClassDB::bind_method(D_METHOD("set_default_material", "default_material"), &TrenchbroomMapSettings::set_default_material);
	ClassDB::bind_method(D_METHOD("get_default_material"), &TrenchbroomMapSettings::get_default_material);
	ClassDB::bind_method(D_METHOD("set_default_material_albedo_uniform", "default_material_albedo_uniform"), &TrenchbroomMapSettings::set_default_material_albedo_uniform);
	ClassDB::bind_method(D_METHOD("get_default_material_albedo_uniform"), &TrenchbroomMapSettings::get_default_material_albedo_uniform);
	ClassDB::bind_method(D_METHOD("set_shader_material_uniform_map_patterns", "shader_material_uniform_map_patterns"), &TrenchbroomMapSettings::set_shader_material_uniform_map_patterns);
	ClassDB::bind_method(D_METHOD("get_shader_material_uniform_map_patterns"), &TrenchbroomMapSettings::get_shader_material_uniform_map_patterns);
	ClassDB::bind_method(D_METHOD("set_albedo_map_pattern", "albedo_map_pattern"), &TrenchbroomMapSettings::set_albedo_map_pattern);
	ClassDB::bind_method(D_METHOD("get_albedo_map_pattern"), &TrenchbroomMapSettings::get_albedo_map_pattern);
	ClassDB::bind_method(D_METHOD("set_normal_map_pattern", "normal_map_pattern"), &TrenchbroomMapSettings::set_normal_map_pattern);
	ClassDB::bind_method(D_METHOD("get_normal_map_pattern"), &TrenchbroomMapSettings::get_normal_map_pattern);
	ClassDB::bind_method(D_METHOD("set_metallic_map_pattern", "metallic_map_pattern"), &TrenchbroomMapSettings::set_metallic_map_pattern);
	ClassDB::bind_method(D_METHOD("get_metallic_map_pattern"), &TrenchbroomMapSettings::get_metallic_map_pattern);
	ClassDB::bind_method(D_METHOD("set_roughness_map_pattern", "roughness_map_pattern"), &TrenchbroomMapSettings::set_roughness_map_pattern);
	ClassDB::bind_method(D_METHOD("get_roughness_map_pattern"), &TrenchbroomMapSettings::get_roughness_map_pattern);
	ClassDB::bind_method(D_METHOD("set_emission_map_pattern", "emission_map_pattern"), &TrenchbroomMapSettings::set_emission_map_pattern);
	ClassDB::bind_method(D_METHOD("get_emission_map_pattern"), &TrenchbroomMapSettings::get_emission_map_pattern);
	ClassDB::bind_method(D_METHOD("set_ao_map_pattern", "ao_map_pattern"), &TrenchbroomMapSettings::set_ao_map_pattern);
	ClassDB::bind_method(D_METHOD("get_ao_map_pattern"), &TrenchbroomMapSettings::get_ao_map_pattern);
	ClassDB::bind_method(D_METHOD("set_height_map_pattern", "height_map_pattern"), &TrenchbroomMapSettings::set_height_map_pattern);
	ClassDB::bind_method(D_METHOD("get_height_map_pattern"), &TrenchbroomMapSettings::get_height_map_pattern);
	ClassDB::bind_method(D_METHOD("set_orm_map_pattern", "orm_map_pattern"), &TrenchbroomMapSettings::set_orm_map_pattern);
	ClassDB::bind_method(D_METHOD("get_orm_map_pattern"), &TrenchbroomMapSettings::get_orm_map_pattern);
	ClassDB::bind_method(D_METHOD("set_save_generated_materials", "save_generated_materials"), &TrenchbroomMapSettings::set_save_generated_materials);
	ClassDB::bind_method(D_METHOD("get_save_generated_materials"), &TrenchbroomMapSettings::get_save_generated_materials);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "inverse_scale_factor"), "set_inverse_scale_factor", "get_inverse_scale_factor");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "scale_factor", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_scale_factor");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "entity_fgd", PROPERTY_HINT_RESOURCE_TYPE, "BlaziumFGDFile"), "set_entity_fgd", "get_entity_fgd");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_groups_hierarchy"), "set_use_groups_hierarchy", "get_use_groups_hierarchy");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "uv_unwrap_texel_size"), "set_uv_unwrap_texel_size", "get_uv_unwrap_texel_size");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "entity_node_groups"), "set_entity_node_groups", "get_entity_node_groups");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "entity_name_property"), "set_entity_name_property", "get_entity_name_property");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "entity_smoothing_property"), "set_entity_smoothing_property", "get_entity_smoothing_property");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "entity_smoothing_angle_property"), "set_entity_smoothing_angle_property", "get_entity_smoothing_angle_property");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "vertex_merge_distance_property"), "set_vertex_merge_distance_property", "get_vertex_merge_distance_property");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "cull_interior_faces_property"), "set_cull_interior_faces_property", "get_cull_interior_faces_property");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "base_texture_dir", PROPERTY_HINT_DIR), "set_base_texture_dir", "get_base_texture_dir");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "texture_file_extensions"), "set_texture_file_extensions", "get_texture_file_extensions");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "clip_texture"), "set_clip_texture", "get_clip_texture");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "skip_texture"), "set_skip_texture", "get_skip_texture");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "origin_texture"), "set_origin_texture", "get_origin_texture");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "texture_wads", PROPERTY_HINT_ARRAY_TYPE, "QuakeWadFile"), "set_texture_wads", "get_texture_wads");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "base_material_dir", PROPERTY_HINT_DIR), "set_base_material_dir", "get_base_material_dir");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "material_file_extension"), "set_material_file_extension", "get_material_file_extension");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "default_material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_default_material", "get_default_material");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "default_material_albedo_uniform"), "set_default_material_albedo_uniform", "get_default_material_albedo_uniform");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "shader_material_uniform_map_patterns"), "set_shader_material_uniform_map_patterns", "get_shader_material_uniform_map_patterns");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "albedo_map_pattern"), "set_albedo_map_pattern", "get_albedo_map_pattern");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "normal_map_pattern"), "set_normal_map_pattern", "get_normal_map_pattern");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "metallic_map_pattern"), "set_metallic_map_pattern", "get_metallic_map_pattern");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "roughness_map_pattern"), "set_roughness_map_pattern", "get_roughness_map_pattern");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "emission_map_pattern"), "set_emission_map_pattern", "get_emission_map_pattern");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "ao_map_pattern"), "set_ao_map_pattern", "get_ao_map_pattern");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "height_map_pattern"), "set_height_map_pattern", "get_height_map_pattern");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "orm_map_pattern"), "set_orm_map_pattern", "get_orm_map_pattern");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "save_generated_materials"), "set_save_generated_materials", "get_save_generated_materials");
}

void TrenchbroomMapSettings::set_inverse_scale_factor(real_t p_value) {
	if (p_value == 0.0) {
		ERR_PRINT("Error: Cannot set Inverse Scale Factor to Zero");
		return;
	}
	inverse_scale_factor = p_value;
	scale_factor = 1.0 / p_value;
}

void TrenchbroomMapSettings::set_clip_texture(const String &p_tex) {
	clip_texture = p_tex.to_lower();
}

void TrenchbroomMapSettings::set_skip_texture(const String &p_tex) {
	skip_texture = p_tex.to_lower();
}

void TrenchbroomMapSettings::set_origin_texture(const String &p_tex) {
	origin_texture = p_tex.to_lower();
}
