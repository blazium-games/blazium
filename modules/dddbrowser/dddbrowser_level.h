/**************************************************************************/
/*  dddbrowser_level.h                                                    */
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

#include "scene/3d/node_3d.h"

class DDDBrowserLevel : public Node3D {
	GDCLASS(DDDBrowserLevel, Node3D);

public:
	enum Rating {
		RATING_GENERAL,
		RATING_MODERATE,
		RATING_ADULT,
	};

	enum GameType {
		GAME_TYPE_NONE,
		GAME_TYPE_FPS,
	};

private:
	String scene_name = "Exported Scene";
	String scene_id;
	String author;
	String description;
	String version = "1.0";
	String schema_version = "1.0";
	Rating rating = RATING_GENERAL;
	String thumbnail_url;
	String manifest_url;
	String skybox_uri;
	String skybox_px;
	String skybox_nx;
	String skybox_py;
	String skybox_ny;
	String skybox_pz;
	String skybox_nz;
	Vector3 skybox_rotation;
	String base_url;
	bool export_movement_bounds = false;
	Vector3 bounds_min = Vector3(-100, -100, -100);
	Vector3 bounds_max = Vector3(100, 100, 100);
	String gamemode_file;
	String world_id;
	Vector3 world_position;
	GameType game_type = GAME_TYPE_NONE;
	bool use_autosave_notification = false;
	String autosave_text = "Game Saved";
	float autosave_x = 0.0f;
	float autosave_y = 0.0f;
	String autosave_font;
	Color autosave_color = Color(1, 1, 1);

protected:
	static void _bind_methods();

public:
	void set_scene_name(const String &p_name);
	String get_scene_name() const;
	void set_scene_id(const String &p_id);
	String get_scene_id() const;
	void set_author(const String &p_author);
	String get_author() const;
	void set_description(const String &p_description);
	String get_description() const;
	void set_version(const String &p_version);
	String get_version() const;
	void set_schema_version(const String &p_version);
	String get_schema_version() const;
	void set_rating(Rating p_rating);
	Rating get_rating() const;
	void set_thumbnail_url(const String &p_url);
	String get_thumbnail_url() const;
	void set_manifest_url(const String &p_url);
	String get_manifest_url() const;
	void set_skybox_uri(const String &p_uri);
	String get_skybox_uri() const;
	void set_skybox_px(const String &p_uri);
	String get_skybox_px() const;
	void set_skybox_nx(const String &p_uri);
	String get_skybox_nx() const;
	void set_skybox_py(const String &p_uri);
	String get_skybox_py() const;
	void set_skybox_ny(const String &p_uri);
	String get_skybox_ny() const;
	void set_skybox_pz(const String &p_uri);
	String get_skybox_pz() const;
	void set_skybox_nz(const String &p_uri);
	String get_skybox_nz() const;
	void set_skybox_rotation(const Vector3 &p_rot);
	Vector3 get_skybox_rotation() const;
	void set_base_url(const String &p_url);
	String get_base_url() const;
	void set_export_movement_bounds(bool p_enable);
	bool get_export_movement_bounds() const;
	void set_bounds_min(const Vector3 &p_min);
	Vector3 get_bounds_min() const;
	void set_bounds_max(const Vector3 &p_max);
	Vector3 get_bounds_max() const;
	void set_gamemode_file(const String &p_file);
	String get_gamemode_file() const;
	void set_world_id(const String &p_id);
	String get_world_id() const;
	void set_world_position(const Vector3 &p_pos);
	Vector3 get_world_position() const;
	void set_game_type(GameType p_type);
	GameType get_game_type() const;
	void set_use_autosave_notification(bool p_use);
	bool get_use_autosave_notification() const;
	void set_autosave_text(const String &p_text);
	String get_autosave_text() const;
	void set_autosave_x(float p_x);
	float get_autosave_x() const;
	void set_autosave_y(float p_y);
	float get_autosave_y() const;
	void set_autosave_font(const String &p_font);
	String get_autosave_font() const;
	void set_autosave_color(const Color &p_color);
	Color get_autosave_color() const;

	String rating_string() const;
	String game_type_string() const;
	Dictionary build_skybox_dictionary() const;
	Dictionary build_autosave_notification_dictionary() const;
};

VARIANT_ENUM_CAST(DDDBrowserLevel::Rating);
VARIANT_ENUM_CAST(DDDBrowserLevel::GameType);
