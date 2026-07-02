/**************************************************************************/
/*  trenchbroom_defaults.cpp                                              */
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

#include "trenchbroom_defaults.h"

#include "fgd/blazium_fgd_base_class.h"
#include "fgd/blazium_fgd_file.h"
#include "fgd/blazium_fgd_solid_class.h"
#include "netradiant/netradiant_custom_gamepack_config.h"
#include "trenchbroom/trenchbroom_game_config.h"
#include "trenchbroom/trenchbroom_tag.h"
#include "trenchbroom_local_config.h"
#include "trenchbroom_map_settings.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/class_db.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/material.h"

void TrenchbroomDefaults::_bind_methods() {
	ClassDB::bind_static_method("TrenchbroomDefaults", D_METHOD("get_defaults_dir"), &TrenchbroomDefaults::get_defaults_dir);
	ClassDB::bind_static_method("TrenchbroomDefaults", D_METHOD("get_module_defaults_dir"), &TrenchbroomDefaults::get_module_defaults_dir);
	ClassDB::bind_static_method("TrenchbroomDefaults", D_METHOD("resolve_defaults_path", "path"), &TrenchbroomDefaults::resolve_defaults_path);
	ClassDB::bind_static_method("TrenchbroomDefaults", D_METHOD("get_default_texture_path"), &TrenchbroomDefaults::get_default_texture_path);
	ClassDB::bind_static_method("TrenchbroomDefaults", D_METHOD("get_default_palette_path"), &TrenchbroomDefaults::get_default_palette_path);
	ClassDB::bind_static_method("TrenchbroomDefaults", D_METHOD("get_quake2_palette_path"), &TrenchbroomDefaults::get_quake2_palette_path);
	ClassDB::bind_static_method("TrenchbroomDefaults", D_METHOD("ensure_default_assets"), &TrenchbroomDefaults::ensure_default_assets);
	ClassDB::bind_static_method("TrenchbroomDefaults", D_METHOD("create_default_fgd"), &TrenchbroomDefaults::create_default_fgd);
	ClassDB::bind_static_method("TrenchbroomDefaults", D_METHOD("create_default_map_settings"), &TrenchbroomDefaults::create_default_map_settings);
	ClassDB::bind_static_method("TrenchbroomDefaults", D_METHOD("create_default_game_config"), &TrenchbroomDefaults::create_default_game_config);
	ClassDB::bind_static_method("TrenchbroomDefaults", D_METHOD("make_face_tag", "name", "pattern"), &TrenchbroomDefaults::make_face_tag);
	ClassDB::bind_static_method("TrenchbroomDefaults", D_METHOD("make_brush_tag", "name", "pattern"), &TrenchbroomDefaults::make_brush_tag);
}

static Ref<TrenchbroomTag> _make_face_tag(const String &p_name, const String &p_pattern) {
	Ref<TrenchbroomTag> tag;
	tag.instantiate();
	tag->set_tag_name(p_name);
	tag->set_tag_pattern(p_pattern);
	tag->set_tag_match_type(TrenchbroomTag::TAG_MATCH_TEXTURE);
	TypedArray<String> attributes;
	attributes.push_back("transparent");
	tag->set_tag_attributes(attributes);
	return tag;
}

static Ref<TrenchbroomTag> _make_brush_tag(const String &p_name, const String &p_pattern) {
	Ref<TrenchbroomTag> tag;
	tag.instantiate();
	tag->set_tag_name(p_name);
	tag->set_tag_pattern(p_pattern);
	tag->set_tag_match_type(TrenchbroomTag::TAG_MATCH_CLASSNAME);
	return tag;
}

static Ref<BlaziumFGDBaseClass> _make_phong_base() {
	Ref<BlaziumFGDBaseClass> base;
	base.instantiate();
	base->set_classname("Phong");
	base->set_description("Phong shading options for SolidClass geometry.");
	Dictionary props;
	Dictionary phong_enum;
	phong_enum["Disabled"] = 0;
	phong_enum["Smooth shading"] = 1;
	props["_phong"] = phong_enum;
	props["_phong_angle"] = 89.0;
	base->set_class_properties(props);
	Dictionary desc;
	Array phong_desc;
	phong_desc.push_back("Phong shading");
	phong_desc.push_back(0);
	desc["_phong"] = phong_desc;
	desc["_phong_angle"] = "Phong smoothing angle";
	base->set_class_property_descriptions(desc);
	return base;
}

static Ref<BlaziumFGDBaseClass> _make_vertex_merge_base() {
	Ref<BlaziumFGDBaseClass> base;
	base.instantiate();
	base->set_classname("VertexMergeDistance");
	base->set_description("Adjustable value to snap vertices to on map build. This can reduce instances of seams between polygons.");
	Dictionary props;
	props["_vertex_merge_distance"] = 0.03125;
	base->set_class_properties(props);
	Dictionary desc;
	desc["_vertex_merge_distance"] = "Adjustable value to snap vertices to on map build. This can reduce instances of seams between polygons.";
	base->set_class_property_descriptions(desc);
	return base;
}

static Ref<BlaziumFGDBaseClass> _make_cull_interior_base() {
	Ref<BlaziumFGDBaseClass> base;
	base.instantiate();
	base->set_classname("CullInteriorFaces");
	base->set_description("Cull interior faces option for SolidClass Geometry");
	Dictionary props;
	props["_cull_interior_faces"] = false;
	base->set_class_properties(props);
	Dictionary desc;
	desc["_cull_interior_faces"] = "If true, cull interior faces with matching vertices or faces that are flush within a larger face. Note: This has a performance impact that scales with how many brushes are in the brush entity.";
	base->set_class_property_descriptions(desc);
	return base;
}

static void _apply_solid_bases(const Ref<BlaziumFGDSolidClass> &p_solid, const Ref<BlaziumFGDBaseClass> &p_phong, const Ref<BlaziumFGDBaseClass> &p_vertex, const Ref<BlaziumFGDBaseClass> &p_cull) {
	Array bases;
	bases.push_back(p_phong);
	bases.push_back(p_vertex);
	bases.push_back(p_cull);
	p_solid->set_base_classes(bases);
	Dictionary meta;
	meta["color"] = Color(0.8, 0.8, 0.8, 1);
	p_solid->set_meta_properties(meta);
}

static Ref<BlaziumFGDSolidClass> _make_worldspawn(const Ref<BlaziumFGDBaseClass> &p_vertex, const Ref<BlaziumFGDBaseClass> &p_cull) {
	Ref<BlaziumFGDSolidClass> solid;
	solid.instantiate();
	solid->set_classname("worldspawn");
	solid->set_description("Default static world geometry. Builds a StaticBody3D with a single MeshInstance3D and a single convex CollisionShape3D shape.");
	solid->set_node_class("StaticBody3D");
	solid->set_spawn_type(BlaziumFGDSolidClass::SPAWN_WORLDSPAWN);
	solid->set_origin_type(BlaziumFGDSolidClass::ORIGIN_ABSOLUTE);
	solid->set_collision_shape_type(BlaziumFGDSolidClass::COLLISION_CONVEX);
	solid->set_collision_mask(0);
	Array bases;
	bases.push_back(p_vertex);
	bases.push_back(p_cull);
	solid->set_base_classes(bases);
	Dictionary meta;
	meta["color"] = Color(0.8, 0.8, 0.8, 1);
	solid->set_meta_properties(meta);
	return solid;
}

static Ref<BlaziumFGDSolidClass> _make_func_geo(const Ref<BlaziumFGDBaseClass> &p_phong, const Ref<BlaziumFGDBaseClass> &p_vertex, const Ref<BlaziumFGDBaseClass> &p_cull) {
	Ref<BlaziumFGDSolidClass> solid;
	solid.instantiate();
	solid->set_classname("func_geo");
	solid->set_description("Static collidable geometry. Builds a StaticBody3D with a MeshInstance3D, a single concave CollisionShape3D, and an OccluderInstance3D.");
	solid->set_node_class("StaticBody3D");
	solid->set_build_occlusion(true);
	solid->set_collision_shape_type(BlaziumFGDSolidClass::COLLISION_CONCAVE);
	solid->set_collision_mask(0);
	_apply_solid_bases(solid, p_phong, p_vertex, p_cull);
	return solid;
}

static Ref<BlaziumFGDSolidClass> _make_func_detail(const Ref<BlaziumFGDBaseClass> &p_phong, const Ref<BlaziumFGDBaseClass> &p_vertex, const Ref<BlaziumFGDBaseClass> &p_cull) {
	Ref<BlaziumFGDSolidClass> solid;
	solid.instantiate();
	solid->set_classname("func_detail");
	solid->set_description("Static collidable geometry. Builds a StaticBody3D with a MeshInstance3D and a single concave CollisionShape3D. Does not occlude other VisualInstance3D nodes.");
	solid->set_node_class("StaticBody3D");
	solid->set_collision_shape_type(BlaziumFGDSolidClass::COLLISION_CONCAVE);
	solid->set_collision_mask(0);
	_apply_solid_bases(solid, p_phong, p_vertex, p_cull);
	return solid;
}

static Ref<BlaziumFGDSolidClass> _make_func_illusionary(const Ref<BlaziumFGDBaseClass> &p_phong, const Ref<BlaziumFGDBaseClass> &p_vertex, const Ref<BlaziumFGDBaseClass> &p_cull) {
	Ref<BlaziumFGDSolidClass> solid;
	solid.instantiate();
	solid->set_classname("func_illusionary");
	solid->set_description("Static geometry with no collision. Builds a Node3D with a MeshInstance3D and an Occluder3D to aid in render culling of other VisualInstance3D nodes.");
	solid->set_node_class("Node3D");
	solid->set_build_occlusion(true);
	solid->set_collision_shape_type(BlaziumFGDSolidClass::COLLISION_NONE);
	_apply_solid_bases(solid, p_phong, p_vertex, p_cull);
	return solid;
}

static Ref<BlaziumFGDSolidClass> _make_func_detail_illusionary(const Ref<BlaziumFGDBaseClass> &p_phong, const Ref<BlaziumFGDBaseClass> &p_vertex, const Ref<BlaziumFGDBaseClass> &p_cull) {
	Ref<BlaziumFGDSolidClass> solid;
	solid.instantiate();
	solid->set_classname("func_detail_illusionary");
	solid->set_description("Static geometry with no collision. Builds a Node3D with a MeshInstance3D. Does not occlude other VisualInstance3D nodes.");
	solid->set_node_class("Node3D");
	solid->set_collision_shape_type(BlaziumFGDSolidClass::COLLISION_NONE);
	_apply_solid_bases(solid, p_phong, p_vertex, p_cull);
	return solid;
}

String TrenchbroomDefaults::get_defaults_dir() {
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (!project_settings || !project_settings->has_setting("blazium/trenchbroom/defaults_path")) {
		return String();
	}
	return project_settings->get_setting("blazium/trenchbroom/defaults_path");
}

String TrenchbroomDefaults::get_module_defaults_dir() {
	return get_defaults_dir();
}

String TrenchbroomDefaults::resolve_defaults_path(const String &p_path) {
	if (p_path.is_empty()) {
		return p_path;
	}
	if (p_path.begins_with("res://") || p_path.begins_with("user://") || p_path.is_absolute_path()) {
		return p_path;
	}
	const String defaults_dir = get_defaults_dir();
	if (defaults_dir.is_empty()) {
		return p_path;
	}
	return defaults_dir.path_join(p_path);
}

String TrenchbroomDefaults::get_default_texture_path() {
	return resolve_defaults_path("textures/default_texture.png");
}

String TrenchbroomDefaults::get_default_palette_path() {
	return resolve_defaults_path("palette.lmp");
}

String TrenchbroomDefaults::get_quake2_palette_path() {
	return resolve_defaults_path("quake2_palette.lmp");
}

static void _ensure_dir(const String &p_dir) {
	if (DirAccess::dir_exists_absolute(p_dir)) {
		return;
	}
	Ref<DirAccess> root = DirAccess::open("res://");
	if (root.is_valid() && p_dir.begins_with("res://")) {
		root->make_dir_recursive(p_dir.trim_prefix("res://"));
	} else {
		DirAccess::make_dir_recursive_absolute(p_dir);
	}
}

static void _ensure_default_palette_file(const String &p_path) {
	if (FileAccess::exists(p_path)) {
		return;
	}
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE);
	if (file.is_null()) {
		return;
	}
	PackedByteArray palette;
	palette.resize(768);
	for (int i = 0; i < 256; i++) {
		palette.write[i * 3] = (uint8_t)i;
		palette.write[(i * 3) + 1] = (uint8_t)i;
		palette.write[(i * 3) + 2] = (uint8_t)i;
	}
	file->store_buffer(palette);
}

static void _ensure_default_texture_png(const String &p_path, const Color &p_primary, const Color &p_secondary) {
	if (FileAccess::exists(p_path)) {
		return;
	}
	Ref<Image> image = Image::create_empty(64, 64, false, Image::FORMAT_RGB8);
	for (int y = 0; y < 64; y++) {
		for (int x = 0; x < 64; x++) {
			const bool checker = ((x / 8) + (y / 8)) % 2 == 0;
			image->set_pixel(x, y, checker ? p_primary : p_secondary);
		}
	}
	image->save_png(p_path);
}

static void _copy_file_if_source_exists(const String &p_source, const String &p_dest) {
	if (FileAccess::exists(p_dest) || !FileAccess::exists(p_source)) {
		return;
	}
	Ref<FileAccess> source = FileAccess::open(p_source, FileAccess::READ);
	if (source.is_null()) {
		return;
	}
	Ref<FileAccess> dest = FileAccess::open(p_dest, FileAccess::WRITE);
	if (dest.is_null()) {
		return;
	}
	dest->store_buffer(source->get_buffer(source->get_length()));
}

void TrenchbroomDefaults::ensure_default_assets() {
	const String defaults_dir = get_defaults_dir();
	if (defaults_dir.is_empty()) {
		return;
	}
	_ensure_dir(defaults_dir);
	_ensure_dir(defaults_dir.path_join("textures"));

	const String palette_path = get_default_palette_path();
	const String quake2_palette_path = get_quake2_palette_path();
	const String textures_palette_path = defaults_dir.path_join("textures/palette.lmp");
	_ensure_default_palette_file(palette_path);
	_ensure_default_palette_file(quake2_palette_path);
	_copy_file_if_source_exists(palette_path, textures_palette_path);
	if (!FileAccess::exists(textures_palette_path)) {
		_ensure_default_palette_file(textures_palette_path);
	}

	_ensure_default_texture_png(get_default_texture_path(), Color(0.8, 0.2, 0.2), Color(0.4, 0.1, 0.1));
	_ensure_default_texture_png(defaults_dir.path_join("textures/clip.png"), Color(1.0, 0.0, 1.0), Color(0.6, 0.0, 0.6));
	_ensure_default_texture_png(defaults_dir.path_join("textures/skip.png"), Color(0.0, 1.0, 0.0), Color(0.0, 0.6, 0.0));
	_ensure_default_texture_png(defaults_dir.path_join("textures/origin.png"), Color(0.0, 0.0, 1.0), Color(0.0, 0.0, 0.6));

	const String icon_path = defaults_dir.path_join("icon32.png");
	if (!FileAccess::exists(icon_path)) {
		Ref<Image> icon = Image::create_empty(32, 32, false, Image::FORMAT_RGBA8);
		icon->fill(Color(0.8, 0.2, 0.2, 1.0));
		icon->save_png(icon_path);
	}

	const String map_settings_path = defaults_dir.path_join("trenchbroom_default_map_settings.tres");
	if (!ResourceLoader::exists(map_settings_path)) {
		Ref<TrenchbroomMapSettings> settings = create_default_map_settings();
		if (settings.is_valid()) {
			ResourceSaver::save(settings, map_settings_path);
		}
	}

	const String local_config_path = defaults_dir.path_join("trenchbroom_local_config.tres");
	if (!ResourceLoader::exists(local_config_path)) {
		Ref<TrenchbroomLocalConfig> local_config;
		local_config.instantiate();
		if (local_config.is_valid()) {
			ResourceSaver::save(local_config, local_config_path);
		}
	}
}

static void _resolve_map_settings_paths(const Ref<TrenchbroomMapSettings> &p_settings) {
	if (p_settings.is_null()) {
		return;
	}
	const String texture_dir = p_settings->get_base_texture_dir();
	if (!texture_dir.is_empty() && !texture_dir.begins_with("res://") && !texture_dir.begins_with("user://") && !texture_dir.is_absolute_path()) {
		p_settings->set_base_texture_dir(TrenchbroomDefaults::resolve_defaults_path(texture_dir));
	}
}

Ref<BlaziumFGDFile> TrenchbroomDefaults::create_default_fgd() {
	const String fgd_path = resolve_defaults_path("blazium_fgd.tres");
	if (ResourceLoader::exists(fgd_path)) {
		Ref<BlaziumFGDFile> loaded = ResourceLoader::load(fgd_path);
		if (loaded.is_valid()) {
			return loaded;
		}
	}

	Ref<BlaziumFGDBaseClass> phong = _make_phong_base();
	Ref<BlaziumFGDBaseClass> vertex = _make_vertex_merge_base();
	Ref<BlaziumFGDBaseClass> cull = _make_cull_interior_base();

	Ref<BlaziumFGDFile> fgd;
	fgd.instantiate();
	fgd->set_fgd_name("blazium");
	Array defs;
	defs.push_back(phong);
	defs.push_back(vertex);
	defs.push_back(cull);
	defs.push_back(_make_worldspawn(vertex, cull));
	defs.push_back(_make_func_geo(phong, vertex, cull));
	defs.push_back(_make_func_detail(phong, vertex, cull));
	defs.push_back(_make_func_detail_illusionary(phong, vertex, cull));
	defs.push_back(_make_func_illusionary(phong, vertex, cull));
	fgd->set_entity_definitions(defs);
	return fgd;
}

Ref<TrenchbroomMapSettings> TrenchbroomDefaults::create_default_map_settings() {
	const String settings_path = resolve_defaults_path("trenchbroom_default_map_settings.tres");
	if (!settings_path.is_empty() && ResourceLoader::exists(settings_path)) {
		Ref<TrenchbroomMapSettings> loaded = ResourceLoader::load(settings_path);
		if (loaded.is_valid()) {
			_resolve_map_settings_paths(loaded);
			Ref<StandardMaterial3D> material = loaded->get_default_material();
			if (material.is_null() || material->get_texture(StandardMaterial3D::TEXTURE_ALBEDO).is_null()) {
				const String texture_png_path = get_default_texture_path();
				Ref<StandardMaterial3D> rebuilt_material;
				rebuilt_material.instantiate();
				Ref<Image> image;
				image.instantiate();
				if (!texture_png_path.is_empty()) {
					const String global_texture_path = ProjectSettings::get_singleton()->globalize_path(texture_png_path);
					if (image->load(global_texture_path) == OK) {
						rebuilt_material->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, ImageTexture::create_from_image(image));
					}
				}
				if (rebuilt_material->get_texture(StandardMaterial3D::TEXTURE_ALBEDO).is_null()) {
					rebuilt_material->set_albedo(Color(0.8, 0.2, 0.2));
				}
				loaded->set_default_material(rebuilt_material);
			}
			return loaded;
		}
	}

	Ref<TrenchbroomMapSettings> settings;
	settings.instantiate();
	settings->set_inverse_scale_factor(32.0);
	settings->set_base_texture_dir(resolve_defaults_path("textures"));
	settings->set_clip_texture("clip");
	settings->set_skip_texture("skip");
	settings->set_origin_texture("origin");

	TypedArray<String> extensions;
	extensions.push_back("png");
	extensions.push_back("jpg");
	extensions.push_back("jpeg");
	extensions.push_back("bmp");
	extensions.push_back("tga");
	extensions.push_back("webp");
	extensions.push_back("wal");
	settings->set_texture_file_extensions(extensions);

	const String material_path = resolve_defaults_path("textures/default_material.tres");
	const String texture_png_path = get_default_texture_path();
	Ref<StandardMaterial3D> default_material;
	if (!material_path.is_empty() && ResourceLoader::exists(material_path)) {
		Ref<Material> loaded_material = ResourceLoader::load(material_path);
		default_material = loaded_material;
	}
	if (default_material.is_null()) {
		default_material.instantiate();
	}
	if (default_material->get_texture(StandardMaterial3D::TEXTURE_ALBEDO).is_null() && !texture_png_path.is_empty()) {
		Ref<Image> image;
		image.instantiate();
		const String global_texture_path = ProjectSettings::get_singleton()->globalize_path(texture_png_path);
		if (image->load(global_texture_path) == OK) {
			default_material->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, ImageTexture::create_from_image(image));
		} else {
			image = Image::create_empty(64, 64, false, Image::FORMAT_RGB8);
			for (int y = 0; y < 64; y++) {
				for (int x = 0; x < 64; x++) {
					const bool checker = ((x / 8) + (y / 8)) % 2 == 0;
					image->set_pixel(x, y, checker ? Color(0.8, 0.2, 0.2) : Color(0.4, 0.1, 0.1));
				}
			}
			default_material->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, ImageTexture::create_from_image(image));
		}
	}
	settings->set_default_material(default_material);

	settings->set_entity_fgd(create_default_fgd());
	return settings;
}

Ref<TrenchbroomGameConfig> TrenchbroomDefaults::create_default_game_config() {
	const String config_path = resolve_defaults_path("trenchbroom_game_config.tres");
	if (ResourceLoader::exists(config_path)) {
		Ref<TrenchbroomGameConfig> loaded = ResourceLoader::load(config_path);
		if (loaded.is_valid()) {
			return loaded;
		}
	}

	Ref<TrenchbroomGameConfig> config;
	config.instantiate();
	config->set_game_name("Blazium");
	config->set_fgd_file(create_default_fgd());
	config->set_palette_path("textures/palette.lmp");

	const String icon_path = resolve_defaults_path("icon32.png");
	if (!icon_path.is_empty() && ResourceLoader::exists(icon_path)) {
		config->set_icon(ResourceLoader::load(icon_path));
	}

	TypedArray<TrenchbroomTag> face_tags;
	face_tags.push_back(make_face_tag("Clip", "clip"));
	face_tags.push_back(make_face_tag("Skip", "skip"));
	face_tags.push_back(make_face_tag("Origin", "origin"));
	config->set_brushface_tags(face_tags);

	TypedArray<TrenchbroomTag> brush_tags;
	brush_tags.push_back(make_brush_tag("Func", "func*"));
	brush_tags.push_back(make_brush_tag("Trigger", "trigger*"));
	config->set_brush_tags(brush_tags);

	return config;
}

Ref<TrenchbroomTag> TrenchbroomDefaults::make_face_tag(const String &p_name, const String &p_pattern) {
	return _make_face_tag(p_name, p_pattern);
}

Ref<TrenchbroomTag> TrenchbroomDefaults::make_brush_tag(const String &p_name, const String &p_pattern) {
	return _make_brush_tag(p_name, p_pattern);
}
