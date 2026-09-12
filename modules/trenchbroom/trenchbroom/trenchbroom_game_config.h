/**************************************************************************/
/*  trenchbroom_game_config.h                                             */
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
#include "scene/resources/texture.h"

#include "modules/trenchbroom/fgd/blazium_fgd_file.h"
#include "trenchbroom_tag.h"

class TrenchbroomGameConfig : public Resource {
	GDCLASS(TrenchbroomGameConfig, Resource);

public:
	enum GameConfigVersion {
		CONFIG_LATEST,
		CONFIG_VERSION4,
		CONFIG_VERSION8,
		CONFIG_VERSION9,
	};

protected:
	static void _bind_methods();

	String game_name = "Blazium";
	Ref<Texture2D> icon;
	Array map_formats;
	String textures_root_folder = "textures";
	TypedArray<String> texture_exclusion_patterns;
	String palette_path = "textures/palette.lmp";
	Ref<BlaziumFGDFile> fgd_file;
	String entity_scale = "32";
	bool set_default_properties = false;
	bool generate_model_point_class_models = true;
	TypedArray<TrenchbroomTag> brush_tags;
	TypedArray<TrenchbroomTag> brushface_tags;
	Vector2 default_uv_scale = Vector2(1, 1);
	GameConfigVersion game_config_version = CONFIG_LATEST;

	String _build_class_text() const;
	String build_class_text() const { return _build_class_text(); }
	String _parse_tags(const TypedArray<TrenchbroomTag> &p_tags) const;
	String _parse_default_uv_scale(const Vector2 &p_texture_scale) const;
	String _get_game_config_v4_text() const;
	String _get_game_config_v9v8_text() const;
	Callable _get_export_file_func() const;

public:
	void set_game_name(const String &p_name) { game_name = p_name; }
	String get_game_name() const { return game_name; }

	void set_icon(const Ref<Texture2D> &p_icon) { icon = p_icon; }
	Ref<Texture2D> get_icon() const { return icon; }

	void set_map_formats(const Array &p_formats) { map_formats = p_formats; }
	Array get_map_formats() const { return map_formats; }

	void set_textures_root_folder(const String &p_folder) { textures_root_folder = p_folder; }
	String get_textures_root_folder() const { return textures_root_folder; }

	void set_texture_exclusion_patterns(const TypedArray<String> &p_patterns) { texture_exclusion_patterns = p_patterns; }
	TypedArray<String> get_texture_exclusion_patterns() const { return texture_exclusion_patterns; }

	void set_palette_path(const String &p_path) { palette_path = p_path; }
	String get_palette_path() const { return palette_path; }

	void set_fgd_file(const Ref<BlaziumFGDFile> &p_fgd) { fgd_file = p_fgd; }
	Ref<BlaziumFGDFile> get_fgd_file() const { return fgd_file; }

	void set_entity_scale(const String &p_scale) { entity_scale = p_scale; }
	String get_entity_scale() const { return entity_scale; }

	void set_set_default_properties(bool p_set) { set_default_properties = p_set; }
	bool get_set_default_properties() const { return set_default_properties; }

	void set_generate_model_point_class_models(bool p_generate) { generate_model_point_class_models = p_generate; }
	bool get_generate_model_point_class_models() const { return generate_model_point_class_models; }

	void set_brush_tags(const TypedArray<TrenchbroomTag> &p_tags) { brush_tags = p_tags; }
	TypedArray<TrenchbroomTag> get_brush_tags() const { return brush_tags; }

	void set_brushface_tags(const TypedArray<TrenchbroomTag> &p_tags) { brushface_tags = p_tags; }
	TypedArray<TrenchbroomTag> get_brushface_tags() const { return brushface_tags; }

	void set_default_uv_scale(const Vector2 &p_scale) { default_uv_scale = p_scale; }
	Vector2 get_default_uv_scale() const { return default_uv_scale; }

	void set_game_config_version(GameConfigVersion p_version) { game_config_version = p_version; }
	GameConfigVersion get_game_config_version() const { return game_config_version; }

	void export_file();
	TrenchbroomGameConfig();
};

VARIANT_ENUM_CAST(TrenchbroomGameConfig::GameConfigVersion);
