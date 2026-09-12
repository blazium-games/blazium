/**************************************************************************/
/*  trenchbroom_map_settings.h                                            */
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

#pragma once

#include "core/io/resource.h"
#include "core/variant/typed_array.h"
#include "fgd/blazium_fgd_file.h"
#include "import/quake_wad_file.h"
#include "scene/resources/material.h"

class TrenchbroomMapSettings : public Resource {
	GDCLASS(TrenchbroomMapSettings, Resource);

protected:
	static void _bind_methods();

	real_t scale_factor = 0.03125;
	real_t inverse_scale_factor = 32.0;
	Ref<BlaziumFGDFile> entity_fgd;
	bool use_groups_hierarchy = false;
	real_t uv_unwrap_texel_size = 2.0;
	TypedArray<String> entity_node_groups;
	String entity_name_property;
	String entity_smoothing_property = "_phong";
	String entity_smoothing_angle_property = "_phong_angle";
	String vertex_merge_distance_property = "_vertex_merge_distance";
	String cull_interior_faces_property = "_cull_interior_faces";
	String base_texture_dir = "res://textures";
	TypedArray<String> texture_file_extensions;
	String clip_texture = "clip";
	String skip_texture = "skip";
	String origin_texture = "origin";
	TypedArray<QuakeWadFile> texture_wads;
	String base_material_dir;
	String material_file_extension = "tres";
	Ref<Material> default_material;
	String default_material_albedo_uniform;
	Dictionary shader_material_uniform_map_patterns;
	String albedo_map_pattern = "%s_albedo";
	String normal_map_pattern = "%s_normal";
	String metallic_map_pattern = "%s_metallic";
	String roughness_map_pattern = "%s_roughness";
	String emission_map_pattern = "%s_emission";
	String ao_map_pattern = "%s_ao";
	String height_map_pattern = "%s_height";
	String orm_map_pattern = "%s_orm";
	bool save_generated_materials = true;

public:
	void set_inverse_scale_factor(real_t p_value);
	real_t get_inverse_scale_factor() const { return inverse_scale_factor; }
	real_t get_scale_factor() const { return scale_factor; }

	void set_entity_fgd(const Ref<BlaziumFGDFile> &p_fgd) { entity_fgd = p_fgd; }
	Ref<BlaziumFGDFile> get_entity_fgd() const { return entity_fgd; }

	void set_use_groups_hierarchy(bool p_use) { use_groups_hierarchy = p_use; }
	bool get_use_groups_hierarchy() const { return use_groups_hierarchy; }

	void set_uv_unwrap_texel_size(real_t p_size) { uv_unwrap_texel_size = p_size; }
	real_t get_uv_unwrap_texel_size() const { return uv_unwrap_texel_size; }

	void set_entity_node_groups(const TypedArray<String> &p_groups) { entity_node_groups = p_groups; }
	TypedArray<String> get_entity_node_groups() const { return entity_node_groups; }

	void set_entity_name_property(const String &p_prop) { entity_name_property = p_prop; }
	String get_entity_name_property() const { return entity_name_property; }

	void set_entity_smoothing_property(const String &p_prop) { entity_smoothing_property = p_prop; }
	String get_entity_smoothing_property() const { return entity_smoothing_property; }

	void set_entity_smoothing_angle_property(const String &p_prop) { entity_smoothing_angle_property = p_prop; }
	String get_entity_smoothing_angle_property() const { return entity_smoothing_angle_property; }

	void set_vertex_merge_distance_property(const String &p_prop) { vertex_merge_distance_property = p_prop; }
	String get_vertex_merge_distance_property() const { return vertex_merge_distance_property; }

	void set_cull_interior_faces_property(const String &p_prop) { cull_interior_faces_property = p_prop; }
	String get_cull_interior_faces_property() const { return cull_interior_faces_property; }

	void set_base_texture_dir(const String &p_dir) { base_texture_dir = p_dir; }
	String get_base_texture_dir() const { return base_texture_dir; }

	void set_texture_file_extensions(const TypedArray<String> &p_ext) { texture_file_extensions = p_ext; }
	TypedArray<String> get_texture_file_extensions() const { return texture_file_extensions; }

	void set_clip_texture(const String &p_tex);
	String get_clip_texture() const { return clip_texture; }

	void set_skip_texture(const String &p_tex);
	String get_skip_texture() const { return skip_texture; }

	void set_origin_texture(const String &p_tex);
	String get_origin_texture() const { return origin_texture; }

	void set_texture_wads(const TypedArray<QuakeWadFile> &p_wads) { texture_wads = p_wads; }
	TypedArray<QuakeWadFile> get_texture_wads() const { return texture_wads; }

	void set_base_material_dir(const String &p_dir) { base_material_dir = p_dir; }
	String get_base_material_dir() const { return base_material_dir; }

	void set_material_file_extension(const String &p_ext) { material_file_extension = p_ext; }
	String get_material_file_extension() const { return material_file_extension; }

	void set_default_material(const Ref<Material> &p_material) { default_material = p_material; }
	Ref<Material> get_default_material() const { return default_material; }

	void set_default_material_albedo_uniform(const String &p_uniform) { default_material_albedo_uniform = p_uniform; }
	String get_default_material_albedo_uniform() const { return default_material_albedo_uniform; }

	void set_shader_material_uniform_map_patterns(const Dictionary &p_patterns) { shader_material_uniform_map_patterns = p_patterns; }
	Dictionary get_shader_material_uniform_map_patterns() const { return shader_material_uniform_map_patterns; }

	void set_albedo_map_pattern(const String &p_pattern) { albedo_map_pattern = p_pattern; }
	String get_albedo_map_pattern() const { return albedo_map_pattern; }

	void set_normal_map_pattern(const String &p_pattern) { normal_map_pattern = p_pattern; }
	String get_normal_map_pattern() const { return normal_map_pattern; }

	void set_metallic_map_pattern(const String &p_pattern) { metallic_map_pattern = p_pattern; }
	String get_metallic_map_pattern() const { return metallic_map_pattern; }

	void set_roughness_map_pattern(const String &p_pattern) { roughness_map_pattern = p_pattern; }
	String get_roughness_map_pattern() const { return roughness_map_pattern; }

	void set_emission_map_pattern(const String &p_pattern) { emission_map_pattern = p_pattern; }
	String get_emission_map_pattern() const { return emission_map_pattern; }

	void set_ao_map_pattern(const String &p_pattern) { ao_map_pattern = p_pattern; }
	String get_ao_map_pattern() const { return ao_map_pattern; }

	void set_height_map_pattern(const String &p_pattern) { height_map_pattern = p_pattern; }
	String get_height_map_pattern() const { return height_map_pattern; }

	void set_orm_map_pattern(const String &p_pattern) { orm_map_pattern = p_pattern; }
	String get_orm_map_pattern() const { return orm_map_pattern; }

	void set_save_generated_materials(bool p_save) { save_generated_materials = p_save; }
	bool get_save_generated_materials() const { return save_generated_materials; }
};
