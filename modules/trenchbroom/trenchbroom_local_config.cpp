/**************************************************************************/
/*  trenchbroom_local_config.cpp                                          */
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

#include "trenchbroom_local_config.h"

#include "core/config/project_settings.h"
#include "core/error/error_macros.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "core/string/print_string.h"

static String _property_key(TrenchbroomLocalConfig::Property p_property) {
	switch (p_property) {
		case TrenchbroomLocalConfig::PROPERTY_FGD_OUTPUT_FOLDER:
			return "FGD_OUTPUT_FOLDER";
		case TrenchbroomLocalConfig::PROPERTY_TRENCHBROOM_GAME_CONFIG_FOLDER:
			return "TRENCHBROOM_GAME_CONFIG_FOLDER";
		case TrenchbroomLocalConfig::PROPERTY_NETRADIANT_CUSTOM_GAMEPACKS_FOLDER:
			return "NETRADIANT_CUSTOM_GAMEPACKS_FOLDER";
		case TrenchbroomLocalConfig::PROPERTY_MAP_EDITOR_GAME_PATH:
			return "MAP_EDITOR_GAME_PATH";
	}
	return String();
}

void TrenchbroomLocalConfig::_bind_methods() {
	ClassDB::bind_method(D_METHOD("reload_trenchbroom_settings"), &TrenchbroomLocalConfig::reload_trenchbroom_settings);
	ClassDB::bind_method(D_METHOD("export_trenchbroom_settings"), &TrenchbroomLocalConfig::export_trenchbroom_settings);
	ClassDB::bind_static_method("TrenchbroomLocalConfig", D_METHOD("get_setting", "name"), &TrenchbroomLocalConfig::get_setting);
	ClassDB::bind_method(D_METHOD("set_fgd_output_folder", "folder"), &TrenchbroomLocalConfig::set_fgd_output_folder);
	ClassDB::bind_method(D_METHOD("get_fgd_output_folder"), &TrenchbroomLocalConfig::get_fgd_output_folder);
	ClassDB::bind_method(D_METHOD("set_trenchbroom_game_config_folder", "folder"), &TrenchbroomLocalConfig::set_trenchbroom_game_config_folder);
	ClassDB::bind_method(D_METHOD("get_trenchbroom_game_config_folder"), &TrenchbroomLocalConfig::get_trenchbroom_game_config_folder);
	ClassDB::bind_method(D_METHOD("set_netradiant_custom_gamepacks_folder", "folder"), &TrenchbroomLocalConfig::set_netradiant_custom_gamepacks_folder);
	ClassDB::bind_method(D_METHOD("get_netradiant_custom_gamepacks_folder"), &TrenchbroomLocalConfig::get_netradiant_custom_gamepacks_folder);
	ClassDB::bind_method(D_METHOD("set_map_editor_game_path", "folder"), &TrenchbroomLocalConfig::set_map_editor_game_path);
	ClassDB::bind_method(D_METHOD("get_map_editor_game_path"), &TrenchbroomLocalConfig::get_map_editor_game_path);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "fgd_output_folder", PROPERTY_HINT_GLOBAL_DIR), "set_fgd_output_folder", "get_fgd_output_folder");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "trenchbroom_game_config_folder", PROPERTY_HINT_GLOBAL_DIR), "set_trenchbroom_game_config_folder", "get_trenchbroom_game_config_folder");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "netradiant_custom_gamepacks_folder", PROPERTY_HINT_GLOBAL_DIR), "set_netradiant_custom_gamepacks_folder", "get_netradiant_custom_gamepacks_folder");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "map_editor_game_path", PROPERTY_HINT_GLOBAL_DIR), "set_map_editor_game_path", "get_map_editor_game_path");

	BIND_ENUM_CONSTANT(PROPERTY_FGD_OUTPUT_FOLDER);
	BIND_ENUM_CONSTANT(PROPERTY_TRENCHBROOM_GAME_CONFIG_FOLDER);
	BIND_ENUM_CONSTANT(PROPERTY_NETRADIANT_CUSTOM_GAMEPACKS_FOLDER);
	BIND_ENUM_CONSTANT(PROPERTY_MAP_EDITOR_GAME_PATH);
}

Variant TrenchbroomLocalConfig::_get_default_value(Variant::Type p_type) const {
	switch (p_type) {
		case Variant::STRING:
			return String();
		case Variant::INT:
			return 0;
		case Variant::FLOAT:
			return 0.0;
		case Variant::BOOL:
			return false;
		case Variant::VECTOR2:
			return Vector2();
		case Variant::VECTOR3:
			return Vector3();
		case Variant::ARRAY:
			return Array();
		case Variant::DICTIONARY:
			return Dictionary();
		default:
			ERR_PRINT("Invalid setting type. Returning null");
			return Variant();
	}
}

void TrenchbroomLocalConfig::_try_loading() {
	if (!loaded) {
		const_cast<TrenchbroomLocalConfig *>(this)->reload_trenchbroom_settings();
	}
}

void TrenchbroomLocalConfig::set_fgd_output_folder(const String &p_folder) {
	settings_dict[_property_key(PROPERTY_FGD_OUTPUT_FOLDER)] = p_folder;
	loaded = true;
}

String TrenchbroomLocalConfig::get_fgd_output_folder() const {
	const_cast<TrenchbroomLocalConfig *>(this)->_try_loading();
	const String key = _property_key(PROPERTY_FGD_OUTPUT_FOLDER);
	if (settings_dict.has(key)) {
		return settings_dict[key];
	}
	return String();
}

void TrenchbroomLocalConfig::set_trenchbroom_game_config_folder(const String &p_folder) {
	settings_dict[_property_key(PROPERTY_TRENCHBROOM_GAME_CONFIG_FOLDER)] = p_folder;
	loaded = true;
}

String TrenchbroomLocalConfig::get_trenchbroom_game_config_folder() const {
	const_cast<TrenchbroomLocalConfig *>(this)->_try_loading();
	const String key = _property_key(PROPERTY_TRENCHBROOM_GAME_CONFIG_FOLDER);
	if (settings_dict.has(key)) {
		return settings_dict[key];
	}
	return String();
}

void TrenchbroomLocalConfig::set_netradiant_custom_gamepacks_folder(const String &p_folder) {
	settings_dict[_property_key(PROPERTY_NETRADIANT_CUSTOM_GAMEPACKS_FOLDER)] = p_folder;
	loaded = true;
}

String TrenchbroomLocalConfig::get_netradiant_custom_gamepacks_folder() const {
	const_cast<TrenchbroomLocalConfig *>(this)->_try_loading();
	const String key = _property_key(PROPERTY_NETRADIANT_CUSTOM_GAMEPACKS_FOLDER);
	if (settings_dict.has(key)) {
		return settings_dict[key];
	}
	return String();
}

void TrenchbroomLocalConfig::set_map_editor_game_path(const String &p_folder) {
	settings_dict[_property_key(PROPERTY_MAP_EDITOR_GAME_PATH)] = p_folder;
	loaded = true;
}

String TrenchbroomLocalConfig::get_map_editor_game_path() const {
	const_cast<TrenchbroomLocalConfig *>(this)->_try_loading();
	const String key = _property_key(PROPERTY_MAP_EDITOR_GAME_PATH);
	if (settings_dict.has(key)) {
		return settings_dict[key];
	}
	return String();
}

Variant TrenchbroomLocalConfig::get_setting(Property p_name) {
	Ref<TrenchbroomLocalConfig> settings;
	settings.instantiate();
	settings->reload_trenchbroom_settings();
	const String key = _property_key(p_name);
	if (settings->settings_dict.has(key)) {
		return settings->settings_dict[key];
	}
	return String();
}

void TrenchbroomLocalConfig::reload_trenchbroom_settings() {
	loaded = true;
	settings_dict = Dictionary();

	String path = "user://trenchbroom_config.json";
	if (!FileAccess::exists(path)) {
		String application_name = GLOBAL_GET("application/config/name");
		application_name = application_name.replace(" ", "_");
		path = "user://" + application_name + "_TrenchbroomConfig.json";
		if (!FileAccess::exists(path)) {
			return;
		}
	}

	const String settings_text = FileAccess::get_file_as_string(path);
	if (settings_text.is_empty()) {
		return;
	}

	Variant parsed = JSON::parse_string(settings_text);
	if (parsed.get_type() != Variant::DICTIONARY) {
		settings_dict = Dictionary();
		return;
	}

	Dictionary parsed_dict = parsed;
	for (const Variant &key : parsed_dict.keys()) {
		settings_dict[key] = parsed_dict[key];
	}
	notify_property_list_changed();
}

void TrenchbroomLocalConfig::export_trenchbroom_settings() {
	if (settings_dict.is_empty()) {
		return;
	}

	const String path = "user://trenchbroom_config.json";
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
	if (file.is_null()) {
		ERR_PRINT("Failed to open trenchbroom config for writing: " + path);
		return;
	}

	file->store_line(JSON::stringify(settings_dict));
	loaded = false;
	print_line("Saved settings to " + file->get_path_absolute());
}

bool TrenchbroomLocalConfig::_get(const StringName &p_name, Variant &r_ret) const {
	if (p_name == StringName("_export_settings_func")) {
		r_ret = callable_mp(const_cast<TrenchbroomLocalConfig *>(this), &TrenchbroomLocalConfig::export_trenchbroom_settings);
		return true;
	}
	if (p_name == StringName("_reload_settings_func")) {
		r_ret = callable_mp(const_cast<TrenchbroomLocalConfig *>(this), &TrenchbroomLocalConfig::reload_trenchbroom_settings);
		return true;
	}
	if (p_name == StringName("fgd_output_folder")) {
		r_ret = get_fgd_output_folder();
		return true;
	}
	if (p_name == StringName("trenchbroom_game_config_folder")) {
		r_ret = get_trenchbroom_game_config_folder();
		return true;
	}
	if (p_name == StringName("netradiant_custom_gamepacks_folder")) {
		r_ret = get_netradiant_custom_gamepacks_folder();
		return true;
	}
	if (p_name == StringName("map_editor_game_path")) {
		r_ret = get_map_editor_game_path();
		return true;
	}
	return false;
}

bool TrenchbroomLocalConfig::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == StringName("fgd_output_folder")) {
		set_fgd_output_folder(p_value);
		return true;
	}
	if (p_name == StringName("trenchbroom_game_config_folder")) {
		set_trenchbroom_game_config_folder(p_value);
		return true;
	}
	if (p_name == StringName("netradiant_custom_gamepacks_folder")) {
		set_netradiant_custom_gamepacks_folder(p_value);
		return true;
	}
	if (p_name == StringName("map_editor_game_path")) {
		set_map_editor_game_path(p_value);
		return true;
	}
	return false;
}

void TrenchbroomLocalConfig::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::CALLABLE, "_export_settings_func", PROPERTY_HINT_TOOL_BUTTON, "Export Trenchbroom Settings,Save", PROPERTY_USAGE_EDITOR));
	p_list->push_back(PropertyInfo(Variant::CALLABLE, "_reload_settings_func", PROPERTY_HINT_TOOL_BUTTON, "Reload Trenchbroom Settings,Reload", PROPERTY_USAGE_EDITOR));
}
