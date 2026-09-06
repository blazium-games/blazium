/**************************************************************************/
/*  netradiant_custom_gamepack_config.cpp                                 */
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

#include "netradiant_custom_gamepack_config.h"

#include "modules/trenchbroom/trenchbroom_local_config.h"

#include "core/error/error_macros.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/class_db.h"
#include "core/string/print_string.h"

static Ref<NetRadiantCustomShader> _make_default_shader(const String &p_texture_path, const String &p_attribute) {
	Ref<NetRadiantCustomShader> shader;
	shader.instantiate();
	shader->set_texture_path(p_texture_path);
	TypedArray<String> attributes;
	attributes.push_back(p_attribute);
	shader->set_shader_attributes(attributes);
	return shader;
}

NetRadiantCustomGamePackConfig::NetRadiantCustomGamePackConfig() {
	model_types = PackedStringArray({ "glb", "gltf", "obj" });
	sound_types = PackedStringArray({ "wav", "ogg" });
	texture_types = PackedStringArray({ "png", "jpg", "jpeg", "bmp", "tga" });

	if (netradiant_custom_shaders.is_empty()) {
		netradiant_custom_shaders.push_back(_make_default_shader("textures/clip", "qer_trans 0.4"));
		netradiant_custom_shaders.push_back(_make_default_shader("textures/skip", "qer_trans 0.4"));
		netradiant_custom_shaders.push_back(_make_default_shader("textures/origin", "qer_trans 0.4"));
	}
}

void NetRadiantCustomGamePackConfig::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_gamepack_name", "gamepack_name"), &NetRadiantCustomGamePackConfig::set_gamepack_name);
	ClassDB::bind_method(D_METHOD("get_gamepack_name"), &NetRadiantCustomGamePackConfig::get_gamepack_name);
	ClassDB::bind_method(D_METHOD("set_game_name", "game_name"), &NetRadiantCustomGamePackConfig::set_game_name);
	ClassDB::bind_method(D_METHOD("get_game_name"), &NetRadiantCustomGamePackConfig::get_game_name);
	ClassDB::bind_method(D_METHOD("set_base_game_path", "base_game_path"), &NetRadiantCustomGamePackConfig::set_base_game_path);
	ClassDB::bind_method(D_METHOD("get_base_game_path"), &NetRadiantCustomGamePackConfig::get_base_game_path);
	ClassDB::bind_method(D_METHOD("set_fgd_file", "fgd_file"), &NetRadiantCustomGamePackConfig::set_fgd_file);
	ClassDB::bind_method(D_METHOD("get_fgd_file"), &NetRadiantCustomGamePackConfig::get_fgd_file);
	ClassDB::bind_method(D_METHOD("set_generate_model_point_class_models", "generate_model_point_class_models"), &NetRadiantCustomGamePackConfig::set_generate_model_point_class_models);
	ClassDB::bind_method(D_METHOD("get_generate_model_point_class_models"), &NetRadiantCustomGamePackConfig::get_generate_model_point_class_models);
	ClassDB::bind_method(D_METHOD("set_netradiant_custom_shaders", "netradiant_custom_shaders"), &NetRadiantCustomGamePackConfig::set_netradiant_custom_shaders);
	ClassDB::bind_method(D_METHOD("get_netradiant_custom_shaders"), &NetRadiantCustomGamePackConfig::get_netradiant_custom_shaders);
	ClassDB::bind_method(D_METHOD("set_model_types", "model_types"), &NetRadiantCustomGamePackConfig::set_model_types);
	ClassDB::bind_method(D_METHOD("get_model_types"), &NetRadiantCustomGamePackConfig::get_model_types);
	ClassDB::bind_method(D_METHOD("set_sound_types", "sound_types"), &NetRadiantCustomGamePackConfig::set_sound_types);
	ClassDB::bind_method(D_METHOD("get_sound_types"), &NetRadiantCustomGamePackConfig::get_sound_types);
	ClassDB::bind_method(D_METHOD("set_map_type", "map_type"), &NetRadiantCustomGamePackConfig::set_map_type);
	ClassDB::bind_method(D_METHOD("get_map_type"), &NetRadiantCustomGamePackConfig::get_map_type);
	ClassDB::bind_method(D_METHOD("set_texture_types", "texture_types"), &NetRadiantCustomGamePackConfig::set_texture_types);
	ClassDB::bind_method(D_METHOD("get_texture_types"), &NetRadiantCustomGamePackConfig::get_texture_types);
	ClassDB::bind_method(D_METHOD("set_default_scale", "default_scale"), &NetRadiantCustomGamePackConfig::set_default_scale);
	ClassDB::bind_method(D_METHOD("get_default_scale"), &NetRadiantCustomGamePackConfig::get_default_scale);
	ClassDB::bind_method(D_METHOD("set_clip_texture", "clip_texture"), &NetRadiantCustomGamePackConfig::set_clip_texture);
	ClassDB::bind_method(D_METHOD("get_clip_texture"), &NetRadiantCustomGamePackConfig::get_clip_texture);
	ClassDB::bind_method(D_METHOD("set_skip_texture", "skip_texture"), &NetRadiantCustomGamePackConfig::set_skip_texture);
	ClassDB::bind_method(D_METHOD("get_skip_texture"), &NetRadiantCustomGamePackConfig::get_skip_texture);
	ClassDB::bind_method(D_METHOD("set_default_build_menu_variables", "default_build_menu_variables"), &NetRadiantCustomGamePackConfig::set_default_build_menu_variables);
	ClassDB::bind_method(D_METHOD("get_default_build_menu_variables"), &NetRadiantCustomGamePackConfig::get_default_build_menu_variables);
	ClassDB::bind_method(D_METHOD("set_default_build_menu_commands", "default_build_menu_commands"), &NetRadiantCustomGamePackConfig::set_default_build_menu_commands);
	ClassDB::bind_method(D_METHOD("get_default_build_menu_commands"), &NetRadiantCustomGamePackConfig::get_default_build_menu_commands);
	ClassDB::bind_method(D_METHOD("export_file"), &NetRadiantCustomGamePackConfig::export_file);
	ClassDB::bind_method(D_METHOD("_get_export_file_func"), &NetRadiantCustomGamePackConfig::_get_export_file_func);

	BIND_ENUM_CONSTANT(MAP_TYPE_QUAKE_1);
	BIND_ENUM_CONSTANT(MAP_TYPE_QUAKE_3);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "gamepack_name"), "set_gamepack_name", "get_gamepack_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "game_name"), "set_game_name", "get_game_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "base_game_path", PROPERTY_HINT_DIR), "set_base_game_path", "get_base_game_path");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "fgd_file", PROPERTY_HINT_RESOURCE_TYPE, "BlaziumFGDFile"), "set_fgd_file", "get_fgd_file");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "generate_model_point_class_models"), "set_generate_model_point_class_models", "get_generate_model_point_class_models");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "netradiant_custom_shaders", PROPERTY_HINT_ARRAY_TYPE, "NetRadiantCustomShader"), "set_netradiant_custom_shaders", "get_netradiant_custom_shaders");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "model_types"), "set_model_types", "get_model_types");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "sound_types"), "set_sound_types", "get_sound_types");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "map_type", PROPERTY_HINT_ENUM, "Quake 1,Quake 3"), "set_map_type", "get_map_type");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "texture_types"), "set_texture_types", "get_texture_types");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "default_scale"), "set_default_scale", "get_default_scale");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "clip_texture"), "set_clip_texture", "get_clip_texture");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "skip_texture"), "set_skip_texture", "get_skip_texture");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "default_build_menu_variables"), "set_default_build_menu_variables", "get_default_build_menu_variables");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "default_build_menu_commands"), "set_default_build_menu_commands", "get_default_build_menu_commands");
	ADD_PROPERTY(PropertyInfo(Variant::CALLABLE, "_export_file_func", PROPERTY_HINT_TOOL_BUTTON, "Export Gamepack", PROPERTY_USAGE_EDITOR), "", "_get_export_file_func");
}

Callable NetRadiantCustomGamePackConfig::_get_export_file_func() const {
	return callable_mp(const_cast<NetRadiantCustomGamePackConfig *>(this), &NetRadiantCustomGamePackConfig::export_file);
}

String NetRadiantCustomGamePackConfig::_build_shader_text() const {
	String shader_text;
	for (int i = 0; i < netradiant_custom_shaders.size(); i++) {
		Ref<NetRadiantCustomShader> shader = netradiant_custom_shaders[i];
		if (shader.is_null()) {
			continue;
		}
		shader_text += shader->get_texture_path() + "\n{\n";
		const TypedArray<String> attributes = shader->get_shader_attributes();
		for (int j = 0; j < attributes.size(); j++) {
			shader_text += "\t" + String(attributes[j]) + "\n";
		}
		shader_text += "}\n";
	}
	return shader_text;
}

String NetRadiantCustomGamePackConfig::_build_gamepack_text() const {
	String texturetypes_str;
	for (int i = 0; i < texture_types.size(); i++) {
		texturetypes_str += texture_types[i];
		if (i < texture_types.size() - 1) {
			texturetypes_str += " ";
		}
	}

	String modeltypes_str;
	for (int i = 0; i < model_types.size(); i++) {
		modeltypes_str += model_types[i];
		if (i < model_types.size() - 1) {
			modeltypes_str += " ";
		}
	}

	String soundtypes_str;
	for (int i = 0; i < sound_types.size(); i++) {
		soundtypes_str += sound_types[i];
		if (i < sound_types.size() - 1) {
			soundtypes_str += " ";
		}
	}

	const String maptype_str = map_type == MAP_TYPE_QUAKE_3 ? "mapq3" : "mapq1";

	return vformat(
			R"(<?xml version="1.0"?>
<game
  type="q3"
  index="1"
  name="%s"
  enginepath_win32="C:/%s/"
  engine_win32="%s.exe"
  enginepath_linux="/usr/local/games/%s/"
  engine_linux="%s"
  basegame="%s"
  basegamename="%s"
  unknowngamename="Custom %s modification"
  shaderpath="scripts"
  archivetypes="pk3"
  texturetypes="%s"
  modeltypes="%s"
  soundtypes="%s"
  maptypes="%s"
  shaders="quake3"
  entityclass="quake3"
  entityclasstype="fgd"
  entities="quake"
  brushtypes="quake"
  patchtypes="quake3"
  q3map2_type="quake3"
  default_scale="%s"
  shader_weapclip="%s"
  shader_caulk="%s"
  shader_nodraw="%s"
  shader_nodrawnonsolid="%s"
  common_shaders_name="Common"
  common_shaders_dir="common/"
/>
)",
			game_name,
			game_name,
			gamepack_name,
			game_name,
			gamepack_name,
			base_game_path,
			game_name,
			game_name,
			texturetypes_str,
			modeltypes_str,
			soundtypes_str,
			maptype_str,
			default_scale,
			clip_texture,
			skip_texture,
			clip_texture,
			skip_texture);
}

void NetRadiantCustomGamePackConfig::export_file() {
	const String game_path = TrenchbroomLocalConfig::get_setting(TrenchbroomLocalConfig::PROPERTY_MAP_EDITOR_GAME_PATH);
	if (game_path.is_empty()) {
		ERR_PRINT("Skipping export: Map Editor Game Path not set in Project Configuration");
		return;
	}

	const String gamepacks_folder = TrenchbroomLocalConfig::get_setting(TrenchbroomLocalConfig::PROPERTY_NETRADIANT_CUSTOM_GAMEPACKS_FOLDER);
	if (gamepacks_folder.is_empty()) {
		ERR_PRINT("Skipping export: No NetRadiant Custom gamepacks folder");
		return;
	}

	if (fgd_file.is_null()) {
		ERR_PRINT("Skipping export: No FGD file");
		return;
	}

	if (DirAccess::open(gamepacks_folder.path_join("games")).is_null()) {
		ERR_PRINT("Skipping export: No 'games' folder. Is this the NetRadiant Custom gamepacks folder?");
		return;
	}

	const PackedStringArray gamepack_dir_paths = {
		gamepacks_folder.path_join(gamepack_name + ".game"),
		gamepacks_folder.path_join(gamepack_name + ".game").path_join(base_game_path),
		gamepacks_folder.path_join(gamepack_name + ".game/scripts"),
		game_path.path_join("scripts"),
	};

	for (int i = 0; i < gamepack_dir_paths.size(); i++) {
		const String path = gamepack_dir_paths[i];
		if (DirAccess::open(path).is_null()) {
			print_line("Couldn't open " + path + ", creating...");
			if (DirAccess::make_dir_recursive_absolute(path) != OK) {
				ERR_PRINT("Skipping export: Failed to create directory");
				return;
			}
		}
	}

	String target_file_path = gamepacks_folder.path_join("games").path_join(gamepack_name + ".game");
	print_line("Exporting NetRadiant Custom Gamepack to " + target_file_path);
	Ref<FileAccess> file = FileAccess::open(target_file_path, FileAccess::WRITE);
	if (file.is_valid()) {
		file->store_string(_build_gamepack_text());
	} else {
		ERR_PRINT("Error: Could not modify " + target_file_path);
	}

	const String shader_text = _build_shader_text();

	target_file_path = gamepacks_folder.path_join(gamepack_name + ".game/scripts").path_join(gamepack_name + ".shader");
	print_line("Exporting NetRadiant Custom shader definitions to " + target_file_path);
	file = FileAccess::open(target_file_path, FileAccess::WRITE);
	if (file.is_valid()) {
		file->store_string(shader_text);
	} else {
		ERR_PRINT("Error: Could not modify " + target_file_path);
	}

	target_file_path = game_path.path_join("scripts").path_join(gamepack_name + ".shader");
	print_line("Exporting NetRadiant Custom shader definitions to " + target_file_path);
	file = FileAccess::open(target_file_path, FileAccess::WRITE);
	if (file.is_valid()) {
		file->store_string(shader_text);
	} else {
		ERR_PRINT("Error: could not modify " + target_file_path);
	}

	target_file_path = gamepacks_folder.path_join(gamepack_name + ".game/scripts").path_join("shaderlist.txt");
	print_line("Exporting NetRadiant Custom shader list to " + target_file_path);
	file = FileAccess::open(target_file_path, FileAccess::WRITE);
	if (file.is_valid()) {
		file->store_string(gamepack_name);
	} else {
		ERR_PRINT("Error: Could not modify " + target_file_path);
	}

	target_file_path = game_path.path_join("scripts/shaderlist.txt");
	print_line("Exporting NetRadiant Custom shader list to " + target_file_path);
	file = FileAccess::open(target_file_path, FileAccess::WRITE);
	if (file.is_valid()) {
		file->store_string(gamepack_name);
	} else {
		ERR_PRINT("Error: Could not modify " + target_file_path);
	}

	target_file_path = gamepacks_folder.path_join(gamepack_name + ".game").path_join("default_build_menu.xml");
	print_line("Exporting NetRadiant Custom default build menu to " + target_file_path);
	file = FileAccess::open(target_file_path, FileAccess::WRITE);
	if (file.is_valid()) {
		file->store_string("<?xml version=\"1.0\"?>\n<project version=\"2.0\">\n");

		const Array variable_keys = default_build_menu_variables.keys();
		for (int i = 0; i < variable_keys.size(); i++) {
			const Variant key = variable_keys[i];
			if (key.get_type() != Variant::STRING) {
				ERR_PRINT(vformat("Variable '%s' is an invalid key type", key));
				continue;
			}
			const Variant value = default_build_menu_variables[key];
			if (value.get_type() != Variant::STRING) {
				ERR_PRINT(vformat("Variable key '%s' value is invalid type", key));
				continue;
			}
			file->store_string(vformat("\t<var name=\"%s\">%s</var>\n", String(key), String(value)));
		}

		const Array command_keys = default_build_menu_commands.keys();
		for (int i = 0; i < command_keys.size(); i++) {
			const Variant key = command_keys[i];
			if (key.get_type() != Variant::STRING) {
				ERR_PRINT(vformat("Build option '%s' is an invalid type", key));
				continue;
			}
			file->store_string(vformat("\t<build name=\"%s\">\n", String(key)));
			const Variant command_value = default_build_menu_commands[key];
			if (command_value.get_type() == Variant::STRING) {
				file->store_string(vformat("\t\t<command>%s</command>\n\t</build>\n", String(command_value)));
			} else if (command_value.get_type() == Variant::ARRAY) {
				const Array commands = command_value;
				for (int j = 0; j < commands.size(); j++) {
					if (commands[j].get_type() != Variant::STRING) {
						ERR_PRINT(vformat("Build option '%s' has invalid command", key));
						continue;
					}
					file->store_string(vformat("\t\t<command>%s</command>\n", String(commands[j])));
				}
				file->store_string("\t</build>\n");
			}
		}

		file->store_string("</project>");
	}

	Ref<BlaziumFGDFile> export_fgd = fgd_file->duplicate();
	if (export_fgd.is_valid()) {
		export_fgd->set_generate_model_point_class_models(generate_model_point_class_models);
		const Error export_err = export_fgd->do_export_file(
				BlaziumFGDFile::EDITOR_NET_RADIANT_CUSTOM,
				gamepacks_folder.path_join(gamepack_name + ".game").path_join(base_game_path));
		if (export_err != OK) {
			ERR_PRINT("Could not export FGD.");
		} else {
			print_line("NetRadiant Custom Gamepack export complete\n");
		}
	}
}
