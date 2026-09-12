/**************************************************************************/
/*  trenchbroom_util.cpp                                                  */
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

#include "trenchbroom_util.h"

#include "modules/trenchbroom/core/data.h"
#include "modules/trenchbroom/import/quake_wad_file.h"
#include "modules/trenchbroom/trenchbroom_defaults.h"

#include "modules/trenchbroom/trenchbroom_map_settings.h"
#include "scene/resources/image_texture.h"

#include "core/io/dir_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/callable_method_pointer.h"
#include "core/object/worker_thread_pool.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/string/print_string.h"
#include "core/templates/hash_set.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh_data_tool.h"
#include "scene/resources/surface_tool.h"
#include "scene/resources/texture.h"

static const Vector3 _VEC3_UP_ID(0.0, 0.0, 1.0);
static const Vector3 _VEC3_RIGHT_ID(0.0, 1.0, 0.0);
static const Vector3 _VEC3_FORWARD_ID(1.0, 0.0, 0.0);

static const Vector3 _QUAKE_STD_UV_BASE_NORMALS[] = {
	Vector3(0.0, 0.0, 1.0),
	Vector3(0.0, 0.0, -1.0),
	Vector3(1.0, 0.0, 0.0),
	Vector3(-1.0, 0.0, 0.0),
	Vector3(0.0, 1.0, 0.0),
	Vector3(0.0, -1.0, 0.0),
};

static const Vector3 _QUAKE_STD_UV_BASE_U_AXES[] = {
	Vector3(1.0, 0.0, 0.0),
	Vector3(1.0, 0.0, 0.0),
	Vector3(0.0, 1.0, 0.0),
	Vector3(0.0, 1.0, 0.0),
	Vector3(1.0, 0.0, 0.0),
	Vector3(1.0, 0.0, 0.0),
};

static const Vector3 _QUAKE_STD_UV_BASE_V_AXES[] = {
	Vector3(0.0, -1.0, 0.0),
	Vector3(0.0, -1.0, 0.0),
	Vector3(0.0, 0.0, -1.0),
	Vector3(0.0, 0.0, -1.0),
	Vector3(0.0, 0.0, -1.0),
	Vector3(0.0, 0.0, -1.0),
};

static const int _QUAKE_STD_UV_BASE_COUNT = 6;

static String _default_texture_path() {
	return TrenchbroomDefaults::get_default_texture_path();
}

static Ref<Texture2D> _fallback_texture() {
	static Ref<ImageTexture> fallback;
	if (fallback.is_null()) {
		Ref<Image> image = Image::create_empty(64, 64, false, Image::FORMAT_RGB8);
		for (int y = 0; y < 64; y++) {
			for (int x = 0; x < 64; x++) {
				const bool checker = ((x / 8) + (y / 8)) % 2 == 0;
				image->set_pixel(x, y, checker ? Color(0.8, 0.2, 0.2) : Color(0.4, 0.1, 0.1));
			}
		}
		fallback = ImageTexture::create_from_image(image);
	}
	return fallback;
}

static const BaseMaterial3D::TextureParam _PBR_TEXTURES[] = {
	BaseMaterial3D::TEXTURE_ALBEDO,
	BaseMaterial3D::TEXTURE_NORMAL,
	BaseMaterial3D::TEXTURE_METALLIC,
	BaseMaterial3D::TEXTURE_ROUGHNESS,
	BaseMaterial3D::TEXTURE_EMISSION,
	BaseMaterial3D::TEXTURE_AMBIENT_OCCLUSION,
	BaseMaterial3D::TEXTURE_HEIGHTMAP,
	BaseMaterial3D::TEXTURE_ORM,
};

static const int _PBR_FEATURES[] = {
	-1,
	BaseMaterial3D::FEATURE_NORMAL_MAPPING,
	-1,
	-1,
	BaseMaterial3D::FEATURE_EMISSION,
	BaseMaterial3D::FEATURE_AMBIENT_OCCLUSION,
	BaseMaterial3D::FEATURE_HEIGHT_MAPPING,
	-1,
};

static const int _PBR_TEXTURE_COUNT = 8;

static String _format_pattern(const String &p_pattern, const Vector<String> &p_values) {
	String result = p_pattern;
	for (int i = 0; i < p_values.size(); i++) {
		const int pos = result.find("%s");
		if (pos == -1) {
			break;
		}
		result = result.substr(0, pos) + p_values[i] + result.substr(pos + 2);
	}
	return result;
}

static real_t _sign_real(real_t p_value) {
	return p_value < 0.0 ? -1.0 : (p_value > 0.0 ? 1.0 : 0.0);
}

String TrenchbroomUtil::newline() {
	if (OS::get_singleton()->get_name() == "Windows") {
		return "\r\n";
	}
	return "\n";
}

Vector3 TrenchbroomUtil::op_vec3_sum(const Vector3 &p_lhs, const Vector3 &p_rhs) {
	return p_lhs + p_rhs;
}

Vector3 TrenchbroomUtil::op_vec3_avg(const PackedVector3Array &p_array) {
	if (p_array.is_empty()) {
		ERR_PRINT("Cannot average empty Vector3 array!");
		return Vector3();
	}
	Vector3 sum;
	for (int i = 0; i < p_array.size(); i++) {
		sum += p_array[i];
	}
	return sum / p_array.size();
}

Vector3 TrenchbroomUtil::id_to_opengl(const Vector3 &p_vec) {
	return Vector3(p_vec.y, p_vec.z, p_vec.x);
}

bool TrenchbroomUtil::is_point_in_convex_hull(const LocalVector<Plane> &p_planes, const Vector3 &p_vertex) {
	for (uint32_t i = 0; i < p_planes.size(); i++) {
		const real_t distance = p_planes[i].normal.dot(p_vertex) - p_planes[i].d;
		if (distance > VERTEX_EPSILON) {
			return false;
		}
	}
	return true;
}

Ref<Texture2D> TrenchbroomUtil::load_texture(const String &p_texture_name, const TypedArray<QuakeWadFile> &p_wad_resources, const TrenchbroomMapSettings *p_map_settings) {
	if (p_map_settings) {
		const String texture_base_dir = TrenchbroomDefaults::resolve_defaults_path(p_map_settings->get_base_texture_dir());
		const TypedArray<String> extensions = p_map_settings->get_texture_file_extensions();
		for (int i = 0; i < extensions.size(); i++) {
			const String extension = extensions[i];
			const String texture_path = texture_base_dir.path_join(p_texture_name + "." + extension);
			if (ResourceLoader::exists(texture_path)) {
				Ref<Resource> texture_file = ResourceLoader::load(texture_path);
				Ref<Texture2D> texture = texture_file;
				if (texture.is_valid()) {
					return texture;
				}
				ERR_PRINT(vformat("Error: Texture load failed! (%s) not a valid Texture2D resource", texture_path));
			}
		}
	}

	const String texture_name_lower = p_texture_name.to_lower();
	for (int i = 0; i < p_wad_resources.size(); i++) {
		Ref<QuakeWadFile> wad = p_wad_resources[i];
		if (!wad.is_valid()) {
			continue;
		}
		const Dictionary textures = wad->get_textures();
		if (textures.has(texture_name_lower)) {
			Ref<Texture2D> texture = textures[texture_name_lower];
			if (texture.is_valid()) {
				return texture;
			}
		}
	}

	const String fallback_path = _default_texture_path();
	if (ResourceLoader::exists(fallback_path)) {
		Ref<Texture2D> fallback = ResourceLoader::load(fallback_path);
		if (fallback.is_valid()) {
			return fallback;
		}
	}

	return _fallback_texture();
}

bool TrenchbroomUtil::is_skip(const String &p_texture, const TrenchbroomMapSettings *p_map_settings) {
	if (p_map_settings) {
		return p_texture.to_lower() == p_map_settings->get_skip_texture();
	}
	return false;
}

bool TrenchbroomUtil::is_clip(const String &p_texture, const TrenchbroomMapSettings *p_map_settings) {
	if (p_map_settings) {
		return p_texture.to_lower() == p_map_settings->get_clip_texture();
	}
	return false;
}

bool TrenchbroomUtil::is_origin(const String &p_texture, const TrenchbroomMapSettings *p_map_settings) {
	if (p_map_settings) {
		return p_texture.to_lower() == p_map_settings->get_origin_texture();
	}
	return false;
}

bool TrenchbroomUtil::filter_face(const String &p_texture, const TrenchbroomMapSettings *p_map_settings) {
	if (!p_map_settings) {
		return false;
	}
	const String texture = p_texture.to_lower();
	return texture == p_map_settings->get_skip_texture() ||
			texture == p_map_settings->get_clip_texture() ||
			texture == p_map_settings->get_origin_texture();
}

void TrenchbroomUtil::build_base_material(const TrenchbroomMapSettings *p_map_settings, Ref<BaseMaterial3D> p_material, const String &p_texture) {
	if (!p_map_settings || !p_material.is_valid()) {
		return;
	}

	String path = TrenchbroomDefaults::resolve_defaults_path(p_map_settings->get_base_texture_dir()).path_join(p_texture);
	if (DirAccess::exists(path)) {
		path = path.path_join(p_texture);
	}

	const PackedStringArray pbr_suffixes = {
		p_map_settings->get_albedo_map_pattern(),
		p_map_settings->get_normal_map_pattern(),
		p_map_settings->get_metallic_map_pattern(),
		p_map_settings->get_roughness_map_pattern(),
		p_map_settings->get_emission_map_pattern(),
		p_map_settings->get_ao_map_pattern(),
		p_map_settings->get_height_map_pattern(),
		p_map_settings->get_orm_map_pattern(),
	};

	const TypedArray<String> extensions = p_map_settings->get_texture_file_extensions();

	for (int i = 0; i < pbr_suffixes.size() && i < _PBR_TEXTURE_COUNT; i++) {
		if (pbr_suffixes[i].is_empty()) {
			continue;
		}

		String pbr = pbr_suffixes[i];
		int token = pbr.find("%s");
		if (token != -1) {
			if (pbr.find("%s", token + 1) != -1) {
				token = 2;
			} else {
				token = 1;
			}
		}

		if (token < 1) {
			ERR_PRINT(vformat("No string replacement tokens found in auto-PBR pattern '%s'! Must have at least one instance of '%%s' per pattern.", pbr_suffixes[i]));
			continue;
		}

		for (int j = 0; j < extensions.size(); j++) {
			const String extension = extensions[j];
			if (token > 1) {
				pbr = _format_pattern(pbr_suffixes[i], { path, extension });
			} else {
				pbr = _format_pattern(pbr_suffixes[i], { path });
				pbr += "." + extension;
			}

			if (ResourceLoader::exists(pbr)) {
				if (_PBR_FEATURES[i] > -1) {
					p_material->set_feature((BaseMaterial3D::Feature)_PBR_FEATURES[i], true);
				}
				p_material->set_texture(_PBR_TEXTURES[i], ResourceLoader::load(pbr));
				break;
			}
		}
	}
}

void TrenchbroomUtil::collect_texture_names(const LocalVector<EntityData> &p_entity_data, const TrenchbroomMapSettings *p_map_settings, HashSet<String> &r_names) {
	r_names.clear();
	if (!p_map_settings) {
		return;
	}

	for (uint32_t entity_index = 0; entity_index < p_entity_data.size(); entity_index++) {
		const EntityData &entity = p_entity_data[entity_index];
		if (!entity.is_visual()) {
			continue;
		}

		for (uint32_t brush_index = 0; brush_index < entity.brushes.size(); brush_index++) {
			const BrushData &brush = entity.brushes[brush_index];
			for (uint32_t face_index = 0; face_index < brush.faces.size(); face_index++) {
				const String &texture_name = brush.faces[face_index].texture;
				if (!filter_face(texture_name, p_map_settings)) {
					r_names.insert(texture_name);
				}
			}
		}

		for (uint32_t patch_index = 0; patch_index < entity.patches.size(); patch_index++) {
			const String &texture_name = entity.patches[patch_index].texture;
			if (!filter_face(texture_name, p_map_settings)) {
				r_names.insert(texture_name);
			}
		}
	}
}

void TrenchbroomUtil::register_texture_entry(
		const String &p_texture_name,
		const TrenchbroomMapSettings *p_map_settings,
		const TypedArray<QuakeWadFile> &p_wad_resources,
		Dictionary &p_texture_materials,
		Dictionary &p_texture_sizes,
		Mutex &p_mutex,
		LocalVector<Pair<String, Ref<Material>>> *p_materials_to_save) {
	if (!p_map_settings || filter_face(p_texture_name, p_map_settings)) {
		return;
	}

	{
		MutexLock lock(p_mutex);
		if (p_texture_materials.has(p_texture_name)) {
			return;
		}
	}

	String material_path = TrenchbroomDefaults::resolve_defaults_path(p_map_settings->get_base_material_dir());
	if (material_path.is_empty()) {
		material_path = TrenchbroomDefaults::resolve_defaults_path(p_map_settings->get_base_texture_dir());
	}
	material_path = material_path.path_join(p_texture_name) + "." + p_map_settings->get_material_file_extension();
	material_path = material_path.replace("*", "");

	Ref<Material> material;
	Vector2 texture_size;

	if (ResourceLoader::exists(material_path)) {
		material = ResourceLoader::load(material_path);

		Ref<BaseMaterial3D> base_material = material;
		if (base_material.is_valid()) {
			Ref<Texture2D> albedo = base_material->get_texture(BaseMaterial3D::TEXTURE_ALBEDO);
			if (albedo.is_valid()) {
				texture_size = albedo->get_size();
			}
		} else {
			Ref<ShaderMaterial> shader_material = material;
			if (shader_material.is_valid()) {
				Variant albedo = shader_material->get_shader_parameter(p_map_settings->get_default_material_albedo_uniform());
				Ref<Texture2D> albedo_texture = albedo;
				if (albedo_texture.is_valid()) {
					texture_size = albedo_texture->get_size();
				}
			}
		}

		if (texture_size == Vector2()) {
			Ref<Texture2D> texture = load_texture(p_texture_name, p_wad_resources, p_map_settings);
			if (texture.is_valid()) {
				texture_size = texture->get_size();
			}
		}
		if (texture_size == Vector2()) {
			texture_size = Vector2(p_map_settings->get_inverse_scale_factor(), p_map_settings->get_inverse_scale_factor());
		}
	} else if (p_map_settings->get_default_material().is_valid()) {
		material = p_map_settings->get_default_material()->duplicate(false);
		Ref<Texture2D> texture = load_texture(p_texture_name, p_wad_resources, p_map_settings);
		if (texture.is_valid()) {
			texture_size = texture->get_size();
		} else {
			texture_size = Vector2(p_map_settings->get_inverse_scale_factor(), p_map_settings->get_inverse_scale_factor());
		}

		Ref<BaseMaterial3D> base_material = material;
		if (base_material.is_valid()) {
			base_material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, texture);
			build_base_material(p_map_settings, base_material, p_texture_name);
		} else {
			Ref<ShaderMaterial> shader_material = material;
			if (shader_material.is_valid()) {
				shader_material->set_shader_parameter(p_map_settings->get_default_material_albedo_uniform(), texture);
				const String base_path = TrenchbroomDefaults::resolve_defaults_path(p_map_settings->get_base_texture_dir());
				const Dictionary uniform_patterns = p_map_settings->get_shader_material_uniform_map_patterns();
				Array uniform_keys = uniform_patterns.keys();
				for (int u = 0; u < uniform_keys.size(); u++) {
					const String uniform = uniform_keys[u];
					const String pattern = uniform_patterns[uniform];
					if (pattern.find("%s") < 0) {
						ERR_PRINT(vformat("No string replacement tokens found in ShaderMaterial uniform map pattern '%s'! Must have one instance of '%%s' per pattern.", pattern));
						continue;
					}
					for (int e = 0; e < p_map_settings->get_texture_file_extensions().size(); e++) {
						const String ext = p_map_settings->get_texture_file_extensions()[e];
						String uniform_texture_path = _format_pattern(pattern, { p_texture_name }) + "." + ext;
						uniform_texture_path = base_path.path_join(uniform_texture_path);
						if (ResourceLoader::exists(uniform_texture_path)) {
							shader_material->set_shader_parameter(uniform, ResourceLoader::load(uniform_texture_path));
							break;
						}
					}
				}
			}
		}

		if (p_map_settings->get_save_generated_materials() && material.is_valid() &&
				p_texture_name != p_map_settings->get_clip_texture() &&
				p_texture_name != p_map_settings->get_skip_texture() &&
				p_texture_name != p_map_settings->get_origin_texture() &&
				texture.is_valid() && texture->get_path() != _default_texture_path()) {
			if (p_materials_to_save) {
				p_materials_to_save->push_back(Pair<String, Ref<Material>>(material_path, material));
			} else {
				Ref<DirAccess> dir = DirAccess::open(material_path.get_base_dir());
				if (!dir.is_valid()) {
					dir = DirAccess::open("res://");
					if (dir.is_valid()) {
						dir->make_dir_recursive(material_path.get_base_dir().trim_prefix("res://"));
					}
				}
				ResourceSaver::save(material, material_path);
			}
		}
	} else {
		ERR_PRINT("Error: No default material found in map settings");
		return;
	}

	MutexLock lock(p_mutex);
	if (p_texture_materials.has(p_texture_name)) {
		return;
	}
	p_texture_materials[p_texture_name] = material;
	p_texture_sizes[p_texture_name] = texture_size;
}

struct TextureMapParallelRunner {
	const TrenchbroomMapSettings *map_settings = nullptr;
	TypedArray<QuakeWadFile> wad_resources;
	Dictionary texture_materials;
	Dictionary texture_sizes;
	Mutex mutex;
	LocalVector<String> texture_names;
	LocalVector<Pair<String, Ref<Material>>> materials_to_save;

	void register_texture_at_index(int p_index) {
		if (p_index < 0 || p_index >= (int)texture_names.size()) {
			return;
		}
		TrenchbroomUtil::register_texture_entry(
				texture_names[p_index],
				map_settings,
				wad_resources,
				texture_materials,
				texture_sizes,
				mutex,
				&materials_to_save);
	}
};

static TextureMapParallelRunner *g_texture_map_runner = nullptr;

static void texture_map_register_at_index(int p_index) {
	if (g_texture_map_runner) {
		g_texture_map_runner->register_texture_at_index(p_index);
	}
}

Array TrenchbroomUtil::build_texture_map_parallel(const LocalVector<EntityData> &p_entity_data, const TrenchbroomMapSettings *p_map_settings) {
	Dictionary texture_materials;
	Dictionary texture_sizes;

	if (!p_map_settings) {
		Array result;
		result.push_back(texture_materials);
		result.push_back(texture_sizes);
		return result;
	}

	const uint64_t collect_start = OS::get_singleton()->get_ticks_usec();

	HashSet<String> unique_names;
	collect_texture_names(p_entity_data, p_map_settings, unique_names);

	TextureMapParallelRunner runner;
	runner.map_settings = p_map_settings;
	runner.texture_names.reserve(unique_names.size());
	for (const String &name : unique_names) {
		runner.texture_names.push_back(name);
	}

	const TypedArray<QuakeWadFile> texture_wads = p_map_settings->get_texture_wads();
	for (int i = 0; i < texture_wads.size(); i++) {
		Ref<QuakeWadFile> wad = texture_wads[i];
		if (wad.is_valid() && runner.wad_resources.find(wad) == -1) {
			runner.wad_resources.push_back(wad);
		}
	}

	const uint64_t collect_end = OS::get_singleton()->get_ticks_usec();
	print_profile_info(vformat("Collected %d unique texture names (%.2f ms)", runner.texture_names.size(), (collect_end - collect_start) / 1000.0), "[TEX]");

	const int texture_count = runner.texture_names.size();
	if (texture_count > 0) {
		const uint64_t register_start = OS::get_singleton()->get_ticks_usec();

		WorkerThreadPool *pool = WorkerThreadPool::get_singleton();
		if (pool) {
			g_texture_map_runner = &runner;
			WorkerThreadPool::GroupID task_id = pool->add_group_task(
					callable_mp_static(texture_map_register_at_index),
					texture_count, -1, false, "Register Trenchbroom Textures");
			pool->wait_for_group_task_completion(task_id);
			g_texture_map_runner = nullptr;
		} else {
			for (int i = 0; i < texture_count; i++) {
				runner.register_texture_at_index(i);
			}
		}

		const uint64_t register_end = OS::get_singleton()->get_ticks_usec();
		print_profile_info(vformat("Registered %d textures (%.2f ms)", texture_count, (register_end - register_start) / 1000.0), "[TEX]");

		for (uint32_t i = 0; i < runner.materials_to_save.size(); i++) {
			const String &material_path = runner.materials_to_save[i].first;
			const Ref<Material> &material = runner.materials_to_save[i].second;
			Ref<DirAccess> dir = DirAccess::open(material_path.get_base_dir());
			if (!dir.is_valid()) {
				dir = DirAccess::open("res://");
				if (dir.is_valid()) {
					dir->make_dir_recursive(material_path.get_base_dir().trim_prefix("res://"));
				}
			}
			ResourceSaver::save(material, material_path);
		}
	}

	texture_materials = runner.texture_materials;
	texture_sizes = runner.texture_sizes;

	Array result;
	result.push_back(texture_materials);
	result.push_back(texture_sizes);
	return result;
}

Array TrenchbroomUtil::build_texture_map(const LocalVector<EntityData> &p_entity_data, const TrenchbroomMapSettings *p_map_settings) {
	return build_texture_map_parallel(p_entity_data, p_map_settings);
}

Vector2 TrenchbroomUtil::get_valve_uv(const Vector3 &p_vertex, const Vector3 &p_u_axis, const Vector3 &p_v_axis, const Transform2D &p_uv_basis, const Vector2 &p_texture_size) {
	Vector2 uv(p_u_axis.dot(p_vertex), p_v_axis.dot(p_vertex));
	const Vector2 scale(p_uv_basis.columns[0].x, p_uv_basis.columns[1].y);
	uv += p_uv_basis.get_origin() * scale;
	uv /= scale;
	uv.x /= p_texture_size.x;
	uv.y /= p_texture_size.y;
	return uv;
}

Vector2 TrenchbroomUtil::get_quake_uv(const Vector3 &p_vertex, const Vector3 &p_normal, const Transform2D &p_uv_in, const Vector2 &p_texture_size) {
	int best_index = 0;
	real_t best_dot_product = -INFINITY;
	for (int i = 0; i < _QUAKE_STD_UV_BASE_COUNT; i++) {
		const real_t d = p_normal.dot(_QUAKE_STD_UV_BASE_NORMALS[i]);
		if (d > best_dot_product) {
			best_dot_product = d;
			best_index = i;
		}
	}

	const real_t rot = Math::atan2(-p_uv_in.columns[0].y, p_uv_in.columns[0].x);
	const Vector3 base_u_axis = _QUAKE_STD_UV_BASE_U_AXES[best_index];
	const Vector3 base_v_axis = _QUAKE_STD_UV_BASE_V_AXES[best_index];
	const Vector3 rot_axis = base_v_axis.cross(base_u_axis).normalized();
	const Vector3 u_axis = base_u_axis.rotated(rot_axis, rot);
	const Vector3 v_axis = base_v_axis.rotated(rot_axis, rot);

	const Vector2 rot_x(Math::cos(rot), -Math::sin(rot));
	const Vector2 rot_y(Math::sin(rot), Math::cos(rot));
	real_t sx = p_uv_in.columns[0].dot(rot_x);
	real_t sy = p_uv_in.columns[1].dot(rot_y);
	if (Math::is_zero_approx(sx)) {
		sx = p_uv_in.columns[0].length();
	}
	if (Math::is_zero_approx(sy)) {
		sy = p_uv_in.columns[1].length();
	}

	Vector2 uv_out(u_axis.dot(p_vertex) / sx, v_axis.dot(p_vertex) / sy);
	uv_out += p_uv_in.get_origin();
	uv_out /= p_texture_size;
	return uv_out;
}

Vector2 TrenchbroomUtil::get_face_vertex_uv(const Vector3 &p_vertex, const FaceData &p_face, const Vector2 &p_texture_size) {
	if (p_face.uv_axes.size() >= 2) {
		return get_valve_uv(p_vertex, p_face.uv_axes[0], p_face.uv_axes[1], p_face.uv, p_texture_size);
	}
	return get_quake_uv(p_vertex, p_face.plane.normal, p_face.uv, p_texture_size);
}

PackedFloat32Array TrenchbroomUtil::get_valve_tangent(const Vector3 &p_u, const Vector3 &p_v, const Vector3 &p_normal) {
	const Vector3 u_axis = p_u.normalized();
	const Vector3 v_axis = p_v.normalized();
	const real_t v_sign = -_sign_real(p_normal.cross(u_axis).dot(v_axis));
	PackedFloat32Array result;
	result.resize(4);
	result.set(0, u_axis.x);
	result.set(1, u_axis.y);
	result.set(2, u_axis.z);
	result.set(3, v_sign);
	return result;
}

PackedFloat32Array TrenchbroomUtil::get_quake_tangent(const Vector3 &p_normal, real_t p_uv_y_scale, real_t p_uv_rotation) {
	const real_t dx = p_normal.dot(_VEC3_RIGHT_ID);
	const real_t dy = p_normal.dot(_VEC3_UP_ID);
	const real_t dz = p_normal.dot(_VEC3_FORWARD_ID);
	const real_t dxa = Math::abs(dx);
	const real_t dya = Math::abs(dy);
	const real_t dza = Math::abs(dz);

	Vector3 u_axis;
	real_t v_sign = 0.0;

	if (dya >= dxa && dya >= dza) {
		u_axis = _VEC3_FORWARD_ID;
		v_sign = _sign_real(dy);
	} else if (dxa >= dya && dxa >= dza) {
		u_axis = _VEC3_FORWARD_ID;
		v_sign = -_sign_real(dx);
	} else if (dza >= dya && dza >= dxa) {
		u_axis = _VEC3_RIGHT_ID;
		v_sign = _sign_real(dz);
	}

	v_sign *= _sign_real(p_uv_y_scale);
	u_axis = u_axis.rotated(p_normal, Math::deg_to_rad(-p_uv_rotation) * v_sign);

	PackedFloat32Array result;
	result.resize(4);
	result.set(0, u_axis.x);
	result.set(1, u_axis.y);
	result.set(2, u_axis.z);
	result.set(3, v_sign);
	return result;
}

PackedFloat32Array TrenchbroomUtil::get_face_tangent(const FaceData &p_face) {
	if (p_face.uv_axes.size() >= 2) {
		return get_valve_tangent(p_face.uv_axes[0], p_face.uv_axes[1], p_face.plane.normal);
	}
	return get_quake_tangent(p_face.plane.normal, p_face.uv.get_scale().y, p_face.uv.get_rotation());
}

Ref<ArrayMesh> TrenchbroomUtil::smooth_mesh_by_angle(const Ref<ArrayMesh> &p_mesh, real_t p_angle_deg) {
	if (!p_mesh.is_valid()) {
		ERR_PRINT("Need a source mesh to smooth");
		return Ref<ArrayMesh>();
	}

	const real_t angle = Math::deg_to_rad(CLAMP(p_angle_deg, 0.0, 360.0));

	Vector<Vector3> mesh_vertices;
	Vector<Vector3> mesh_normals;

	struct SurfaceInfo {
		Ref<MeshDataTool> mdt;
		int ofs = 0;
		Ref<Material> mat;
	};
	Vector<SurfaceInfo> surface_data;

	for (int surface_index = 0; surface_index < p_mesh->get_surface_count(); surface_index++) {
		Ref<MeshDataTool> mdt;
		mdt.instantiate();
		if (mdt->create_from_surface(p_mesh, surface_index) != OK) {
			continue;
		}

		SurfaceInfo info;
		info.mdt = mdt;
		info.ofs = mesh_vertices.size();
		info.mat = p_mesh->surface_get_material(surface_index);
		surface_data.push_back(info);

		for (int i = 0; i < mdt->get_vertex_count(); i++) {
			mesh_vertices.push_back(mdt->get_vertex(i));
			mesh_normals.push_back(mdt->get_vertex_normal(i));
		}
	}

	HashMap<String, Vector<int>> groups;
	for (int i = 0; i < mesh_vertices.size(); i++) {
		const Vector3 pos = mesh_vertices[i];
		const Vector3 key_vec(
				Math::snapped(pos.x, VERTEX_EPSILON),
				Math::snapped(pos.y, VERTEX_EPSILON),
				Math::snapped(pos.z, VERTEX_EPSILON));
		const String key = vformat("%f,%f,%f", key_vec.x, key_vec.y, key_vec.z);
		if (!groups.has(key)) {
			Vector<int> group;
			group.push_back(i);
			groups[key] = group;
		} else {
			groups[key].push_back(i);
		}
	}

	for (KeyValue<String, Vector<int>> &group_entry : groups) {
		Vector<int> &group = group_entry.value;
		for (int i = 0; i < group.size(); i++) {
			const int vertex_index = group[i];
			const Vector3 this_normal = mesh_normals[vertex_index];
			Vector3 normal_out;
			for (int j = 0; j < group.size(); j++) {
				const Vector3 other = mesh_normals[group[j]];
				if (this_normal.angle_to(other) <= angle) {
					normal_out += other;
				}
			}
			mesh_normals.write[vertex_index] = normal_out.normalized();
		}
	}

	Ref<ArrayMesh> smoothed_mesh;
	smoothed_mesh.instantiate();

	for (int surface_index = 0; surface_index < surface_data.size(); surface_index++) {
		const SurfaceInfo &info = surface_data[surface_index];
		Ref<MeshDataTool> mdt = info.mdt;
		const int offset = info.ofs;

		for (int i = 0; i < mdt->get_vertex_count(); i++) {
			mdt->set_vertex_normal(i, mesh_normals[offset + i]);
		}

		Ref<SurfaceTool> st;
		st.instantiate();
		st->begin(Mesh::PRIMITIVE_TRIANGLES);
		st->set_material(info.mat);

		for (int i = 0; i < mdt->get_face_count(); i++) {
			for (int j = 0; j < 3; j++) {
				const int index = mdt->get_face_vertex(i, j);
				st->set_normal(mdt->get_vertex_normal(index));
				st->set_uv(mdt->get_vertex_uv(index));
				st->set_tangent(mdt->get_vertex_tangent(index));
				st->add_vertex(mdt->get_vertex(index));
			}
		}

		st->commit(smoothed_mesh);
	}

	return smoothed_mesh;
}

void TrenchbroomUtil::print_profile_info(const String &p_message, const String &p_signature) {
	print_line(p_signature + " " + p_message);
}

Vector3 TrenchbroomUtil::sample_bezier_curve(const PackedVector3Array &p_controls, real_t p_t) {
	if (p_controls.is_empty()) {
		return Vector3();
	}
	PackedVector3Array points = p_controls;
	const int control_count = p_controls.size();
	for (int i = 0; i < control_count; i++) {
		for (int j = 0; j < control_count - 1 - i; j++) {
			points.write[j] = points[j].lerp(points[j + 1], p_t);
		}
	}
	return points[0];
}

Vector3 TrenchbroomUtil::sample_bezier_surface(const PackedVector3Array &p_controls, int p_width, int p_height, real_t p_u, real_t p_v) {
	PackedVector3Array curve;
	curve.resize(p_width);
	for (int x = 0; x < p_width; x++) {
		PackedVector3Array column;
		column.resize(p_height);
		for (int y = 0; y < p_height; y++) {
			const int idx = y * p_width + x;
			if (idx < p_controls.size()) {
				column.write[y] = p_controls[idx];
			}
		}
		curve.write[x] = sample_bezier_curve(column, p_v);
	}
	return sample_bezier_curve(curve, p_u);
}

PackedInt32Array TrenchbroomUtil::get_triangle_indices(int p_width, int p_height) {
	PackedInt32Array indices;
	if (p_width < 2 || p_height < 2) {
		return indices;
	}

	for (int row = 0; row < p_height - 1; row++) {
		for (int col = 0; col < p_width - 1; col++) {
			indices.push_back(col + row * p_width);
			indices.push_back((col + 1) + row * p_width);
			indices.push_back(col + (row + 1) * p_width);

			indices.push_back((col + 1) + row * p_width);
			indices.push_back((col + 1) + (row + 1) * p_width);
			indices.push_back(col + (row + 1) * p_width);
		}
	}
	return indices;
}

PackedVector3Array TrenchbroomUtil::elevate_quadratic(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2) {
	PackedVector3Array result;
	result.push_back(p0);
	result.push_back(p0 + ((p1 - p0) * (2.0 / 3.0)));
	result.push_back(p2 + ((p1 - p2) * (2.0 / 3.0)));
	result.push_back(p2);
	return result;
}
