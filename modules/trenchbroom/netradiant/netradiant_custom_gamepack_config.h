/**************************************************************************/
/*  netradiant_custom_gamepack_config.h                                   */
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
#include "modules/trenchbroom/fgd/blazium_fgd_file.h"
#include "netradiant_custom_shader.h"

class NetRadiantCustomGamePackConfig : public Resource {
	GDCLASS(NetRadiantCustomGamePackConfig, Resource);

public:
	enum NetRadiantCustomMapType {
		MAP_TYPE_QUAKE_1,
		MAP_TYPE_QUAKE_3,
	};

protected:
	static void _bind_methods();

	String gamepack_name = "blazium";
	String game_name = "Blazium";
	String base_game_path;
	Ref<BlaziumFGDFile> fgd_file;
	bool generate_model_point_class_models = true;
	TypedArray<NetRadiantCustomShader> netradiant_custom_shaders;
	PackedStringArray model_types;
	PackedStringArray sound_types;
	NetRadiantCustomMapType map_type = MAP_TYPE_QUAKE_3;
	PackedStringArray texture_types;
	String default_scale = "1.0";
	String clip_texture = "textures/clip";
	String skip_texture = "textures/skip";
	Dictionary default_build_menu_variables;
	Dictionary default_build_menu_commands;

	String _build_shader_text() const;
	String _build_gamepack_text() const;
	Callable _get_export_file_func() const;

public:
	NetRadiantCustomGamePackConfig();

	void set_gamepack_name(const String &p_name) { gamepack_name = p_name.to_lower(); }
	String get_gamepack_name() const { return gamepack_name; }

	void set_game_name(const String &p_name) { game_name = p_name; }
	String get_game_name() const { return game_name; }

	void set_base_game_path(const String &p_path) { base_game_path = p_path; }
	String get_base_game_path() const { return base_game_path; }

	void set_fgd_file(const Ref<BlaziumFGDFile> &p_fgd) { fgd_file = p_fgd; }
	Ref<BlaziumFGDFile> get_fgd_file() const { return fgd_file; }

	void set_generate_model_point_class_models(bool p_generate) { generate_model_point_class_models = p_generate; }
	bool get_generate_model_point_class_models() const { return generate_model_point_class_models; }

	void set_netradiant_custom_shaders(const TypedArray<NetRadiantCustomShader> &p_shaders) { netradiant_custom_shaders = p_shaders; }
	TypedArray<NetRadiantCustomShader> get_netradiant_custom_shaders() const { return netradiant_custom_shaders; }

	void set_model_types(const PackedStringArray &p_types) { model_types = p_types; }
	PackedStringArray get_model_types() const { return model_types; }

	void set_sound_types(const PackedStringArray &p_types) { sound_types = p_types; }
	PackedStringArray get_sound_types() const { return sound_types; }

	void set_map_type(NetRadiantCustomMapType p_type) { map_type = p_type; }
	NetRadiantCustomMapType get_map_type() const { return map_type; }

	void set_texture_types(const PackedStringArray &p_types) { texture_types = p_types; }
	PackedStringArray get_texture_types() const { return texture_types; }

	void set_default_scale(const String &p_scale) { default_scale = p_scale; }
	String get_default_scale() const { return default_scale; }

	void set_clip_texture(const String &p_texture) { clip_texture = p_texture; }
	String get_clip_texture() const { return clip_texture; }

	void set_skip_texture(const String &p_texture) { skip_texture = p_texture; }
	String get_skip_texture() const { return skip_texture; }

	void set_default_build_menu_variables(const Dictionary &p_variables) { default_build_menu_variables = p_variables; }
	Dictionary get_default_build_menu_variables() const { return default_build_menu_variables; }

	void set_default_build_menu_commands(const Dictionary &p_commands) { default_build_menu_commands = p_commands; }
	Dictionary get_default_build_menu_commands() const { return default_build_menu_commands; }

	void export_file();
};

VARIANT_ENUM_CAST(NetRadiantCustomGamePackConfig::NetRadiantCustomMapType);
