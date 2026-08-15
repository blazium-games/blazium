/**************************************************************************/
/*  dddbrowser_level.cpp                                                  */
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

#include "core/object/class_db.h"
#include "dddbrowser_level.h"

void DDDBrowserLevel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_scene_name", "name"), &DDDBrowserLevel::set_scene_name);
	ClassDB::bind_method(D_METHOD("get_scene_name"), &DDDBrowserLevel::get_scene_name);
	ClassDB::bind_method(D_METHOD("set_scene_id", "id"), &DDDBrowserLevel::set_scene_id);
	ClassDB::bind_method(D_METHOD("get_scene_id"), &DDDBrowserLevel::get_scene_id);
	ClassDB::bind_method(D_METHOD("set_author", "author"), &DDDBrowserLevel::set_author);
	ClassDB::bind_method(D_METHOD("get_author"), &DDDBrowserLevel::get_author);
	ClassDB::bind_method(D_METHOD("set_description", "description"), &DDDBrowserLevel::set_description);
	ClassDB::bind_method(D_METHOD("get_description"), &DDDBrowserLevel::get_description);
	ClassDB::bind_method(D_METHOD("set_version", "version"), &DDDBrowserLevel::set_version);
	ClassDB::bind_method(D_METHOD("get_version"), &DDDBrowserLevel::get_version);
	ClassDB::bind_method(D_METHOD("set_schema_version", "version"), &DDDBrowserLevel::set_schema_version);
	ClassDB::bind_method(D_METHOD("get_schema_version"), &DDDBrowserLevel::get_schema_version);
	ClassDB::bind_method(D_METHOD("set_rating", "rating"), &DDDBrowserLevel::set_rating);
	ClassDB::bind_method(D_METHOD("get_rating"), &DDDBrowserLevel::get_rating);
	ClassDB::bind_method(D_METHOD("set_thumbnail_url", "url"), &DDDBrowserLevel::set_thumbnail_url);
	ClassDB::bind_method(D_METHOD("get_thumbnail_url"), &DDDBrowserLevel::get_thumbnail_url);
	ClassDB::bind_method(D_METHOD("set_manifest_url", "url"), &DDDBrowserLevel::set_manifest_url);
	ClassDB::bind_method(D_METHOD("get_manifest_url"), &DDDBrowserLevel::get_manifest_url);
	ClassDB::bind_method(D_METHOD("set_skybox_uri", "uri"), &DDDBrowserLevel::set_skybox_uri);
	ClassDB::bind_method(D_METHOD("get_skybox_uri"), &DDDBrowserLevel::get_skybox_uri);
	ClassDB::bind_method(D_METHOD("set_skybox_px", "uri"), &DDDBrowserLevel::set_skybox_px);
	ClassDB::bind_method(D_METHOD("get_skybox_px"), &DDDBrowserLevel::get_skybox_px);
	ClassDB::bind_method(D_METHOD("set_skybox_nx", "uri"), &DDDBrowserLevel::set_skybox_nx);
	ClassDB::bind_method(D_METHOD("get_skybox_nx"), &DDDBrowserLevel::get_skybox_nx);
	ClassDB::bind_method(D_METHOD("set_skybox_py", "uri"), &DDDBrowserLevel::set_skybox_py);
	ClassDB::bind_method(D_METHOD("get_skybox_py"), &DDDBrowserLevel::get_skybox_py);
	ClassDB::bind_method(D_METHOD("set_skybox_ny", "uri"), &DDDBrowserLevel::set_skybox_ny);
	ClassDB::bind_method(D_METHOD("get_skybox_ny"), &DDDBrowserLevel::get_skybox_ny);
	ClassDB::bind_method(D_METHOD("set_skybox_pz", "uri"), &DDDBrowserLevel::set_skybox_pz);
	ClassDB::bind_method(D_METHOD("get_skybox_pz"), &DDDBrowserLevel::get_skybox_pz);
	ClassDB::bind_method(D_METHOD("set_skybox_nz", "uri"), &DDDBrowserLevel::set_skybox_nz);
	ClassDB::bind_method(D_METHOD("get_skybox_nz"), &DDDBrowserLevel::get_skybox_nz);
	ClassDB::bind_method(D_METHOD("set_skybox_rotation", "rotation"), &DDDBrowserLevel::set_skybox_rotation);
	ClassDB::bind_method(D_METHOD("get_skybox_rotation"), &DDDBrowserLevel::get_skybox_rotation);
	ClassDB::bind_method(D_METHOD("set_base_url", "url"), &DDDBrowserLevel::set_base_url);
	ClassDB::bind_method(D_METHOD("get_base_url"), &DDDBrowserLevel::get_base_url);
	ClassDB::bind_method(D_METHOD("set_export_movement_bounds", "enable"), &DDDBrowserLevel::set_export_movement_bounds);
	ClassDB::bind_method(D_METHOD("get_export_movement_bounds"), &DDDBrowserLevel::get_export_movement_bounds);
	ClassDB::bind_method(D_METHOD("set_bounds_min", "min"), &DDDBrowserLevel::set_bounds_min);
	ClassDB::bind_method(D_METHOD("get_bounds_min"), &DDDBrowserLevel::get_bounds_min);
	ClassDB::bind_method(D_METHOD("set_bounds_max", "max"), &DDDBrowserLevel::set_bounds_max);
	ClassDB::bind_method(D_METHOD("get_bounds_max"), &DDDBrowserLevel::get_bounds_max);
	ClassDB::bind_method(D_METHOD("set_gamemode_file", "file"), &DDDBrowserLevel::set_gamemode_file);
	ClassDB::bind_method(D_METHOD("get_gamemode_file"), &DDDBrowserLevel::get_gamemode_file);
	ClassDB::bind_method(D_METHOD("set_world_id", "id"), &DDDBrowserLevel::set_world_id);
	ClassDB::bind_method(D_METHOD("get_world_id"), &DDDBrowserLevel::get_world_id);
	ClassDB::bind_method(D_METHOD("set_world_position", "position"), &DDDBrowserLevel::set_world_position);
	ClassDB::bind_method(D_METHOD("get_world_position"), &DDDBrowserLevel::get_world_position);
	ClassDB::bind_method(D_METHOD("set_game_type", "type"), &DDDBrowserLevel::set_game_type);
	ClassDB::bind_method(D_METHOD("get_game_type"), &DDDBrowserLevel::get_game_type);
	ClassDB::bind_method(D_METHOD("set_use_autosave_notification", "use"), &DDDBrowserLevel::set_use_autosave_notification);
	ClassDB::bind_method(D_METHOD("get_use_autosave_notification"), &DDDBrowserLevel::get_use_autosave_notification);
	ClassDB::bind_method(D_METHOD("set_autosave_text", "text"), &DDDBrowserLevel::set_autosave_text);
	ClassDB::bind_method(D_METHOD("get_autosave_text"), &DDDBrowserLevel::get_autosave_text);
	ClassDB::bind_method(D_METHOD("set_autosave_x", "x"), &DDDBrowserLevel::set_autosave_x);
	ClassDB::bind_method(D_METHOD("get_autosave_x"), &DDDBrowserLevel::get_autosave_x);
	ClassDB::bind_method(D_METHOD("set_autosave_y", "y"), &DDDBrowserLevel::set_autosave_y);
	ClassDB::bind_method(D_METHOD("get_autosave_y"), &DDDBrowserLevel::get_autosave_y);
	ClassDB::bind_method(D_METHOD("set_autosave_font", "font"), &DDDBrowserLevel::set_autosave_font);
	ClassDB::bind_method(D_METHOD("get_autosave_font"), &DDDBrowserLevel::get_autosave_font);
	ClassDB::bind_method(D_METHOD("set_autosave_color", "color"), &DDDBrowserLevel::set_autosave_color);
	ClassDB::bind_method(D_METHOD("get_autosave_color"), &DDDBrowserLevel::get_autosave_color);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "scene_name"), "set_scene_name", "get_scene_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "scene_id"), "set_scene_id", "get_scene_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "author"), "set_author", "get_author");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "description", PROPERTY_HINT_MULTILINE_TEXT), "set_description", "get_description");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "version"), "set_version", "get_version");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "schema_version"), "set_schema_version", "get_schema_version");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "rating", PROPERTY_HINT_ENUM, "General,Moderate,Adult"), "set_rating", "get_rating");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "thumbnail_url"), "set_thumbnail_url", "get_thumbnail_url");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "manifest_url"), "set_manifest_url", "get_manifest_url");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "base_url"), "set_base_url", "get_base_url");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "game_type", PROPERTY_HINT_ENUM, "None,FPS"), "set_game_type", "get_game_type");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "gamemode_file", PROPERTY_HINT_FILE, "*.luau"), "set_gamemode_file", "get_gamemode_file");

	ADD_GROUP("World", "world_");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "world_id"), "set_world_id", "get_world_id");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "world_position"), "set_world_position", "get_world_position");

	ADD_GROUP("Skybox", "skybox_");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "skybox_uri"), "set_skybox_uri", "get_skybox_uri");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "skybox_px"), "set_skybox_px", "get_skybox_px");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "skybox_nx"), "set_skybox_nx", "get_skybox_nx");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "skybox_py"), "set_skybox_py", "get_skybox_py");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "skybox_ny"), "set_skybox_ny", "get_skybox_ny");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "skybox_pz"), "set_skybox_pz", "get_skybox_pz");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "skybox_nz"), "set_skybox_nz", "get_skybox_nz");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "skybox_rotation"), "set_skybox_rotation", "get_skybox_rotation");

	ADD_GROUP("Movement Bounds", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "export_movement_bounds"), "set_export_movement_bounds", "get_export_movement_bounds");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "bounds_min"), "set_bounds_min", "get_bounds_min");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "bounds_max"), "set_bounds_max", "get_bounds_max");

	ADD_GROUP("Autosave Notification", "autosave_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_autosave_notification"), "set_use_autosave_notification", "get_use_autosave_notification");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "autosave_text"), "set_autosave_text", "get_autosave_text");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "autosave_x"), "set_autosave_x", "get_autosave_x");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "autosave_y"), "set_autosave_y", "get_autosave_y");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "autosave_font"), "set_autosave_font", "get_autosave_font");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "autosave_color"), "set_autosave_color", "get_autosave_color");

	BIND_ENUM_CONSTANT(RATING_GENERAL);
	BIND_ENUM_CONSTANT(RATING_MODERATE);
	BIND_ENUM_CONSTANT(RATING_ADULT);
	BIND_ENUM_CONSTANT(GAME_TYPE_NONE);
	BIND_ENUM_CONSTANT(GAME_TYPE_FPS);
}

void DDDBrowserLevel::set_scene_name(const String &p_name) {
	scene_name = p_name;
}
String DDDBrowserLevel::get_scene_name() const {
	return scene_name;
}
void DDDBrowserLevel::set_scene_id(const String &p_id) {
	scene_id = p_id;
}
String DDDBrowserLevel::get_scene_id() const {
	return scene_id;
}
void DDDBrowserLevel::set_author(const String &p_author) {
	author = p_author;
}
String DDDBrowserLevel::get_author() const {
	return author;
}
void DDDBrowserLevel::set_description(const String &p_description) {
	description = p_description;
}
String DDDBrowserLevel::get_description() const {
	return description;
}
void DDDBrowserLevel::set_version(const String &p_version) {
	version = p_version;
}
String DDDBrowserLevel::get_version() const {
	return version;
}
void DDDBrowserLevel::set_schema_version(const String &p_version) {
	schema_version = p_version;
}
String DDDBrowserLevel::get_schema_version() const {
	return schema_version;
}
void DDDBrowserLevel::set_rating(Rating p_rating) {
	rating = p_rating;
}
DDDBrowserLevel::Rating DDDBrowserLevel::get_rating() const {
	return rating;
}
void DDDBrowserLevel::set_thumbnail_url(const String &p_url) {
	thumbnail_url = p_url;
}
String DDDBrowserLevel::get_thumbnail_url() const {
	return thumbnail_url;
}
void DDDBrowserLevel::set_manifest_url(const String &p_url) {
	manifest_url = p_url;
}
String DDDBrowserLevel::get_manifest_url() const {
	return manifest_url;
}
void DDDBrowserLevel::set_skybox_uri(const String &p_uri) {
	skybox_uri = p_uri;
}
String DDDBrowserLevel::get_skybox_uri() const {
	return skybox_uri;
}
void DDDBrowserLevel::set_skybox_px(const String &p_uri) {
	skybox_px = p_uri;
}
String DDDBrowserLevel::get_skybox_px() const {
	return skybox_px;
}
void DDDBrowserLevel::set_skybox_nx(const String &p_uri) {
	skybox_nx = p_uri;
}
String DDDBrowserLevel::get_skybox_nx() const {
	return skybox_nx;
}
void DDDBrowserLevel::set_skybox_py(const String &p_uri) {
	skybox_py = p_uri;
}
String DDDBrowserLevel::get_skybox_py() const {
	return skybox_py;
}
void DDDBrowserLevel::set_skybox_ny(const String &p_uri) {
	skybox_ny = p_uri;
}
String DDDBrowserLevel::get_skybox_ny() const {
	return skybox_ny;
}
void DDDBrowserLevel::set_skybox_pz(const String &p_uri) {
	skybox_pz = p_uri;
}
String DDDBrowserLevel::get_skybox_pz() const {
	return skybox_pz;
}
void DDDBrowserLevel::set_skybox_nz(const String &p_uri) {
	skybox_nz = p_uri;
}
String DDDBrowserLevel::get_skybox_nz() const {
	return skybox_nz;
}
void DDDBrowserLevel::set_skybox_rotation(const Vector3 &p_rot) {
	skybox_rotation = p_rot;
}
Vector3 DDDBrowserLevel::get_skybox_rotation() const {
	return skybox_rotation;
}
void DDDBrowserLevel::set_base_url(const String &p_url) {
	base_url = p_url;
}
String DDDBrowserLevel::get_base_url() const {
	return base_url;
}
void DDDBrowserLevel::set_export_movement_bounds(bool p_enable) {
	export_movement_bounds = p_enable;
}
bool DDDBrowserLevel::get_export_movement_bounds() const {
	return export_movement_bounds;
}
void DDDBrowserLevel::set_bounds_min(const Vector3 &p_min) {
	bounds_min = p_min;
}
Vector3 DDDBrowserLevel::get_bounds_min() const {
	return bounds_min;
}
void DDDBrowserLevel::set_bounds_max(const Vector3 &p_max) {
	bounds_max = p_max;
}
Vector3 DDDBrowserLevel::get_bounds_max() const {
	return bounds_max;
}
void DDDBrowserLevel::set_gamemode_file(const String &p_file) {
	gamemode_file = p_file;
}
String DDDBrowserLevel::get_gamemode_file() const {
	return gamemode_file;
}
void DDDBrowserLevel::set_world_id(const String &p_id) {
	world_id = p_id;
}
String DDDBrowserLevel::get_world_id() const {
	return world_id;
}
void DDDBrowserLevel::set_world_position(const Vector3 &p_pos) {
	world_position = p_pos;
}
Vector3 DDDBrowserLevel::get_world_position() const {
	return world_position;
}
void DDDBrowserLevel::set_game_type(GameType p_type) {
	game_type = p_type;
}
DDDBrowserLevel::GameType DDDBrowserLevel::get_game_type() const {
	return game_type;
}
void DDDBrowserLevel::set_use_autosave_notification(bool p_use) {
	use_autosave_notification = p_use;
}
bool DDDBrowserLevel::get_use_autosave_notification() const {
	return use_autosave_notification;
}
void DDDBrowserLevel::set_autosave_text(const String &p_text) {
	autosave_text = p_text;
}
String DDDBrowserLevel::get_autosave_text() const {
	return autosave_text;
}
void DDDBrowserLevel::set_autosave_x(float p_x) {
	autosave_x = p_x;
}
float DDDBrowserLevel::get_autosave_x() const {
	return autosave_x;
}
void DDDBrowserLevel::set_autosave_y(float p_y) {
	autosave_y = p_y;
}
float DDDBrowserLevel::get_autosave_y() const {
	return autosave_y;
}
void DDDBrowserLevel::set_autosave_font(const String &p_font) {
	autosave_font = p_font;
}
String DDDBrowserLevel::get_autosave_font() const {
	return autosave_font;
}
void DDDBrowserLevel::set_autosave_color(const Color &p_color) {
	autosave_color = p_color;
}
Color DDDBrowserLevel::get_autosave_color() const {
	return autosave_color;
}

String DDDBrowserLevel::rating_string() const {
	switch (rating) {
		case RATING_MODERATE:
			return "MODERATE";
		case RATING_ADULT:
			return "ADULT";
		default:
			return "GENERAL";
	}
}

String DDDBrowserLevel::game_type_string() const {
	return game_type == GAME_TYPE_FPS ? "FPS" : "NONE";
}

Dictionary DDDBrowserLevel::build_skybox_dictionary() const {
	Dictionary sky;
	const bool has_faces = !skybox_px.is_empty() && !skybox_nx.is_empty() && !skybox_py.is_empty() && !skybox_ny.is_empty() && !skybox_pz.is_empty() && !skybox_nz.is_empty();
	if (has_faces) {
		Dictionary faces;
		faces["px"] = skybox_px;
		faces["nx"] = skybox_nx;
		faces["py"] = skybox_py;
		faces["ny"] = skybox_ny;
		faces["pz"] = skybox_pz;
		faces["nz"] = skybox_nz;
		sky["faces"] = faces;
	} else if (!skybox_uri.is_empty()) {
		sky["uri"] = skybox_uri;
	} else {
		return Dictionary();
	}
	Dictionary rot;
	rot["x"] = skybox_rotation.x;
	rot["y"] = skybox_rotation.y;
	rot["z"] = skybox_rotation.z;
	sky["rotation"] = rot;
	return sky;
}

Dictionary DDDBrowserLevel::build_autosave_notification_dictionary() const {
	if (!use_autosave_notification) {
		return Dictionary();
	}
	Dictionary d;
	d["text"] = autosave_text;
	d["x"] = autosave_x;
	d["y"] = autosave_y;
	if (!autosave_font.is_empty()) {
		d["font"] = autosave_font;
	}
	Dictionary color;
	color["x"] = autosave_color.r;
	color["y"] = autosave_color.g;
	color["z"] = autosave_color.b;
	d["color"] = color;
	return d;
}
