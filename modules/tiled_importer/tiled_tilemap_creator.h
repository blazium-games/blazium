/**************************************************************************/
/*  tiled_tilemap_creator.h                                               */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
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

#include "scene/2d/tile_map.h"

// Equivalent to TilemapCreator.gd

#include "scene/2d/node_2d.h"
#include "scene/2d/parallax_2d.h"
#include "scene/2d/parallax_background.h"
#include "scene/2d/parallax_layer.h"
#include "scene/2d/tile_map_layer.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/texture_rect.h"
#include "tiled_common.h"
#include "tiled_custom_types.h"
#include "tileson_gd_bindings.h"

class Sprite2D;
class CollisionObject2D;

class TiledTilemapCreator {
public:
	enum GodotType {
		GODOT_TYPE_EMPTY,
		GODOT_TYPE_BODY,
		GODOT_TYPE_CBODY,
		GODOT_TYPE_RBODY,
		GODOT_TYPE_ABODY,
		GODOT_TYPE_AREA,
		GODOT_TYPE_NAVIGATION,
		GODOT_TYPE_OCCLUDER,
		GODOT_TYPE_LINE,
		GODOT_TYPE_PATH,
		GODOT_TYPE_POLYGON,
		GODOT_TYPE_INSTANCE,
		GODOT_TYPE_PARALLAX,
		GODOT_TYPE_UNKNOWN
	};

private:
	String _map_orientation;
	int _map_width = 0;
	int _map_height = 0;
	int _map_tile_width = 0;
	int _map_tile_height = 0;
	bool _infinite = false;
	int _parallax_origin_x = 0;
	int _parallax_origin_y = 0;
	String _background_color = "";

	TileMapLayer *_tilemap_layer = nullptr;
	Ref<TileSet> _tileset;
	String _current_tileset_orientation;
	String _current_object_alignment;
	Node2D *_base_node = nullptr;
	ParallaxBackground *_parallax_background = nullptr;
	ColorRect *_background = nullptr;
	bool _parallax_layer_existing = false;
	Ref<TiledCustomTypes> _custom_types;

	String _base_path;
	String _base_name;
	String _encoding;
	String _compression;
	Array _first_gids;
	Array _atlas_sources;
	bool _use_default_filter = false;
	bool _map_wangset_to_terrain = false;
	bool _add_class_as_metadata = false;
	bool _add_id_as_metadata = false;
	bool _dont_use_alternative_tiles = false;
	String _custom_data_prefix;
	String _tileset_save_path;
	Dictionary _object_groups;
	Ref<TiledMap> _base_map;
	Ref<TiledTileson> _tileson;

	float _iso_rot = 0.0f;
	float _iso_skew = 0.0f;
	Vector2 _iso_scale;

	int _godot_version = 0;

	static void recursively_change_owner(Node *p_node, Node *p_new_owner);

	void handle_layer(Ref<TiledLayer> p_layer, Node2D *p_parent);
	void handle_parallaxes(Node *p_parent, Node *p_layer_node, Ref<TiledLayer> p_layer_dict);
	Array handle_data(const Variant &p_data, int p_map_size);
	void create_polygons_on_alternative_tiles(TileData *p_source_data, TileData *p_target_data, int p_alt_id);
	void create_map_from_data(const Array &p_layer_data, int p_offset_x, int p_offset_y, int p_map_width);

	void handle_object(Ref<TiledObject> p_obj_ro, Node *p_layer_node, Ref<TileSet> p_tileset, const Vector2 &p_offset);
	void handle_properties(Node *p_target_node, const Array &p_properties);

	Vector2 get_instance_offset(float p_width, float p_height, const String &p_r_alignment, const String &p_c_alignment);
	Vector2 get_position_offset(float p_width, float p_height, float p_rotation);
	void set_sprite_offset(Sprite2D *p_sprite, float p_width, float p_height, const String &p_alignment);
	void convert_metadata_to_obj_properties(TileData *p_td, Ref<TiledObject> p_obj);

	PackedVector2Array polygon_from_array(const Array &p_poly_array);
	PackedVector2Array polygon_from_rectangle(float p_width, float p_height);
	PackedVector2Array get_rotated_polygon(const PackedVector2Array &p_polygon, float p_rotation);
	Vector2 transpose_coords(float p_x, float p_y, bool p_iso_offset = false);

	int get_atlas_source_index(int p_gid);
	int get_matching_source_id(int p_gid);
	Vector2i get_tile_offset(int p_gid);
	int get_first_gid_index(int p_gid);
	int get_num_tiles_for_source_id(int p_source_id);

	String get_tileset_orientation(int p_gid);
	String get_tileset_alignment(int p_gid);
	bool is_partitioned_tileset(int p_source_id);
	Ref<TiledLayer> get_object_group(int p_index);
	Ref<TiledObject> get_object(int p_index);
	PackedVector2Array get_object_polygon(int p_obj_id);
	void add_collision_shapes(CollisionObject2D *p_parent, Ref<TiledLayer> p_object_group, float p_tile_width, float p_tile_height, bool p_flipped_h, bool p_flipped_v, const Vector2 &p_scale);

public:
	TiledTilemapCreator();

	void set_use_default_filter(bool p_value) { _use_default_filter = p_value; }
	void set_add_class_as_metadata(bool p_value) { _add_class_as_metadata = p_value; }
	void set_add_id_as_metadata(bool p_value) { _add_id_as_metadata = p_value; }
	void set_no_alternative_tiles(bool p_value) { _dont_use_alternative_tiles = p_value; }
	void set_map_wangset_to_terrain(bool p_value) { _map_wangset_to_terrain = p_value; }
	void set_custom_data_prefix(const String &p_value) { _custom_data_prefix = p_value; }
	void set_save_tileset_to(const String &p_path) { _tileset_save_path = p_path; }
	void set_custom_types(Ref<TiledCustomTypes> p_custom_types) { _custom_types = p_custom_types; }

	Ref<TileSet> get_tileset() { return _tileset; }

	Node *create_tilemap(const String &p_source_file);

	static GodotType get_godot_type(const String &p_godot_type_string);
	static String get_godot_node_type_property(const Array &p_props, bool &r_property_found);
	static String get_godot_node_type_property(Ref<TiledObject> p_obj, bool &r_property_found);
	static GodotType get_godot_node_type(const String &p_class_type, const String &p_tson_type, const Array &p_props);
	static GodotType get_godot_node_type(Ref<TiledObject> p_obj);
	static GodotType get_godot_node_type(Ref<TiledLayer> p_layer);
};
