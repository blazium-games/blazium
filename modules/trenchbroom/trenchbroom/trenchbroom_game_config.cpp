/**************************************************************************/
/*  trenchbroom_game_config.cpp                                           */
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

#include "trenchbroom_game_config.h"

#include "core/config/project_settings.h"
#include "core/error/error_macros.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/object/class_db.h"
#include "core/string/print_string.h"
#include "modules/trenchbroom/fgd/blazium_fgd_file.h"
#include "modules/trenchbroom/trenchbroom_defaults.h"
#include "modules/trenchbroom/trenchbroom_local_config.h"
#include "trenchbroom_tag.h"

void TrenchbroomGameConfig::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_game_name", "game_name"), &TrenchbroomGameConfig::set_game_name);
	ClassDB::bind_method(D_METHOD("get_game_name"), &TrenchbroomGameConfig::get_game_name);
	ClassDB::bind_method(D_METHOD("set_icon", "icon"), &TrenchbroomGameConfig::set_icon);
	ClassDB::bind_method(D_METHOD("get_icon"), &TrenchbroomGameConfig::get_icon);
	ClassDB::bind_method(D_METHOD("set_map_formats", "map_formats"), &TrenchbroomGameConfig::set_map_formats);
	ClassDB::bind_method(D_METHOD("get_map_formats"), &TrenchbroomGameConfig::get_map_formats);
	ClassDB::bind_method(D_METHOD("set_textures_root_folder", "textures_root_folder"), &TrenchbroomGameConfig::set_textures_root_folder);
	ClassDB::bind_method(D_METHOD("get_textures_root_folder"), &TrenchbroomGameConfig::get_textures_root_folder);
	ClassDB::bind_method(D_METHOD("set_texture_exclusion_patterns", "texture_exclusion_patterns"), &TrenchbroomGameConfig::set_texture_exclusion_patterns);
	ClassDB::bind_method(D_METHOD("get_texture_exclusion_patterns"), &TrenchbroomGameConfig::get_texture_exclusion_patterns);
	ClassDB::bind_method(D_METHOD("set_palette_path", "palette_path"), &TrenchbroomGameConfig::set_palette_path);
	ClassDB::bind_method(D_METHOD("get_palette_path"), &TrenchbroomGameConfig::get_palette_path);
	ClassDB::bind_method(D_METHOD("set_fgd_file", "fgd_file"), &TrenchbroomGameConfig::set_fgd_file);
	ClassDB::bind_method(D_METHOD("get_fgd_file"), &TrenchbroomGameConfig::get_fgd_file);
	ClassDB::bind_method(D_METHOD("set_entity_scale", "entity_scale"), &TrenchbroomGameConfig::set_entity_scale);
	ClassDB::bind_method(D_METHOD("get_entity_scale"), &TrenchbroomGameConfig::get_entity_scale);
	ClassDB::bind_method(D_METHOD("set_set_default_properties", "set_default_properties"), &TrenchbroomGameConfig::set_set_default_properties);
	ClassDB::bind_method(D_METHOD("get_set_default_properties"), &TrenchbroomGameConfig::get_set_default_properties);
	ClassDB::bind_method(D_METHOD("set_generate_model_point_class_models", "generate_model_point_class_models"), &TrenchbroomGameConfig::set_generate_model_point_class_models);
	ClassDB::bind_method(D_METHOD("get_generate_model_point_class_models"), &TrenchbroomGameConfig::get_generate_model_point_class_models);
	ClassDB::bind_method(D_METHOD("set_brush_tags", "brush_tags"), &TrenchbroomGameConfig::set_brush_tags);
	ClassDB::bind_method(D_METHOD("get_brush_tags"), &TrenchbroomGameConfig::get_brush_tags);
	ClassDB::bind_method(D_METHOD("set_brushface_tags", "brushface_tags"), &TrenchbroomGameConfig::set_brushface_tags);
	ClassDB::bind_method(D_METHOD("get_brushface_tags"), &TrenchbroomGameConfig::get_brushface_tags);
	ClassDB::bind_method(D_METHOD("set_default_uv_scale", "default_uv_scale"), &TrenchbroomGameConfig::set_default_uv_scale);
	ClassDB::bind_method(D_METHOD("get_default_uv_scale"), &TrenchbroomGameConfig::get_default_uv_scale);
	ClassDB::bind_method(D_METHOD("set_game_config_version", "game_config_version"), &TrenchbroomGameConfig::set_game_config_version);
	ClassDB::bind_method(D_METHOD("get_game_config_version"), &TrenchbroomGameConfig::get_game_config_version);
	ClassDB::bind_method(D_METHOD("export_file"), &TrenchbroomGameConfig::export_file);
	ClassDB::bind_method(D_METHOD("_get_export_file_func"), &TrenchbroomGameConfig::_get_export_file_func);
	ClassDB::bind_method(D_METHOD("build_class_text"), &TrenchbroomGameConfig::build_class_text);

	BIND_ENUM_CONSTANT(CONFIG_LATEST);
	BIND_ENUM_CONSTANT(CONFIG_VERSION4);
	BIND_ENUM_CONSTANT(CONFIG_VERSION8);
	BIND_ENUM_CONSTANT(CONFIG_VERSION9);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "game_name"), "set_game_name", "get_game_name");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "icon", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_icon", "get_icon");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "map_formats"), "set_map_formats", "get_map_formats");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "textures_root_folder", PROPERTY_HINT_DIR), "set_textures_root_folder", "get_textures_root_folder");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "texture_exclusion_patterns"), "set_texture_exclusion_patterns", "get_texture_exclusion_patterns");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "palette_path", PROPERTY_HINT_FILE, "*.lmp"), "set_palette_path", "get_palette_path");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "fgd_file", PROPERTY_HINT_RESOURCE_TYPE, "BlaziumFGDFile"), "set_fgd_file", "get_fgd_file");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "entity_scale"), "set_entity_scale", "get_entity_scale");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "set_default_properties"), "set_set_default_properties", "get_set_default_properties");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "generate_model_point_class_models"), "set_generate_model_point_class_models", "get_generate_model_point_class_models");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "brush_tags", PROPERTY_HINT_ARRAY_TYPE, "TrenchbroomTag"), "set_brush_tags", "get_brush_tags");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "brushface_tags", PROPERTY_HINT_ARRAY_TYPE, "TrenchbroomTag"), "set_brushface_tags", "get_brushface_tags");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "default_uv_scale"), "set_default_uv_scale", "get_default_uv_scale");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "game_config_version", PROPERTY_HINT_ENUM, "Latest,Version 4,Version 8,Version 9"), "set_game_config_version", "get_game_config_version");
	ADD_PROPERTY(PropertyInfo(Variant::CALLABLE, "_export_file_func", PROPERTY_HINT_TOOL_BUTTON, "Export GameConfig", PROPERTY_USAGE_EDITOR), "", "_get_export_file_func");
}

Callable TrenchbroomGameConfig::_get_export_file_func() const {
	return callable_mp(const_cast<TrenchbroomGameConfig *>(this), &TrenchbroomGameConfig::export_file);
}

TrenchbroomGameConfig::TrenchbroomGameConfig() {
	if (map_formats.is_empty()) {
		Dictionary valve_format;
		valve_format["format"] = "Valve";
		valve_format["initialmap"] = "initial_valve.map";
		map_formats.push_back(valve_format);

		Dictionary standard_format;
		standard_format["format"] = "Standard";
		standard_format["initialmap"] = "initial_standard.map";
		map_formats.push_back(standard_format);

		Dictionary quake2_format;
		quake2_format["format"] = "Quake2";
		quake2_format["initialmap"] = "initial_quake2.map";
		map_formats.push_back(quake2_format);

		Dictionary quake3_format;
		quake3_format["format"] = "Quake3";
		map_formats.push_back(quake3_format);
	}

	if (texture_exclusion_patterns.is_empty()) {
		texture_exclusion_patterns.push_back("*_albedo");
		texture_exclusion_patterns.push_back("*_ao");
		texture_exclusion_patterns.push_back("*_emission");
		texture_exclusion_patterns.push_back("*_height");
		texture_exclusion_patterns.push_back("*_metallic");
		texture_exclusion_patterns.push_back("*_normal");
		texture_exclusion_patterns.push_back("*_orm");
		texture_exclusion_patterns.push_back("*_roughness");
		texture_exclusion_patterns.push_back("*_sss");
	}

	if (brushface_tags.is_empty()) {
		brushface_tags.push_back(TrenchbroomDefaults::make_face_tag("Clip", "clip"));
		brushface_tags.push_back(TrenchbroomDefaults::make_face_tag("Skip", "skip"));
		brushface_tags.push_back(TrenchbroomDefaults::make_face_tag("Origin", "origin"));
	}

	if (brush_tags.is_empty()) {
		brush_tags.push_back(TrenchbroomDefaults::make_brush_tag("Func", "func*"));
		brush_tags.push_back(TrenchbroomDefaults::make_brush_tag("Trigger", "trigger*"));
	}

	palette_path = "textures/palette.lmp";
}

String TrenchbroomGameConfig::_parse_default_uv_scale(const Vector2 &p_texture_scale) const {
	return vformat("\"scale\": [%s, %s]", String::num(p_texture_scale.x), String::num(p_texture_scale.y));
}

String TrenchbroomGameConfig::_parse_tags(const TypedArray<TrenchbroomTag> &p_tags) const {
	String tags_str;
	for (int tag_index = 0; tag_index < p_tags.size(); tag_index++) {
		Ref<TrenchbroomTag> brush_tag = p_tags[tag_index];
		if (brush_tag.is_null()) {
			continue;
		}
		if ((int)brush_tag->get_tag_match_type() >= TrenchbroomTag::TAG_MATCH_CLASSNAME + 1) {
			continue;
		}

		tags_str += "{\n";
		tags_str += vformat("\t\t\t\t\"name\": \"%s\",\n", brush_tag->get_tag_name());

		String attribs_str;
		const TypedArray<String> attributes = brush_tag->get_tag_attributes();
		for (int attrib_index = 0; attrib_index < attributes.size(); attrib_index++) {
			attribs_str += vformat("\"%s\"", String(attributes[attrib_index]));
			if (attrib_index < attributes.size() - 1) {
				attribs_str += ", ";
			}
		}
		tags_str += vformat("\t\t\t\t\"attribs\": [ %s ],\n", attribs_str);
		tags_str += vformat("\t\t\t\t\"match\": \"%s\",\n", TrenchbroomTag::get_match_key(brush_tag->get_tag_match_type()));
		tags_str += vformat("\t\t\t\t\"pattern\": \"%s\"", brush_tag->get_tag_pattern());
		if (!brush_tag->get_texture_name().is_empty()) {
			tags_str += ",\n";
			tags_str += vformat("\t\t\t\t\"material\": \"%s\"", brush_tag->get_texture_name());
		}
		tags_str += "\n";
		tags_str += "\t\t\t}";
		if (tag_index < p_tags.size() - 1) {
			tags_str += ",";
		}
	}

	if (game_config_version > CONFIG_LATEST && game_config_version < CONFIG_VERSION9) {
		tags_str = tags_str.replace("material", "texture");
	}
	return tags_str;
}

String TrenchbroomGameConfig::_get_game_config_v4_text() const {
	return R"({
	"version": 4,
	"name": "%s",
	"icon": "icon.png",
	"fileformats": [
		%s
	],
	"filesystem": {
		"searchpath": ".",
		"packageformat": { "extension": ".zip", "format": "zip" }
	},
	"textures": {
		"package": { "type": "directory", "root": "%s" },
		"format": { "extensions": ["jpg", "jpeg", "tga", "png", "D", "C"], "format": "image" },
		"excludes": [ %s ],
		"palette": "%s",
		"attribute": ["_tb_textures", "wad"]
	},
	"entities": {
		"definitions": [ %s ],
		"defaultcolor": "0.6 0.6 0.6 1.0",
		"modelformats": [ "bsp, mdl, md2" ],
		"scale": %s
	},
	"tags": {
		"brush": [
			%s
		],
		"brushface": [
			%s
		]
	},
	"faceattribs": {
		"defaults": {
			%s
		},
		"contentflags": [],
		"surfaceflags": []
	}
}
)";
}

String TrenchbroomGameConfig::_get_game_config_v9v8_text() const {
	return R"({
	"version": 9,
	"name": "%s",
	"icon": "icon.png",
	"fileformats": [
		%s
	],
	"filesystem": {
		"searchpath": ".",
		"packageformat": { "extension": ".zip", "format": "zip" }
	},
	"materials": {
		"root": "%s",
		"extensions": [".bmp", ".exr", ".hdr", ".jpeg", ".jpg", ".png", ".tga", ".webp", ".D", ".C"],
		"excludes": [ %s ],
		"palette": "%s",
		"attribute": "wad"
	},
	"entities": {
		"definitions": [ %s ],
		"defaultcolor": "0.6 0.6 0.6 1.0",
		"scale": %s,
		"setDefaultProperties": %s
	},
	"tags": {
		"brush": [
			%s
		],
		"brushface": [
			%s
		]
	},
	"faceattribs": {
		"defaults": {
			%s
		},
		"contentflags": [],
		"surfaceflags": []
	}
}
)";
}

String TrenchbroomGameConfig::_build_class_text() const {
	String map_formats_str;
	for (int i = 0; i < map_formats.size(); i++) {
		Dictionary map_format = map_formats[i];
		map_formats_str += vformat("{ \"format\": \"%s\"", String(map_format.get("format", "")));
		if (map_format.has("initialmap")) {
			map_formats_str += vformat(", \"initialmap\": \"%s\"", String(map_format.get("initialmap", "")));
		}
		if (i < map_formats.size() - 1) {
			map_formats_str += " },\n\t\t";
		} else {
			map_formats_str += " }";
		}
	}

	String texture_exclusion_patterns_str;
	for (int i = 0; i < texture_exclusion_patterns.size(); i++) {
		texture_exclusion_patterns_str += vformat("\"%s\"", String(texture_exclusion_patterns[i]));
		if (i < texture_exclusion_patterns.size() - 1) {
			texture_exclusion_patterns_str += ", ";
		}
	}

	const String fgd_filename_str = fgd_file.is_valid() ? vformat("\"%s.fgd\"", fgd_file->get_fgd_name()) : "\"\"";
	const String brush_tags_str = _parse_tags(brush_tags);
	const String brushface_tags_str = _parse_tags(brushface_tags);
	const String uv_scale_str = _parse_default_uv_scale(default_uv_scale);

	switch (game_config_version) {
		case CONFIG_LATEST:
		case CONFIG_VERSION8:
		case CONFIG_VERSION9: {
			String config_text = _get_game_config_v9v8_text();
			if (game_config_version == CONFIG_VERSION8) {
				config_text = config_text.replace(": 9,", ": 8,");
				config_text = config_text.replace("material", "texture");
			}
			return vformat(
					config_text,
					game_name,
					map_formats_str,
					textures_root_folder,
					texture_exclusion_patterns_str,
					palette_path,
					fgd_filename_str,
					entity_scale,
					set_default_properties ? "true" : "false",
					brush_tags_str,
					brushface_tags_str,
					uv_scale_str);
		}
		case CONFIG_VERSION4:
			return vformat(
					_get_game_config_v4_text(),
					game_name,
					map_formats_str,
					textures_root_folder,
					texture_exclusion_patterns_str,
					palette_path,
					fgd_filename_str,
					entity_scale,
					brush_tags_str,
					brushface_tags_str,
					uv_scale_str);
		default:
			ERR_PRINT("Unsupported Game Config Version!");
			return String();
	}
}

void TrenchbroomGameConfig::export_file() {
	const String config_folder = TrenchbroomLocalConfig::get_setting(TrenchbroomLocalConfig::PROPERTY_TRENCHBROOM_GAME_CONFIG_FOLDER);
	if (config_folder.is_empty()) {
		ERR_PRINT("Skipping export: No TrenchBroom Game folder");
		return;
	}
	if (fgd_file.is_null()) {
		ERR_PRINT("Skipping export: No FGD file");
		return;
	}

	if (!DirAccess::dir_exists_absolute(config_folder)) {
		print_line("Couldn't open directory, creating...");
		if (DirAccess::make_dir_recursive_absolute(config_folder) != OK) {
			ERR_PRINT("Skipping export: Failed to create directory");
			return;
		}
	}

	if (icon.is_valid()) {
		const String icon_path = config_folder.path_join("icon.png");
		print_line("Exporting icon to " + icon_path);
		Ref<Image> export_icon = icon->get_image();
		if (export_icon.is_valid()) {
			export_icon->resize(32, 32, Image::INTERPOLATE_LANCZOS);
			export_icon->save_png(icon_path);
		}
	} else {
		const String fallback_icon_path = TrenchbroomDefaults::resolve_defaults_path("icon32.png");
		if (!fallback_icon_path.is_empty()) {
			const String global_icon_path = ProjectSettings::get_singleton()->globalize_path(fallback_icon_path);
			Ref<Image> fallback_icon;
			fallback_icon.instantiate();
			if (fallback_icon->load(global_icon_path) == OK) {
				const String icon_path = config_folder.path_join("icon.png");
				print_line("Exporting icon to " + icon_path);
				fallback_icon->resize(32, 32, Image::INTERPOLATE_LANCZOS);
				fallback_icon->save_png(icon_path);
			}
		}
	}

	const String target_file_path = config_folder.path_join("GameConfig.cfg");
	print_line("Exporting TrenchBroom Game Config to " + target_file_path);
	Ref<FileAccess> file = FileAccess::open(target_file_path, FileAccess::WRITE);
	if (file.is_null()) {
		ERR_PRINT("Failed to open GameConfig.cfg for writing");
		return;
	}
	file->store_string(_build_class_text());
	file.unref();

	Ref<BlaziumFGDFile> export_fgd = fgd_file->duplicate();
	if (export_fgd.is_valid()) {
		export_fgd->set_generate_model_point_class_models(generate_model_point_class_models);
		const Error export_err = export_fgd->do_export_file(BlaziumFGDFile::EDITOR_TRENCHBROOM, config_folder);
		if (export_err != OK) {
			ERR_PRINT("Could not export FGD.");
		} else {
			print_line("TrenchBroom Game Config export complete\n");
		}
	}
}
